"""Prompt for Wi-Fi credentials and patch the ignored ESP-IDF sdkconfig.

Credentials are written only to the generated, git-ignored
`build/esp32/sdkconfig` (the runner passes it to `idf.py` as `SDKCONFIG`);
they never enter tracked files, shell history or logs. Run with the project
virtual environment Python from any directory, then rebuild and reflash:

    .venv\\Scripts\\python.exe tools\\set_wifi.py
    python tools\\run.py fw-esp32
"""

from __future__ import annotations

import argparse
import getpass
import re
from pathlib import Path
from typing import Sequence


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SDKCONFIG = PROJECT_ROOT / "build" / "esp32" / "sdkconfig"


def kconfig_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def replace(text: str, name: str, value: str) -> str:
    line = f"CONFIG_LA_WIFI_{name}={kconfig_string(value)}"
    pattern = re.compile(rf"^CONFIG_LA_WIFI_{name}=.*$", re.MULTILINE)
    if pattern.search(text):
        return pattern.sub(lambda _match: line, text, count=1)
    return text.rstrip("\n") + "\n" + line + "\n"


def apply(sdkconfig: Path, ssid: str, password: str) -> None:
    text = sdkconfig.read_text(encoding="utf-8", errors="surrogateescape")
    text = replace(text, "SSID", ssid)
    text = replace(text, "PASSWORD", password)
    sdkconfig.write_text(text, encoding="utf-8", errors="surrogateescape")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sdkconfig",
        type=Path,
        default=DEFAULT_SDKCONFIG,
        help="generated sdkconfig to patch (default: build/esp32/sdkconfig)",
    )
    args = parser.parse_args(argv)
    if not args.sdkconfig.is_file():
        raise SystemExit(
            f"sdkconfig not found: {args.sdkconfig}\n"
            "Build the gateway once with `python tools/run.py fw-esp32`."
        )
    ssid = input("Wi-Fi SSID: ").strip()
    password = getpass.getpass("Wi-Fi password (empty for open network): ")
    apply(args.sdkconfig, ssid, password)
    print(
        f"Updated {args.sdkconfig} (SSID length {len(ssid)}, "
        f"password length {len(password)})."
    )
    print("Next: python tools/run.py fw-esp32, then reflash the ESP32.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
