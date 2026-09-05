set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-NonInteractive", "-Command"]

default:
    python tools/run.py --help

gen:
    python tools/run.py gen

sim:
    python tools/run.py sim

lint:
    python tools/run.py lint

test-native:
    python tools/run.py test-native

test-esp32-mocks:
    python tools/run.py test-esp32-mocks

test-host:
    python tools/run.py test-host

test-esp32-target:
    python tools/run.py test-esp32-target

bit:
    python tools/run.py bit

fw-zynq:
    python tools/run.py fw-zynq

fw-esp32:
    python tools/run.py fw-esp32

program:
    python tools/run.py program

flash-esp:
    python tools/run.py flash-esp

hil:
    python tools/run.py hil

release:
    python tools/run.py release

check-env:
    python tools/run.py check-env

query-board:
    python tools/run.py query-board
