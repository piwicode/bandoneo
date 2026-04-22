#!/usr/bin/env python3
"""Re-run check_ioc_netlist.py whenever any watched file changes.

Uses mtime polling so it has no external dependencies. Watches the .ioc and
netlist files referenced by BOARDS, plus the checker script itself.
"""

from __future__ import annotations

import subprocess
import sys
import time
from pathlib import Path

from check_ioc_netlist import BOARDS

POLL_INTERVAL_S = 1.0
SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
CHECKER = SCRIPT_DIR / "check_ioc_netlist.py"


def watched_files() -> list[Path]:
    paths = {CHECKER, Path(__file__).resolve()}
    for board in BOARDS:
        paths.add(REPO_ROOT / board["ioc"])
        paths.add(REPO_ROOT / board["netlist"])
    return sorted(paths)


def snapshot(paths: list[Path]) -> dict[Path, float]:
    state: dict[Path, float] = {}
    for p in paths:
        try:
            state[p] = p.stat().st_mtime
        except FileNotFoundError:
            state[p] = 0.0
    return state


def run_checker() -> None:
    sys.stdout.write("\x1b[2J\x1b[H")
    sys.stdout.flush()
    print(f"--- {time.strftime('%H:%M:%S')} running {CHECKER.name} ---")
    subprocess.run([sys.executable, str(CHECKER)], check=False)


def main() -> int:
    paths = watched_files()
    print(f"Watching {len(paths)} file(s) (poll every {POLL_INTERVAL_S}s):")
    for p in paths:
        print(f"  {p}")

    state = snapshot(paths)
    run_checker()

    try:
        while True:
            time.sleep(POLL_INTERVAL_S)
            new_state = snapshot(paths)
            changed = [p for p, m in new_state.items() if m != state.get(p)]
            if changed:
                for p in changed:
                    print(f"\nchanged: {p}")
                state = new_state
                run_checker()
    except KeyboardInterrupt:
        print("\nstopped.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
