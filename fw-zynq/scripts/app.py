"""P4.1 Vitis 2026.1 application script (unverified WIP - see docs/status.md).

Full `python tools/run.py fw-zynq` gate: create the platform from the XSA,
add the FreeRTOS domain, apply the BSP policy overrides, build the la_app
hello component and report the ELF path.

Blocked on the machine as of 2026-09-06: the Vitis platform-creation service
fails for every XSA and built-in preset (unmodified AMD examples included);
see docs/status.md for the evidence. This entry point is prepared so the
gate can run as soon as the environment is fixed.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import la_platform as platform_mod  # noqa: E402  (project module in same dir)


def main() -> int:
    root, xsa, profile = platform_mod.usage(sys.argv)
    client = platform_mod.make_client()
    try:
        platform_mod.ensure_clean_workspace(client, root)
        platform = platform_mod.create_platform_and_domain(client, root, xsa)
        platform_mod.create_app_and_build(client, root, platform, profile)
        elf = platform_mod.verify_elf_and_report(root, profile)
    finally:
        try:
            import vitis

            vitis.dispose()
        except Exception:
            pass
    print(f"FW-ZYNQ PASS: {elf}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
