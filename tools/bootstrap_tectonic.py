"""Fetch a hash-verified portable Tectonic0.17.0 without a system installation."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import urllib.request
import zipfile

VERSION = "0.17.0"
URL = "https://github.com/tectonic-typesetting/tectonic/releases/download/tectonic%400.17.0/tectonic-0.17.0-x86_64-pc-windows-msvc.zip"
SHA256 = "f61ce51f0b0ade1015b7de7ef368541c5424e9756ecbd0d7af97d6d48030845f"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    args = parser.parse_args()
    if not args.directory.is_dir():
        parser.error("Destination directory must already exist")
    archive = args.directory / f"tectonic-{VERSION}.zip"
    payload = urllib.request.urlopen(URL, timeout=120).read()
    if hashlib.sha256(payload).hexdigest() != SHA256:
        raise RuntimeError("Release archive SHA256 mismatch")
    archive.write_bytes(payload)
    with zipfile.ZipFile(archive) as zipped:
        candidates = [name for name in zipped.namelist() if Path(name).name == "tectonic.exe"]
        if len(candidates) != 1:
            raise RuntimeError("Expected one compiler executable")
        executable = args.directory / "tectonic.exe"
        executable.write_bytes(zipped.read(candidates[0]))
    print(f"Verified Tectonic{VERSION}: {executable}; archive SHA256={SHA256}")


if __name__ == "__main__":
    main()
