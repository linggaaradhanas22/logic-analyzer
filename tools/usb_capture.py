"""U03 USB wire/URB capture helper around USBPcapCMD and tshark.

USBPcap installs one control device per root hub and requires an elevated
shell; after installing USBPcap the machine must be rebooted before the filter
attaches to existing root hubs (USBPcap README). See `docs/usb-link.md`
section 14 for the setup record and the pre-registered observations.

    .venv\\Scripts\\python.exe tools\\usb_capture.py list
    .venv\\Scripts\\python.exe tools\\usb_capture.py capture --seconds 10
    .venv\\Scripts\\python.exe tools\\usb_capture.py decode build\\captures\\usb-*.pcap

`capture` writes a pcap that opens in Wireshark; `decode` prints a compact
tshark field table for the product device (VID 1209, PID 0001). Npcap is not
required for USBPcap captures; Wireshark's Npcap warning does not affect
reading or USB capture.
"""

from __future__ import annotations

import argparse
import ctypes
import re
import signal
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_USBPCAP = Path(r"C:\Program Files\USBPcap\USBPcapCMD.exe")
DEFAULT_TSHARK = Path(r"C:\Program Files\Wireshark\tshark.exe")
TARGET_VID = 0x1209
TARGET_PID = 0x0001

_IFACE = re.compile(r"interface\s*\{value=([^}]+)\}\s*\{display=([^}]+)\}")
_TSHARK_FIELDS = [
    "frame.number",
    "frame.time_relative",
    "usb.src",
    "usb.dst",
    "usb.transfer_type",
    "usb.endpoint_address",
    "usb.data_len",
    "usb.bmRequestType",
    "usb.setup.bRequest",
    "usb.setup.wValue",
    "usb.setup.wIndex",
    "usb.setup.wLength",
    "usb.capdata",
    "usb.data_fragment",
]


def is_admin() -> bool:
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def find_tool(default: Path, name: str, override: str | None = None) -> str | None:
    if override:
        return override
    if default.is_file():
        return str(default)
    from shutil import which

    return which(name)


def list_interfaces(usbpcap: str) -> int:
    result = subprocess.run([usbpcap, "--extcap-interfaces"],
                            capture_output=True, text=True, timeout=60)
    interfaces = _IFACE.findall(result.stdout)
    if not interfaces:
        print("no USBPcap interfaces found", file=sys.stderr)
        if not is_admin():
            print("run this command from an elevated shell", file=sys.stderr)
        print(
            "if USBPcap was just installed, reboot Windows so the filter "
            "attaches to the existing root hubs",
            file=sys.stderr,
        )
        return 1
    for value, display in interfaces:
        print(f"{display}  {value}")
    return 0


def capture(usbpcap: str, tshark: str | None, seconds: float, hub: int,
            out: Path, addresses: str | None, no_inject: bool) -> int:
    if not is_admin():
        print("capture requires an elevated shell (USBPcap driver)", file=sys.stderr)
        return 2
    out.parent.mkdir(parents=True, exist_ok=True)
    command = [usbpcap, "-d", rf"\\.\USBPcap{hub}", "-o", str(out)]
    if addresses:
        command += ["--devices", addresses]
    else:
        command += ["-A"]
    if not no_inject:
        command += ["--inject-descriptors"]
    print(f"capture: {' '.join(command)}")
    process = subprocess.Popen(command, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True,
                               creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
    time.sleep(seconds)
    process.send_signal(signal.CTRL_BREAK_EVENT)
    try:
        process.wait(timeout=15)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait(timeout=10)
    stdout, stderr = process.communicate()
    if stdout.strip():
        print(stdout.strip())
    if stderr.strip():
        print(stderr.strip(), file=sys.stderr)
    if not out.is_file():
        print(f"capture produced no file: {out}", file=sys.stderr)
        return 1
    size = out.stat().st_size
    print(f"wrote {out} ({size} bytes)")
    if size == 0:
        print("capture is empty; check the root hub number with `list`", file=sys.stderr)
        return 1
    if tshark:
        count = subprocess.run(
            [tshark, "-r", str(out), "-T", "fields", "-e", "frame.number"],
            capture_output=True, text=True, timeout=120)
        packets = len([line for line in count.stdout.splitlines() if line.strip()])
        print(f"tshark read {packets} packets")
    return 0


def decode(tshark: str, pcap: Path, vid: int, pid: int, limit: int,
           display_filter: str | None) -> int:
    if not pcap.is_file():
        print(f"capture not found: {pcap}", file=sys.stderr)
        return 2
    if display_filter is None:
        display_filter = f"usb.idVendor == 0x{vid:04x} && usb.idProduct == 0x{pid:04x}"
    command = [tshark, "-r", str(pcap), "-Y", display_filter,
               "-T", "fields", "-E", "separator=|", "-E", "occurrence=a"]
    for field in _TSHARK_FIELDS:
        command += ["-e", field]
    result = subprocess.run(command, capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        print(result.stderr.strip(), file=sys.stderr)
        return result.returncode
    rows = [line for line in result.stdout.splitlines() if line.strip()]
    print("|".join(_TSHARK_FIELDS))
    for row in rows[:limit]:
        print(row)
    if len(rows) > limit:
        print(f"... {len(rows) - limit} more rows (--limit)")
    print(f"rows: {len(rows)}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="U03 USBPcap capture helper")
    parser.add_argument("--usbpcap", default=None, help="USBPcapCMD.exe override")
    parser.add_argument("--tshark", default=None, help="tshark.exe override")
    sub = parser.add_subparsers(dest="mode", required=True)

    sub.add_parser("list", help="list USBPcap root-hub interfaces")

    capture_parser = sub.add_parser("capture", help="record a USBPcap capture")
    capture_parser.add_argument("--seconds", type=float, default=10.0)
    capture_parser.add_argument("--hub", type=int, default=1)
    capture_parser.add_argument("--out", type=Path, default=None)
    capture_parser.add_argument("--addresses", default=None,
                                help="comma-separated USB device addresses")
    capture_parser.add_argument("--no-inject", action="store_true",
                                help="do not inject descriptors of connected devices")

    decode_parser = sub.add_parser("decode", help="print a tshark field table")
    decode_parser.add_argument("pcap", type=Path)
    decode_parser.add_argument("--vid", type=lambda v: int(v, 0), default=TARGET_VID)
    decode_parser.add_argument("--pid", type=lambda v: int(v, 0), default=TARGET_PID)
    decode_parser.add_argument("--limit", type=int, default=200)
    decode_parser.add_argument("--filter", default=None, help="tshark display filter override")

    args = parser.parse_args(argv)
    usbpcap = find_tool(DEFAULT_USBPCAP, "USBPcapCMD.exe", args.usbpcap)
    tshark = find_tool(DEFAULT_TSHARK, "tshark.exe", args.tshark)

    if args.mode == "list":
        if not usbpcap:
            print("USBPcapCMD.exe not found", file=sys.stderr)
            return 2
        return list_interfaces(usbpcap)
    if args.mode == "capture":
        if not usbpcap:
            print("USBPcapCMD.exe not found", file=sys.stderr)
            return 2
        out = args.out
        if out is None:
            stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
            out = PROJECT_ROOT / "build" / "captures" / f"usb-{stamp}.pcap"
        return capture(usbpcap, tshark, args.seconds, args.hub, out,
                       args.addresses, args.no_inject)
    if args.mode == "decode":
        if not tshark:
            print("tshark.exe not found", file=sys.stderr)
            return 2
        return decode(tshark, args.pcap, args.vid, args.pid, args.limit,
                      args.filter)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
