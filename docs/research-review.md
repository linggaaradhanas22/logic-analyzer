# v0.4 Research Review

Phase 0 records the corrections carried into the implementation plan:

- The development workflow is native Windows. WSL, containers, Linux CI and an
  ESP-IDF Linux target are not fallback prerequisites.
- Both burst and live capture use ordinary AXI DMA scatter-gather descriptors.
  Cyclic hardware mode is disabled, and only `AcqTask` may change descriptor
  ownership.
- Live packets close at 64 K words or after 2 ms of FCLK time. A pending run is
  flushed before TLAST; the packet capacity is not a latency guarantee by
  itself.
- Decoder event throughput includes timestamps and framing. A 1 MHz SPI stream
  represented as one event per byte intentionally exercises overload handling
  on the UART link; it is not a zero-drop acceptance gate.
- Static event blocks are passed through a FreeRTOS queue of handles so decoder
  tasks do not concurrently write an unsynchronized MessageBuffer.
- Native host mocks validate portable logic only. DMA coherency, interrupts,
  FreeRTOS scheduling, UART flow control and Wi-Fi require target or bench
  evidence.

The source plan is copied to `docs/plan.md`. This document is evidence of the
Phase 0 documentation step, not a replacement for measured P8 results.
