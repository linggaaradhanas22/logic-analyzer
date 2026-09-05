# Architecture Scaffold

The intended data path is eight 3.3 V inputs on Zybo JC, a 100 MHz PL sampler,
trigger/transition encoding, ordinary AXI DMA SG into DDR, FreeRTOS capture and
decoder tasks on the Zynq PS, and a framed UART link to an ESP32 gateway.

Phase 0 only establishes the source layout and native automation. RTL, AXI
integration, cache/MMU configuration and measured utilization belong to later
phases and are not implied by this scaffold.
