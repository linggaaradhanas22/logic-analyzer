"""Transport abstraction for la-cli (tcp://host:port and serial://port).

A transport delivers raw bytes both ways; protocol framing lives in
protocol.py. Serial ports COM10 and above need the Windows device-prefixed
name, which is applied automatically here.
"""

from __future__ import annotations

import re
import socket
import struct
import time
from abc import ABC, abstractmethod

TCP_URI = re.compile(r"^tcp://([^:/]+):(\d+)$")
SERIAL_URI = re.compile(r"^serial://(.+)$")


class TransportError(RuntimeError):
    """Raised when a transport cannot be opened or fails mid-command."""


class Transport(ABC):
    """Byte stream with time-bounded reads.

    recv() returns up to max_bytes, or b'' when the timeout expired with no
    data. EOF is reported through the eof property: for stream sockets EOF
    is permanent; serial ports never EOF.
    """

    @abstractmethod
    def send(self, data: bytes) -> None: ...

    @abstractmethod
    def recv(self, max_bytes: int, timeout_s: float) -> bytes:
        """Returns up to max_bytes, or b'' on timeout."""

    @abstractmethod
    def close(self) -> None: ...

    def kill(self) -> None:
        """P8.4 fault injection: abort the transport so the peer sees an
        abrupt disconnect (TCP RST for TcpTransport). Defaults to close()."""
        self.close()

    @property
    @abstractmethod
    def eof(self) -> bool: ...

    @property
    @abstractmethod
    def description(self) -> str: ...


class TcpTransport(Transport):
    def __init__(self, host: str, port: int, connect_timeout_s: float = 5.0) -> None:
        self._sock = socket.create_connection((host, port), timeout=connect_timeout_s)
        self._sock.settimeout(connect_timeout_s)
        self._description = f"tcp://{host}:{port}"
        self._eof = False

    def send(self, data: bytes) -> None:
        try:
            self._sock.sendall(data)
        except OSError as exc:
            raise TransportError(f"send failed on {self._description}: {exc}") from exc

    def recv(self, max_bytes: int, timeout_s: float) -> bytes:
        self._sock.settimeout(timeout_s)
        try:
            data = self._sock.recv(max_bytes)
        except socket.timeout:
            return b""
        except OSError as exc:
            raise TransportError(f"recv failed on {self._description}: {exc}") from exc
        if not data:
            self._eof = True
        return data

    def close(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass

    def kill(self) -> None:
        """Aborts with a TCP RST (SO_LINGER 0) so the peer sees an abrupt
        disconnect instead of a clean FIN."""
        try:
            self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                                  struct.pack("ii", 1, 0))
        except OSError:
            pass
        self.close()

    @property
    def eof(self) -> bool:
        return self._eof

    @property
    def description(self) -> str:
        return self._description


class SerialTransport(Transport):
    def __init__(self, port: str, baud: int = 921600, timeout_s: float = 1.0) -> None:
        import serial

        try:
            self._serial = serial.Serial(port=port, baudrate=baud, timeout=timeout_s)
        except serial.SerialException as exc:
            raise TransportError(f"cannot open serial port {port!r}: {exc}") from exc
        self._description = f"serial://{port}"

    def send(self, data: bytes) -> None:
        try:
            self._serial.write(data)
        except serial.SerialException as exc:
            raise TransportError(f"send failed on {self._description}: {exc}") from exc

    def recv(self, max_bytes: int, timeout_s: float) -> bytes:
        try:
            self._serial.timeout = timeout_s
            return self._serial.read(max_bytes)
        except serial.SerialException as exc:
            raise TransportError(f"recv failed on {self._description}: {exc}") from exc

    def close(self) -> None:
        try:
            self._serial.close()
        except Exception:
            pass

    @property
    def eof(self) -> bool:
        return False

    @property
    def description(self) -> str:
        return self._description


def normalise_serial_name(name: str) -> str:
    """Windows COM10+ needs the device path; plain COM names stay as given."""
    match = re.fullmatch(r"COM(\d+)", name, re.IGNORECASE)
    if match and int(match.group(1)) >= 10:
        return f"\\\\.\\{name.upper()}"
    return name


def open_uri(uri: str) -> Transport:
    """Opens tcp://host:port or serial://COMx (also accepts a bare COM name)."""
    uri = uri.strip()
    match = TCP_URI.match(uri)
    if match:
        return TcpTransport(match.group(1), int(match.group(2)))
    match = SERIAL_URI.match(uri)
    if match:
        return SerialTransport(normalise_serial_name(match.group(1)))
    if re.fullmatch(r"COM\d+", uri, re.IGNORECASE):
        return SerialTransport(normalise_serial_name(uri))
    raise TransportError(
        f"unknown target {uri!r}; use tcp://host:port, serial://COMx or a bare COM name"
    )


class PortCandidate:
    def __init__(self, index: int, device: str, description: str, vid_pid: str, serial: str) -> None:
        self.index = index
        self.device = device
        self.description = description
        self.vid_pid = vid_pid
        self.serial_number = serial

    def __str__(self) -> str:
        return (f"[{self.index}] {self.device} - {self.description} "
                f"(VID:PID {self.vid_pid or '-'}, serial {self.serial_number or '-'})")


# VID:PID families accepted for the analyzer link adapter.
KNOWN_VID_PID = {
    0x10C4: "CP210x",   # Silicon Labs USB-UART
    0x1A86: "CH340",    # QinHeng Electronics
    0x0403: "FTDI",     # FT232 etc.
}


def list_candidates() -> list[PortCandidate]:
    """Lists serial ports whose VID matches a known USB-UART family."""
    import serial.tools.list_ports

    candidates = []
    for index, info in enumerate(serial.tools.list_ports.comports()):
        if info.vid is None or info.vid not in KNOWN_VID_PID:
            continue
        vid_pid = f"{info.vid:04X}:{info.pid:04X}" if info.pid is not None else f"{info.vid:04X}"
        candidates.append(
            PortCandidate(index, info.device, info.description, vid_pid, info.serial_number)
        )
    return candidates


def select_serial_target(forced: str | None) -> str:
    """Resolves the serial target: an explicit port wins; otherwise a single
    unambiguous USB-UART candidate is chosen and ambiguity is an error."""
    if forced:
        return normalise_serial_name(forced)
    candidates = list_candidates()
    if len(candidates) == 1:
        return normalise_serial_name(candidates[0].device)
    if not candidates:
        raise TransportError("no USB-UART (CP210x/CH340/FTDI) ports found; pass serial://COMx")
    listing = "\n".join(str(candidate) for candidate in candidates)
    raise TransportError(
        f"multiple USB-UART ports found - select one explicitly with serial://COMx:\n{listing}"
    )
