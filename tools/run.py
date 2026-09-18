"""Canonical native-Windows project runner.

The runner owns paths and child-process environments so commands work without
depending on the caller's current directory. It intentionally does not provide
Linux, WSL, container or Make fallbacks.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Sequence


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_ROOT = PROJECT_ROOT / "build"
REPORT_ROOT = BUILD_ROOT / "reports"
TMP_ROOT = BUILD_ROOT / "tmp"
CACHE_ROOT = BUILD_ROOT / "cache"

TARGETS = (
    "gen",
    "sim",
    "lint",
    "test-native",
    "test-esp32-mocks",
    "test-esp32-target",
    "bit",
    "fw-zynq",
    "jtag-zynq",
    "fw-esp32",
    "boot",
    "package",
    "program",
    "flash-esp",
    "test-host",
    "test-e2e",
    "hil",
    "release",
    "check-env",
    "query-board",
)


def _inside(path: Path) -> bool:
    try:
        path.resolve(strict=False).relative_to(PROJECT_ROOT)
    except ValueError:
        return False
    return True


def _set_environment_value(environment: dict[str, str], name: str, value: str) -> None:
    existing = next((key for key in environment if key.lower() == name.lower()), None)
    environment[existing or name] = value


def _find_vsdevcmd() -> Path | None:
    candidates: list[Path] = []
    override = os.environ.get("VSDEVCMD_BAT")
    if override:
        path = Path(os.path.expandvars(override)).expanduser()
        if not path.is_absolute():
            path = PROJECT_ROOT / path
        candidates.append(path)
    for variable in ("VSINSTALLDIR", "VCINSTALLDIR"):
        value = os.environ.get(variable)
        if not value:
            continue
        root = Path(os.path.expandvars(value)).expanduser()
        if variable == "VCINSTALLDIR":
            root = root.parent
        candidates.append(root / "Common7" / "Tools" / "VsDevCmd.bat")
    for variable in ("ProgramFiles(x86)", "ProgramFiles"):
        base = Path(os.environ.get(variable, r"C:\Program Files"))
        visual_studio = base / "Microsoft Visual Studio"
        if visual_studio.exists():
            candidates.extend(
                visual_studio.glob("*/BuildTools/Common7/Tools/VsDevCmd.bat")
            )
            candidates.extend(
                visual_studio.glob("*/Community/Common7/Tools/VsDevCmd.bat")
            )
            candidates.extend(
                visual_studio.glob("*/Professional/Common7/Tools/VsDevCmd.bat")
            )
            candidates.extend(
                visual_studio.glob("*/Enterprise/Common7/Tools/VsDevCmd.bat")
            )
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve(strict=False)
    return None


def _import_msvc_environment(environment: dict[str, str]) -> None:
    path_key = next((key for key in environment if key.lower() == "path"), "PATH")
    if shutil.which("cl.exe", path=environment.get(path_key, "")):
        return
    devcmd = _find_vsdevcmd()
    if devcmd is None:
        return
    command = (
        f'call "{devcmd}" -arch=x64 -host_arch=x64'
        " && set"
    )
    try:
        result = subprocess.run(
            command,
            cwd=PROJECT_ROOT,
            env=environment,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=60,
            check=False,
            shell=True,
        )
    except (OSError, subprocess.TimeoutExpired):
        return
    if result.returncode != 0:
        return
    for line in result.stdout.splitlines():
        if "=" not in line:
            continue
        name, value = line.split("=", 1)
        if not name or any(character.isspace() for character in name):
            continue
        _set_environment_value(environment, name, value)


def _idf_paths() -> tuple[Path, Path]:
    idf_root = Path(os.path.expandvars(os.environ.get("IDF_PATH", str(PROJECT_ROOT / ".deps" / "esp-idf")))).expanduser()
    tools_root = Path(
        os.path.expandvars(
            os.environ.get("IDF_TOOLS_PATH", str(PROJECT_ROOT / ".tools" / "espressif"))
        )
    ).expanduser()
    if not idf_root.is_absolute():
        idf_root = PROJECT_ROOT / idf_root
    if not tools_root.is_absolute():
        tools_root = PROJECT_ROOT / tools_root
    return idf_root.resolve(strict=False), tools_root.resolve(strict=False)


def _idf_python() -> Path:
    override = os.environ.get("IDF_PYTHON_EXE")
    if override:
        path = Path(os.path.expandvars(override)).expanduser()
        if not path.is_absolute():
            path = PROJECT_ROOT / path
        if path.is_file():
            return path.resolve(strict=False)
    configured = os.environ.get("IDF_PYTHON_ENV_PATH")
    if configured:
        root = Path(os.path.expandvars(configured)).expanduser()
        if not root.is_absolute():
            root = PROJECT_ROOT / root
        candidate = root / "Scripts" / "python.exe"
        if candidate.is_file():
            return candidate.resolve(strict=False)
    _, tools_root = _idf_paths()
    candidates = sorted(
        (tools_root / "python_env").glob("idf*_py*_env/Scripts/python.exe"),
        reverse=True,
    )
    return candidates[0].resolve(strict=False) if candidates else Path(sys.executable)


def _import_idf_environment(environment: dict[str, str]) -> None:
    idf_root, tools_root = _idf_paths()
    idf_tools = idf_root / "tools" / "idf_tools.py"
    if not idf_tools.is_file():
        return
    _set_environment_value(environment, "IDF_PATH", str(idf_root))
    _set_environment_value(environment, "IDF_TOOLS_PATH", str(tools_root))
    try:
        result = subprocess.run(
            [sys.executable, str(idf_tools), "export", "--format=key-value"],
            cwd=idf_root,
            env=environment,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=120,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return
    if result.returncode != 0:
        return
    current_path = next(
        (value for key, value in environment.items() if key.lower() == "path"),
        "",
    )
    for line in result.stdout.splitlines():
        if "=" not in line:
            continue
        name, value = line.split("=", 1)
        if not name or any(character.isspace() for character in name):
            continue
        if name.upper() == "PATH":
            value = value.replace("%PATH%", current_path)
        _set_environment_value(environment, name, value)


def _environment(target: str | None = None) -> dict[str, str]:
    for directory in (
        REPORT_ROOT,
        TMP_ROOT,
        CACHE_ROOT,
        BUILD_ROOT / "questa",
        BUILD_ROOT / "native",
        BUILD_ROOT / "vivado",
        BUILD_ROOT / "vitis",
        BUILD_ROOT / "esp32",
        BUILD_ROOT / "release",
    ):
        directory.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["TEMP"] = str(TMP_ROOT)
    environment["TMP"] = str(TMP_ROOT)
    environment["TMPDIR"] = str(TMP_ROOT)
    environment["PIP_CACHE_DIR"] = str(CACHE_ROOT / "pip")
    environment.setdefault("IDF_TOOLS_PATH", str(PROJECT_ROOT / ".tools" / "espressif"))
    environment.setdefault("IDF_PATH", str(PROJECT_ROOT / ".deps" / "esp-idf"))
    environment["PYTHONPATH"] = os.pathsep.join(
        [str(PROJECT_ROOT), environment.get("PYTHONPATH", "")]
    ).rstrip(os.pathsep)
    if target in {"test-native", "test-esp32-mocks", "release"}:
        _import_msvc_environment(environment)
    if target in {"fw-esp32", "test-esp32-target"}:
        _import_idf_environment(environment)
    if target == "fw-esp32":
        _set_environment_value(environment, "SDKCONFIG", str(BUILD_ROOT / "esp32" / "sdkconfig"))
    return environment


def _find(name: str, *, env_name: str | None = None, roots: Sequence[Path] = ()) -> Path | None:
    override = os.environ.get(env_name) if env_name else None
    candidates: list[Path] = []
    if override:
        value = Path(os.path.expandvars(override)).expanduser()
        if not value.is_absolute():
            value = PROJECT_ROOT / value
        if value.is_file():
            return value.resolve(strict=False)
        candidates.extend(value / suffix for suffix in (f"{name}.exe", f"{name}.bat", f"{name}.cmd", name))
    for root in roots:
        if not root.is_absolute():
            root = PROJECT_ROOT / root
        candidates.extend(
            root / suffix
            for suffix in (f"{name}.exe", f"{name}.bat", f"{name}.cmd", name)
        )
        candidates.extend(
            root / "bin" / suffix
            for suffix in (f"{name}.exe", f"{name}.bat", f"{name}.cmd", name)
        )
    for suffix in (f"{name}.exe", f"{name}.bat", f"{name}.cmd", name):
        found = shutil.which(suffix)
        if found:
            return Path(found).resolve(strict=False)
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve(strict=False)
    return None


def _tool(name: str) -> Path | None:
    program_files = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
    amd_root = Path(os.environ.get("AMD_ROOT", r"C:\AMDDesignTools\2026.1"))
    def configured(name: str, default: Path) -> Path:
        value = Path(os.path.expandvars(os.environ.get(name, str(default)))).expanduser()
        return value if value.is_absolute() else PROJECT_ROOT / value

    vivado = configured("VIVADO_HOME", amd_root / "Vivado")
    vitis = configured("VITIS_HOME", amd_root / "Vitis")
    questa = configured("QUESTA_ROOT", Path(r"C:\altera_lite\25.1std\questa_fse\win64"))
    idf_root = configured("IDF_PATH", PROJECT_ROOT / ".deps" / "esp-idf")
    roots: dict[str, tuple[Path, ...]] = {
        "cmake": (program_files / "CMake",),
        "ninja": (program_files / "Ninja",),
        "ctest": (program_files / "CMake",),
        "vivado": (vivado,),
        "vitis": (vitis,),
        "xsdb": (vitis,),
        "hw_server": (vivado,),
        "bootgen": (vitis, vivado),
        "vlib": (questa,),
        "vlog": (questa,),
        "vsim": (questa,),
        "idf.py": (idf_root,),
    }
    env_names = {
        "cmake": "CMAKE_EXE",
        "ninja": "NINJA_EXE",
        "vivado": "VIVADO_EXE",
        "vitis": "VITIS_EXE",
        "xsdb": "XSDB_EXE",
        "hw_server": "HW_SERVER_EXE",
        "bootgen": "BOOTGEN_EXE",
        "vlib": "VLIB_EXE",
        "vlog": "VLOG_EXE",
        "vsim": "VSIM_EXE",
        "idf.py": "IDF_PY",
    }
    found = _find(name, env_name=env_names.get(name), roots=roots.get(name, ()))
    if found is None and name == "idf.py":
        candidate = idf_root / "tools" / "idf.py"
        if candidate.is_file():
            found = candidate.resolve(strict=False)
    return found


def _command(
    executable: Path,
    arguments: Sequence[str],
    *,
    python_executable: Path | None = None,
) -> list[str]:
    if executable.suffix.lower() in {".bat", ".cmd"}:
        return [os.environ.get("COMSPEC", "cmd.exe"), "/d", "/s", "/c", str(executable), *arguments]
    if executable.suffix.lower() == ".py":
        return [str(python_executable or Path(sys.executable)), str(executable), *arguments]
    return [str(executable), *arguments]


def _write_report(target: str, result: dict[str, Any]) -> None:
    path = REPORT_ROOT / f"{target}.json"
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(result, handle, indent=2, sort_keys=True)
        handle.write("\n")


def _blocked(target: str, reason: str) -> int:
    result = {
        "schema": 1,
        "target": target,
        "status": "blocked",
        "reason": reason,
        "project_root": str(PROJECT_ROOT),
        "checked_at": datetime.now(timezone.utc).isoformat(),
    }
    _write_report(target, result)
    print(f"BLOCKED [{target}]: {reason}", file=sys.stderr)
    return 2


def _run_steps(target: str, steps: Sequence[tuple[Sequence[str], Path]], *, timeout: float | None = 3600.0) -> int:
    environment = _environment(target)
    log_path = REPORT_ROOT / f"{target}.log"
    started = time.monotonic()
    records: list[dict[str, Any]] = []
    status = "pass"
    return_code = 0
    with log_path.open("w", encoding="utf-8", errors="replace", newline="\n") as log:
        for raw_command, cwd in steps:
            if not _inside(cwd):
                status = "blocked"
                return_code = 2
                message = f"working directory is outside PROJECT_ROOT: {cwd}"
                print(f"BLOCKED [{target}]: {message}", file=sys.stderr)
                records.append({"command": list(raw_command), "cwd": str(cwd), "error": message})
                break
            printable = subprocess.list2cmdline(list(raw_command))
            print(f"[{target}] {printable}")
            log.write(f"$ {printable}\n")
            step_started = time.monotonic()
            try:
                process = subprocess.Popen(
                    list(raw_command),
                    cwd=str(cwd),
                    env=environment,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                )
                output, _ = process.communicate(timeout=timeout)
                if output:
                    print(output, end="")
                    log.write(output)
                    log.flush()
                code = process.returncode
                error = None
            except FileNotFoundError as exc:
                code = 127
                error = f"executable could not start: {exc}"
            except subprocess.TimeoutExpired:
                process.kill()
                output, _ = process.communicate()
                if output:
                    print(output, end="")
                    log.write(output)
                code = 124
                error = f"command exceeded timeout of {timeout} seconds"
            elapsed = time.monotonic() - step_started
            records.append(
                {
                    "command": list(raw_command),
                    "cwd": str(cwd),
                    "returncode": code,
                    "elapsed_seconds": round(elapsed, 3),
                    "error": error,
                }
            )
            if error:
                print(f"ERROR [{target}]: {error}", file=sys.stderr)
                log.write(f"ERROR: {error}\n")
            if code != 0:
                status = "fail"
                return_code = code
                break
    result = {
        "schema": 1,
        "target": target,
        "status": status,
        "returncode": return_code,
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "project_root": str(PROJECT_ROOT),
        "log": str(log_path),
        "steps": records,
    }
    _write_report(target, result)
    return return_code


def _python_script(name: str, arguments: Sequence[str]) -> tuple[list[str], Path]:
    script = PROJECT_ROOT / "tools" / name
    return [sys.executable, str(script), *arguments], PROJECT_ROOT


def _cmake_steps(target: str) -> tuple[list[tuple[Sequence[str], Path]], int | None]:
    cmake = _tool("cmake")
    if cmake is None:
        return [], _blocked(target, "CMake is missing; install CMake or set CMAKE_EXE in a native Windows shell")
    if _tool("ninja") is None:
        return [], _blocked(target, "Ninja is missing; install Ninja or set NINJA_EXE in a native Windows shell")
    build = BUILD_ROOT / "native"
    configure = _command(
        cmake,
        [
            "-S",
            str(PROJECT_ROOT),
            "-B",
            str(build),
            "-G",
            "Ninja",
            f"-DPython3_EXECUTABLE={sys.executable}",
        ],
    )
    build_command = _command(cmake, ["--build", str(build)])
    test_arguments = ["--test-dir", str(build), "--output-on-failure"]
    if target == "test-native":
        test_arguments.extend(["-R", "^la_native"])
    elif target == "test-esp32-mocks":
        test_arguments.extend(["-R", "^la_esp32_mocks"])
    test_command = _command(_tool("ctest") or cmake.parent / "ctest.exe", test_arguments)
    return [(configure, PROJECT_ROOT), (build_command, PROJECT_ROOT), (test_command, PROJECT_ROOT)], None


def _run_target(target: str, forwarded: Sequence[str]) -> int:
    if target == "check-env":
        command, cwd = _python_script("check_env.py", list(forwarded))
        return _run_steps(target, [(command, cwd)])
    if target == "query-board":
        command, cwd = _python_script("query_board.py", list(forwarded))
        return _run_steps(target, [(command, cwd)])
    if target == "gen":
        command, cwd = _python_script("gen_regmap.py", [])
        check_command, check_cwd = _python_script("gen_regmap.py", ["--check"])
        return _run_steps(target, [(command, cwd), (check_command, check_cwd)])
    if target in {"test-native", "test-esp32-mocks"}:
        steps, blocked = _cmake_steps(target)
        if blocked is not None:
            return blocked
        return _run_steps(target, steps)
    if target == "test-host":
        command = [sys.executable, "-m", "pytest", "host/tests"]
        return _run_steps(target, [(command, PROJECT_ROOT)])
    if target in {"lint", "sim"}:
        arguments = ["--lint"] if target == "lint" else []
        command, cwd = _python_script("regress.py", arguments)
        return _run_steps(target, [(command, cwd)])
    if target == "test-esp32-target":
        idf = _tool("idf.py")
        if idf is None:
            return _blocked(
                target,
                "native ESP-IDF is missing; provision .deps/esp-idf and set IDF_PATH",
            )
        forwarded_list = list(forwarded)
        port = os.environ.get("LA_ESP32_PORT")
        has_port = any(
            argument == "--port" or argument.startswith("--port=")
            for argument in forwarded_list
        )
        if port and not has_port:
            forwarded_list.append(f"--port={port}")
            has_port = True
        if has_port:
            script = PROJECT_ROOT / "fw-esp32" / "scripts" / "run_target_tests.py"
            command = [
                sys.executable,
                str(script),
                "--project-root",
                str(PROJECT_ROOT),
                "--idf",
                str(idf),
                "--idf-python",
                str(_idf_python()),
                *forwarded_list,
            ]
            return _run_steps(target, [(command, PROJECT_ROOT)], timeout=600.0)
        build_dir = BUILD_ROOT / "esp32-tests"
        command = _command(
            idf,
            [
                "-B",
                str(build_dir),
                "-D",
                f"SDKCONFIG={build_dir / 'sdkconfig'}",
                "build",
            ],
            python_executable=_idf_python(),
        )
        code = _run_steps(target, [(command, PROJECT_ROOT / "fw-esp32" / "tests")])
        if code != 0:
            return code
        return _blocked(
            target,
            "Unity target tests built under build/esp32-tests; run "
            "`python tools/run.py test-esp32-target -- --port COMx` to flash, "
            "execute and capture",
        )
    if target == "test-e2e":
        target_uri = os.environ.get("LA_E2E_TARGET")
        forwarded_list = list(forwarded)
        has_target = any(
            argument == "--target" or argument.startswith("--target=")
            for argument in forwarded_list
        )
        if not target_uri and not has_target:
            return _blocked(
                target,
                "E2E tests require a running analyzer link; set "
                "LA_E2E_TARGET=tcp://HOST:5555 or run "
                "`python tools/run.py test-e2e -- --target tcp://HOST:5555`",
            )
        command = [sys.executable, "-m", "pytest", "tests/e2e"]
        if target_uri:
            command.append(f"--target={target_uri}")
        command.extend(forwarded_list)
        return _run_steps(target, [(command, PROJECT_ROOT)])
    if target == "hil":
        port = os.environ.get("LA_HIL_PORT")
        forwarded_list = list(forwarded)
        has_port = any(
            argument == "--port" or argument.startswith("--port=")
            for argument in forwarded_list
        )
        if not port and not has_port:
            return _blocked(
                target,
                "HIL tests require authorized Zybo hardware, JD wiring with "
                "RTS/CTS and exclusive COM-port access; set LA_HIL_PORT=COMx "
                "or run `python tools/run.py hil -- --port COMx`",
            )
        command = [sys.executable, "-m", "pytest", "tests/hil"]
        if port:
            command.append(f"--port={port}")
        command.extend(forwarded_list)
        return _run_steps(target, [(command, PROJECT_ROOT)])
    if target == "bit":
        vivado = _tool("vivado")
        script = PROJECT_ROOT / "fpga" / "scripts" / "build.tcl"
        top = PROJECT_ROOT / "fpga" / "rtl" / "la_top.sv"
        if vivado is None:
            return _blocked(target, "Vivado is missing; install AMD Vivado 2026.1 or set VIVADO_EXE")
        if not top.is_file():
            return _blocked(target, f"RTL top is not present yet: {top} (P3 implementation gate)")
        command = _command(vivado, ["-mode", "batch", "-source", str(script), "-tclargs", str(PROJECT_ROOT)])
        return _run_steps(target, [(command, BUILD_ROOT / "vivado")])
    if target == "fw-zynq":
        vitis = _tool("vitis")
        script = PROJECT_ROOT / "fw-zynq" / "scripts" / "app.py"
        cli_smoke = PROJECT_ROOT / "fw-zynq" / "scripts" / "debug.py"
        xsa = BUILD_ROOT / "release" / "logic_analyzer.xsa"
        if vitis is None:
            return _blocked(target, "Vitis is missing; install AMD Vitis 2026.1 or set VITIS_EXE")
        if not xsa.exists():
            smoke_code = _run_steps(
                "vitis-cli",
                [(_command(vitis, ["-s", str(cli_smoke)]), BUILD_ROOT / "vitis")],
            )
            if smoke_code != 0:
                return smoke_code
            return _blocked(target, f"XSA is missing: run `python tools/run.py bit` first ({xsa})")
        command = _command(
            vitis,
            ["-s", str(script), "--", str(PROJECT_ROOT), str(xsa), *forwarded],
        )
        return _run_steps(target, [(command, BUILD_ROOT / "vitis")])
    if target == "jtag-zynq":
        xsdb = _tool("xsdb")
        script = PROJECT_ROOT / "fw-zynq" / "scripts" / "jtag_load.tcl"
        profile = "gateway"
        if "--profile" in forwarded:
            index = forwarded.index("--profile")
            if index + 1 < len(forwarded):
                profile = forwarded[index + 1]
        app_name = {
            "usb": "la_app_usb",
            "example": "la_usb_example",
        }.get(profile, "la_app")
        artifacts = (
            BUILD_ROOT / "release" / "logic_analyzer.bit",
            BUILD_ROOT / "vitis" / "ws" / "lap" / "zynq_fsbl" / "build" / "fsbl.elf",
            BUILD_ROOT / "vitis" / "ws" / app_name / "build" / f"{app_name}.elf",
        )
        if xsdb is None:
            return _blocked(target, "XSDB is missing; install AMD Vitis Embedded 2026.1 or set XSDB_EXE")
        missing = [str(path) for path in artifacts if not path.is_file()]
        if missing:
            return _blocked(
                target,
                "required artifact(s) missing; run `python tools/run.py bit` and "
                f"`python tools/run.py fw-zynq` first: {', '.join(missing)}",
            )
        command = _command(xsdb, [str(script), str(PROJECT_ROOT), *forwarded])
        return _run_steps(target, [(command, BUILD_ROOT / "vitis")], timeout=120.0)
    if target == "fw-esp32":
        idf = _tool("idf.py")
        if idf is None:
            return _blocked(target, "native ESP-IDF is missing; provision .deps/esp-idf and set IDF_PATH")
        command = _command(
            idf,
            [
                "-B",
                str(BUILD_ROOT / "esp32"),
                "-D",
                f"SDKCONFIG={BUILD_ROOT / 'esp32' / 'sdkconfig'}",
                "build",
                *forwarded,
            ],
            python_executable=_idf_python(),
        )
        return _run_steps(target, [(command, PROJECT_ROOT / "fw-esp32")])
    if target == "boot":
        script = PROJECT_ROOT / "fw-zynq" / "scripts" / "make_bootbin.py"
        command = [sys.executable, str(script), str(PROJECT_ROOT)]
        return _run_steps(target, [(command, PROJECT_ROOT)])
    if target == "package":
        script = PROJECT_ROOT / "tools" / "make_release.py"
        command = [sys.executable, str(script), str(PROJECT_ROOT)]
        return _run_steps(target, [(command, PROJECT_ROOT)])
    if target == "program":
        return _blocked(
            target,
            "programming is a [HUMAN] gate: set JP5=JTAG, attach the Zybo FTDI, "
            "confirm the intended target, then run the documented Vivado program.tcl command",
        )
    if target == "flash-esp":
        return _blocked(
            target,
            "flashing is a [HUMAN] gate: select the exact ESP32 COM port and "
            "run `idf.py -B build/esp32 -p COMx flash monitor` after confirming the module",
        )
    if target == "release":
        steps, blocked = _cmake_steps(target)
        if blocked is not None:
            return blocked
        gen_command, cwd = _python_script("gen_regmap.py", [])
        return _run_steps(target, [(gen_command, cwd), *steps])
    return _blocked(target, f"unknown target {target!r}")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=TARGETS)
    parser.add_argument("forwarded", nargs=argparse.REMAINDER, help="arguments forwarded to the target tool")
    args = parser.parse_args(argv)
    forwarded = list(args.forwarded)
    if forwarded[:1] == ["--"]:
        forwarded = forwarded[1:]
    return _run_target(args.target, forwarded)


if __name__ == "__main__":
    raise SystemExit(main())
