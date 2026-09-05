"""Query the installed Vivado board parts without assuming a board-file version."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Sequence


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPORT_ROOT = PROJECT_ROOT / "build" / "reports"


def _configured(value: str, default: Path) -> Path:
    path = Path(os.path.expandvars(value)).expanduser() if value else default
    return path if path.is_absolute() else PROJECT_ROOT / path


def _find_vivado() -> Path | None:
    override = os.environ.get("VIVADO_EXE")
    vivado_home = _configured(
        os.environ.get("VIVADO_HOME", ""),
        Path(r"C:\AMDDesignTools\2026.1\Vivado"),
    )
    candidates: list[Path] = []
    if override:
        override_path = _configured(override, PROJECT_ROOT)
        candidates.extend(
            [override_path]
            if override_path.is_file()
            else [override_path / "vivado.exe", override_path / "vivado.bat", override_path / "vivado"]
        )
    candidates.extend(
        vivado_home / "bin" / name
        for name in ("vivado.exe", "vivado.bat", "vivado")
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve(strict=False)
    return None


def _command(executable: Path) -> list[str]:
    arguments = [
        "-mode",
        "batch",
        "-source",
        str(PROJECT_ROOT / "fpga" / "scripts" / "query_board.tcl"),
        "-tclargs",
        str(PROJECT_ROOT),
    ]
    if executable.suffix.lower() in {".bat", ".cmd"}:
        return [os.environ.get("COMSPEC", "cmd.exe"), "/d", "/s", "/c", str(executable), *arguments]
    return [str(executable), *arguments]


def _board_repository_commit() -> str | None:
    repository = PROJECT_ROOT / ".deps" / "vivado-boards"
    if not repository.is_dir():
        return None
    try:
        result = subprocess.run(
            ["git", "-C", str(repository), "rev-parse", "HEAD"],
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
    commit = result.stdout.strip()
    return commit if result.returncode == 0 and commit else None


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-lock", action="store_true")
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args(argv)
    vivado = _find_vivado()
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {
        "schema": 1,
        "checked_at": datetime.now(timezone.utc).isoformat(),
        "project_root": str(PROJECT_ROOT),
        "vivado": str(vivado) if vivado else None,
    }
    if vivado is None:
        report.update({"status": "blocked", "reason": "Vivado is missing; set VIVADO_HOME or VIVADO_EXE"})
        with (REPORT_ROOT / "board-query.json").open("w", encoding="utf-8", newline="\n") as handle:
            json.dump(report, handle, indent=2, sort_keys=True)
            handle.write("\n")
        print(f"BLOCKED: {report['reason']}", file=sys.stderr)
        return 2

    command = _command(vivado)
    try:
        result = subprocess.run(
            command,
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=args.timeout,
            check=False,
        )
        output = result.stdout + result.stderr
        returncode = result.returncode
    except (OSError, subprocess.TimeoutExpired) as exc:
        output = str(exc)
        returncode = 124 if isinstance(exc, subprocess.TimeoutExpired) else 127

    board_parts = [
        line.split("=", 1)[1].strip()
        for line in output.splitlines()
        if line.startswith("BOARD_PART=")
    ]
    report.update({
        "status": "pass" if returncode == 0 and board_parts else "blocked",
        "returncode": returncode,
        "board_parts": board_parts,
        "output": output,
    })
    with (REPORT_ROOT / "board-query.json").open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
        handle.write("\n")
    if returncode != 0 or not board_parts:
        print("BLOCKED: no installed Zybo board part was resolved", file=sys.stderr)
        return 1

    print("Installed Zybo board parts:")
    for board_part in board_parts:
        print(f"  {board_part}")
    if args.write_lock:
        lock_path = PROJECT_ROOT / "tools" / "board-lock.json"
        with lock_path.open("r", encoding="utf-8") as handle:
            lock = json.load(handle)
        lock["board_part"] = board_parts[0]
        commit = _board_repository_commit()
        if commit:
            lock["commit"] = commit
            lock["status"] = "board-part-and-commit-resolved"
        else:
            lock["status"] = "board-part-resolved;-commit-still-required"
        with lock_path.open("w", encoding="utf-8", newline="\n") as handle:
            json.dump(lock, handle, indent=2, sort_keys=True)
            handle.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
