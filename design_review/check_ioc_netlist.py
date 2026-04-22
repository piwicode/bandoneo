#!/usr/bin/env python3
"""Verify consistency between STM32CubeMX .ioc and Protel netlist.

For every pin configured in the .ioc (with a GPIO_Label or a non-GPIO Signal),
check the corresponding net name on U1 (the MCU) in the netlist. When labels
and net names disagree the script prints the mismatch; when a pin configured
in the .ioc is not connected on U1 in the netlist (or vice versa) it is
reported as well.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

BOARDS = [
    {
        "name": "MainBoard",
        "ioc": "code/mother-wb5mm/mother-wb5mm.ioc",
        "netlist": "design_review/export/Netlist_MainBoard.net",
        "refdes": "U1",
        "part": "STM32WB5MMGH6TR",
    },
    {
        "name": "WingLeft",
        "ioc": "code/wing-f103/wing-f103/wing-f103.ioc",
        "netlist": "design_review/export/Netlist_WingLeft.net",
        "refdes": "U37",
        "part": "STM32F103C8T6",
    },
    {
        "name": "WingRight",
        "ioc": "code/wing-f103/wing-f103/wing-f103.ioc",
        "netlist": "design_review/export/Netlist_WingRight.net",
        "refdes": "U46",
        "part": "STM32F103C8T6",
    },
]

# Signals whose net name is conventional and does not need a GPIO_Label.
SIGNAL_NET_HINTS = {
    "USB_DM": {"USB_DM", "USB_D-", "USB_N"},
    "USB_DP": {"USB_DP", "USB_D+", "USB_P"},
    "SYS_JTMS-SWDIO": {"SWDIO", "SWD_IO", "JTMS"},
    "SYS_JTCK-SWCLK": {"SWCLK", "SWD_CLK", "JTCK"},
    "SYS_JTDO-SWO": {"SWO", "SWD_SWO", "JTDO"},
    "SYS_JTDO-TRACESWO": {"SWO", "SWD_SWO", "JTDO", "TRACESWO"},
}

# These .ioc signals are internal or do not need a matching net.
SKIP_SIGNALS = {"GPIO_Input", "GPIO_Output", "GPIO_Analog"}


def parse_ioc(path: Path) -> dict[str, dict]:
    """Return {pin: {'signal': str, 'label': str|None}} for PA.., PB.., ..."""
    pin_re = re.compile(r"^(P[A-H]\d{1,2}(?:-[A-Z0-9]+)?)\.(Signal|GPIO_Label)=(.*)$")
    pins: dict[str, dict] = defaultdict(dict)
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = pin_re.match(line.strip())
        if not m:
            continue
        pin, key, value = m.group(1), m.group(2), m.group(3).strip()
        pins[pin]["label" if key == "GPIO_Label" else "signal"] = value
    return pins


def parse_netlist(path: Path, refdes: str, part: str) -> dict[str, str]:
    """Return {mcu_pin_name: net_name} for pins of refdes/part."""
    text = path.read_text(encoding="utf-8", errors="replace")
    # Net blocks look like:
    # (
    # NET_NAME
    # REF-PIN PART-PINNAME Passive
    # ...
    # )
    net_block_re = re.compile(r"\(\s*\n(.*?)\n\)", re.DOTALL)
    pin_line_re = re.compile(
        rf"^{re.escape(refdes)}-(\d+)\s+{re.escape(part)}-(\S+)\s",
    )
    mapping: dict[str, str] = {}
    for block in net_block_re.findall(text):
        lines = [ln.strip() for ln in block.splitlines() if ln.strip()]
        if not lines:
            continue
        net_name = lines[0].strip()
        for ln in lines[1:]:
            m = pin_line_re.match(ln)
            if m:
                pin_name = m.group(2)  # e.g. PA3, NRST
                mapping[pin_name] = net_name
    return mapping


def check(
    ioc_pins: dict[str, dict],
    net_map: dict[str, str],
    refdes: str,
) -> list[str]:
    errors: list[str] = []

    for pin, info in sorted(ioc_pins.items()):
        signal = info.get("signal")
        label = info.get("label")
        net = net_map.get(pin)

        if net is None:
            errors.append(
                f"[MISSING]  {pin} configured in .ioc (signal={signal}, label={label}) "
                f"but no net found on {refdes}-{pin}"
            )
            continue

        if label:
            if net != label:
                errors.append(
                    f"[MISMATCH] {pin}: .ioc label={label!r} != netlist net={net!r}"
                )
        elif signal in SIGNAL_NET_HINTS:
            hints = SIGNAL_NET_HINTS[signal]
            net_upper = net.upper()
            if not any(h in net_upper for h in hints):
                errors.append(
                    f"[HINT]     {pin}: signal {signal} expected net containing "
                    f"one of {sorted(hints)}, got {net!r}"
                )
        elif signal and signal not in SKIP_SIGNALS:
            # Peripheral signal without explicit label: warn if net looks unrelated.
            # e.g. SPI1_SCK should likely appear in the net name.
            token = signal.split("_", 1)[-1].rstrip("+-")  # SCK, MOSI, RX, ...
            if token and token not in net.upper():
                errors.append(
                    f"[CHECK]    {pin}: signal {signal} -> net {net!r} "
                    f"(no token {token!r} in net name; verify manually)"
                )

    ioc_pin_set = set(ioc_pins)
    for pin, net in sorted(net_map.items()):
        if pin.startswith("P") and pin not in ioc_pin_set and net.upper() not in {
            "GND", "AGND", "VDD", "VCC", "NC",
        }:
            errors.append(
                f"[UNCONF]   {pin} connected to net {net!r} on {refdes} "
                f"but not configured in .ioc"
            )

    return errors


def check_board(board: dict, repo_root: Path) -> int:
    name = board["name"]
    ioc_path = repo_root / board["ioc"]
    netlist_path = repo_root / board["netlist"]
    refdes, part = board["refdes"], board["part"]

    print(f"=== {name}: {refdes} ({part}) ===")
    if not ioc_path.is_file():
        print(f"  ioc file not found: {ioc_path}", file=sys.stderr)
        return 2
    if not netlist_path.is_file():
        print(f"  netlist not found: {netlist_path}", file=sys.stderr)
        return 2

    ioc_pins = parse_ioc(ioc_path)
    net_map = parse_netlist(netlist_path, refdes, part)
    print(f"  ioc pins configured: {len(ioc_pins)}")
    print(f"  {refdes} pins found in netlist: {len(net_map)}")

    errors = check(ioc_pins, net_map, refdes)
    if not errors:
        print("  OK: .ioc and netlist are consistent.\n")
        return 0
    for e in errors:
        print(f"  {e}")
    print(f"  {len(errors)} inconsistency(ies) found.\n")
    return 1


def main() -> int:
    argparse.ArgumentParser(description=__doc__).parse_args()
    repo_root = Path(__file__).resolve().parents[1]
    exit_code = 0
    for board in BOARDS:
        rc = check_board(board, repo_root)
        if rc != 0:
            exit_code = rc if exit_code == 0 else exit_code
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
