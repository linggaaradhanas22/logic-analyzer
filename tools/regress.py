"""Procedural Questa regression runner for the native Windows flow."""

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
TESTLIST = PROJECT_ROOT / "fpga" / "tb" / "testlist.json"
WORK_ROOT = PROJECT_ROOT / "build" / "questa"
REPORT_ROOT = PROJECT_ROOT / "build" / "reports"


def _resolve(value: str | os.PathLike[str]) -> Path:
    path = Path(os.path.expandvars(os.fspath(value))).expanduser()
    if not path.is_absolute():
        path = PROJECT_ROOT / path
    return path.resolve(strict=False)


def _find(name: str, env_name: str) -> Path | None:
    override = os.environ.get(env_name)
    roots = [_resolve(os.environ["QUESTA_ROOT"])] if os.environ.get("QUESTA_ROOT") else [
        Path(r"C:\altera_lite\25.1std\questa_fse\win64")
    ]
    if override:
        candidate = _resolve(override)
        if candidate.is_file():
            return candidate
        candidates = [candidate / name, candidate / f"{name}.exe", candidate / f"{name}.bat"]
    else:
        candidates = []
    for root in roots:
        candidates.extend(
            root / suffix
            for suffix in (name, f"{name}.exe", f"{name}.bat", f"{name}.cmd")
        )
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve(strict=False)
    for candidate_name in (name, f"{name}.exe", f"{name}.bat", f"{name}.cmd"):
        found = next((Path(item) for item in os.environ.get("PATH", "").split(os.pathsep)
                      if (Path(item) / candidate_name).is_file()), None)
        if found:
            return found.resolve(strict=False)
    return None


def _command(executable: Path, arguments: Sequence[str]) -> list[str]:
    if executable.suffix.lower() in {".bat", ".cmd"}:
        return [
            os.environ.get("COMSPEC", "cmd.exe"),
            "/d",
            "/s",
            "/c",
            subprocess.list2cmdline([str(executable), *arguments]),
        ]
    return [str(executable), *arguments]


def _run(command: Sequence[str], cwd: Path, timeout: float) -> tuple[int, str, bool]:
    try:
        result = subprocess.run(
            list(command),
            cwd=cwd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
        return result.returncode, result.stdout + result.stderr, False
    except FileNotFoundError as exc:
        return 127, f"executable could not start: {exc}\n", False
    except subprocess.TimeoutExpired as exc:
        output = (exc.stdout or b"") + (exc.stderr or b"")
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        return 124, str(output) + f"\nTIMEOUT after {timeout} seconds\n", True


def _failure_markers(output: str) -> list[str]:
    lowered = output.lower()
    markers = []
    if "fatal" in lowered:
        markers.append("fatal")
    if "** error" in lowered or "fail:" in lowered:
        markers.append("error/fail marker")
    return markers


def _simulation_markers(output: str, result_path: Path, expected_marker: str) -> list[str]:
    markers = _failure_markers(output)
    if expected_marker not in output:
        markers.append("missing PASS marker")
    if not result_path.is_file():
        markers.append("missing result file")
    else:
        result = result_path.read_text(encoding="utf-8", errors="replace")
        if expected_marker not in result:
            markers.append("result mismatch")
    return markers


def _self_test() -> int:
    cases = [
        ("pass", [sys.executable, "-c", "print('PASS: synthetic')"], False),
        ("intentional-failure", [sys.executable, "-c", "print('FAIL: synthetic'); raise SystemExit(3)"], False),
        ("missing-pass", [sys.executable, "-c", "print('synthetic output')"], False),
        ("timeout", [sys.executable, "-c", "import time; time.sleep(2)"], True),
    ]
    for name, command, expect_timeout in cases:
        code, output, timed_out = _run(command, PROJECT_ROOT, 0.2 if expect_timeout else 5.0)
        if name == "pass" and (code != 0 or "PASS:" not in output):
            print("regress self-test failed: PASS detection", file=sys.stderr)
            return 1
        if name == "intentional-failure" and code == 0:
            print("regress self-test failed: intentional failure was accepted", file=sys.stderr)
            return 1
        if name == "missing-pass":
            markers = _simulation_markers(output, REPORT_ROOT / "__regress_missing_result__", "PASS: synthetic")
            if "missing PASS marker" not in markers:
                print("regress self-test failed: missing PASS was accepted", file=sys.stderr)
                return 1
        if name == "timeout" and (not timed_out or code != 124):
            print("regress self-test failed: timeout detection", file=sys.stderr)
            return 1
    print("PASS: regress-self-test")
    return 0


def _load_tests() -> list[dict[str, Any]]:
    with TESTLIST.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    tests = data.get("tests") if isinstance(data, dict) else None
    if not isinstance(tests, list) or not tests:
        raise ValueError("testlist must contain a non-empty tests list")
    return tests


def _write_report(value: dict[str, Any]) -> None:
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    with (REPORT_ROOT / "questa-regress.json").open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(value, handle, indent=2, sort_keys=True)
        handle.write("\n")


def run_regression(lint: bool, timeout: float) -> int:
    vlib = _find("vlib", "VLIB_EXE")
    vlog = _find("vlog", "VLOG_EXE")
    vsim = _find("vsim", "VSIM_EXE")
    required = {"vlib": vlib, "vlog": vlog}
    if not lint:
        required["vsim"] = vsim
    missing = [name for name, path in required.items() if path is None]
    if missing:
        message = "Questa prerequisite missing: " + ", ".join(missing) + ". Set QUESTA_ROOT or the *_EXE variable."
        print(f"BLOCKED: {message}", file=sys.stderr)
        _write_report({
            "schema": 1,
            "status": "blocked",
            "reason": message,
            "checked_at": datetime.now(timezone.utc).isoformat(),
        })
        return 2

    try:
        tests = _load_tests()
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"regress: invalid testlist: {exc}", file=sys.stderr)
        return 1

    WORK_ROOT.mkdir(parents=True, exist_ok=True)
    overall = "pass"
    case_reports: list[dict[str, Any]] = []
    for case in tests:
        name = case.get("name")
        top = case.get("top")
        source_names = case.get("sources")
        if not isinstance(name, str) or not isinstance(top, str) or not isinstance(source_names, list):
            print(f"regress: malformed test entry: {case!r}", file=sys.stderr)
            overall = "fail"
            continue
        work = WORK_ROOT / name
        work.mkdir(parents=True, exist_ok=True)
        library_work = work / "work"
        if library_work.exists():
            shutil.rmtree(library_work)
        sources = [_resolve(item) for item in source_names if isinstance(item, str)]
        missing_sources = [str(path) for path in sources if not path.is_file()]
        if missing_sources:
            print(f"regress: missing sources for {name}: {missing_sources}", file=sys.stderr)
            overall = "fail"
            case_reports.append({"name": name, "status": "fail", "missing_sources": missing_sources})
            continue

        case_started = time.monotonic()
        log_parts: list[str] = []
        vector_path = work / "vectors.txt"
        result_path = work / "results.txt"
        vector_path.write_text("90 90\n", encoding="ascii", newline="\n")
        if result_path.exists():
            result_path.unlink()
        commands = [
            _command(vlib, ["work"]),
            _command(vlog, ["-sv", "-work", "work", *[str(path) for path in sources]]),
        ]
        if not lint:
            commands.append(
                _command(
                    vsim,
                    [
                        "-c",
                        "-lib",
                        "work",
                        top,
                        f"+VECTOR_FILE={vector_path}",
                        f"+RESULT_FILE={result_path}",
                        "-do",
                        "run -all; quit -f",
                    ],
                )
            )
        case_code = 0
        timed_out = False
        markers: list[str] = []
        for command in commands:
            code, output, command_timed_out = _run(command, work, timeout)
            log_parts.append(f"$ {subprocess.list2cmdline(command)}\n{output}")
            case_code = code
            timed_out = timed_out or command_timed_out
            markers.extend(_failure_markers(output))
            if code != 0 or command_timed_out:
                break
        combined = "\n".join(log_parts)
        log_path = work / "transcript.log"
        log_path.write_text(combined, encoding="utf-8", newline="\n")
        expected_marker = f"PASS: {name}"
        if not lint:
            markers.extend(_simulation_markers(combined, result_path, expected_marker))
        case_status = "pass" if case_code == 0 and not timed_out and not markers else "fail"
        if case_status != "pass":
            overall = "fail"
        case_reports.append({
            "name": name,
            "top": top,
            "status": case_status,
            "returncode": case_code,
            "timeout": timed_out,
            "markers": sorted(set(markers)),
            "elapsed_seconds": round(time.monotonic() - case_started, 3),
            "vector": str(vector_path),
            "result": str(result_path),
            "log": str(log_path),
        })

    report = {
        "schema": 1,
        "status": overall,
        "lint": lint,
        "checked_at": datetime.now(timezone.utc).isoformat(),
        "project_root": str(PROJECT_ROOT),
        "cases": case_reports,
    }
    _write_report(report)
    if overall == "pass":
        print("PASS: questa-regression")
        return 0
    print("FAIL: questa-regression", file=sys.stderr)
    return 1


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lint", action="store_true", help="compile test sources without simulation")
    parser.add_argument("--self-test", action="store_true", help="test PASS/failure/timeout handling without Questa")
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args(argv)
    if args.self_test:
        return _self_test()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    return run_regression(args.lint, args.timeout)


if __name__ == "__main__":
    raise SystemExit(main())
