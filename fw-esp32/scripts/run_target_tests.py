"""P5.2 target Unity test runner: build, flash and capture the ESP32 tests.

Invoked by `python tools/run.py test-esp32-target -- --port COM5` with the
native ESP-IDF environment already imported by the runner. It builds the
standalone Unity app under fw-esp32/tests, flashes it, resets the board into
run mode, captures the console until the app reports completion and parses
the Unity summary. The raw console text is kept under
build/reports/test-esp32-target-capture.log.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path

UNITY_SUMMARY = re.compile(r"(\d+) Tests (\d+) Failures (\d+) Ignored")


def _idf_command(idf: str, idf_python: str, arguments: list[str]) -> list[str]:
    """Builds an idf.py invocation for the .py/.bat/exe wrapper shapes."""
    lower = idf.lower()
    if lower.endswith(".py"):
        return [idf_python, idf, *arguments]
    if lower.endswith((".bat", ".cmd")):
        return [os.environ.get("COMSPEC", "cmd.exe"), "/d", "/s", "/c", idf, *arguments]
    return [idf, *arguments]


def _esptool_flash(project_root: Path, port: str, idf_python: str,
                   timeout_s: float) -> int:
    """Flashes with reset disabled: the DOIT board's auto-reset does not
    assert GPIO0, so the board must already be in manual download mode."""
    idf_path = Path(os.environ.get("IDF_PATH", str(project_root / ".deps" / "esp-idf")))
    esptool = idf_path / "components" / "esptool_py" / "esptool" / "esptool.py"
    build_dir = project_root / "build" / "esp32-tests"
    command = [
        idf_python,
        str(esptool),
        "--chip",
        "esp32",
        "-p",
        port,
        "-b",
        "460800",
        "--before",
        "no_reset",
        "--after",
        "hard_reset",
        "write_flash",
        "@flash_args",
    ]
    print("$ " + subprocess.list2cmdline(command), flush=True)
    try:
        result = subprocess.run(command, cwd=str(build_dir), timeout=timeout_s)
    except subprocess.TimeoutExpired:
        print("FAIL: esptool flash timed out", file=sys.stderr)
        return 1
    if result.returncode != 0:
        print("FAIL: esptool flash returned %d" % result.returncode, file=sys.stderr)
        return 1
    return 0


def _build_and_flash(project_root: Path, port: str, idf: str, idf_python: str,
                     timeout_s: float, no_reset: bool) -> int:
    tests_dir = project_root / "fw-esp32" / "tests"
    build_dir = project_root / "build" / "esp32-tests"
    if no_reset:
        return _esptool_flash(project_root, port, idf_python, timeout_s)
    arguments = [
        "-B",
        str(build_dir),
        "-D",
        f"SDKCONFIG={build_dir / 'sdkconfig'}",
        "-p",
        port,
        "flash",
    ]
    command = _idf_command(idf, idf_python, arguments)
    print("$ " + subprocess.list2cmdline(command), flush=True)
    try:
        result = subprocess.run(command, cwd=str(tests_dir), timeout=timeout_s)
    except subprocess.TimeoutExpired:
        print("FAIL: idf.py flash timed out", file=sys.stderr)
        return 1
    if result.returncode != 0:
        print(
            "FAIL: idf.py flash returned %d; enter manual download mode "
            "(hold BOOT, tap EN/RST, release BOOT) and re-run with --no-reset"
            % result.returncode,
            file=sys.stderr,
        )
        return 1
    return 0


def _capture(project_root: Path, port: str, timeout_s: float) -> tuple[str, int]:
    import serial

    lines: list[str] = []
    try:
        console = serial.Serial(port, 115200, timeout=0.2)
    except Exception as exc:
        print(f"FAIL: cannot open {port}: {exc}", file=sys.stderr)
        return "", 1
    try:
        # Auto-reset into run mode: GPIO0 high (DTR false), pulse EN low.
        console.setDTR(False)
        console.setRTS(True)
        time.sleep(0.1)
        console.setRTS(False)
        time.sleep(0.1)
        console.reset_input_buffer()
        deadline = time.monotonic() + timeout_s
        buffer = b""
        while time.monotonic() < deadline:
            data = console.read(256)
            if not data:
                continue
            buffer += data
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                text = line.decode("utf-8", errors="replace").rstrip("\r")
                lines.append(text)
                print(text, flush=True)
                if "LA_TARGET_TESTS_DONE" in text:
                    deadline = time.monotonic()
                    break
    finally:
        console.close()
    return "\n".join(lines), 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--port", required=True)
    parser.add_argument("--idf", required=True, help="resolved idf.py path")
    parser.add_argument("--idf-python", required=True,
                        help="Python interpreter for the IDF tools environment")
    parser.add_argument("--no-reset", action="store_true",
                        help="flash with esptool --before no_reset (the board "
                        "must already be in manual download mode)")
    parser.add_argument("--build-timeout", type=float, default=600.0)
    parser.add_argument("--capture-timeout", type=float, default=90.0)
    args = parser.parse_args()

    project_root = Path(args.project_root)
    if _build_and_flash(project_root, args.port, args.idf, args.idf_python,
                        args.build_timeout, args.no_reset) != 0:
        return 1
    text, code = _capture(project_root, args.port, args.capture_timeout)
    if code != 0:
        return code

    report_dir = project_root / "build" / "reports"
    report_dir.mkdir(parents=True, exist_ok=True)
    (report_dir / "test-esp32-target-capture.log").write_text(
        text, encoding="utf-8", newline="\n"
    )
    if "LA_TARGET_TESTS_DONE" not in text:
        print("FAIL: the test app did not report completion", file=sys.stderr)
        return 1
    passes = len(re.findall(r":PASS\b", text))
    fails = len(re.findall(r":FAIL\b", text))
    match = UNITY_SUMMARY.search(text)
    if match is not None:
        tests = int(match.group(1))
        failures = int(match.group(2))
    else:
        # unity_run_all_tests() prints one :PASS/:FAIL line per case without
        # the summary block that unity_run_menu() adds.
        tests = passes + fails
        failures = fails
    if passes == 0 or failures != 0:
        print(f"FAIL: {failures} Unity failures of {tests} tests", file=sys.stderr)
        return 1
    print(f"PASS: {tests} Unity tests, 0 failures")
    return 0


if __name__ == "__main__":
    sys.exit(main())
