"""Compile the LaTeX HardwareX class report with the pinned Tectonic engine."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LATEX = ROOT / "docs" / "hardwarex" / "latex"
BUILD = ROOT / "build" / "latex-hardwarex"
TECTONIC = ROOT / ".tools" / "tectonic" / "tectonic.exe"
VARIANTS = (
    ("embedded", "hardwarex-embedded.tex"),
    ("fpga", "hardwarex-fpga.tex"),
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_files() -> list[Path]:
    files = [
        LATEX / "hardwarex-report.tex",
        LATEX / "hardwarex-embedded.tex",
        LATEX / "hardwarex-fpga.tex",
        LATEX / "HardwareXTemplate_20210707.tex",
        Path(__file__),
        ROOT / "tools" / "validate_hardwarex_latex.py",
        ROOT / "tools" / "bootstrap_tectonic.py",
        ROOT / "docs" / "hardwarex" / "manuscript.md",
    ]
    files += sorted((ROOT / "docs" / "hardwarex" / "assets" / "figures").glob("*.png"))
    files += sorted((ROOT / "docs" / "hardwarex" / "assets" / "uart-demo").glob("*.png"))
    return files


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-path", type=Path, default=TECTONIC)
    args = parser.parse_args()
    engine = args.engine_path.resolve()
    if not engine.is_file():
        parser.error(
            f"Tectonic not found at {engine}. Run: "
            f".venv\\Scripts\\python.exe tools\\bootstrap_tectonic.py --directory .tools\\tectonic"
        )
    if not LATEX.is_dir():
        parser.error(f"LaTeX source directory missing: {LATEX}")
    BUILD.mkdir(parents=True, exist_ok=True)
    version = subprocess.run(
        [str(engine), "--version"], check=True, text=True,
        capture_output=True).stdout.strip()
    runs = []
    compiled = {}
    for variant, filename in VARIANTS:
        command = [
            str(engine), "--keep-logs", "--keep-intermediates",
            "--outdir", str(BUILD), filename,
        ]
        result = subprocess.run(
            command, cwd=LATEX, text=True, capture_output=True,
            encoding="utf-8", errors="replace")
        print(result.stdout)
        if result.stderr:
            print(result.stderr)
        if result.returncode:
            raise RuntimeError(
                f"LaTeX build failed for {variant} (exit {result.returncode}); "
                f"inspect {BUILD / (variant + '.log')}")
        pdf = BUILD / (Path(filename).stem + ".pdf")
        if not pdf.is_file() or not pdf.stat().st_size:
            raise RuntimeError(f"Compiled PDF missing: {pdf}")
        target = LATEX / f"hardwarex-{variant}.pdf"
        shutil.copyfile(pdf, target)
        compiled[variant] = target
        runs.append({
            "variant": variant,
            "document": filename,
            "command": command,
            "cwd": str(LATEX),
            "exit_code": 0,
        })
    record = {
        "engine": "tectonic",
        "engine_version": version,
        "utc_timestamp": datetime.now(timezone.utc).isoformat(),
        "variants": [name for name, _ in VARIANTS],
        "runs": runs,
        "source_sha256": {
            str(path.relative_to(ROOT)).replace("\\", "/"): sha256(path)
            for path in source_files()
        },
        "pdf_sha256": {
            str(path.relative_to(ROOT)).replace("\\", "/"): sha256(path)
            for path in compiled.values()
        },
    }
    manifest = LATEX / "latex_build_manifest.json"
    manifest.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "engine": version,
        "variants": list(compiled),
        "pdfs": {name: str(path) for name, path in compiled.items()},
        "manifest": str(manifest),
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
