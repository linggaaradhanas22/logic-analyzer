"""Real-run preview framing, transport callbacks and bounded browser fanout."""
import json
import struct

import pytest

from host.la_cli import protocol as p
from host.la_cli.client import AnalyzerClient
from host.la_cli.transport import open_uri
from host.la_live_web.core import LiveEventHub


def packet(words=(25601, 22272), start=123, omitted=2):
    return struct.pack("<BBHIQI", 1, 1, len(words), 10_000_000, start, omitted) + struct.pack(
        "<" + "I" * len(words), *words)


def test_waveform_exact_runs_and_timestamps():
    wave = p.unpack_waveform(packet())
    assert wave == {"start": 123, "end": 310, "rate": 10_000_000,
                    "omitted": 2, "words": [25601, 22272]}


@pytest.mark.parametrize("data", [b"", packet()[:-1], packet((1,)),
                                        bytes([2]) + packet()[1:]])
def test_waveform_rejects_malformed_data(data):
    with pytest.raises(p.FrameError):
        p.unpack_waveform(data)


def test_live_routes_waveform_without_counting_it_as_decoded_bytes(analyzer):
    mock = analyzer("live_waveform")
    waves = []
    client = AnalyzerClient(open_uri(mock.uri))
    try:
        client.configure_waveform(True)
        events = client.live(p.pack_decoder_cfg_uart(0, 0, 115200, 10_000_000),
                             count=8, timeout_s=2, on_waveform=waves.append)
    finally:
        client.close()
    assert len(waves) == 1 and waves[0]["words"] == [25601, 22272, 22273, 25600]
    assert len([event for event in events if not event.get("overrun")]) == 8


def test_waveform_replay_and_loss_are_bounded_and_resettable():
    hub = LiveEventHub(client_queue=2)
    feed = hub.attach()
    wave = p.unpack_waveform(packet())
    for _ in range(30):
        hub.publish_waveform(wave)
    assert len(hub._wave_ring) == 16
    assert hub.events == 0
    assert hub.stats()["client_dropped"] == 28
    assert json.loads(feed.queue.get_nowait())["waveform"] == wave
    hub.reset()
    assert feed.queue.empty() and not hub._wave_ring
    assert hub.stats()["waveform_frames"] == hub.stats()["client_dropped"] == 0
