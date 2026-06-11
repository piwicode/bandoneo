"""Verify the SPI bus configuration in the .ioc files matches design_review/hardware.md
("SPI Bus Configuration: Wing Master <-> Main Slave").
"""

import unittest
from pathlib import Path

from ioc_parser import parse_ioc, spi_instances, spi_pins

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
