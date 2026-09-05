"""Check the native Windows toolchain and project path contract."""

from __future__ import annotations

import argparse
import importlib.metadata
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_ROOT = PROJECT_ROOT / "build"
REPORT_ROOT = BUILD_ROOT / "reports"
TMP_ROOT = BUILD_ROOT / "tmp"


def _path(value: str | os.PathLike[str]) -> Path:
    candidate = Path(os.path.expandvars(os.fspath(value))).expanduser()
    if not candidate.is_absolute():
        candidate = PROJECT_ROOT / candidate
    return candidate.resolve(strict=False)


def _inside(path: Path, parent: Path = PROJECT_ROOT) -> bool:
    try:
        path.relative_to(parent)
    except ValueError:
        return False
    return True


def _env_path(name: str, default: Path) -> Path:
    value = os.environ.get(name)
    return _path(value) if value else default.resolve(strict=False)


def _idf_python() -> Path:
    configured = os.environ.get("IDF_PYTHON_ENV_PATH")
    if configured:
        root = _path(configured)
        candidate = root / "Scripts" / "python.exe"
        if candidate.is_file():
            return candidate.resolve(strict=False)
    tools_root = _env_path("IDF_TOOLS_PATH", PROJECT_ROOT / ".tools" / "espressif")
    candidates = sorted(
        (tools_root / "python_env").glob("idf*_py*_env/Scripts/python.exe"),
        reverse=True,
    )
    return candidates[0].resolve(strict=False) if candidates else Path(sys.executable)


def _candidate_names(name: str) -> Iterable[str]:
    if not Path(name).suffix:
        yield f"{name}.exe"
        yield f"{name}.bat"
        yield f"{name}.cmd"
    yield name


def _find_tool(name: str, *, roots: Iterable[Path] = (), override: str | None = None) -> Path | None:
    candidates: list[Path] = []
    if override:
        override_path = _path(override)
        if override_path.is_file():
            return override_path
        candidates.extend(override_path / item for item in _candidate_names(name))
    for root in roots:
        for item in _candidate_names(name):
            candidates.append(root / item)
            candidates.append(root / "bin" / item)
    for item in _candidate_names(name):
        found = shutil.which(item)
        if found:
            return Path(found).resolve(strict=False)
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve(strict=False)
    return None


def _version(path: Path | None, *, tool: str) -> tuple[str | None, str | None]:
    if path is None:
        return None, "not found"
    if tool == "hw_server":
        # hw_server is a daemon and leaves a child process behind when its
        # startup wrapper is killed. Its version is verified by the AMD bundle
        # during the hardware-manager gate instead of starting it here.
        return None, "daemon version deferred to the hardware-manager smoke test"
    suffix = path.suffix.lower()
    if tool == "vlib":
        # vlib has no version switch; -help is the non-destructive probe.
        command = [str(path), "-help"]
    elif suffix in {".bat", ".cmd"}:
        version_flag = "-version" if tool in {"vlog", "vsim", "vivado"} else "--version"
        command = [os.environ.get("COMSPEC", "cmd.exe"), "/d", "/s", "/c", str(path), version_flag]
    elif suffix == ".py":
        command = [str(_idf_python() if tool == "idf.py" else sys.executable), str(path), "--version"]
    else:
        version_flag = "-version" if tool in {"vlog", "vsim", "vivado"} else "--version"
        command = [str(path), version_flag]
    if tool == "cl":
        command = [str(path)]
    try:
        result = subprocess.run(
            command,
            cwd=PROJECT_ROOT,
            env=_child_environment(),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=15,
            check=False,
        )
    except OSError as exc:
        return None, str(exc)
    except subprocess.TimeoutExpired as exc:
        output = (exc.stdout or b"") + (exc.stderr or b"")
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        first_line = next((line.strip() for line in output.splitlines() if line.strip()), "")
        return (first_line or None), str(exc)
    output = (result.stdout + "\n" + result.stderr).strip()
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    first_line = next(
        (line for line in lines if re.search(r"(?:v20\d{2}\.\d|version|Version)", line)),
        lines[0] if lines else "",
    )
    if not first_line:
        first_line = f"exit code {result.returncode}"
    if result.returncode != 0 and not output:
        return None, f"command exited {result.returncode}"
    return first_line, None


def _child_environment() -> dict[str, str]:
    environment = os.environ.copy()
    for directory in (TMP_ROOT, BUILD_ROOT / "cache"):
        directory.mkdir(parents=True, exist_ok=True)
    environment["TEMP"] = str(TMP_ROOT)
    environment["TMP"] = str(TMP_ROOT)
    environment["TMPDIR"] = str(TMP_ROOT)
    environment["PIP_CACHE_DIR"] = str(BUILD_ROOT / "cache" / "pip")
    environment.setdefault("IDF_TOOLS_PATH", str(PROJECT_ROOT / ".tools" / "espressif"))
    return environment


def _visual_studio_cl() -> Path | None:
    def preferred(matches: list[Path]) -> Path:
        normalized = {
            path: path.as_posix().lower().replace("/", "\\")
            for path in matches
        }
        return max(
            matches,
            key=lambda path: (
                "\\hostx64\\x64\\" in normalized[path],
                "\\x64\\" in normalized[path],
                str(path),
            ),
        )

    direct = _find_tool("cl", override=os.environ.get("CL_EXE"))
    if direct:
        return direct
    for variable in ("VCINSTALLDIR", "VSINSTALLDIR"):
        value = os.environ.get(variable)
        if not value:
            continue
        root = _path(value)
        matches = sorted(root.rglob("cl.exe")) if root.exists() else []
        if matches:
            return preferred(matches).resolve(strict=False)
    for base in (
        Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Microsoft Visual Studio",
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio",
    ):
        if base.exists():
            matches = sorted(base.rglob("cl.exe"))
            if matches:
                return preferred(matches).resolve(strict=False)
    return None


def _tool_roots() -> tuple[Path, Path, Path, Path]:
    amd_root = _env_path("AMD_ROOT", Path(r"C:\AMDDesignTools\2026.1"))
    vivado = _env_path("VIVADO_HOME", amd_root / "Vivado")
    vitis = _env_path("VITIS_HOME", amd_root / "Vitis")
    questa = _env_path("QUESTA_ROOT", Path(r"C:\altera_lite\25.1std\questa_fse\win64"))
    return vivado, vitis, questa, amd_root


def _tool_results() -> dict[str, dict[str, Any]]:
    vivado, vitis, questa, _ = _tool_roots()
    tools: dict[str, tuple[Path | None, bool]] = {
        "python": (Path(sys.executable).resolve(strict=False), True),
        "vlib": (_find_tool("vlib", roots=(questa,), override=os.environ.get("VLIB_EXE")), True),
        "vlog": (_find_tool("vlog", roots=(questa,), override=os.environ.get("VLOG_EXE")), True),
        "vsim": (_find_tool("vsim", roots=(questa,), override=os.environ.get("VSIM_EXE")), True),
        "vivado": (_find_tool("vivado", roots=(vivado,), override=os.environ.get("VIVADO_EXE")), True),
        "vitis": (_find_tool("vitis", roots=(vitis,), override=os.environ.get("VITIS_EXE")), True),
        "hw_server": (
            _find_tool("hw_server", roots=(vivado,), override=os.environ.get("HW_SERVER_EXE")),
            True,
        ),
        "bootgen": (_find_tool("bootgen", roots=(vitis, vivado), override=os.environ.get("BOOTGEN_EXE")), True),
        "cl": (_visual_studio_cl(), True),
        "cmake": (
            _find_tool(
                "cmake",
                roots=(
                    _path(os.environ["CMAKE_HOME"]) if os.environ.get("CMAKE_HOME") else Path(r"C:\Program Files\CMake"),
                ),
                override=os.environ.get("CMAKE_EXE"),
            ),
            True,
        ),
        "ninja": (
            _find_tool(
                "ninja",
                roots=(Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Ninja",),
                override=os.environ.get("NINJA_EXE"),
            ),
            True,
        ),
        "pulseview": (
            _find_tool(
                "pulseview",
                roots=(
                    PROJECT_ROOT / "build" / "pulseview",
                    Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "PulseView",
                    Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "sigrok" / "PulseView",
                ),
                override=os.environ.get("PULSEVIEW_EXE"),
            ),
            True,
        ),
        "idf.py": (
            _find_tool(
                "idf.py",
                roots=(
                    _env_path("IDF_PATH", PROJECT_ROOT / ".deps" / "esp-idf"),
                    _env_path("IDF_PATH", PROJECT_ROOT / ".deps" / "esp-idf") / "tools",
                ),
                override=os.environ.get("IDF_PY"),
            ),
            True,
        ),
    }
    result: dict[str, dict[str, Any]] = {}
    for name, (path, required) in tools.items():
        version, error = _version(path, tool=name)
        result[name] = {
            "required_for_p0": required,
            "path": str(path) if path else None,
            "version": version,
            "error": error,
            "status": "found" if path else "missing",
        }
    if result["vlib"]["status"] == "found" and result["vlog"]["version"]:
        result["vlib"]["version"] = result["vlog"]["version"]
        result["vlib"]["version_source"] = "vlog probe; vlib has no version switch"

    arm_candidates: list[Path] = []
    arm_override = os.environ.get("ARM_TOOLCHAIN_ROOT")
    if arm_override:
        arm_candidates.append(_path(arm_override))
    _, vitis, _, _ = _tool_roots()
    arm_candidates.extend(
        [
            vitis / "gnu" / "aarch32" / "nt" / "x86_64" / "arm" / "arm-none-eabi" / "bin",
            vitis / "gnu" / "aarch32" / "nt" / "x86_64" / "arm" / "bin",
        ]
    )
    for program_files in (
        Path(os.environ.get("ProgramFiles", r"C:\Program Files")),
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")),
    ):
        arm_root = program_files / "Arm GNU Toolchain arm-none-eabi"
        if arm_root.exists():
            arm_candidates.extend(arm_root.glob("*/bin"))
    arm = _find_tool("arm-none-eabi-gcc", roots=arm_candidates, override=os.environ.get("ARM_GCC_EXE"))
    version, error = _version(arm, tool="arm-none-eabi-gcc")
    result["arm-none-eabi-gcc"] = {
        "required_for_p0": True,
        "path": str(arm) if arm else None,
        "version": version,
        "error": error,
        "status": "found" if arm else "missing",
    }
    return result


def _path_checks() -> tuple[list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []
    project_paths = {
        "PROJECT_ROOT": PROJECT_ROOT,
        "PYTHON_ENV": _env_path("VIRTUAL_ENV", PROJECT_ROOT / ".venv"),
        "IDF_PATH": _env_path("IDF_PATH", PROJECT_ROOT / ".deps" / "esp-idf"),
        "IDF_TOOLS_PATH": _env_path("IDF_TOOLS_PATH", PROJECT_ROOT / ".tools" / "espressif"),
        "BUILD_ROOT": BUILD_ROOT,
        "QUESTA_WORK": BUILD_ROOT / "questa",
        "NATIVE_WORK": BUILD_ROOT / "native",
        "VIVADO_WORK": BUILD_ROOT / "vivado",
        "VITIS_WORK": BUILD_ROOT / "vitis",
        "ESP32_WORK": BUILD_ROOT / "esp32",
        "RELEASE_ROOT": BUILD_ROOT / "release",
        "REPORT_ROOT": REPORT_ROOT,
        "TEMP_ROOT": TMP_ROOT,
        "CACHE_ROOT": BUILD_ROOT / "cache",
    }
    for name, path in project_paths.items():
        if not _inside(path):
            errors.append(f"{name} resolves outside PROJECT_ROOT: {path}")

    for variable in ("TEMP", "TMP", "TMPDIR", "PIP_CACHE_DIR"):
        value = os.environ.get(variable)
        if value:
            configured = _path(value)
            if not _inside(configured):
                warnings.append(
                    f"{variable} is outside PROJECT_ROOT ({configured}); runner child processes will override it"
                )

    for variable in (
        "CMAKE_BUILD_DIR",
        "VIVADO_PROJECT_DIR",
        "VITIS_WORKSPACE",
        "ESP32_BUILD_DIR",
        "CAPTURE_OUTPUT_DIR",
    ):
        value = os.environ.get(variable)
        if value and not _inside(_path(value)):
            errors.append(f"{variable} resolves outside PROJECT_ROOT: {_path(value)}")

    probe = TMP_ROOT / "env-check" / "path-probe" / ("x" * 80) / "marker.txt"
    try:
        probe.parent.mkdir(parents=True, exist_ok=True)
        probe.write_text("path probe\n", encoding="ascii", newline="\n")
        probe.unlink()
        if len(str(probe)) >= 240:
            warnings.append(f"planned path is near the Windows legacy limit ({len(str(probe))} chars): {probe}")
    except OSError as exc:
        errors.append(f"cannot create project-local path probe {probe}: {exc}")
    return errors, warnings


def _device_checks(vivado: Path | None) -> tuple[dict[str, Any], list[str]]:
    details: dict[str, Any] = {}
    errors: list[str] = []
    vivado_home, _, _, _ = _tool_roots()
    if vivado is None:
        errors.append("Vivado executable missing; cannot verify Xilinx device packages")
        return details, errors
    parts = [vivado_home / "data" / "parts", vivado_home / "data" / "parts" / "xilinx"]
    part_root = next((candidate for candidate in parts if candidate.exists()), None)
    details["parts_root"] = str(part_root) if part_root else None
    if part_root is None:
        errors.append(f"Vivado device data missing below {vivado_home / 'data' / 'parts'}")
    board_roots = (
        PROJECT_ROOT / "fpga" / "board_files",
        vivado_home / "data" / "boards" / "board_files",
    )
    board_root = next((candidate for candidate in board_roots if candidate.exists()), None)
    details["board_files_root"] = str(board_root) if board_root else None
    if board_root is None:
        errors.append(f"Vivado board files missing: {board_roots[1]}")
    return details, errors


def _write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(value, handle, indent=2, sort_keys=True)
        handle.write("\n")


def _report_status(name: str) -> str:
    path = REPORT_ROOT / name
    if not path.is_file():
        return "not-run"
    try:
        with path.open("r", encoding="utf-8") as handle:
            value = json.load(handle)
    except (OSError, json.JSONDecodeError):
        return "not-run"
    status = value.get("status") if isinstance(value, dict) else None
    return status if status in {"pass", "fail", "blocked"} else "not-run"


def _python_packages() -> dict[str, str | None]:
    try:
        import tomllib
    except ModuleNotFoundError:
        return {}

    try:
        with (PROJECT_ROOT / "pyproject.toml").open("rb") as handle:
            project = tomllib.load(handle).get("project", {})
    except (OSError, tomllib.TOMLDecodeError):
        return {}
    dependencies = project.get("dependencies", []) if isinstance(project, dict) else []
    packages: dict[str, str | None] = {}
    for dependency in dependencies:
        if not isinstance(dependency, str):
            continue
        name = re.split(r"[<>=!~\[]", dependency, maxsplit=1)[0].strip()
        if not name:
            continue
        try:
            packages[name] = importlib.metadata.version(name)
        except importlib.metadata.PackageNotFoundError:
            packages[name] = None
    return packages


def _board_lock() -> dict[str, Any]:
    path = PROJECT_ROOT / "tools" / "board-lock.json"
    try:
        with path.open("r", encoding="utf-8") as handle:
            value = json.load(handle)
    except (OSError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


def _git_revision(path: Path) -> str | None:
    if not path.is_dir():
        return None
    try:
        result = subprocess.run(
            ["git", "-C", str(path), "rev-parse", "HEAD"],
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=15,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    revision = result.stdout.strip()
    return revision if result.returncode == 0 and revision else None


def _lock_file(report: dict[str, Any], tools: dict[str, dict[str, Any]]) -> dict[str, Any]:
    board = _board_lock()
    idf_root = _env_path("IDF_PATH", PROJECT_ROOT / ".deps" / "esp-idf")
    missing_prerequisites = list(dict.fromkeys(report["errors"]))
    return {
        "schema": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "project_root": str(PROJECT_ROOT),
        "platform": {"system": platform.system(), "release": platform.release(), "python": sys.version.split()[0]},
        "status": "blocked" if report["errors"] else "environment-observed",
        "missing_prerequisites": missing_prerequisites,
        "tools": tools,
        "python_packages": _python_packages(),
        "esp_idf_source": {
            "path": str(idf_root),
            "commit": _git_revision(idf_root),
            "version": tools.get("idf.py", {}).get("version"),
        },
        "smoke_tests": {
            "native_cmake": _report_status("test-native.json"),
            "questa": _report_status("sim.json"),
            "vivado": _report_status("bit.json"),
            "vitis_cli": _report_status("vitis-cli.json"),
            "vitis_application": _report_status("fw-zynq.json"),
            "esp_idf": _report_status("fw-esp32.json"),
        },
        "board": {
            "repository": board.get("repository", "https://github.com/Digilent/vivado-boards.git"),
            "commit": board.get("commit"),
            "part": board.get("board_part") or board.get("target_part", "xc7z010clg400-1"),
        },
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-lock", action="store_true", help="write tools/toolchain-lock.json")
    parser.add_argument("--report", type=Path, help="override the report path (must stay in the project root)")
    args = parser.parse_args(argv)

    report_path = _path(args.report) if args.report else REPORT_ROOT / "env-check.json"
    if not _inside(report_path):
        print(f"check_env: report path is outside PROJECT_ROOT: {report_path}", file=sys.stderr)
        return 2

    errors: list[str] = []
    warnings: list[str] = []
    if os.name != "nt":
        errors.append("native Windows prerequisite not met: run this gate on Windows")
    path_errors, path_warnings = _path_checks()
    errors.extend(path_errors)
    warnings.extend(path_warnings)
    tools = _tool_results()
    for name, value in tools.items():
        if value["required_for_p0"] and value["status"] != "found":
            errors.append(f"missing required P0 prerequisite: {name}")
    device_details, device_errors = _device_checks(
        Path(tools["vivado"]["path"]) if tools["vivado"]["path"] else None
    )
    errors.extend(device_errors)

    report: dict[str, Any] = {
        "schema": 1,
        "checked_at": datetime.now(timezone.utc).isoformat(),
        "project_root": str(PROJECT_ROOT),
        "platform": {"system": platform.system(), "release": platform.release(), "python": sys.version},
        "status": "pass" if not errors else "blocked",
        "errors": errors,
        "warnings": warnings,
        "tools": tools,
        "device_support": device_details,
        "project_paths": {
            "build": str(BUILD_ROOT),
            "reports": str(REPORT_ROOT),
            "temporary": str(TMP_ROOT),
            "cache": str(BUILD_ROOT / "cache"),
        },
    }
    _write_json(report_path, report)
    if args.write_lock:
        lock_path = PROJECT_ROOT / "tools" / "toolchain-lock.json"
        _write_json(lock_path, _lock_file(report, tools))

    print(f"Project root: {PROJECT_ROOT}")
    print(f"Environment status: {report['status']}")
    for name, value in tools.items():
        suffix = f" ({value['version']})" if value["version"] else ""
        print(f"  {name}: {value['status']}" + suffix)
    for warning in warnings:
        print(f"WARNING: {warning}")
    for error in errors:
        print(f"BLOCKED: {error}", file=sys.stderr)
    print(f"Report: {report_path}")
    if args.write_lock:
        print(f"Lock: {PROJECT_ROOT / 'tools' / 'toolchain-lock.json'}")
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
