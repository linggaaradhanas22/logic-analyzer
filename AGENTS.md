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
- Treat `docs/status.md` as the required execution ledger. At the start of a
  session inspect it and `git status --short --branch`; after every atomic phase
  or tracked source, test, configuration, tooling or documentation change,
  update it with the date, changed files, verification commands and report
  paths, blockers and next action.
- A tracked change is not complete until it is committed and pushed to
  `origin/main`, or explicitly recorded as WIP in `docs/status.md`. Stage only
  intended files; never stage `.venv/`, `.deps/`, `.tools/`, `build/`, logs,
  captures or secrets.
- Follow the status, commit and push sequence in `docs/status.md`. Verify the
  remote and a clean worktree after pushing. Never force-push; if `origin` or
  the push is unavailable, record the exact blocker and do not claim completion.
- Do not modify the neighboring DDR3 PHY project.
