"""P4.1 Vitis 2026.1 platform script (unverified WIP - see docs/status.md).

Creates the embedded platform for the logic analyzer hardware design and
applies the FreeRTOS policy overrides to the generated BSP.

The Vitis Python API shape is taken from the official examples shipped with
this install (C:/AMDDesignTools/2026.1/Vitis/cli/examples/embedded/):
  create_platform_component -> add_domain -> platform.build().
As of 2026-09-06 the platform-creation service fails on this machine for
every XSA and every built-in preset (unmodified AMD examples included), so
this code is prepared against the documented API but is NOT yet verified by
a successful run; the blocker and its evidence are recorded in
docs/status.md and docs/test-plan.md.
"""

from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path

PLATFORM_NAME = "la_platform"
DOMAIN_NAME = "freertos10_ps7_cortexa9_0"
CPU = "ps7_cortexa9_0"
OS_NAME = "freertos10_xilinx"
APP_NAME = "la_app"


def workspace_path(root: Path) -> Path:
    return (root / "build" / "vitis" / "ws").resolve()


def ensure_clean_workspace(client, root: Path) -> None:
    """Fresh, short workspace under build/vitis for reproducible builds."""
    ws = workspace_path(root)
    if ws.exists():
        shutil.rmtree(ws)
    ws.mkdir(parents=True, exist_ok=True)
    client.set_workspace(path=str(ws))
    print(f"VITIS workspace: {ws}")
    return ws


def set_os_param_if_supported(domain, wanted: dict[str, str]) -> None:
    """Set BSP OS parameters by their supported names where they exist.

    The exact config keys are tool-version dependent (standalone_stdout for
    standalone-style domains, STDOUT/STDIN for the freertos OS .mss); the
    candidate list is tried and the actual accepted keys are printed.
    """
    try:
        params = domain.list_params("os") or []
    except Exception as exc:  # pragma: no cover - API surface differs per version
        print(f"VITIS list_params(os) unavailable: {exc}")
        params = []
    print(f"VITIS OS params offered: {params}")
    for key in wanted:
        if not params or any(str(p).lower() == key.lower() for p in params):
            try:
                domain.set_config("os", key, wanted[key])
                print(f"VITIS os config {key} = {wanted[key]}")
                return
            except Exception as exc:
                print(f"VITIS os config {key} not settable: {exc}")


def apply_freertos_policy_override(root: Path, ws: Path) -> None:
    """Append the repo FreeRTOS policy fragment to the BSP-generated header.

    The generated header path is located after platform.build(); the
    original is preserved next to it as FreeRTOSConfig.h.bsp-orig.
    """
    fragment = root / "fw-zynq" / "FreeRTOSConfig.h"
    if not fragment.is_file():
        raise SystemExit(f"fw-zynq blocked: policy fragment missing: {fragment}")
    headers = list(ws.rglob("FreeRTOSConfig.h"))
    if len(headers) != 1:
        raise SystemExit(
            f"fw-zynq blocked: expected one generated FreeRTOSConfig.h under "
            f"{ws}, found {[str(h) for h in headers]}"
        )
    target = headers[0]
    backup = target.with_name("FreeRTOSConfig.h.bsp-orig")
    if not backup.exists():
        shutil.copyfile(target, backup)
    with target.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write("\n")
        handle.write(fragment.read_text(encoding="utf-8"))
    print(f"VITIS FreeRTOS policy appended to {target} (orig kept at {backup})")


def create_platform_and_domain(client, root: Path, xsa: Path):
    """Create the platform component, add the FreeRTOS domain and build."""
    platform = client.create_platform_component(
        name=PLATFORM_NAME,
        hw_design=str(xsa),
        desc="logic analyzer Zynq platform (P4.1)",
        generate_dtb=False,
    )
    platform.report()
    domain = platform.add_domain(
        name=DOMAIN_NAME, cpu=CPU, os=OS_NAME, display_name=DOMAIN_NAME
    )
    domain.report()
    set_os_param_if_supported(
        domain,
        {
            "stdout": "ps7_uart_1",
            "stdin": "ps7_uart_1",
            "standalone_stdout": "ps7_uart_1",
            "STDOUT": "ps7_uart_1",
            "STDIN": "ps7_uart_1",
        },
    )
    platform.build()
    print(f"VITIS platform built: {PLATFORM_NAME} domain {DOMAIN_NAME}")
    ws = workspace_path(root)
    apply_freertos_policy_override(root, ws)
    return platform


def create_app_and_build(client, root: Path, platform) -> None:
    """Create the la_app application component from the repo sources."""
    xpfm = client.find_platform_in_repos(PLATFORM_NAME)
    app = client.create_app_component(
        name=APP_NAME, platform=xpfm, domain=DOMAIN_NAME
    )
    src = root / "fw-zynq" / "app" / "src"
    app.import_files(from_loc=str(src), files=["main.c"])
    app.build()
    print(f"VITIS app built: {APP_NAME}")


def verify_elf_and_report(root: Path) -> Path:
    ws = workspace_path(root)
    elves = list(ws.rglob(f"{APP_NAME}.elf"))
    if not elves:
        raise SystemExit(
            f"fw-zynq blocked: {APP_NAME}.elf not found under {ws}; "
            "inspect the build log for compiler/linker errors"
        )
    elf = elves[0]
    print(f"FW-ZYNQ ELF: {elf}")
    return elf


def make_client():
    try:
        import vitis  # type: ignore
    except Exception as exc:
        raise SystemExit(f"fw-zynq blocked: vitis python module missing: {exc}")
    return vitis.create_client()


def usage(argv) -> tuple[Path, Path]:
    if len(argv) < 3:
        raise SystemExit("usage: vitis -s <script>.py -- PROJECT_ROOT XSA")
    root = Path(argv[-2]).resolve()
    xsa = Path(argv[-1]).resolve()
    if not xsa.is_file():
        raise SystemExit(f"fw-zynq blocked: XSA is missing: {xsa}")
    return root, xsa


if __name__ == "__main__":  # pragma: no cover - standalone platform-only run
    root, xsa = usage(sys.argv)
    client = make_client()
    try:
        ensure_clean_workspace(client, root)
        platform = create_platform_and_domain(client, root, xsa)
        platform.report()
    finally:
        try:
            import vitis

            vitis.dispose()
        except Exception:
            pass
    print("platform.py complete")
