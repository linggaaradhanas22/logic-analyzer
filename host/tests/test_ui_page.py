"""W03: page assets, id consistency and a real DOM-stub run under Node.

``node --check`` validates syntax; ``host/tests/page_check.mjs`` loads the
page scripts in a vm with a minimal fake DOM, runs ``LAApp.init()`` and a
scripted exercise of the waveform/live/trace renderers, and fails on unknown
``getElementById`` lookups. Node is a development tool here (the page itself
is plain browser JavaScript with no build step).
"""

from __future__ import annotations

import json
import shutil
import subprocess
from pathlib import Path

import pytest

PROJECT_ROOT = Path(__file__).resolve().parents[2]
STATIC = PROJECT_ROOT / "host" / "la_ui" / "static"
SCRIPTS = ("app.js", "waveform.js", "timeline.js")


def _node() -> str:
    node = shutil.which("node")
    if node is None:
        pytest.skip("node is not installed; the page checks need it")
    return node


@pytest.mark.parametrize("name", SCRIPTS)
def test_script_syntax(name):
    node = _node()
    result = subprocess.run(
        [node, "--check", str(STATIC / name)],
        capture_output=True, text=True, timeout=60,
    )
    assert result.returncode == 0, result.stderr


def test_page_references_scripts_and_assets():
    html = (STATIC / "index.html").read_text(encoding="utf-8")
    for name in SCRIPTS:
        assert f"/static/{name}" in html, name
    assert "/static/style.css" in html
    for required in ("wave-canvas", "trace-canvas", "live-panes", "toast",
                     "page-version", "btn-capture", "btn-decode",
                     "btn-trace-dump", "open-file"):
        assert f'id="{required}"' in html, required


def test_dom_stub_run():
    node = _node()
    result = subprocess.run(
        [node, str(PROJECT_ROOT / "host" / "tests" / "page_check.mjs")],
        capture_output=True, text=True, timeout=120, cwd=PROJECT_ROOT,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    report = json.loads(result.stdout.split("PAGE CHECK OK")[0])
    assert report["ok"] is True, report
    assert report["unknownIds"] == []
    for key, value in report["checks"].items():
        assert value is not False, (key, report["checks"])
    assert report["checks"]["init"] is True
    assert report["checks"]["liveBound"] is True
