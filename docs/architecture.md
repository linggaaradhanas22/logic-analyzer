# Zynq Architecture and Integration Notes

The data path is eight 3.3 V inputs on Zybo JC, a 100 MHz PL sampler with
trigger/transition encoding, ordinary AXI DMA scatter-gather into DDR, and
FreeRTOS capture/decoder tasks on the Zynq PS (burst mode MVP). This document
records the implemented PL integration (P3), the measured resource usage and
the constraint/CDC decisions behind it.

## PL Integration (P3.1, `fpga/scripts/build.tcl`)

Single-fabric-clock design: everything PL runs on `FCLK_CLK0` at 100 MHz
(PS7 IO PLL 1000 MHz / 2:5). `proc_sys_reset` (synchronized resets,
`dcm_locked` = 1) drives all AXI fabrics and the DMA; `la_top.srst` is the
active-high inversion of `peripheral_aresetn` (assertion coincides with the
PS reset, release is synchronized).

Block design `system` (top `system_wrapper`):

- `processing_system7` (ps7): full Zybo board preset applied from the pinned
  `fpga/board_files/zybo-z7-10/A.0/preset.xml` (DDR3-1066 32-bit, QSPI, SD,
  USB, ENET0, console UART1 on MIO 48/49) plus overrides:
  UART0 on EMIO with RTS/CTS (`PCW_UART0_GRP_FULL_ENABLE=1`,
  default 921600), `M_AXI_GP0` enabled, 64-bit `S_AXI_HP0`,
  FCLK_CLK0 100 MHz, `IRQ_F2P` (single input,
  `PCW_USE_FABRIC_INTERRUPT=1`, `PCW_IRQ_F2P_INTR=1`).
- `la_top_0`: RTL module reference (`create_bd_cell -type module
  -reference la_top`) holding the P2 acquisition datapath; ch_in (JC) and
  patgen_o (JE) are external vector ports.
- `axi_dma_0`: S2MM only (`C_INCLUDE_MM2S=0`), SG enabled
  (`C_INCLUDE_SG=1`, ordinary descriptor chains; cyclic is not supported on
  SG cores), `C_SG_LENGTH_WIDTH=26`, 64-bit M_AXI, 32-bit AXIS input, and
  `C_SG_INCLUDE_STSCNTRL_STRM=0` because the status stream is tied off in this
  design (leaving the Vivado default stalled every S2MM transfer until the
  2026-09-10 bench diagnosis).
- `axi_quad_spi_0` (P9.1): `xilinx.com:ip:axi_quad_spi:3.2` in slave mode,
  standard 8-bit, 256-deep FIFO, `ext_spi_clk`/`s_axi_aclk` on FCLK_CLK0, AXI4-Lite
  at 0x4080_0000 through `sc_lite`; `ip2intc_irpt` joins `xlconcat_irq` In2.
  JD-7..10 carry SCLK/SS_N/MOSI/MISO; the JA-1 data-ready output is PS EMIO
  GPIO bit 0 (the EMIO width is set after the enable, or the width resets to
  64; see the 2026-09-13 session log). `docs/spi-link.md` is the interface
  contract.
- AXI fabrics: two SmartConnect instances (`sc_lite`: GP0 -> la_top S_AXI +
  axi_dma S_AXI_LITE; `sc_dma`: DMA M_AXI_S2MM/SG -> S_AXI_HP0). This
  Vivado 2026.1 install ships no `axi_interconnect` definition (catalog
  probe; the on-disk `axi_interconnect_v2_1` IP is excluded from the served
  catalog), so the plan's fabric requirement is met with the supported
  SmartConnect equivalent.
- Interrupts: `xlconcat_irq` fans `la_top.irq_o`, `axi_dma.s2mm_introut` and
  `axi_quad_spi.ip2intc_irpt` into PS `IRQ_F2P` (three inputs).
- External UART0 EMIO pins `uart0_tx/rx/rtsn/ctsn` -> JD (see XDC below).

Address map (assigned in `build.tcl` with explicit offsets/ranges):

| Master | Slave | Offset | Range |
|---|---|---|---|
| ps7 GP0 | la_top registers | 0x4000_0000 | 64K |
| ps7 GP0 | axi_dma S_AXI_LITE | 0x4040_0000 | 64K |
| ps7 GP0 | axi_quad_spi AXI4-Lite (P9.1) | 0x4080_0000 | 64K |
| axi_dma S2MM | ps7 DDR (HP0) | 0x0000_0000 | 1G |
| axi_dma SG | ps7 DDR (HP0) | 0x0000_0000 | 1G |

## Constraints and CDC (P3.2, `fpga/constraints/zybo_z7_10.xdc`)

- JC inputs CH0-7: LVCMOS33, sites V15 W15 T11 T10 W14 Y14 T12 U12.
- JE outputs PATGEN0-7: LVCMOS33, sites V12 W16 J15 H15 V13 U17 T17 Y17.
- JD UART0 EMIO: TX T14, RX T15, RTS_N P14, CTS_N R14, all LVCMOS33.
- `ch_in` are asynchronous acquisition inputs whose only consumer is the
  `la_input_sync` two-flop synchronizer (marked `(* async_reg *)`); the
  pad-to-first-stage path is exempted with `set_false_path`, documented in
  the XDC. No other asynchronous inputs exist.
- The PS7 EMIO UART0 pads are driven by PS-internal logic (PS-provided
  clocks/constraints); no user timing exceptions are required on JD.

## P3 Fabricability Rework of P2 RTL

Two P2 blocks were reworked so the memories map to block RAM (the ring needs
512 Kb; the part has 6000 LUTRAM cells ~ 384 Kb):

- `la_pretrig_ring` and `la_axis_fifo` now use synchronous-read BRAM
  (`ram_style = "block"` inferred by size and code pattern) with a one-beat
  output register. External protocol, ordering, capacity, overflow and drop
  accounting are unchanged (verified by the Questa suite); only emission
  latency grows by one cycle. The FIFO is one word-wide array
  `{tlast, tkeep, data}` with tkeep-derived byte write-enables; unkept bytes
  of partial beats are don't-care and consumers must use tkeep.
- `la_patgen` period products (`scale*UART_BIT/SPI_HALF/I2C_HALF - 1`) are
  computed from `div_i` and latched at the enable edge instead of feeding
  DSP multipliers into the per-cycle compare path (timing closure).

## Measured Resource Use (routed, xc7z010clg400-1)

Latest signoff build (P9.1, 2026-09-13, `build/release/utilization.rpt` and
`timing_summary.rpt`):

| Resource | Used | Available | Utilization |
|---|---|---|---|
| Slice LUTs | 6466 | 17600 | 36.7 % |
| Block RAM Tile | 37 | 60 | 61.7 % |
| Bonded IOB | 26 | 100 | 26 % |

Timing: zero failing endpoints, WNS +0.247 ns and WHS +0.019 ns at 100 MHz;
DRC zero errors. The P3 baseline (before the SPI slave, ring/patgen rework and
DMA fixes) was 33.5 % LUT and 60.0 % BRAM with WNS +0.278 ns; the P3.3
reset-fix build measured WNS +0.255 ns / WHS +0.024 ns. Consult
`build/release/*.rpt` on the current checkout for the exact numbers, since each
signoff run replaces them.

## Artifacts and Next Steps

- `build/release/logic_analyzer.xsa` (write_hw_platform -fixed -include_bit)
  and `build/release/logic_analyzer.bit` are the firmware platform inputs; the
  current pair is the P9.1 build with the SPI slave and the JA-1 data-ready.
- P3.3 `fpga/scripts/program.tcl` is the [HUMAN] JTAG programming step; it was
  exercised on 2026-09-10 (LD0 heartbeat observed). `jtag-zynq` is the
  scripted load used for every later bench session.
- The P9.1 SPI link is bench-verified for framed transfers at 5/10/18 MHz. The
  2026-09-20 V00 pivot makes the Zynq PS USB0 CDC-ACM profile the default
  product transport (`la_app_usb`, 1.62 MB/s measured over 64 MiB) and moves
  the live browser view to the PC host bridge (`host/la_live_web`, SSE); the
  ESP32 gateway, its `la_web` page and the Wi-Fi drain (~0.28 MB/s, S3
  USB-OTG full-speed ceiling about 1.2 MB/s) remain a supported optional
  profile.
- Remaining honest caveats: PS DDR behavior with the board-preset negative
  DQS-to-CLK skew values is only verified at boot/bring-up on hardware;
  `IRQ_F2P` carries three sources (analyzer, DMA, SPI) and the drivers must
  distinguish them by status reads; the SmartConnect auto-configuration
  warnings for ps7/axi_dma widths were reviewed (endpoint widths are explicit:
  GP0 32-bit lite, S2MM 64-bit, SG 32-bit, HP0 64-bit); 19/20 MHz SPI loses
  chunks to slave FIFO underruns, so 18 MHz is the measured burst ceiling on
  the current wiring.
