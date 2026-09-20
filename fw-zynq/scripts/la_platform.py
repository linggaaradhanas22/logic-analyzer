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
import subprocess
import sys
from pathlib import Path

PLATFORM_NAME = "lap"
DOMAIN_NAME = "fr"
CPU = "ps7_cortexa9_0"
OS_NAME = "freertos"
APP_NAME = "la_app"
USB_APP_NAME = "la_app_usb"
EXAMPLE_APP_NAME = "la_usb_example"


def workspace_path(root: Path) -> Path:
    return (root / "build" / "vitis" / "ws").resolve()


def ensure_clean_workspace(client, root: Path) -> None:
    """Fresh, short workspace under build/vitis for reproducible builds."""
    ws = workspace_path(root)
    if ws.exists():
        _make_tree_writable(ws)
        shutil.rmtree(ws)
    ws.mkdir(parents=True, exist_ok=True)
    client.set_workspace(path=str(ws))
    print(f"VITIS workspace: {ws}")
    return ws


def _make_tree_writable(root: Path) -> None:
    """Clear read-only attributes; Vitis SDT/FSBL outputs are written
    read-only and Python >=3.12 rmtree no longer clears them itself."""
    import stat

    for entry in root.rglob("*"):
        try:
            entry.chmod(entry.stat().st_mode | stat.S_IWRITE)
        except OSError:
            pass


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
    offered = {
        str(p.get("parameter_name", "") if isinstance(p, dict) else p).lower()
        for p in params
    }
    for key in wanted:
        if not params or key.lower() in offered:
            try:
                domain.set_config("os", key, wanted[key])
                print(f"VITIS os config {key} = {wanted[key]}")
            except Exception as exc:
                print(f"VITIS os config {key} not settable: {exc}")


FRAGMENT_MARKER = "LA_FREERTOS_CONFIG_H"


def domain_bsp_header(ws: Path) -> Path:
    """Deterministic path of the BSP-owned generated FreeRTOSConfig.h."""
    return (
        ws / PLATFORM_NAME / CPU / DOMAIN_NAME / "bsp" / "include" / "FreeRTOSConfig.h"
    )


def domain_bsp_gen_header(ws: Path) -> Path:
    """Generated-header copy under the ninja tree (libsrc builds include this
    copy first via -I; the platform template refresh rewrites it on every
    platform.build(), so it must be patched again after each build)."""
    return domain_bsp_build_dir(ws) / "include" / "FreeRTOSConfig.h"


def domain_bsp_build_dir(ws: Path) -> Path:
    """Deterministic path of the domain BSP ninja build directory."""
    return (
        ws
        / PLATFORM_NAME
        / CPU
        / DOMAIN_NAME
        / "bsp"
        / "libsrc"
        / "build_configs"
        / "gen_bsp"
    )


def domain_export_include(ws: Path) -> Path:
    """Deterministic path of the platform export include copy of the header
    (the directory application components actually compile against)."""
    return (
        ws
        / PLATFORM_NAME
        / "export"
        / PLATFORM_NAME
        / "sw"
        / DOMAIN_NAME
        / "include"
        / "FreeRTOSConfig.h"
    )


CONSOLE_UART0_BASE = "0xe0000000"
CONSOLE_UART1_BASE = "0xe0001000"


def _console_uart_headers(ws: Path) -> list[Path]:
    """All generated copies of FreeRTOSUARTConfig.h (BSP include root, the
    ninja-tree gen_bsp copy libsrc actually compiles against, and the
    platform export copy applications include)."""
    candidates = [
        ws / PLATFORM_NAME / CPU / DOMAIN_NAME / "bsp" / "include"
        / "FreeRTOSUARTConfig.h",
        domain_bsp_build_dir(ws) / "include" / "FreeRTOSUARTConfig.h",
        ws / PLATFORM_NAME / "export" / PLATFORM_NAME / "sw" / DOMAIN_NAME
        / "include" / "FreeRTOSUARTConfig.h",
    ]
    return [p for p in candidates if p.is_file()]


def apply_console_uart_override(ws: Path) -> list[Path]:
    """Point BSP stdin/stdout at the UART1 console (MIO 48/49 -> FTDI COM).

    The freertos10_xilinx domain does not honour the stdout/STDIN os
    parameters and the Vitis generator defaults STDOUT_BASEADDRESS to
    UART0 (0xe0000000), which on this design is the EMIO UART0 routed to
    the JD header (the ESP32 link), not the console. xil_printf's outbyte
    (xuartps_hw.c) transmits on STDOUT_BASEADDRESS, so the hello banner and
    all later console output silently vanish. The generated header is
    therefore rewritten to UART1 (0xe0001000) in every copy after
    platform.build(), with the originals kept for diff evidence, and the
    uartps/freertos object sets are deleted so the ninja rebuild
    recompiles outbyte with the corrected base. Idempotent per file.
    """
    targets = _console_uart_headers(ws)
    if not targets:
        raise SystemExit(
            "fw-zynq blocked: no generated FreeRTOSUARTConfig.h under the "
            "domain BSP; platform.build() must run after add_domain first"
        )
    for target in targets:
        text = target.read_text(encoding="utf-8")
        if f"STDOUT_BASEADDRESS {CONSOLE_UART1_BASE}" in text:
            print(f"VITIS console UART1 override already present in {target}")
            continue
        backup = target.with_name("FreeRTOSUARTConfig.h.uart-orig")
        if not backup.exists():
            shutil.copyfile(target, backup)
        text = text.replace(
            f"STDIN_BASEADDRESS {CONSOLE_UART0_BASE}",
            f"STDIN_BASEADDRESS {CONSOLE_UART1_BASE}",
        )
        text = text.replace(
            f"STDOUT_BASEADDRESS {CONSOLE_UART0_BASE}",
            f"STDOUT_BASEADDRESS {CONSOLE_UART1_BASE}",
        )
        if f"STDOUT_BASEADDRESS {CONSOLE_UART1_BASE}" not in text:
            raise SystemExit(
                f"fw-zynq blocked: expected UART0 base not found in {target}"
            )
        target.write_text(text, encoding="utf-8", newline="\n")
        print(f"VITIS console UART1 override applied to {target} "
              f"(orig kept at {backup})")
    return targets


def rebuild_domain_bsp(ws: Path) -> None:
    """Rebuild the BSP libs after the policy header patches.

    platform.build() regenerates FreeRTOSConfig.h from its template on every
    call, so the override cannot survive a second platform.build(); and the
    ninja tree carries no header depfiles, so a plain incremental build does
    not recompile the kernel either. The freertos kernel and uartps (outbyte
    picks up STDOUT_BASEADDRESS there) object sets are therefore deleted and
    rebuilt in place, then the refreshed archives (per-lib and the merged
    libxil.a) are copied over the platform export copies the application
    links against.
    """
    build_dir = domain_bsp_build_dir(ws)
    for lib in ("freertos10_xilinx", "uartps"):
        lib_dir = build_dir / "libsrc" / lib
        for cmake_dir in lib_dir.rglob("CMakeFiles"):
            for obj_dir in cmake_dir.glob("*.dir"):
                shutil.rmtree(obj_dir)
                print(f"VITIS deleted object set {obj_dir} for header patch")
    ninja = Path(os.environ.get("XILINX_VITIS", "")) / "bin" / "ninja.exe"
    if not ninja.is_file():
        ninja = Path("ninja")
    proc = subprocess.run(
        [str(ninja)],
        cwd=str(build_dir),
        capture_output=True,
        text=True,
        errors="replace",
    )
    tail = (proc.stdout + proc.stderr).strip().splitlines()[-12:]
    print("NINJA BSP rebuild output tail:")
    for line in tail:
        print(f"  {line}")
    if proc.returncode != 0:
        raise SystemExit(
            f"fw-zynq blocked: ninja BSP rebuild failed (rc={proc.returncode}) "
            f"in {build_dir}"
        )
    export_lib = (
        ws
        / PLATFORM_NAME
        / "export"
        / PLATFORM_NAME
        / "sw"
        / DOMAIN_NAME
        / "lib"
    )
    archives = [build_dir / "libxil.a"]
    archives += sorted((build_dir / "libsrc").glob("*/*/lib*.a"))
    for archive in archives:
        if archive.is_file():
            shutil.copyfile(archive, export_lib / archive.name)
    print(f"VITIS BSP libs rebuilt in {build_dir} and exported to {export_lib}")


def _append_fragment_once(target: Path, fragment_text: str) -> bool:
    """Append the fragment to one header copy unless already present."""
    text = target.read_text(encoding="utf-8")
    if FRAGMENT_MARKER in text:
        print(f"VITIS FreeRTOS policy already applied to {target}")
        return False
    backup = target.with_name("FreeRTOSConfig.h.bsp-orig")
    if not backup.exists():
        shutil.copyfile(target, backup)
    with target.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write("\n")
        handle.write(fragment_text)
    print(f"VITIS FreeRTOS policy appended to {target} (orig kept at {backup})")
    return True


def apply_freertos_policy_override(root: Path, ws: Path) -> Path:
    """Append the repo FreeRTOS policy fragment to every generated header copy.

    The BSP owns every other value (configCPU_CLOCK_HZ comes from
    xparameters); only the policy knobs in the fragment are overridden by
    appending after the generated body. Two copies exist after a
    platform.build(): bsp/include (library include root) and
    bsp/libsrc/build_configs/gen_bsp/include (the copy libsrc builds see
    first via -I). Both must carry the override; the originals are kept as
    FreeRTOSConfig.h.bsp-orig for diff evidence. Idempotent per file.
    """
    fragment = root / "fw-zynq" / "FreeRTOSConfig.h"
    if not fragment.is_file():
        raise SystemExit(f"fw-zynq blocked: policy fragment missing: {fragment}")
    targets = [
        h
        for h in (domain_bsp_header(ws), domain_bsp_gen_header(ws))
        if h.is_file()
    ]
    if not targets:
        raise SystemExit(
            f"fw-zynq blocked: no generated FreeRTOSConfig.h under the domain "
            f"BSP; platform.build() must run after add_domain first"
        )
    fragment_text = fragment.read_text(encoding="utf-8")
    for target in targets:
        _append_fragment_once(target, fragment_text)
    return targets[0]


def create_platform_and_domain(client, root: Path, xsa: Path):
    """Create the platform component, add the FreeRTOS domain and build.

    Ordering matters: add_domain generates the BSP header; the first
    platform.build() compiles the BSP libs, the FSBL app and fills the
    platform export tree; the FreeRTOS policy override must be appended
    BEFORE the build that compiles the kernel (the freertos library is
    built with configSUPPORT_STATIC_ALLOCATION off otherwise and the app
    consumes the export copy of the header), so the override is applied
    and then platform.build() runs a second, incremental time to rebuild
    the kernel and refresh the export include copy.
    """
    platform = client.create_platform_component(
        name=PLATFORM_NAME,
        hw_design=str(xsa),
        desc="logic analyzer Zynq platform (P4.1)",
        generate_dtb=False,
    )
    platform.report()
    domain = platform.add_domain(
        name=DOMAIN_NAME, cpu=CPU, os=OS_NAME, display_name="freertos_ps7_cortexa9_0"
    )
    domain.report()
    set_os_param_if_supported(
        domain,
        {
            # Observed freertos-domain param names (2026.1): the classic
            # standalone-style keys are accepted silently but have no effect;
            # only freertos_stdout/freertos_stdin change the generated
            # FreeRTOSUARTConfig.h. The post-build console override below is
            # still applied as the authoritative fix (and as the guard when
            # a platform is created without these params).
            "freertos_stdout": "ps7_uart_1",
            "freertos_stdin": "ps7_uart_1",
            "stdout": "ps7_uart_1",
            "stdin": "ps7_uart_1",
            "standalone_stdout": "ps7_uart_1",
            "STDOUT": "ps7_uart_1",
            "STDIN": "ps7_uart_1",
        },
    )
    ws = workspace_path(root)
    platform.build()
    print(f"VITIS platform built: {PLATFORM_NAME} domain {DOMAIN_NAME}")
    target = apply_freertos_policy_override(root, ws)
    apply_console_uart_override(ws)
    rebuild_domain_bsp(ws)
    export_hdr = domain_export_include(ws)
    if export_hdr.is_file() and FRAGMENT_MARKER not in export_hdr.read_text(encoding="utf-8"):
        shutil.copyfile(target, export_hdr)
        print(f"VITIS policy header copied to export include {export_hdr}")
    return platform


APP_SOURCE_GROUPS = (
    ("fw-zynq", "app", "src"),
    ("fw-zynq", "app", "src", "drivers"),
    ("fw-zynq", "app", "src", "tasks"),
    ("fw-zynq", "app", "src", "live"),
    ("common", "la_proto", "src"),
    ("common", "la_proto", "include"),
    ("common", "la_decode", "src"),
    ("common", "la_decode", "include"),
)

# The `example` profile builds the unmodified AMD usbps mass-storage example
# (driver usbps_v2_9, copied verbatim from the installed tool tree into
# fw-zynq/example). It is a hardware-control experiment, not the product
# firmware: it runs bare-metal style from the BSP main() call, enumerates as a
# USB mass-storage device and serves a 1 MiB virtual disk. See docs/status.md
# and docs/usb-link.md for the experiment record.
EXAMPLE_SOURCE_GROUPS = (("fw-zynq", "example"),)


def _collect_app_files(root: Path, profile: str) -> list[str]:
    """Flat source/header list for the app component (all names unique)."""
    groups = EXAMPLE_SOURCE_GROUPS if profile == "example" else APP_SOURCE_GROUPS
    files: list[str] = []
    for group in groups:
        directory = root.joinpath(*group)
        if not directory.is_dir():
            raise SystemExit(f"fw-zynq blocked: source directory missing: {directory}")
        for entry in sorted(directory.iterdir()):
            if entry.suffix in (".c", ".h") and entry.is_file():
                files.append(entry.name)
    return files


def app_component_name(profile: str) -> str:
    """Application component name for a build profile (U02 contract)."""
    if profile == "usb":
        return USB_APP_NAME
    if profile == "example":
        return EXAMPLE_APP_NAME
    return APP_NAME


def _patch_user_config(app_dir: Path, sources: list[str], profile: str) -> None:
    """Deterministic USER_COMPILE_* blocks after import.

    import_files updates UserConfig.cmake itself, but the generated list
    ordering depends on the import calls; rewriting the blocks keeps the
    build reproducible and independent of that behaviour. All imported
    sources live flat in src/, so the CMake source directory is enough. The
    USB profile adds the LA_TRANSPORT_USB compile definition; the example
    profile maps the driver's generated USBPS_BASEADDR macro (normally
    produced by the driver example CMake) to the BSP xparameters symbol and
    enables the storage-class SCSI trace on the console.
    """
    import re

    config = app_dir / "src" / "UserConfig.cmake"
    if not config.is_file():
        raise SystemExit(f"fw-zynq blocked: UserConfig.cmake missing at {config}")
    text = config.read_text(encoding="utf-8")
    source_block = "set(USER_COMPILE_SOURCES\n" + "".join(
        f'"{name}"\n' for name in sources if name.endswith(".c")
    ) + ")"
    text = re.sub(r"set\(USER_COMPILE_SOURCES.*?\)\n", source_block + "\n", text,
                  count=1, flags=re.S)
    text = re.sub(r'set\(USER_INCLUDE_DIRECTORIES.*?\)\n',
                  'set(USER_INCLUDE_DIRECTORIES\n".."\n)\n', text, count=1, flags=re.S)
    if profile == "example":
        defines = (
            "XPS_USB0_BASEADDR=XPAR_XUSBPS_0_BASEADDR",
            "CLASS_STORAGE_DEBUG=1",
        )
    else:
        defines = (("LA_TRANSPORT_USB=1",) if profile == "usb" else ("",))
    define_block = "set(USER_COMPILE_DEFINITIONS\n" + "".join(
        f'"{name}"\n' for name in defines
    ) + ")"
    text = re.sub(r"set\(USER_COMPILE_DEFINITIONS.*?\)\n",
                  define_block + "\n", text, count=1, flags=re.S)
    config.write_text(text, encoding="utf-8", newline="\n")
    print(f"VITIS UserConfig sources installed: {len(sources)} files "
          f"(profile {profile})")


def create_app_and_build(client, root: Path, platform, profile: str) -> None:
    """Create the application component for the selected profile."""
    name = app_component_name(profile)
    xpfm = client.find_platform_in_repos(PLATFORM_NAME)
    app = client.create_app_component(
        name=name, platform=xpfm, domain=DOMAIN_NAME
    )
    files = _collect_app_files(root, profile)
    groups = EXAMPLE_SOURCE_GROUPS if profile == "example" else APP_SOURCE_GROUPS
    for group in groups:
        directory = root.joinpath(*group)
        names = sorted(
            entry.name
            for entry in directory.iterdir()
            if entry.suffix in (".c", ".h") and entry.is_file()
        )
        if names:
            app.import_files(from_loc=str(directory), files=names,
                             dest_dir_in_cmp="src")
    app_dir = workspace_path(root) / name
    _patch_user_config(app_dir, files, profile)
    # import_files refuses to overwrite the Vitis-generated src/lscript.ld,
    # so the P4.3 linker script (NC carve + ASSERTs) replaces it in place.
    # The example profile keeps the generated stock script: it has no NC
    # carve contract and the vendor example only needs plain DDR .bss.
    lscript = app_dir / "src" / "lscript.ld"
    if not lscript.is_file():
        raise SystemExit(f"fw-zynq blocked: generated lscript.ld missing at {lscript}")
    if profile == "example":
        print(f"VITIS example profile keeps the generated linker script {lscript}")
    else:
        shutil.copyfile(root / "fw-zynq" / "lscript.ld", lscript)
        print(f"VITIS P4.3 linker script installed at {lscript}")
    app.build()
    print(f"VITIS app built: {name} (profile {profile})")


def verify_elf_and_report(root: Path, profile: str) -> Path:
    name = app_component_name(profile)
    ws = workspace_path(root)
    elves = list(ws.rglob(f"{name}.elf"))
    if not elves:
        raise SystemExit(
            f"fw-zynq blocked: {name}.elf not found under {ws}; "
            "inspect the build log for compiler/linker errors"
        )
    elf = elves[0]
    print(f"FW-ZYNQ ELF ({profile}): {elf}")
    return elf


def make_client():
    try:
        import vitis  # type: ignore
    except Exception as exc:
        raise SystemExit(f"fw-zynq blocked: vitis python module missing: {exc}")
    return vitis.create_client()


def usage(argv) -> tuple[Path, Path, str]:
    tokens = list(argv[1:])
    # V00 (2026-09-20): USB is the default product profile; the gateway
    # remains a fully supported optional profile (`--profile gateway`).
    profile = "usb"
    if "--profile" in tokens:
        index = tokens.index("--profile")
        if index + 1 >= len(tokens):
            raise SystemExit("fw-zynq blocked: --profile needs a value")
        profile = tokens[index + 1]
        del tokens[index:index + 2]
    if profile not in ("gateway", "usb", "example"):
        raise SystemExit(f"fw-zynq blocked: unknown profile: {profile}")
    if len(tokens) < 2:
        raise SystemExit(
            "usage: vitis -s <script>.py -- PROJECT_ROOT XSA "
            "[--profile usb|gateway|example] (default usb)"
        )
    root = Path(tokens[-2]).resolve()
    xsa = Path(tokens[-1]).resolve()
    if not xsa.is_file():
        raise SystemExit(f"fw-zynq blocked: XSA is missing: {xsa}")
    return root, xsa, profile


if __name__ == "__main__":  # pragma: no cover - standalone platform-only run
    root, xsa, _profile = usage(sys.argv)
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
