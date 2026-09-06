"""la-cli commands: ports, ping, status and burst capture with export."""

from __future__ import annotations

import re
import sys

import click

from . import protocol as p
from .client import AnalyzerClient, AnalyzerError
from .convert import export_capture
from .transport import TransportError, list_candidates, open_uri

FS_HZ = 100_000_000  # la_top FCLK
CHANNELS = 8


def _parse_size(text: str) -> int:
    """Parses sizes like 4096, 64k or 1M (SI decimal suffixes)."""
    match = re.fullmatch(r"(\d+)([kKmMgG]?)", text.strip())
    if not match:
        raise click.BadParameter(f"invalid size {text!r}; use e.g. 4096, 64k or 1M")
    value = int(match.group(1))
    suffix = match.group(2).lower()
    if suffix == "k":
        value *= 1000
    elif suffix == "m":
        value *= 1_000_000
    elif suffix == "g":
        value *= 1_000_000_000
    return value


def _sample_div(rate_hz: int) -> int:
    if rate_hz <= 0 or rate_hz > FS_HZ:
        raise click.BadParameter(f"rate must be within 1 Hz .. {FS_HZ} Hz")
    return max(0, round(FS_HZ / rate_hz) - 1)


@click.group(help="RTOS logic analyzer host tool (P6).")
def cli() -> None:
    pass


@cli.command(help="List USB-UART (CP210x/CH340/FTDI) serial candidates.")
def ports() -> None:
    candidates = list_candidates()
    if not candidates:
        click.echo("no USB-UART (CP210x/CH340/FTDI) ports found")
        return
    for candidate in candidates:
        click.echo(str(candidate))
    if len(candidates) > 1:
        click.echo("multiple ports found - select one explicitly: serial://COMx")


def _make_client(target: str) -> AnalyzerClient:
    try:
        transport = open_uri(target)
    except TransportError as exc:
        raise click.ClickException(str(exc)) from exc
    return AnalyzerClient(transport)


@cli.command(help="Ping the analyzer: target tcp://host:port or serial://COMx.")
@click.option("--target", "-t", required=True, help="analyzer link target URI")
@click.option("--timeout", "timeout_s", default=5.0, show_default=True, type=float)
def ping(target: str, timeout_s: float) -> None:
    client = _make_client(target)
    try:
        result = client.ping(timeout_s=timeout_s)
    except (AnalyzerError, TransportError) as exc:
        raise click.ClickException(str(exc)) from exc
    finally:
        client.close()
    click.echo(
        f"pong: fw 0x{result['fw_version']:08X} pl 0x{result['pl_id']:08X} "
        f"uptime {result['uptime_ms']} ms"
    )


@cli.command(help="Fetch the analyzer STATUS snapshot.")
@click.option("--target", "-t", required=True, help="analyzer link target URI")
@click.option("--timeout", "timeout_s", default=5.0, show_default=True, type=float)
def status(target: str, timeout_s: float) -> None:
    client = _make_client(target)
    try:
        result = client.status(timeout_s=timeout_s)
        logs = client.log_lines
    except (AnalyzerError, TransportError) as exc:
        raise click.ClickException(str(exc)) from exc
    finally:
        client.close()
    state_names = {0: "idle", 1: "armed", 2: "triggered", 3: "done", 6: "streaming"}
    click.echo(
        f"state {state_names.get(result['state'], result['state'])} "
        f"status_reg 0x{result['status_reg']:08X} "
        f"word_count {result['word_count']} trig_index {result['trig_index']}"
    )
    click.echo(
        f"cpu {result['cpu_load_permille']} permille "
        f"min_free_stack {result['min_free_stack']} words"
    )
    names = ("uart", "spi", "i2c")
    for name, stats in zip(names, result["decoders"]):
        click.echo(
            f"decoder {name}: events {stats['events']} "
            f"dropped_segments {stats['dropped_segments']}"
        )
    for line in logs:
        click.echo(f"LOG: {line}")


@cli.command(help="Arm a burst capture and export it (.sr, .vcd or .png).")
@click.option("--target", "-t", required=True, help="analyzer link target URI")
@click.option("--rate", default="10M", show_default=True,
              help="requested sample rate, e.g. 1M, 10M or 100M (must fit 100 MHz / (div+1))")
@click.option("--words", "words_text", default="64k", show_default=True,
              help="requested post-trigger samples, e.g. 4096, 64k or 1M")
@click.option("--mode", type=click.Choice(["raw", "txn"]), default="raw", show_default=True)
@click.option("--pre", "pre_samples", default=0, show_default=True,
              help="pre-trigger samples")
@click.option("--trig-rise", "trig_rise", default=None, type=int,
              help="trigger channel for a rising edge")
@click.option("--trig-fall", "trig_fall", default=None, type=int,
              help="trigger channel for a falling edge")
@click.option("--out", "output", required=True, help="output file: capture.sr, .vcd or .png")
@click.option("--timeout", "timeout_s", default=30.0, show_default=True, type=float)
def capture(target: str, rate: str, words_text: str, mode: str, pre_samples: int,
            trig_rise: int | None, trig_fall: int | None, output: str,
            timeout_s: float) -> None:
    rate_hz = _parse_size(rate)
    words = _parse_size(words_text)
    if words <= 0:
        raise click.BadParameter("--words must be positive")
    for channel, label in ((trig_rise, "trig-rise"), (trig_fall, "trig-fall")):
        if channel is not None and not 0 <= channel < CHANNELS:
            raise click.BadParameter(f"--{label} channel must be within 0..{CHANNELS - 1}")

    sample_div = _sample_div(rate_hz)
    actual_rate = FS_HZ // (sample_div + 1)
    trig_mask = 0
    trig_rise_bits = 0
    trig_fall_bits = 0
    if trig_rise is not None:
        trig_rise_bits = 1 << trig_rise
        trig_mask |= trig_rise_bits
    if trig_fall is not None:
        trig_fall_bits = 1 << trig_fall
        trig_mask |= trig_fall_bits
    config = p.pack_config(
        ctrl_flags=0,
        sample_div=sample_div,
        mode={"raw": 0, "txn": 1}[mode],
        chan_en=0xFF,
        trig_mask=trig_mask,
        trig_rise=trig_rise_bits,
        trig_fall=trig_fall_bits,
        pre_samples=pre_samples,
        post_samples=words,
        timeout=0,
    )
    if actual_rate != rate_hz:
        click.echo(f"note: rate {rate_hz} Hz maps to actual {actual_rate} Hz "
                   f"(sample_div {sample_div})")

    client = _make_client(target)
    try:
        result = client.capture(config, timeout_s=timeout_s)
        logs = client.log_lines
    except (AnalyzerError, TransportError) as exc:
        raise click.ClickException(str(exc)) from exc
    finally:
        client.close()
    for line in logs:
        click.echo(f"LOG: {line}")
    if result.encoding == 1:
        click.echo(
            f"captured {result.meta['total_words']} transition words "
            f"({len(result.data)} bytes)"
        )
    else:
        samples = len(result.data)
        click.echo(f"captured {result.meta['total_words']} words = {samples} samples "
                   f"(rate {result.sample_rate_hz} Hz)")
    try:
        exported = export_capture(output, result.data, result.encoding, result.sample_rate_hz)
    except ValueError as exc:
        raise click.ClickException(str(exc)) from exc
    click.echo(f"exported {exported} samples to {output}")
    if result.parser_stats["crc_errors"] or result.parser_stats["format_errors"]:
        click.echo(
            f"warning: parser saw {result.parser_stats['crc_errors']} CRC and "
            f"{result.parser_stats['format_errors']} format errors"
        )


@cli.command(help="Abort any running capture.")
@click.option("--target", "-t", required=True, help="analyzer link target URI")
@click.option("--timeout", "timeout_s", default=5.0, show_default=True, type=float)
def abort(target: str, timeout_s: float) -> None:
    client = _make_client(target)
    try:
        client.abort(timeout_s=timeout_s)
    except (AnalyzerError, TransportError) as exc:
        raise click.ClickException(str(exc)) from exc
    finally:
        client.close()
    click.echo("aborted")


@cli.command(help="Decoded live events: --decoder uart:rx=0,baud=115200 (P7.5; the "
                  "burst-phase firmware NAKs this until the live path exists).")
@click.option("--target", "-t", required=True, help="analyzer link target URI")
@click.option("--decoder", "decoder_spec", default="uart:rx=0,baud=115200", show_default=True,
              help="decoder configuration, currently uart:rx=CH,baud=BAUD")
@click.option("--rate", default="100M", show_default=True,
              help="sample rate used to derive samples/bit for the decoder")
@click.option("--count", default=None, type=int, help="stop after N events")
@click.option("--csv", is_flag=True, help="emit CSV rows instead of text")
@click.option("--timeout", "timeout_s", default=None, type=float,
              help="overall deadline in seconds (default: until --count or Ctrl+C)")
def live(target: str, decoder_spec: str, rate: str, count: int | None, csv: bool,
         timeout_s: float | None) -> None:
    match = re.fullmatch(r"uart:rx=(\d+),baud=(\d+)", decoder_spec.strip())
    if not match:
        raise click.BadParameter("decoder must be uart:rx=CH,baud=BAUD in this phase")
    rx_channel = int(match.group(1))
    baud = int(match.group(2))
    if not 0 <= rx_channel < CHANNELS or baud <= 0:
        raise click.BadParameter("invalid UART decoder parameters")
    rate_hz = _parse_size(rate)
    sample_div = _sample_div(rate_hz)
    actual_rate = FS_HZ // (sample_div + 1)
    decoder_cfg = p.pack_decoder_cfg_uart(0, rx_channel, baud, actual_rate)

    client = _make_client(target)
    try:
        try:
            events = client.live(decoder_cfg, count=count, timeout_s=timeout_s)
        except KeyboardInterrupt:
            client.stop_live()
            click.echo("live stopped")
            return
    except (AnalyzerError, TransportError) as exc:
        raise click.ClickException(str(exc)) from exc
    finally:
        client.close()
    overruns = [event for event in events if event.get("overrun")]
    decoded = [event for event in events if not event.get("overrun")]
    if csv:
        click.echo("kind,slot,t_sample,data_hex")
        for event in decoded:
            click.echo(
                f"{p.EVENT_NAMES.get(event['kind'], event['kind'])},{event['slot']},"
                f"{event['t_sample']},{event['data'].hex()}"
            )
    else:
        for event in decoded:
            name = p.EVENT_NAMES.get(event["kind"], f"kind{event['kind']}")
            printable = event["data"].decode("ascii", errors="replace")
            click.echo(f"t={event['t_sample']} slot={event['slot']} {name} "
                       f"0x{event['data'].hex()} {printable!r}")
    for event in overruns:
        click.echo(
            f"overrun: {event['dropped']} events dropped at stage {event['stage']}"
        )
    click.echo(f"live: {len(decoded)} decoded events, {len(overruns)} overruns")


def main() -> int:
    try:
        return cli.main(prog_name="la-cli", standalone_mode=True)
    except click.ClickException as exc:  # pragma: no cover - click handles this
        click.echo(f"error: {exc.format_message()}", err=True)
        return 1
    except SystemExit as exc:
        return int(exc.code) if exc.code is not None else 0


if __name__ == "__main__":
    sys.exit(main())
