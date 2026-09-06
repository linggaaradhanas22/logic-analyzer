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
  SG cores), `C_SG_LENGTH_WIDTH=26`, 64-bit M_AXI, 32-bit AXIS input.
- AXI fabrics: two SmartConnect instances (`sc_lite`: GP0 -> la_top S_AXI +
  axi_dma S_AXI_LITE; `sc_dma`: DMA M_AXI_S2MM/SG -> S_AXI_HP0). This
  Vivado 2026.1 install ships no `axi_interconnect` definition (catalog
  probe; the on-disk `axi_interconnect_v2_1` IP is excluded from the served
  catalog), so the plan's fabric requirement is met with the supported
  SmartConnect equivalent.
- Interrupts: `xlconcat_irq` fans `la_top.irq_o` and `axi_dma.s2mm_introut`
  into PS `IRQ_F2P`.
- External UART0 EMIO pins `uart0_tx/rx/rtsn/ctsn` -> JD (see XDC below).

Address map (assigned in `build.tcl` with explicit offsets/ranges):

| Master | Slave | Offset | Range |
|---|---|---|---|
| ps7 GP0 | la_top registers | 0x4000_0000 | 64K |
| ps7 GP0 | axi_dma S_AXI_LITE | 0x4040_0000 | 64K |
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

## Measured Resource Use (P3, routed, xc7z010clg400-1, 2026-09-06)

`build/release/utilization.rpt` (top-level design, slow corner):

| Resource | Used | Available | Utilization |
|---|---|---|---|
| Slice LUTs | 5897 | 17600 | 33.5 % |
| LUT as Memory | 679 | 6000 | 11.3 % |
| Slice Registers | 8510 | 35200 | 24.2 % |
| Block RAM Tile | 36 | 60 | 60.0 % |
| RAMB36E1 / RAMB18E1 | 17 / 38 | 60 / 120 | 28.3 % / 31.7 % |

Timing: `build/release/timing_summary.rpt` reports zero failing endpoints,
WNS +0.278 ns at 100 MHz after the `la_patgen` period-latch rework. DRC:
`build/release/drc.rpt` has no errors; the only advisory (REQP-181,
write-first BRAM mode inside the Xilinx AXI DMA IP) is a stock IP note.

## Artifacts and Next Steps

- `build/release/logic_analyzer.xsa` (write_hw_platform -fixed -include_bit)
  and `build/release/logic_analyzer.bit` are the P4 platform inputs.
- P3.3 `fpga/scripts/program.tcl` is the [HUMAN] JTAG programming step;
  blink/bench checks are not yet performed.
- Remaining honest caveats: PS DDR behavior with the board-preset negative
  DQS-to-CLK skew values is only verified at boot/bring-up on hardware;
  `IRQ_F2P` carries both analyzer and DMA interrupts through the fabric and
  the driver must distinguish them by status reads; the SmartConnect
  auto-configuration warnings for ps7/axi_dma widths were reviewed (endpoint
  widths are explicit: GP0 32-bit lite, S2MM 64-bit, SG 32-bit, HP0 64-bit).
