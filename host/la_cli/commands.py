"""la-cli commands: ports, ping, status and burst capture with export."""

from __future__ import annotations

import re
import sys
import time
from pathlib import Path

import click

from . import protocol as p
from . import trace as trace_mod
from .client import AnalyzerClient, AnalyzerError
from .convert import export_capture
from .transport import TransportError, list_candidates, open_uri

FS_HZ = 100_000_000  # la_top FCLK
CHANNELS = 8
PATGEN_MODES = {
    "counter": 0, "walking": 1, "prbs": 2, "uart": 3, "spi": 4, "i2c": 5,
    "uart_text": 7, "uart_bin": 8, "uart_ferr": 9,
    "spi_multi": 10, "spi16": 11,
    "i2c_read": 12, "i2c_nack": 13, "i2c_alt": 14,
}
PATGEN_CHOICES = ["off", *PATGEN_MODES]


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
@click.option("--patgen", "patgen_mode",
              type=click.Choice(PATGEN_CHOICES),
              default="off", show_default=True,
              help="drive the on-board pattern generator into the capture (internal self-test mux)")
@click.option("--trig-rise", "trig_rise", default=None, type=int,
              help="trigger channel for a rising edge")
@click.option("--trig-fall", "trig_fall", default=None, type=int,
              help="trigger channel for a falling edge")
@click.option("--out", "output", required=True, help="output file: capture.sr, .vcd or .png")
@click.option("--timeout", "timeout_s", default=30.0, show_default=True, type=float)
def capture(target: str, rate: str, words_text: str, mode: str, pre_samples: int,
            patgen_mode: str, trig_rise: int | None, trig_fall: int | None,
            output: str, timeout_s: float) -> None:
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
        ctrl_flags=p.CTRL_SELFTEST_EN if patgen_mode != "off" else 0,
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
        if patgen_mode != "off":
            # la_patgen latches mode/DIV on the EN rising edge; clear EN first
            # so a repeated --patgen session relatches instead of keeping the
            # previous mode.
            client.configure_patgen(0)
            client.configure_patgen(p.PATGEN_EN_MASK | (PATGEN_MODES[patgen_mode] << 4))
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


def _printable_bytes(data: bytes) -> str:
    """Renders decoded bytes for the live text view. Non-printable bytes
    become '.', which also keeps the line encodable on a cp1252 console when
    the stream starts mid-frame."""
    return "".join(chr(byte) if 32 <= byte < 127 else "." for byte in data)


def _parse_stop_on(value: str | None) -> bytes | None:
    """P7.7: parses --stop-on hex bytes ("4c41", "4C 41" or "0x4c41")."""
    if value is None:
        return None
    text = value.replace(" ", "").replace(":", "").replace("-", "")
    if text[:2].lower() == "0x":
        text = text[2:]
    if not text or len(text) % 2 != 0:
        raise click.BadParameter("--stop-on needs an even number of hex digits")
    try:
        return bytes.fromhex(text)
    except ValueError as exc:
        raise click.BadParameter("--stop-on must be hex bytes") from exc


def _parse_decoder_spec(spec: str, sample_rate_hz: int, slot: int = 0,
                        stop_on: bytes | None = None) -> bytes:
    """Parses one live decoder spec (P7.6/V09): `uart:rx=CH,baud=BAUD`,
    `spi:sck=CH[,mosi=CH][,miso=CH][,cs=CH][,cpol=0][,cpha=0][,word=8]`
    `[,lsb=0][,cs_active=0]` or `i2c:scl=CH,sda=CH`. `slot` is the decoder
    slot the payload targets (0..2); `stop_on` is the P7.7 decoded-pattern
    stop, limited by the decoder type's reserved bytes."""
    name, sep, params_text = spec.strip().partition(":")
    if not sep:
        raise click.BadParameter("decoder must be uart:..., spi:... or i2c:...")
    params: dict[str, int] = {}
    for item in params_text.split(","):
        item = item.strip()
        if not item:
            continue
        key, eq, value = item.partition("=")
        if not eq or not re.fullmatch(r"-?\d+", value.strip()):
            raise click.BadParameter(f"invalid decoder parameter {item!r}")
        params[key.strip()] = int(value)
    try:
        if name == "uart":
            if set(params) != {"rx", "baud"}:
                raise ValueError("uart needs rx and baud")
            if params["baud"] <= 0:
                raise ValueError("baud must be positive")
            return p.pack_decoder_cfg_uart(slot, params["rx"], params["baud"],
                                           sample_rate_hz, stop_on=stop_on)
        if name == "spi":
            known = {"sck", "mosi", "miso", "cs", "cpol", "cpha", "word", "lsb", "cs_active"}
            if "sck" not in params or set(params) - known:
                raise ValueError("spi needs sck and only known keys")
            return p.pack_decoder_cfg_spi(
                slot, params["sck"], params.get("mosi", -1), params.get("miso", -1),
                params.get("cs", -1), params.get("cpol", 0), params.get("cpha", 0),
                params.get("word", 8), params.get("lsb", 0), params.get("cs_active", 0),
                stop_on=stop_on)
        if name == "i2c":
            if set(params) != {"scl", "sda"}:
                raise ValueError("i2c needs scl and sda")
            return p.pack_decoder_cfg_i2c(slot, params["scl"], params["sda"],
                                          stop_on=stop_on)
    except (KeyError, ValueError) as exc:
        raise click.BadParameter(str(exc)) from exc
    raise click.BadParameter("decoder must be uart:..., spi:... or i2c:...")


def _prepare_live(decoder_specs, rate: str, stop_on: str | None):
    """Shared live-option parsing for `la-cli live` and `la-cli web` (V03/V09).

    `decoder_specs` is one spec string or a sequence of up to three; spec N
    configures decoder slot N. Returns (slot_configs, stream_config,
    stop_bytes, actual_rate_hz)."""
    if isinstance(decoder_specs, str):
        decoder_specs = (decoder_specs,)
    decoder_specs = tuple(decoder_specs)
    if not decoder_specs:
        raise click.BadParameter("at least one --decoder is required")
    if len(decoder_specs) > 3:
        raise click.BadParameter("at most three --decoder options are supported")
    rate_hz = _parse_size(rate)
    sample_div = _sample_div(rate_hz)
    actual_rate = FS_HZ // (sample_div + 1)
    stop_bytes = _parse_stop_on(stop_on)
    slot_configs = {
        slot: _parse_decoder_spec(spec, actual_rate, slot=slot,
                                  stop_on=stop_bytes)
        for slot, spec in enumerate(decoder_specs)
    }
    # Live decode consumes transition words; the 2 ms flush interval matches
    # the PL default (LIVE_FLUSH_CYCLES at 100 MHz).
    stream_config = p.pack_config(
        ctrl_flags=p.CTRL_STREAM,
        sample_div=sample_div,
        mode=1,  # transition encoding
        chan_en=0xFF,
        post_samples=0,
        txn_max_run=0x00FFFFFF,
        live_flush_cycles=200_000,
    )
    return slot_configs, stream_config, stop_bytes, actual_rate


def _configure_patgen(client: AnalyzerClient, patgen_mode: str) -> None:
    if patgen_mode == "off":
        return
    # la_patgen latches mode/DIV on the EN rising edge; clear EN first so a
    # repeated --patgen session relatches instead of keeping the previous mode.
    client.configure_patgen(0)
    client.configure_patgen(p.PATGEN_EN_MASK | (PATGEN_MODES[patgen_mode] << 4))


PATGEN_PINS = {
    "uart": "CH0", "uart_text": "CH0", "uart_bin": "CH0", "uart_ferr": "CH0",
    "spi": "sck=CH0, mosi=CH1, cs=CH2",
    "spi_multi": "sck=CH0, mosi=CH1, cs=CH2",
    "spi16": "sck=CH0, mosi=CH1, cs=CH2",
    "i2c": "scl=CH3, sda=CH4",
    "i2c_read": "scl=CH3, sda=CH4",
    "i2c_nack": "scl=CH3, sda=CH4",
    "i2c_alt": "scl=CH3, sda=CH4",
}


def _patgen_note(patgen_mode: str, slot_configs: dict[int, bytes]) -> None:
    """Warns when several decoders share a single-protocol pattern source.

    The internal patgen drives one protocol's pins at a time (and not the
    generic counter/walking/prbs modes), so extra decoders need external
    signals on their own channels (docs/using-the-analyzer.md)."""
    if patgen_mode not in PATGEN_PINS:
        return
    if len(slot_configs) <= 1:
        return
    click.echo(
        f"note: --patgen {patgen_mode} drives only {PATGEN_PINS[patgen_mode]}; "
        "the other decoders need external signals on their own channels"
    )


def _serve_live(client: AnalyzerClient, target: str,
                slot_configs: dict[int, bytes], stream_config: bytes,
                count: int | None, compact: bool, stop_bytes: bytes | None,
                timeout_s: float | None, bind: str, port: int,
                decoder_specs: tuple[str, ...], patgen_mode: str,
                autostart: bool) -> dict:
    """V03/V07/V09: serves the host web bridge with per-decoder control.

    The bridge owns the target for its lifetime (the caller already opened
    it) and starts idle. Each decoder pane's Start button
    (POST /control/start?slot=N) enables that slot — starting the session
    when it is the first one and sending a mid-session DECODER_CFG otherwise;
    Stop (POST /control/stop?slot=N) disables only that slot and ends the
    session when it was the last enabled one. `--autostart` enables every
    configured slot immediately and exits when the session ends (scripted
    smoke/tests). Events are counted, not retained; `count` caps the browser
    feed exactly even when the final device batch overshoots."""
    from ..la_live_web import LiveEventHub, LiveSessionController, LiveWebServer

    hub = LiveEventHub()
    hub.target = target
    hub.decoder = " | ".join(
        f"slot {slot}: {spec}" for slot, spec in enumerate(decoder_specs)
    )
    hub.slot_labels = [
        decoder_specs[slot] if slot < len(decoder_specs) else ""
        for slot in range(3)
    ]
    controller = LiveSessionController(
        client, hub, slot_configs, stream_config,
        count=count, timeout_s=timeout_s,
        event_format=p.EVT_FORMAT_BLOCK if compact else p.EVT_FORMAT_RECORDS,
        stop_bytes=stop_bytes,
        prepare=(lambda: _configure_patgen(client, patgen_mode))
        if patgen_mode != "off" else None,
        # Interactive sessions keep draining after a loss report; the scripted
        # autostart path ends on it like the classic live command.
        stop_on_overrun=autostart,
    )
    server = LiveWebServer(hub, host=bind, port=port, controller=controller)
    try:
        url = server.start()
    except OSError as exc:
        raise click.ClickException(
            f"cannot bind the live web view on {bind}:{port}: {exc}"
        ) from exc
    click.echo(f"live web view: {url} (target {target})")
    _patgen_note(patgen_mode, slot_configs)
    try:
        if autostart:
            controller.start_all()
            controller.wait()
        else:
            click.echo("open the page and click a decoder's Start (Ctrl+C to quit)")
            while True:
                time.sleep(0.2)
    except KeyboardInterrupt:
        click.echo("live web view stopped")
    finally:
        controller.stop_all()
        server.stop()
    stats = hub.stats()
    message_word = "message" if stats["messages"] == 1 else "messages"
    click.echo(
        f"live web: {stats['events']} events in {stats['messages']} {message_word} "
        f"({stats['dropped']} dropped: {stats['ring_dropped']} ring, "
        f"{stats['client_dropped']} client)"
    )
    if controller.error:
        click.echo(f"last session error: {controller.error}")
    if client.live_match_index is not None:
        click.echo(f"stop-on match at sample {client.live_match_index}")
    return stats


@cli.command(help="Decoded live events: --decoder uart:rx=0,baud=115200 (P7.5).")
@click.option("--target", "-t", required=True, help="analyzer link target URI")
@click.option("--decoder", "decoder_specs", multiple=True,
              default=("uart:rx=0,baud=115200",), show_default=True,
              help="up to three specs, one per decoder slot: uart:rx=CH,baud=BAUD | "
                   "spi:sck=CH,mosi=CH,cs=CH,... | i2c:scl=CH,sda=CH")
@click.option("--rate", default="100M", show_default=True,
              help="sample rate used to derive samples/bit for the decoder")
@click.option("--patgen", "patgen_mode",
              type=click.Choice(PATGEN_CHOICES),
              default="off", show_default=True,
              help="configure the on-board pattern generator before starting")
@click.option("--count", default=None, type=int, help="stop after N events")
@click.option("--compact", is_flag=True,
              help="request the compact EVT_BLOCK event encoding (P9.2)")
@click.option("--stop-on", "stop_on", default=None,
              help="stop when any decoder matches these hex bytes (P7.7), e.g. 4c41")
@click.option("--csv", is_flag=True, help="emit CSV rows instead of text")
@click.option("--serve", is_flag=True,
              help="serve the host live web view instead of printing events (V03)")
@click.option("--bind", default="127.0.0.1", show_default=True,
              help="live web view bind address (--serve)")
@click.option("--port", default=8080, show_default=True, type=int,
              help="live web view port (--serve)")
@click.option("--autostart", is_flag=True,
              help="start every configured decoder immediately and exit when "
                   "the session ends instead of waiting for the page's buttons (--serve)")
@click.option("--timeout", "timeout_s", default=None, type=float,
              help="overall deadline in seconds (default: until --count or Ctrl+C)")
def live(target: str, decoder_specs: tuple[str, ...], rate: str, patgen_mode: str,
         count: int | None, compact: bool, stop_on: str | None, csv: bool,
         serve: bool, bind: str, port: int, autostart: bool,
         timeout_s: float | None) -> None:
    if serve and csv:
        raise click.BadParameter("--csv cannot be combined with --serve")
    slot_configs, stream_config, stop_bytes, _actual_rate = _prepare_live(
        decoder_specs, rate, stop_on
    )

    client = _make_client(target)
    try:
        if serve:
            _serve_live(client, target, slot_configs, stream_config, count, compact,
                        stop_bytes, timeout_s, bind, port, decoder_specs,
                        patgen_mode, autostart)
            return
        _configure_patgen(client, patgen_mode)
        _patgen_note(patgen_mode, slot_configs)
        try:
            events = client.live(
                list(slot_configs.values()), config=stream_config, count=count,
                timeout_s=timeout_s,
                event_format=p.EVT_FORMAT_BLOCK if compact else p.EVT_FORMAT_RECORDS,
                stop_on_match=stop_bytes is not None,
            )
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
            click.echo(f"t={event['t_sample']} slot={event['slot']} {name} "
                       f"0x{event['data'].hex()} {_printable_bytes(event['data'])!r}")
    for event in overruns:
        click.echo(
            f"overrun: {event['dropped']} events dropped at stage {event['stage']}"
        )
    if client.live_match_index is not None:
        click.echo(f"stop-on match at sample {client.live_match_index}")
    click.echo(
        f"live: {len(decoded)} decoded events, {len(overruns)} overruns "
        f"({client.live_stats['wire_bytes']} wire bytes, "
        f"{'compact' if compact else 'v1'})"
    )


@cli.command(name="web", help="Host live web view (V03): serve decoded events at http://HOST:PORT/.")
@click.option("--target", "-t", required=True, help="analyzer link target URI")
@click.option("--decoder", "decoder_specs", multiple=True,
              default=("uart:rx=0,baud=115200",), show_default=True,
              help="up to three specs, one per decoder slot: uart:rx=CH,baud=BAUD | "
                   "spi:sck=CH,mosi=CH,cs=CH,... | i2c:scl=CH,sda=CH")
@click.option("--rate", default="100M", show_default=True,
              help="sample rate used to derive samples/bit for the decoder")
@click.option("--patgen", "patgen_mode",
              type=click.Choice(PATGEN_CHOICES),
              default="off", show_default=True,
              help="configure the on-board pattern generator before starting")
@click.option("--count", default=None, type=int, help="stop after N events")
@click.option("--compact", is_flag=True,
              help="request the compact EVT_BLOCK event encoding (P9.2)")
@click.option("--stop-on", "stop_on", default=None,
              help="stop when any decoder matches these hex bytes (P7.7), e.g. 4c41")
@click.option("--bind", default="127.0.0.1", show_default=True,
              help="live web view bind address")
@click.option("--port", default=8080, show_default=True, type=int,
              help="live web view port")
@click.option("--autostart", is_flag=True,
              help="start every configured decoder immediately and exit when "
                   "the session ends instead of waiting for the page's buttons")
@click.option("--timeout", "timeout_s", default=None, type=float,
              help="overall deadline in seconds (default: until --count or Ctrl+C)")
def web(target: str, decoder_specs: tuple[str, ...], rate: str, patgen_mode: str,
        count: int | None, compact: bool, stop_on: str | None, bind: str,
        port: int, autostart: bool, timeout_s: float | None) -> None:
    """`la-web` / `la-cli web`: the same implementation as `live --serve`."""
    slot_configs, stream_config, stop_bytes, _actual_rate = _prepare_live(
        decoder_specs, rate, stop_on
    )
    client = _make_client(target)
    try:
        _serve_live(client, target, slot_configs, stream_config, count, compact,
                    stop_bytes, timeout_s, bind, port, decoder_specs,
                    patgen_mode, autostart)
    except (AnalyzerError, TransportError) as exc:
        raise click.ClickException(str(exc)) from exc
    finally:
        client.close()


def web_main() -> int:
    """Console-script entry point for `la-web` (same command as `la-cli web`)."""
    try:
        return web.main(prog_name="la-web", standalone_mode=True)
    except SystemExit as exc:  # pragma: no cover - click exits through SystemExit
        return int(exc.code) if exc.code is not None else 0


@cli.command(name="trace", help="P8.3 task/queue trace: --start, --stop or --dump (default).")
@click.option("--target", "-t", required=True, help="analyzer link target URI")
@click.option("--start", "do_start", is_flag=True, help="clear the ring and start recording")
@click.option("--stop", "do_stop", is_flag=True, help="stop recording without dumping")
@click.option("--dump", "do_dump", is_flag=True, help="stop and dump the ring (default)")
@click.option("--out", "output", default=None, help="write the raw dump lines to this file")
@click.option("--png", "png_output", default=None, help="render a task/queue timeline PNG")
@click.option("--title", default=None, help="timeline title")
@click.option("--timeout", "timeout_s", default=30.0, show_default=True, type=float)
def trace_cmd(target: str, do_start: bool, do_stop: bool, do_dump: bool,
              output: str | None, png_output: str | None, title: str | None,
              timeout_s: float) -> None:
    if sum((do_start, do_stop, do_dump)) > 1:
        raise click.BadParameter("choose only one of --start, --stop or --dump")
    client = _make_client(target)
    try:
        if do_start:
            client.trace_control(p.TRACE_START, timeout_s=timeout_s)
            click.echo("trace started")
            return
        if do_stop:
            client.trace_control(p.TRACE_STOP, timeout_s=timeout_s)
            click.echo("trace stopped")
            return
        lines = client.trace_dump(timeout_s=timeout_s)
    except (AnalyzerError, TransportError) as exc:
        raise click.ClickException(str(exc)) from exc
    finally:
        client.close()
    try:
        dump = trace_mod.parse_trace(lines)
    except trace_mod.TraceParseError as exc:
        raise click.ClickException(f"trace dump parse failed: {exc}") from exc
    summary = trace_mod.summarize(dump)
    if output:
        Path(output).write_text("\n".join(lines) + "\n", encoding="utf-8")
        click.echo(f"wrote {summary['records']} dump lines to {output}")
    if png_output:
        try:
            trace_mod.render_timeline(dump, png_output, title=title or "P8.3 trace")
        except trace_mod.TraceParseError as exc:
            raise click.ClickException(f"trace render failed: {exc}") from exc
        click.echo(f"rendered timeline to {png_output}")
    counts = ", ".join(f"{token}={count}" for token, count in sorted(summary["counts"].items()))
    click.echo(
        f"trace: {summary['records']} records, {summary['dropped']} dropped, "
        f"{summary['duration_s'] * 1000:.1f} ms, "
        f"{summary['tasks']} tasks, {summary['queues']} queues"
    )
    if counts:
        click.echo(f"trace tokens: {counts}")


@cli.command(name="link", help="P9.1 bulk transport for burst frames: --spi or --uart (default).")
@click.option("--target", "-t", required=True, help="analyzer link target URI")
@click.option("--spi", "use_spi", is_flag=True, help="route burst DATA_* frames over the SPI slave")
@click.option("--uart", "use_uart", is_flag=True, help="keep burst DATA_* frames on the UART (default)")
@click.option("--clock", "spi_clock_mhz", type=int, default=0,
              help="ESP32 SPI master clock in MHz for the bulk link (0 = firmware default)")
def link_cmd(target: str, use_spi: bool, use_uart: bool, spi_clock_mhz: int) -> None:
    if use_spi and use_uart:
        raise click.BadParameter("choose only one of --spi or --uart")
    if spi_clock_mhz and not use_spi:
        raise click.BadParameter("--clock requires --spi")
    transport = p.LINK_SPI if use_spi else p.LINK_UART
    client = _make_client(target)
    try:
        client.configure_link(transport, spi_clock_mhz=spi_clock_mhz)
    except (AnalyzerError, TransportError) as exc:
        raise click.ClickException(str(exc)) from exc
    finally:
        client.close()
    suffix = f", clock {spi_clock_mhz} MHz" if spi_clock_mhz else ""
    click.echo(f"link: burst transport {'spi' if transport == p.LINK_SPI else 'uart'}{suffix}")


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
