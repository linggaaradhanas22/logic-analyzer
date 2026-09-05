"""Vitis application-script placeholder for the first FreeRTOS build."""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: vitis -s app.py -- PROJECT_ROOT XSA", file=sys.stderr)
        return 2
    root = Path(sys.argv[-2]).resolve()
    xsa = Path(sys.argv[-1]).resolve()
    if not xsa.is_file():
        print(f"fw-zynq blocked: XSA is missing: {xsa}", file=sys.stderr)
        return 2
    print(f"PROJECT_ROOT={root}")
    print(f"XSA={xsa}")
    print("Vitis FreeRTOS platform/application API integration is scheduled for P4.1.")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
