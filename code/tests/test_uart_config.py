"""Verify the USART1 debug-console configuration in the .ioc files.

Both boards expose USART1 on the STDC14 VCP pair (PA9/PA10 on main-g474,
PA10/PB6 on wing-g474) and are read with the same `just console` recipe
(code/*/justfile), so they must agree on a baud rate.
"""

import unittest
from pathlib import Path

from ioc_parser import parse_ioc

REPO_ROOT = Path(__file__).resolve().parents[2]

CONSOLE_BAUD_RATE = "921600"


def boards() -> dict[str, dict]:
    """Map board name -> parsed .ioc settings, e.g. {"main-g474": {...}, "wing-g474": {...}}."""
    return {board: parse_ioc(REPO_ROOT / "code" / board / f"{board}.ioc")
            for board in ("main-g474", "wing-g474")}


class TestConsoleUart(unittest.TestCase):
    """USART1 must be asynchronous and run at CONSOLE_BAUD_RATE on both boards."""

    def test_uart_baud_rate(self):
        for board, settings in boards().items():
            with self.subTest(board=board):
                self.assertEqual(settings.get("USART1.VirtualMode-Asynchronous"), "VM_ASYNC")
                self.assertEqual(settings.get("USART1.BaudRate"), CONSOLE_BAUD_RATE)


if __name__ == "__main__":
    unittest.main()
