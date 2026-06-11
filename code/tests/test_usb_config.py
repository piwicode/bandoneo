"""Verify the main board .ioc settings that the TinyUSB MIDI stack relies on
(code/main-g474/usb/): USB device FS enabled, in-spec 48 MHz clock, and no
CubeMX-generated USB interrupt handlers.
"""

import unittest
from pathlib import Path

from ioc_parser import parse_ioc

REPO_ROOT = Path(__file__).resolve().parents[2]


def main_settings() -> dict[str, str]:
    return parse_ioc(REPO_ROOT / "code" / "main-g474" / "main-g474.ioc")


class TestUsbPeripheral(unittest.TestCase):
    """The USB device (FS) peripheral must be configured correctly: enabled in
    the Mcu.IP list, with DP/DM pins wired (PA11/PA12), and with interrupt
    handlers NOT enabled in the NVIC tab (TinyUSB owns those).
    """

    def test_usb_ip_enabled(self):
        settings = main_settings()
        ips = {v for k, v in settings.items() if k.startswith("Mcu.IP")}
        self.assertIn("USB", ips, "USB peripheral removed from the .ioc")

        self.assertEqual(settings.get("PA11.Signal"), "USB_DM")
        self.assertEqual(settings.get("PA12.Signal"), "USB_DP")
        self.assertEqual(settings.get("PA11.GPIO_Label"), "USB_DM")
        self.assertEqual(settings.get("PA12.GPIO_Label"), "USB_DP")

    def test_usb_irqs_not_in_nvic(self):
        """TinyUSB defines USB_HP/USB_LP/USBWakeUp_IRQHandler in
        code/main-g474/usb/usb_app.c (and enables the NVIC lines itself).
        CubeMX must not generate conflicting handlers in stm32g4xx_it.c.
        """
        settings = main_settings()
        for irq in ("USB_HP_IRQn", "USB_LP_IRQn", "USBWakeUp_IRQn"):
            with self.subTest(irq=irq):
                # Enabled IRQs appear as e.g. "NVIC.USB_LP_IRQn=true\:0\:...";
                # absence (or a leading "false") means not generated.
                value = settings.get(f"NVIC.{irq}", "false")
                self.assertFalse(
                    value.startswith("true"),
                    f"{irq} enabled in the .ioc NVIC tab; conflicts with the TinyUSB handlers",
                )


class TestUsbClock(unittest.TestCase):
    """The board is crystal-less, so the USB kernel clock must be HSI48 trimmed
    by the CRS against USB SOF. The PLL alternative is fed by HSI16 (±1%),
    outside the USB FS ±0.25% budget, and would make enumeration unreliable.
    """

    def test_usb_clock_source_is_hsi48(self):
        settings = main_settings()
        # "CK48CLockSelection" [sic] is CubeMX's key for the USB clock mux; it
        # is absent from the .ioc when left at its default (PLL "Q").
        self.assertEqual(
            settings.get("RCC.CK48CLockSelection"), "RCC_USBCLKSOURCE_HSI48",
            "USB clock mux is not HSI48 (absent key means the default, PLLQ/HSI16)",
        )
        self.assertEqual(settings.get("RCC.USBFreq_Value"), "48000000")

    def test_crs_sync_on_usb_sof(self):
        settings = main_settings()
        # The RCC_VS_USB virtual pin appears when "CRS SYNC Source USB" is
        # selected in the RCC mode panel; without CRS, HSI48 free-runs at ±3%.
        self.assertEqual(
            settings.get("VP_RCC_VS_USB.Signal"), "RCC_VS_USB",
            "CRS SYNC is not enabled (RCC mode panel: CRS SYNC Source USB)",
        )
        self.assertEqual(settings.get("VP_RCC_VS_USB.Mode"), "USB_FS")
        self.assertEqual(settings.get("RCC.CRSFreq_Value"), "48000000")


if __name__ == "__main__":
    unittest.main()
