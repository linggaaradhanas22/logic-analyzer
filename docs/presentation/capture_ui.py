"""Render the current UI with declared synthetic data, without opening hardware."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
if not (ROOT/'build').is_dir():
    raise RuntimeError('Project build directory is missing')
(ROOT/'build/presentation').mkdir(exist_ok=True)
sys.path.insert(0,str(ROOT))
from host.la_ui.server import UiServer, UiState
from host.la_cli.convert import write_sr

# A declared illustration, not a new device capture. Generate 8N1 at 115200
# baud on CH0 with 10 MS/s sampling and inactive CH1-7.
samples=bytearray([1]*200)
bits=[]
for value in b'LA-TEST\n':
    bits.extend([0]+[(value>>i)&1 for i in range(8)]+[1])
for i,bit in enumerate(bits):
    n=round((i+1)*10_000_000/115200)-round(i*10_000_000/115200)
    samples.extend([bit]*n)
samples.extend([1]*200)
write_sr(str(ROOT/'build/presentation/illustrative-uart.sr'),bytes(samples),0,10_000_000)

server = UiServer(UiState(),port=0)
url = server.start()
try:
    subprocess.run(['node',str(Path(__file__).with_name('capture_ui.mjs')),url],check=True,env=os.environ.copy())
finally:
    server.stop()
