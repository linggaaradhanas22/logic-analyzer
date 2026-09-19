"""U03 USB event-trace retrieval and decoder.

The firmware keeps a bounded ring (`la_usb_trace_ring`, see
`fw-zynq/app/src/drivers/la_usb_trace.h`) instead of printing long diagnostic
blocks. This tool resolves the symbol from the ELF with `arm-none-eabi-nm`,
dumps the ring over XSDB/JTAG while the application keeps running, and renders
the entries.

    python tools/usb_trace.py --elf build/vitis/ws/la_app_usb/build/la_app_usb.elf \
        --out build/reports/usb-migration/U03/usb-trace-dump.txt
    python tools/usb_trace.py --decode build/reports/usb-migration/U03/usb-trace-dump.txt

The dump step needs the board connected over JTAG; the decode step is offline
and is the one exercised by the host tests with synthetic dumps.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
CAPACITY = 4096
TAG_NAMES = {
    0: "NONE",
    1: "SETUP",
    2: "OUT_DTD",
    3: "STAGE_PUSH",
    4: "STAGE_POP",
    5: "RX_SB",
    6: "TX_SUBMIT",
    7: "TX_DONE",
    8: "RESET",
    9: "SESSION",
    10: "EP_STATE",
    11: "SWEEP",
    12: "FRAME",
    13: "ECHO",
}
VITIS_ROOTS = (
    Path(r"C:\AMDDesignTools\2026.1\Vitis"),
    Path(r"C:\Xilinx\Vitis"),
)


def _find_tool(names: tuple[str, ...], override: str | None) -> str | None:
    if override:
        return override
    for root in VITIS_ROOTS:
        for name in names:
            candidate = root / "bin" / name
            if candidate.is_file():
                return str(candidate)
        gnu = root / "gnu" / "aarch32" / "nt" / "gcc-arm-none-eabi" / "bin"
        for name in names:
            candidate = gnu / name
            if candidate.is_file():
                return str(candidate)
    for name in names:
        from shutil import which

        found = which(name)
        if found:
            return found
    return None


def resolve_symbol(elf: Path, symbol: str, nm: str) -> int:
    output = subprocess.run(
        [nm, str(elf)], capture_output=True, text=True, check=True
    ).stdout
    for line in output.splitlines():
        fields = line.split()
        if len(fields) >= 3 and fields[-1] == symbol:
            return int(fields[0], 16)
    raise SystemExit(f"symbol not found in {elf}: {symbol}")


def dump(elf: Path, out: Path, xsdb: str, nm: str) -> None:
    address = resolve_symbol(elf, "la_usb_trace_ring", nm)
    script = PROJECT_ROOT / "fw-zynq" / "scripts" / "usb_trace_dump.tcl"
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [xsdb, str(script), hex(address), str(CAPACITY * 4), str(out)],
        check=True,
    )


_HEX = re.compile(r"0x([0-9A-Fa-f]+)")


def parse_dump(text: str) -> tuple[int, list[tuple[int, int, int, int]]]:
    head = None
    words: list[int] = []
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("HEAD "):
            head = int(_HEX.search(line).group(1), 16)
        elif line.startswith("WORDS "):
            declared = int(line.split()[1])
            tokens = [int(value, 16) for value in _HEX.findall(line)]
            # XSDB's mrd list prints "address value" pairs for each word.
            if len(tokens) == 2 * declared:
                tokens = tokens[1::2]
            words.extend(tokens)
    if head is None:
        raise SystemExit("dump has no HEAD line")
    entries = [
        (words[i], words[i + 1], words[i + 2], words[i + 3])
        for i in range(0, len(words) - 3, 4)
    ]
    return head, entries


def decode(text: str) -> list[str]:
    head, entries = parse_dump(text)
    lost = head - CAPACITY if head > CAPACITY else 0
    lines = [
        f"head={head} entries={len(entries)} capacity={CAPACITY} lost={max(0, lost)}"
    ]
    if lost > 0:
        lines.append(
            f"WARNING: {lost} entries were overwritten before this dump; "
            "increase LA_USB_TRACE_CAPACITY or dump sooner"
        )
    for index, (tag, a, b, c) in enumerate(entries):
        name = TAG_NAMES.get(tag, f"TAG{tag}")
        detail = ""
        if name == "SETUP":
            detail = (
                f"bmReqType=0x{(a >> 24) & 0xFF:02X} bRequest=0x{(a >> 16) & 0xFF:02X} "
                f"wValue=0x{a & 0xFFFF:04X} wIndex=0x{(b >> 16) & 0xFFFF:04X} "
                f"wLength={b & 0xFFFF} count={c}"
            )
        elif name == "OUT_DTD":
            detail = f"ep={a >> 8} dtd={a & 0xFF} len={b} head=0x{c:08X}"
        elif name in ("STAGE_PUSH", "STAGE_POP"):
            detail = f"ep={a} len={b} index={c}"
        elif name == "RX_SB":
            detail = f"ep={a} len={b} accepted={c}"
        elif name == "TX_SUBMIT":
            detail = f"slot={a} len={b} echo={c}"
        elif name == "TX_DONE":
            detail = f"slot={a} epcompl=0x{b:08X} tx_bytes={c}"
        elif name == "RESET":
            source = {1: "SET_CONFIGURATION", 2: "CLEAR_FEATURE", 3: "SET_INTERFACE"}.get(
                a, f"source{a}"
            )
            detail = f"source={source} epcr=0x{b:08X} eprdy=0x{c:08X}"
        elif name == "SESSION":
            detail = f"event={a} actions=0x{b:08X} state={c}"
        elif name == "EP_STATE":
            detail = f"eprdy=0x{a:08X} epcompl=0x{b:08X} epctrl1=0x{c:08X}"
        elif name == "SWEEP":
            detail = f"ep={a} serviced={b} prime={c}"
        elif name == "FRAME":
            detail = f"type=0x{a >> 8:02X} seq={a & 0xFF} len={b} first4=0x{c:08X}"
        lines.append(f"[{index:4d}] {name:10s} {detail}")
    return lines


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="U03 USB trace dump/decode")
    parser.add_argument("--elf", type=Path, help="USB-profile ELF for symbol lookup")
    parser.add_argument("--out", type=Path, help="dump file to write")
    parser.add_argument("--decode", type=Path, help="decode an existing dump file")
    parser.add_argument("--xsdb", default=None, help="XSDB executable override")
    parser.add_argument("--nm", default=None, help="arm-none-eabi-nm override")
    args = parser.parse_args(argv)

    if args.decode is not None:
        for line in decode(args.decode.read_text(encoding="utf-8")):
            print(line)
        return 0
    if args.elf is None:
        parser.error("either --elf (dump) or --decode FILE is required")
    out = args.out or (PROJECT_ROOT / "build" / "reports" / "usb-trace-dump.txt")
    xsdb = _find_tool(("xsdb.bat", "xsdb"), args.xsdb or os.environ.get("XSDB_EXE"))
    nm = _find_tool(
        ("arm-none-eabi-nm.exe", "arm-none-eabi-nm"),
        args.nm or os.environ.get("ARM_GCC_NM"),
    )
    if not xsdb or not nm:
        print("usb-trace blocked: XSDB or arm-none-eabi-nm not found", file=sys.stderr)
        return 2
    dump(args.elf, out, xsdb, nm)
    for line in decode(out.read_text(encoding="utf-8")):
        print(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
