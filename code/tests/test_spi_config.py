"""Verify the SPI bus configuration in the .ioc files matches design_review/hardware.md
("SPI Bus Configuration: Wing Master <-> Main Slave").
"""

import unittest
from pathlib import Path

from ioc_parser import parse_ioc, spi_instances, spi_pins, dma_request

REPO_ROOT = Path(__file__).resolve().parents[2]

NOPULL = "GPIO_NOPULL"
PULLUP = "GPIO_PULLUP"

FUNCTIONS = ("NSS", "SCK", "MOSI", "MISO")


def boards() -> dict[str, dict]:
    """Map board name -> parsed .ioc settings, e.g. {"main-g474": {...}, "wing-g474": {...}}."""
    return {board: parse_ioc(REPO_ROOT / "code" / board / f"{board}.ioc") 
            for board in ("main-g474", "wing-g474")}


def label(settings: dict, pin: str) -> str | None:
    return settings.get(f"{pin}.GPIO_Label")


def all_spi_pins(settings: dict) -> dict[tuple[str, str], str]:
    """Map (SPI instance, signal function) -> GPIO pin name across every instance in settings."""
    return {
        (instance, func): pin
        for instance in spi_instances(settings)
        for func, pin in spi_pins(settings, instance).items()
    }


def bus_prefixes(settings: dict) -> dict[str, set[str]]:
    """Map SPI instance -> set of label prefixes shared by all its NSS/SCK/MOSI/MISO pins."""
    prefixes: dict[str, set[str]] = {}
    for (instance, func), pin in all_spi_pins(settings).items():
        prefix = label(settings, pin)[: -len(f"_{func}")]
        prefixes.setdefault(instance, set()).add(prefix)
    return prefixes


class TestSpiLabels(unittest.TestCase):
    """Every SPI-routed pin must carry a User Label whose suffix matches its
    function, and all pins of one bus must share a common label prefix.
    """

    def test_spi_pin_labels(self):
        for board, settings in boards().items():
            prefixes: dict[str, set[str]] = {}
            for (instance, func), pin in all_spi_pins(settings).items():
                with self.subTest(board=board, pin=pin, signal=f"{instance}_{func}"):
                    pin_label = label(settings, pin)
                    self.assertIsNotNone(pin_label, f"{pin} ({instance}_{func}) has no GPIO_Label")
                    self.assertTrue(
                        pin_label.endswith(f"_{func}"),
                        f"{pin}: label {pin_label!r} does not end with _{func} (signal is {instance}_{func})",
                    )
                    prefixes.setdefault(instance, set()).add(pin_label[: -len(f"_{func}")])

            for instance, names in prefixes.items():
                with self.subTest(board=board, instance=instance):
                    self.assertEqual(len(names), 1, f"{instance} pins have inconsistent label prefixes: {names}")


class TestSpiRoles(unittest.TestCase):
    """Each wing is SPI master, the motherboard is SPI slave on both buses, and CRC/frame
    settings must be identical on both ends of a bus for the link to work.
    """

    def test_spi_roles(self):
        for board, settings in boards().items():
            for instance, spi in spi_instances(settings).items():
                with self.subTest(board=board, spi=instance):
                    pins = spi_pins(settings, instance)
                    if board == "main-g474":
                        self.assertEqual(spi["Mode"], "SPI_MODE_SLAVE")
                        self.assertEqual(spi["VirtualType"], "VM_SLAVE")
                        self.assertEqual(settings[f"{pins['NSS']}.Mode"], "NSS_Signal_Hard_Input")
                    elif board == "wing-g474":
                        self.assertEqual(spi["Mode"], "SPI_MODE_MASTER")
                        self.assertEqual(spi["VirtualType"], "VM_MASTER")
                        self.assertEqual(settings[f"{pins['NSS']}.Mode"], "NSS_Signal_Hard_Output")
                    else:
                        self.fail(f"unexpected board {board!r}")


class TestSpiParameters(unittest.TestCase):
    """CRC and frame settings must be identical on both ends of a bus for the link to work."""

    def test_spi_parameters(self):
        for board, settings in boards().items():
            for instance, spi in spi_instances(settings).items():
                with self.subTest(board=board, spi=instance):
                    self.assertEqual(spi["CRCCalculation"], "SPI_CRCCALCULATION_ENABLE")
                    self.assertEqual(spi["CRCLength"], "SPI_CRC_LENGTH_16BIT")
                    self.assertEqual(spi["DataSize"], "SPI_DATASIZE_16BIT")
                    self.assertEqual(spi["Direction"], "SPI_DIRECTION_2LINES")
                    # CubeMX represents the default CRC polynomial as "X0+X1+X2" (= 0x0007),
                    # matching SPI_CRCPR's hardware reset value (RM0440 SPIx_CRCPR). It may
                    # be present explicitly or simply omitted from the .ioc when left default.
                    polynomial = spi.get("CRCPolynomial", "X0+X1+X2")
                    self.assertEqual(polynomial, "X0+X1+X2", f"{board} {instance}: non-default CRCPolynomial")


class TestSpiCrcNssPulse(unittest.TestCase):
    """RM0440: NSS pulse mode (NSSP) must not be enabled together with CRC
    calculation -- with both set, the SPI BSY flag never clears at the end of
    a transfer, so HAL_SPI_Transmit/Receive with HAL_MAX_DELAY hangs forever.

    NSSPMode is absent from both boards' IPParameters, so CubeMX falls back
    to its per-role default when generating MX_SPIx_Init: master with
    hardware NSS output defaults to SPI_NSS_PULSE_ENABLE, everything else to
    SPI_NSS_PULSE_DISABLE (as seen in the generated Core/Src/main.c on both
    boards).
    """

    def test_no_nss_pulse_with_crc(self):
        for board, settings in boards().items():
            for instance, spi in spi_instances(settings).items():
                with self.subTest(board=board, spi=instance):
                    if spi.get("CRCCalculation") != "SPI_CRCCALCULATION_ENABLE":
                        continue
                    if "NSSPMode" in spi:
                        nsspmode = spi["NSSPMode"]
                    else:
                        master_hard_nss = (
                            spi.get("Mode") == "SPI_MODE_MASTER"
                            and spi.get("VirtualNSS") == "VM_NSSHARD"
                        )
                        nsspmode = "SPI_NSS_PULSE_ENABLE" if master_hard_nss else "SPI_NSS_PULSE_DISABLE"
                    self.assertNotEqual(
                        nsspmode, "SPI_NSS_PULSE_ENABLE",
                        f"{board} {instance}: NSS pulse mode enabled with CRC enabled "
                        "(RM0440: invalid combination, hangs HAL_SPI_Transmit)",
                    )


class TestSpiInterruptEnabled(unittest.TestCase):
    """Slaves receive wing frames with interrupt-driven HAL_SPI_Receive_IT (re-armed
    in the RxCplt/Error callbacks, see main-g474 Core/Src/main.c), so every SPI in
    slave mode must have its global interrupt enabled in the .ioc NVIC -- otherwise
    the generated SPIx_IRQHandler / NVIC enable are absent and no frame ever
    completes. Masters transmit with the blocking HAL_SPI_Transmit and don't need it.
    """

    def test_slave_spi_interrupts_enabled(self):
        for board, settings in boards().items():
            for instance, spi in spi_instances(settings).items():
                if spi["Mode"] != "SPI_MODE_SLAVE":
                    continue
                with self.subTest(board=board, spi=instance):
                    # CubeMX encodes the NVIC line as "<enabled>:<preempt>:<sub>:...".
                    nvic = settings.get(f"NVIC.{instance}_IRQn")
                    self.assertIsNotNone(nvic, f"{instance} global interrupt missing from NVIC")
                    self.assertTrue(
                        nvic.startswith("true"),
                        f"{instance} global interrupt not enabled in NVIC (interrupt-driven receive needs it)",
                    )


class TestSpiSlaveDma(unittest.TestCase):
    """Slaves store incoming frames with HAL_SPI_Receive_DMA (re-armed per frame,
    see main-g474 Core/Src/main.c), so every SPI in slave mode needs an SPIx_RX DMA
    request: peripheral->memory, memory-incrementing, 16-bit (halfword) on both
    ends, and in NORMAL mode -- the per-frame re-arm and CRC check rely on the
    transfer completing, not a circular buffer wrapping. The channel's interrupt
    must also be enabled (it delivers transfer-complete and reads the CRC word).
    Masters transmit with the blocking HAL_SPI_Transmit and need no DMA.
    """

    def test_slave_rx_dma_configured(self):
        for board, settings in boards().items():
            for instance, spi in spi_instances(settings).items():
                if spi["Mode"] != "SPI_MODE_SLAVE":
                    continue
                with self.subTest(board=board, spi=instance):
                    dma = dma_request(settings, f"{instance}_RX")
                    self.assertIsNotNone(dma, f"{instance}_RX DMA request not configured in .ioc")
                    self.assertEqual(dma.get("Direction"), "DMA_PERIPH_TO_MEMORY")
                    self.assertEqual(dma.get("MemInc"), "DMA_MINC_ENABLE")
                    self.assertEqual(dma.get("MemDataAlignment"), "DMA_MDATAALIGN_HALFWORD")
                    self.assertEqual(dma.get("PeriphDataAlignment"), "DMA_PDATAALIGN_HALFWORD")
                    self.assertEqual(
                        dma.get("Mode"), "DMA_NORMAL",
                        f"{instance}_RX DMA must be NORMAL mode (per-frame re-arm), not circular",
                    )
                    channel = dma.get("Instance")
                    self.assertIsNotNone(channel, f"{instance}_RX DMA has no channel Instance")
                    nvic = settings.get(f"NVIC.{channel}_IRQn")
                    self.assertIsNotNone(nvic, f"{channel} interrupt missing from NVIC")
                    self.assertTrue(
                        nvic.startswith("true"),
                        f"{channel} interrupt (for {instance}_RX DMA completion) not enabled in NVIC",
                    )


class TestPullups(unittest.TestCase):
    """Pull-up placement per hardware.md: NSS and MISO pulled up on the slave
    (motherboard) side only; SCK/MOSI/MISO/NSS unpulled on the master (wing) side.
    """

    def test_pullups(self):
        for board, settings in boards().items():
            for instance, spi in spi_instances(settings).items():
                pins = spi_pins(settings, instance)

                def pupd(pin: str) -> str:
                    return settings.get(f"{pin}.GPIO_PuPd", NOPULL)

                with self.subTest(board=board, spi=instance):
                    if spi["Mode"] == "SPI_MODE_SLAVE":
                        # Slave side: NSS and MISO pulled up, SCK/MOSI unpulled.
                        self.assertEqual(pupd(pins["NSS"]), PULLUP, "NSS should have an internal pull-up")
                        self.assertEqual(pupd(pins["MISO"]), PULLUP, "MISO should have an internal pull-up")
                        self.assertEqual(pupd(pins["SCK"]), NOPULL, "SCK should have no pull")
                        self.assertEqual(pupd(pins["MOSI"]), NOPULL, "MOSI should have no pull")
                    elif spi["Mode"] == "SPI_MODE_MASTER":
                        # Master side: push-pull outputs and the unused MISO input are all unpulled;
                        # the slave-side pull-ups are sufficient.
                        for func in ("NSS", "SCK", "MOSI", "MISO"):
                            self.assertEqual(pupd(pins[func]), NOPULL, f"{func} should have no pull")
                    else:
                        self.fail(f"{board} {instance}: unexpected SPI Mode {spi['Mode']!r}")


if __name__ == "__main__":
    unittest.main()
