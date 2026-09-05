# RTOS Logic Analyzer

This is the Zybo Z7-10 plus ESP32 logic-analyzer project. The implementation
root is this existing `Source_Code` directory. The project targets native
Windows tools only: Vivado/Vitis 2026.1, Questa, MSVC/CMake/Ninja, CPython and
native ESP-IDF.

The implementation source of truth is `docs/plan.md`. Phase 0 establishes the
runner, environment checks, register-map generation, native C ABI tests and
simulation harness. Hardware gates remain explicit until the required tools,
boards and wiring are available.

## Canonical commands

Run these commands from the project root; `tools/run.py` derives the project
root from its own location. An absolute path to the runner may be used from
elsewhere.

```text
python tools/check_env.py
python tools/run.py gen
python tools/run.py test-native
python tools/run.py test-host
python tools/run.py lint
python tools/run.py sim
```

Use `python tools/run.py --help` for all targets. Each invocation writes a
machine-readable result under `build/reports` and returns the child process's
nonzero status. Missing licensed tools or hardware are named precisely.

## Phase 0 status

The local native gates currently pass for the environment checker, register-map
generation, MSVC/CMake/CTest, ESP32 mocks, host pytest, Questa compile/simulation
and the ESP-IDF v5.5.2 hello-world build. Vivado 2026.1 resolves the pinned
Zybo Z7-10 board part. Vivado implementation, Vitis application integration and
physical programming/HIL remain later-phase or human hardware gates.

See `docs/windows-dev.md` for setup, isolated project paths and troubleshooting.
