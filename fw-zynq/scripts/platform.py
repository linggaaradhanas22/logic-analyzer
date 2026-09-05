"""Vitis 2026.1 platform-script placeholder.

The Vitis Python API is versioned with the installed tool. This file keeps the
project-root/XSA contract explicit until the first XSA exists; it does not
silently use a legacy XSCT compatibility path.
"""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: vitis -s platform.py -- PROJECT_ROOT XSA", file=sys.stderr)
        return 2
    root = Path(sys.argv[-2]).resolve()
    xsa = Path(sys.argv[-1]).resolve()
    if not xsa.is_file():
        print(f"platform blocked: XSA is missing: {xsa}", file=sys.stderr)
        return 2
    print(f"PROJECT_ROOT={root}")
    print(f"XSA={xsa}")
    print("Vitis 2026.1 platform API integration is scheduled for P4.1.")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
