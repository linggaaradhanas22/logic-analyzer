"""P10.4/V05 release packaging: stage the release artifact set and verify it.

Collects the artifacts a release consists of into ``build/release``:

- ``BOOT.BIN`` (produced by ``python tools/run.py boot`` from the FSBL, the
  bitstream and the selected profile's application ELF);
- the host tools wheel under ``build/release/wheel``;
- for the ``gateway`` profile, the ESP32 flash set (``bootloader.bin``,
  ``partition-table.bin`` and ``la_gateway.bin``) under
  ``build/release/esp32``. The ESP32 set is optional for the USB profile
  (the default): when present it is staged and labeled as the gateway
  profile, but a missing ESP32 build does not block a USB release.

Then it writes ``build/release/release-manifest.json`` with sizes and SHA-256
hashes. The ``[HUMAN]`` SD-card boot and the ``v1.0`` tag are separate gates and
are not performed here.

Usage: python tools/make_release.py PROJECT_ROOT [--profile usb|gateway]
"""

from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

PROFILES = ("usb", "gateway")


def parse_args(argv: list[str]) -> tuple[Path, str]:
    tokens = list(argv[1:])
    profile = "usb"
    if "--profile" in tokens:
        index = tokens.index("--profile")
        if index + 1 >= len(tokens):
            raise SystemExit("usage: make_release.py PROJECT_ROOT [--profile usb|gateway]")
        profile = tokens[index + 1]
        del tokens[index:index + 2]
    if profile not in PROFILES:
        raise SystemExit(f"package blocked: unknown profile: {profile}")
    if len(tokens) != 1:
        raise SystemExit("usage: make_release.py PROJECT_ROOT [--profile usb|gateway]")
    return Path(tokens[0]).resolve(), profile


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify_wheel(wheel: Path) -> list[str]:
    problems: list[str] = []
    if not wheel.is_file():
        return [f"wheel missing: {wheel}"]
    with zipfile.ZipFile(wheel) as archive:
        names = archive.namelist()
    required = [
        "host/la_cli/__init__.py",
        "host/la_sigrok_bridge/__init__.py",
        "tools/run.py",
    ]
    for entry in required:
        if not any(name == entry for name in names):
            problems.append(f"wheel missing {entry}")
    if not any(name.endswith(".dist-info/entry_points.txt") for name in names):
        problems.append("wheel missing the la-cli console-script entry point")
    return problems


def main(argv: list[str]) -> int:
    root, profile = parse_args(argv)
    release = root / "build" / "release"
    esp32_build = root / "build" / "esp32"
    release.mkdir(parents=True, exist_ok=True)
    errors: list[str] = []

    boot = release / "BOOT.BIN"
    if not boot.is_file():
        errors.append("BOOT.BIN missing: run `python tools/run.py boot` first")
    elif boot.read_bytes()[:4] != b"\xfe\xff\xff\xea":
        errors.append("BOOT.BIN has an unexpected header")

    esp32_stage = release / "esp32"
    esp32_sources = {
        "bootloader.bin": esp32_build / "bootloader" / "bootloader.bin",
        "partition-table.bin": esp32_build / "partition_table" / "partition-table.bin",
        "la_gateway.bin": esp32_build / "la_gateway.bin",
    }
    esp32_artifacts: list[Path] = []
    esp32_present = all(source.is_file() for source in esp32_sources.values())
    if profile == "gateway" and not esp32_present:
        for name, source in esp32_sources.items():
            if not source.is_file():
                errors.append(
                    f"ESP32 artifact missing: {source}; run `python tools/run.py fw-esp32` first"
                )
    if esp32_present:
        esp32_stage.mkdir(exist_ok=True)
        for name, source in esp32_sources.items():
            shutil.copyfile(source, esp32_stage / name)
            esp32_artifacts.append(esp32_stage / name)

    wheel_dir = release / "wheel"
    if wheel_dir.exists():
        shutil.rmtree(wheel_dir)
    wheel_dir.mkdir()
    build = subprocess.run(
        [sys.executable, "-m", "pip", "wheel", ".", "--no-deps", "--wheel-dir", str(wheel_dir)],
        cwd=str(root),
        capture_output=True,
        text=True,
        errors="replace",
    )
    wheels = sorted(wheel_dir.glob("la_host-*.whl"))
    if build.returncode != 0 or not wheels:
        errors.append(
            "wheel build failed (rc="
            f"{build.returncode}): {(build.stdout + build.stderr).strip().splitlines()[-3:]}"
        )
    else:
        errors.extend(verify_wheel(wheels[0]))

    if errors:
        for error in errors:
            print(f"PACKAGE BLOCKED: {error}")
        return 1

    artifacts = [boot, *esp32_artifacts, wheels[0]]
    manifest = {
        "profile": profile,
        "artifacts": [
            {
                "path": str(path.relative_to(root)).replace("\\", "/"),
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            }
            for path in artifacts
        ],
        "note": "Tooling evidence only: the [HUMAN] SD boot and the v1.0 tag are "
        f"separate gates. Profile: {profile} (BOOT.BIN carries the "
        f"{'la_app_usb' if profile == 'usb' else 'la_app'} application); the ESP32 "
        "images are the optional gateway profile.",
    }
    manifest_path = release / "release-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    for artifact in artifacts:
        print(f"PACKAGE artifact {artifact.relative_to(root)}: {artifact.stat().st_size} bytes")
    print(f"PACKAGE PASS: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
