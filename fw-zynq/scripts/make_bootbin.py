"""P10.4 BOOT.BIN packaging (tool-only step; SD boot is a bench gate).

Collects the release artifacts (bitstream, FSBL ELF, la_app ELF renamed to
la_fw.elf), writes the bootgen image file under build/release and invokes
bootgen so the image creation flow is verified locally before the [HUMAN]
SD-card boot gate. Runs natively on Windows.

Usage: python fw-zynq/scripts/make_bootbin.py PROJECT_ROOT
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

BOOT_SYNC = b"\xfe\xff\xff\xea"  # bootgen Zynq image sync pad word


def _single_elf(workspace: Path, name: str) -> Path:
    matches = sorted(workspace.rglob(f"{name}.elf"))
    if not matches:
        raise SystemExit(
            f"boot blocked: no {name}.elf under {workspace}; run `python tools/run.py fw-zynq` first"
        )
    exported = [match for match in matches if "export" in match.parts]
    if len(matches) > 1 and not exported:
        listing = "\n".join(str(match) for match in matches)
        raise SystemExit(f"boot blocked: multiple {name}.elf candidates:\n{listing}")
    return exported[0] if exported else matches[0]


def _bootgen() -> list[str]:
    override = None
    for variable in ("BOOTGEN_EXE", "XILINX_VIVADO"):
        value = __import__("os").environ.get(variable)
        if value:
            candidate = Path(value)
            if variable == "BOOTGEN_EXE":
                override = candidate
                break
            override = candidate / "bin" / "bootgen.bat"
            break
    if override is None:
        for candidate in (
            Path(r"C:\AMDDesignTools\2026.1\Vivado\bin\bootgen.bat"),
            Path(r"C:\AMDDesignTools\2026.1\Vitis\bin\bootgen.bat"),
        ):
            if candidate.is_file():
                override = candidate
                break
    if override is None or not Path(override).is_file():
        raise SystemExit("boot blocked: bootgen.bat not found; set BOOTGEN_EXE")
    if Path(override).suffix.lower() in (".bat", ".cmd"):
        return ["cmd", "/c", str(override)]
    return [str(override)]


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        raise SystemExit("usage: make_bootbin.py PROJECT_ROOT")
    root = Path(argv[1]).resolve()
    release = root / "build" / "release"
    workspace = root / "build" / "vitis" / "ws"
    bit = release / "logic_analyzer.bit"
    if not bit.is_file():
        raise SystemExit(
            f"boot blocked: bitstream missing at {bit}; run `python tools/run.py bit` first"
        )
    fsbl = _single_elf(workspace, "fsbl")
    app = _single_elf(workspace, "la_app")
    staged_fsbl = release / "fsbl.elf"
    staged_app = release / "la_fw.elf"
    shutil.copyfile(fsbl, staged_fsbl)
    shutil.copyfile(app, staged_app)
    bif = release / "bootgen.bif"
    bif.write_text(
        "the_ROM_image:\n{\n"
        "    [bootloader]fsbl.elf\n"
        "    logic_analyzer.bit\n"
        "    la_fw.elf\n"
        "}\n",
        encoding="ascii",
        newline="\n",
    )
    image = release / "BOOT.BIN"
    if image.exists():
        image.unlink()
    command = _bootgen()
    print(f"BOOTGEN command: {' '.join(command)} -image {bif.name} -arch zynq -o BOOT.BIN")
    proc = subprocess.run(
        command + ["-image", str(bif), "-arch", "zynq", "-o", str(image)],
        cwd=str(release),
        capture_output=True,
        text=True,
        errors="replace",
    )
    tail = (proc.stdout + proc.stderr).strip().splitlines()[-12:]
    for line in tail:
        print(f"  {line}")
    if proc.returncode != 0 or not image.is_file():
        raise SystemExit(
            f"boot blocked: bootgen failed (rc={proc.returncode}); image {'missing' if not image.is_file() else 'present'}"
        )
    if image.stat().st_size < 1024 * 1024:
        raise SystemExit(f"boot blocked: BOOT.BIN implausibly small ({image.stat().st_size} bytes)")
    header = image.read_bytes()[:4]
    if header != BOOT_SYNC:
        raise SystemExit(f"boot blocked: unexpected BOOT.BIN header 0x{header.hex()}")
    sizes = {
        "fsbl.elf": staged_fsbl.stat().st_size,
        "la_fw.elf": staged_app.stat().st_size,
        "logic_analyzer.bit": bit.stat().st_size,
        "BOOT.BIN": image.stat().st_size,
    }
    for name, size in sizes.items():
        print(f"BOOT release artifact {name}: {size} bytes")
    print(f"BOOT-BIN PASS: {image}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
