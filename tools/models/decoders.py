"""Independent transition stimuli and expected events for the P1 decoders.

The models intentionally do not call the C library.  They provide small,
deterministic vectors for native tests and use the same timestamp conventions
documented in ``docs/decoders.md``.
"""

from __future__ import annotations

from dataclasses import dataclass


BYTE = 1
FRAME_ERROR = 2
PARITY_ERROR = 3
START = 4
STOP = 5
DISCONTINUITY = 6
ADDRESS = 7
ACK = 8
NACK = 9
CS_ASSERT = 10
CS_RELEASE = 11

NO_CHANNEL = 0xFF
MOSI = 0x01
MISO = 0x02
READ = 0x04


@dataclass(frozen=True)
class Transition:
    t_sample: int
    sample: int


@dataclass(frozen=True)
class Event:
    kind: int
    t_sample: int
    channel: int = NO_CHANNEL
    flags: int = 0
    data: bytes = b""


def _raw_level(logical: int, inverted: bool) -> int:
    return (logical ^ int(inverted)) & 1


def uart_frame(
    value: int,
    *,
    samples_per_bit: int = 8,
    data_bits: int = 8,
    parity: str = "N",
    stop_bits: int = 1,
    inverted: bool = False,
    channel: int = 0,
    start: int = 100,
    parity_error: bool = False,
    framing_error: bool = False,
) -> tuple[list[Transition], list[Event]]:
    """Return a UART frame and its expected events.

    Data is least-significant bit first.  A transition is emitted only when
    the line changes; the decoder's ``advance`` operation supplies unchanged
    samples between transitions.
    """

    if not 5 <= data_bits <= 9 or samples_per_bit <= 0 or stop_bits not in (1, 2):
        raise ValueError("invalid UART model parameters")
    parity = parity.upper()
    if parity not in {"N", "E", "O"}:
        raise ValueError("parity must be N, E or O")
    value &= (1 << data_bits) - 1
    bits = [0]
    bits.extend((value >> bit) & 1 for bit in range(data_bits))
    ones = sum(bits[1:]) & 1
    if parity == "E":
        bits.append(ones if not parity_error else ones ^ 1)
    elif parity == "O":
        bits.append((ones ^ 1) if not parity_error else ones)
    bits.extend([0 if framing_error else 1] * stop_bits)

    idle = _raw_level(1, inverted)
    current = idle
    transitions = [Transition(start, _raw_level(bits[0], inverted))]
    current = transitions[0].sample
    for index, logical in enumerate(bits[1:], start=1):
        sample = _raw_level(logical, inverted)
        if sample != current:
            transitions.append(Transition(start + index * samples_per_bit, sample))
            current = sample
    end = start + (len(bits) + 1) * samples_per_bit
    if current != idle:
        transitions.append(Transition(end, idle))
    else:
        transitions.append(Transition(end, idle))

    expected: list[Event] = []
    if framing_error:
        expected.append(Event(FRAME_ERROR, start, channel=channel))
    elif parity_error:
        expected.append(Event(PARITY_ERROR, start, channel=channel))
        expected.append(Event(BYTE, start, channel=channel, data=(value & 0xFF).to_bytes(1, "little") if data_bits <= 8 else value.to_bytes(2, "little")))
    else:
        expected.append(Event(BYTE, start, channel=channel, data=(value & 0xFF).to_bytes(1, "little") if data_bits <= 8 else value.to_bytes(2, "little")))
    return transitions, expected


def _set_line(transitions: list[Transition], t: int, sample: int, current: int) -> int:
    if sample != current:
        transitions.append(Transition(t, sample))
        return sample
    return current


def spi_transfer(
    words: list[int],
    *,
    cpol: int = 0,
    cpha: int = 0,
    word_size: int = 8,
    lsb_first: bool = False,
    start: int = 10,
) -> tuple[list[Transition], list[Event]]:
    """Generate one MOSI SPI transaction on channels SCK=0, MOSI=1, CS=2."""

    if cpol not in (0, 1) or cpha not in (0, 1) or word_size not in (8, 16):
        raise ValueError("invalid SPI model parameters")
    mask = (1 << word_size) - 1
    current = (cpol << 0) | (0 << 1) | (1 << 2)
    transitions = [Transition(0, current)]
    expected = [Event(CS_ASSERT, start, channel=2)]
    t = start
    current = _set_line(transitions, t, current & ~(1 << 2), current)
    for word in words:
        word &= mask
        bits = (
            [(word >> bit) & 1 for bit in range(word_size)]
            if lsb_first
            else [(word >> bit) & 1 for bit in range(word_size - 1, -1, -1)]
        )
        for bit_index, bit in enumerate(bits):
            if cpha == 0:
                t += 1
                current = _set_line(transitions, t, (current & ~(1 << 1)) | (bit << 1), current)
                t += 1
                current = _set_line(transitions, t, current ^ (1 << 0), current)
                sample_time = t
                t += 1
                current = _set_line(transitions, t, current ^ (1 << 0), current)
            else:
                t += 1
                current = _set_line(transitions, t, current ^ (1 << 0), current)
                t += 1
                current = _set_line(transitions, t, (current & ~(1 << 1)) | (bit << 1), current)
                t += 1
                current = _set_line(transitions, t, current ^ (1 << 0), current)
                sample_time = t
            if bit_index == word_size - 1:
                data = word.to_bytes(1 if word_size == 8 else 2, "little")
                expected.append(Event(BYTE, sample_time, channel=1, flags=MOSI, data=data))
    t += 1
    current = _set_line(transitions, t, current | (1 << 2), current)
    expected.append(Event(CS_RELEASE, t, channel=2))
    return transitions, expected


def i2c_transaction(
    address: int,
    data: list[int],
    *,
    read: bool = False,
    ack: list[bool] | None = None,
    start: int = 10,
) -> tuple[list[Transition], list[Event]]:
    """Generate an I2C write/read-address transaction on SCL=0 and SDA=1."""

    if not 0 <= address < 0x80:
        raise ValueError("I2C address must be seven bits")
    if ack is None:
        ack = [True] * (len(data) + 1)
    if len(ack) != len(data) + 1:
        raise ValueError("ack list must include the address ACK")
    current = 0x03
    transitions = [Transition(0, current)]
    expected = [Event(START, start, channel=1)]
    t = start
    current = _set_line(transitions, t, current & ~(1 << 1), current)

    values = [((address << 1) | int(read)) & 0xFF, *[value & 0xFF for value in data]]
    for value_index, value in enumerate(values):
        for bit_index in range(8):
            t += 1
            current = _set_line(transitions, t, current & ~(1 << 0), current)
            bit = (value >> (7 - bit_index)) & 1
            t += 1
            current = _set_line(transitions, t, (current & ~(1 << 1)) | (bit << 1), current)
            t += 1
            current = _set_line(transitions, t, current | (1 << 0), current)
            if bit_index == 7:
                if value_index == 0:
                    expected.append(
                        Event(
                            ADDRESS,
                            t,
                            channel=1,
                            flags=READ if read else 0,
                            data=bytes([address]),
                        )
                    )
                else:
                    expected.append(Event(BYTE, t, channel=1, data=bytes([value])))
            t += 1
            current = _set_line(transitions, t, current & ~(1 << 0), current)
        t += 1
        ack_level = 0 if ack[value_index] else 1
        current = _set_line(transitions, t, (current & ~(1 << 1)) | (ack_level << 1), current)
        t += 1
        current = _set_line(transitions, t, current | (1 << 0), current)
        expected.append(Event(ACK if ack[value_index] else NACK, t, channel=1))
        t += 1
        current = _set_line(transitions, t, current & ~(1 << 0), current)

    if (current & (1 << 1)) != 0:
        t += 1
        current = _set_line(transitions, t, current & ~(1 << 1), current)
    t += 1
    current = _set_line(transitions, t, current | (1 << 0), current)
    t += 1
    current = _set_line(transitions, t, current | (1 << 1), current)
    expected.append(Event(STOP, t, channel=1))
    return transitions, expected
