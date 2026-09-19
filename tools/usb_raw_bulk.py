"""U03 raw USB bulk bench probe (libusb/WinUSB, bypasses usbser.sys).

This is the host-side control from `docs/usb-link.md` section 14. It talks to
the USB-profile image over EP1 with libusb after WinUSB is bound with Zadig,
so the CDC class driver cannot be part of the bulk-OUT loss. Modes:

    .venv\\Scripts\\python.exe tools\\usb_raw_bulk.py list
    .venv\\Scripts\\python.exe tools\\usb_raw_bulk.py info
    .venv\\Scripts\\python.exe tools\\usb_raw_bulk.py raw --control --count 12
    .venv\\Scripts\\python.exe tools\\usb_raw_bulk.py records --count 12 --gap-ms 50

`records` sends the same numbered `TEST_RECORD` (0x1B) frames as the serial
detector and accounts the ACKs with the shared host accounting, so the raw
result is directly comparable to `tests/e2e/test_usb_bulk.py`.
"""

from __future__ import annotations

import argparse
import ctypes
import json
import struct
import sys
import time

import usb.core
import usb.util

from host.la_cli import protocol as p
from host.la_cli.client import account_test_records, nak_text

TARGET_VID = 0x1209
TARGET_PID = 0x0001
EXAMPLE_VID = 0x0D7D
EXAMPLE_PID = 0x0100

SET_LINE_CODING = (0x21, 0x20)
SET_CONTROL_LINE_STATE = (0x21, 0x22)
SET_INTERFACE = (0x01, 0x0B)
CLEAR_FEATURE = (0x02, 0x01)
ENDPOINT_HALT = 0x00

ZADIG_HINT = (
    "Windows needs WinUSB bound to the device (Zadig -> 1209:0001 -> WinUSB). "
    "See docs/usb-link.md section 14; restore usbser.sys afterwards."
)


def is_admin() -> bool:
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def get_backend():
    try:
        import libusb_package

        return libusb_package.get_libusb1_backend()
    except Exception:
        return usb.backend.libusb1.get_backend()


def find_device(vid: int, pid: int):
    return usb.core.find(idVendor=vid, idProduct=pid, backend=get_backend())


def describe_device(dev) -> dict:
    info = {
        "vid": f"0x{dev.idVendor:04x}",
        "pid": f"0x{dev.idProduct:04x}",
        "bus": dev.bus,
        "address": dev.address,
        "speed": getattr(dev, "speed", None),
    }
    for key in ("manufacturer", "product", "serial_number"):
        try:
            info[key] = getattr(dev, key)
        except Exception:
            info[key] = None
    return info


def list_devices() -> int:
    backend = get_backend()
    devices = list(usb.core.find(find_all=True, backend=backend))
    print(f"libusb backend: {backend}")
    print(f"devices: {len(devices)}")
    for dev in devices:
        tag = ""
        if (dev.idVendor, dev.idProduct) == (TARGET_VID, TARGET_PID):
            tag = "  <- product USB image"
        elif (dev.idVendor, dev.idProduct) == (EXAMPLE_VID, EXAMPLE_PID):
            tag = "  <- vendor example (not the product image)"
        print(
            f"  {dev.idVendor:04x}:{dev.idProduct:04x} "
            f"bus={dev.bus} addr={dev.address}{tag}"
        )
    return 0


def open_target(vid: int, pid: int):
    dev = find_device(vid, pid)
    if dev is None:
        print(f"device {vid:04x}:{pid:04x} not found", file=sys.stderr)
        print(
            "load the USB profile (`python tools/run.py jtag-zynq -- --profile usb`) "
            "and bind WinUSB with Zadig",
            file=sys.stderr,
        )
        return None
    try:
        dev.set_configuration()
    except Exception:
        pass
    return dev


def show_info(dev) -> int:
    info = describe_device(dev)
    for key, value in info.items():
        print(f"{key}: {value}")
    try:
        cfg = dev.get_active_configuration()
        for intf in cfg:
            print(
                f"interface {intf.bInterfaceNumber} alt {intf.bAlternateSetting} "
                f"class 0x{intf.bInterfaceClass:02x} subclass 0x{intf.bInterfaceSubClass:02x} "
                f"protocol 0x{intf.bInterfaceProtocol:02x}"
            )
            for ep in intf:
                direction = "IN" if usb.util.endpoint_direction(ep.bEndpointAddress) else "OUT"
                print(
                    f"  ep 0x{ep.bEndpointAddress:02x} {direction} "
                    f"{usb.util.endpoint_type(ep.bmAttributes)} "
                    f"max_packet={ep.wMaxPacketSize} interval={ep.bInterval}"
                )
    except (usb.core.USBError, NotImplementedError) as exc:
        print(f"descriptor read failed: {exc}", file=sys.stderr)
        print(ZADIG_HINT, file=sys.stderr)
        return 1
    return 0


def run_control_sequence(dev, interface: int, ep_out: int, ep_in: int) -> None:
    line_coding = struct.pack("<IBBB", 115200, 0, 0, 8)
    requests = [
        ("SET_LINE_CODING", SET_LINE_CODING[0], SET_LINE_CODING[1], 0, 0, line_coding),
        ("SET_CONTROL_LINE_STATE", SET_CONTROL_LINE_STATE[0], SET_CONTROL_LINE_STATE[1], 0x03, 0, None),
        ("SET_INTERFACE", SET_INTERFACE[0], SET_INTERFACE[1], 0, interface, None),
        ("CLEAR_FEATURE(EP_HALT OUT)", CLEAR_FEATURE[0], CLEAR_FEATURE[1], ENDPOINT_HALT, ep_out, None),
        ("CLEAR_FEATURE(EP_HALT IN)", CLEAR_FEATURE[0], CLEAR_FEATURE[1], ENDPOINT_HALT, ep_in, None),
    ]
    for name, request_type, request, value, index, data in requests:
        try:
            dev.ctrl_transfer(request_type, request, value, index, data, timeout=1000)
            print(f"control {name}: ok")
        except usb.core.USBError as exc:
            print(f"control {name}: FAILED ({exc})", file=sys.stderr)


def bulk_burst(dev, ep_out: int, ep_in: int, count: int, size: int,
               gap_ms: float, timeout_ms: int, read_back: bool) -> dict:
    ok = 0
    timeouts = 0
    errors: list[str] = []
    responses = 0
    first_response: bytes | None = None
    payloads = []
    for index in range(count):
        body = struct.pack("<I", index) + bytes((index + j) & 0xFF for j in range(max(0, size - 4)))
        payloads.append(body)
    started = time.monotonic()
    for index, body in enumerate(payloads):
        try:
            written = dev.write(ep_out, body, timeout=timeout_ms)
            if written == len(body):
                ok += 1
            else:
                errors.append(f"write {index}: short ({written}/{len(body)})")
        except usb.core.USBError as exc:
            if "timeout" in str(exc).lower() or "timed out" in str(exc).lower():
                timeouts += 1
            else:
                errors.append(f"write {index}: {exc}")
        if read_back:
            try:
                data = bytes(dev.read(ep_in, 4096, timeout=timeout_ms))
                responses += 1
                if first_response is None:
                    first_response = data
            except usb.core.USBError:
                pass
        if gap_ms > 0:
            time.sleep(gap_ms / 1000.0)
    elapsed = time.monotonic() - started
    return {
        "sent": count,
        "ok": ok,
        "timeouts": timeouts,
        "errors": errors,
        "responses": responses,
        "first_response": first_response.hex() if first_response else None,
        "elapsed_s": round(elapsed, 3),
    }


def records_burst(dev, ep_out: int, ep_in: int, count: int, gap_ms: float,
                  timeout_ms: int, settle_ms: int) -> dict:
    parser = p.FrameParser()
    sent: list[tuple[int, int]] = []
    acked: list = []
    write_errors: list[tuple[int, str]] = []
    sequence = 0

    def on_frame(message_type, frame_sequence, payload):
        if message_type == p.ACK and len(payload) >= 2 and payload[0] == p.TEST_RECORD:
            acked.append(payload[1])
        elif message_type == p.NAK and len(payload) >= 3 and payload[0] == p.TEST_RECORD:
            acked.append(("nak", payload[1], payload[2]))
        return False

    def drain(timeout_ms: int) -> None:
        while True:
            try:
                data = bytes(dev.read(ep_in, 4096, timeout=timeout_ms))
            except usb.core.USBError:
                return
            if not data:
                return
            parser.feed(data, on_frame)

    for record_id in range(count):
        frame = p.encode(p.TEST_RECORD, sequence, p.pack_test_record(record_id))
        try:
            written = dev.write(ep_out, frame, timeout=timeout_ms)
            if written != len(frame):
                write_errors.append((record_id, f"short write {written}/{len(frame)}"))
        except usb.core.USBError as exc:
            write_errors.append((record_id, str(exc)))
        sent.append((record_id, sequence))
        sequence = (sequence + 1) & 0xFF
        if gap_ms > 0:
            time.sleep(gap_ms / 1000.0)
        drain(1)

    deadline = time.monotonic() + settle_ms / 1000.0
    while time.monotonic() < deadline:
        try:
            data = bytes(dev.read(ep_in, 4096, timeout=200))
        except usb.core.USBError as exc:
            if "timeout" in str(exc).lower() or "timed out" in str(exc).lower():
                continue
            break
        if not data:
            continue
        parser.feed(data, on_frame)
    result = {
        "sent": sent,
        "acked": acked,
        "write_errors": write_errors,
        "parser": {
            "crc_errors": parser.crc_errors,
            "format_errors": parser.format_errors,
        },
    }
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="U03 raw USB bulk bench probe")
    parser.add_argument("mode", choices=("list", "info", "raw", "records"))
    parser.add_argument("--vid", type=lambda v: int(v, 0), default=TARGET_VID)
    parser.add_argument("--pid", type=lambda v: int(v, 0), default=TARGET_PID)
    parser.add_argument("--interface", type=int, default=1)
    parser.add_argument("--ep-out", type=lambda v: int(v, 0), default=0x01)
    parser.add_argument("--ep-in", type=lambda v: int(v, 0), default=0x81)
    parser.add_argument("--count", type=int, default=12)
    parser.add_argument("--size", type=int, default=32)
    parser.add_argument("--gap-ms", type=float, default=50.0)
    parser.add_argument("--timeout-ms", type=int, default=1000)
    parser.add_argument("--settle-ms", type=int, default=2000)
    parser.add_argument("--control", action="store_true", help="run the CDC open sequence first")
    parser.add_argument("--read", action="store_true", help="read EP IN after each raw write")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    if args.mode == "list":
        return list_devices()

    dev = open_target(args.vid, args.pid)
    if dev is None:
        return 2
    if args.mode == "info":
        try:
            return show_info(dev)
        finally:
            usb.util.dispose_resources(dev)

    try:
        usb.util.claim_interface(dev, args.interface)
    except (usb.core.USBError, NotImplementedError) as exc:
        print(f"claim interface {args.interface} failed: {exc}", file=sys.stderr)
        print(ZADIG_HINT, file=sys.stderr)
        usb.util.dispose_resources(dev)
        return 2

    try:
        if args.control:
            run_control_sequence(dev, args.interface, args.ep_out, args.ep_in)
        if args.mode == "raw":
            result = bulk_burst(dev, args.ep_out, args.ep_in, args.count,
                                args.size, args.gap_ms, args.timeout_ms, args.read)
            print(json.dumps(result, indent=2) if args.json else
                  f"sent={result['sent']} ok={result['ok']} "
                  f"timeouts={result['timeouts']} responses={result['responses']} "
                  f"errors={len(result['errors'])} elapsed={result['elapsed_s']}s")
            for error in result["errors"]:
                print(f"  {error}", file=sys.stderr)
            return 0 if not result["errors"] else 1

        result = records_burst(dev, args.ep_out, args.ep_in, args.count,
                               args.gap_ms, args.timeout_ms, args.settle_ms)
        summary = account_test_records(result)
        if args.json:
            summary = dict(summary)
            summary["acked_order"] = result["acked"]
            print(json.dumps(summary, indent=2))
        else:
            print(
                f"sent={summary['sent']} acked={summary['acked']} "
                f"missing={summary['missing']} duplicates={summary['duplicates']} "
                f"reordered={summary['reordered']} clean={summary['clean']}"
            )
            for nak in summary["naks"]:
                print(f"  nak seq={nak[1]} error={nak[2]} ({nak_text(nak[2])})")
            for record_id, message in summary["write_errors"]:
                print(f"  write error record={record_id}: {message}", file=sys.stderr)
        return 0 if summary["clean"] else 1
    finally:
        usb.util.dispose_resources(dev)


if __name__ == "__main__":
    raise SystemExit(main())
