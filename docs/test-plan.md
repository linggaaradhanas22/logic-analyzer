# Test Plan Scaffold

Phase 0 gates:

- register-map generation is idempotent;
- native C protocol, UART decoder and segment-ownership tests pass under CTest;
- the ctypes ABI smoke loads the built library;
- Questa harness negative checks detect failure, timeout and missing PASS;
- environment and path checks report every missing native prerequisite.

RTL, firmware, target and HIL gates are added with their corresponding phases.
