# Agent Rules

This repository is the native-Windows implementation of the RTOS logic analyzer
described in `docs/plan.md`.

- Resolve the project root from the script location. Do not depend on the shell's
  current directory.
- Keep project-owned source, environments, generated workspaces, captures and
  release artifacts beneath this directory.
- Use `python tools/run.py <target>` as the canonical entry point. Do not add a
  required WSL, Linux, container, Make, Verilator or ESP-IDF Linux workflow.
- Edit `docs/regmap.yaml`, then run `python tools/run.py gen`; do not hand-edit
  generated register files.
- Every RTL block must eventually have a self-checking procedural Questa test.
- Protocol and decoder changes require `python tools/run.py test-native`.
- ISRs only perform notification or send-from-ISR operations. Buffer ownership
  is transferred by handle, and every queue has a documented full policy.
- AXI DMA uses ordinary scatter-gather descriptors in both modes. Cyclic mode
  and simple-transfer calls are not permitted.
- Hardware, licensed-tool and bench steps that cannot run locally are reported
  as blocked or unverified, never as passing.
- Do not modify the neighboring DDR3 PHY project.
