# P3.1 native Vivado batch build for the RTOS logic analyzer.
#
# Usage:
#   vivado -mode batch -source build.tcl -tclargs PROJECT_ROOT [stage]
#     stage (optional): bd    - create and validate the block design only
#                        synth - bd + synthesis (stop before implementation)
#                        (default) - full flow to bitstream and XSA
#   PROJECT_ROOT and all outputs live below PROJECT_ROOT/build (ignored).
#   `python tools/run.py bit` is the canonical entry point (stage default).
#
# Hardware contract (docs/plan.md P3.1, docs/wiring.md):
#   - Board part digilentinc.com:zybo-z7-10:part0:1.2 (xc7z010clg400-1).
#   - processing_system7 with the board preset from the pinned board files
#     (DDR3, console UART1 on MIO 48/49), plus UART0 on EMIO with full
#     RTS/CTS (TX/RX/RTS/CTS to JD), FCLK_CLK0 100 MHz, IRQ_F2P and 64-bit
#     S_AXI_HP0.
#   - la_top as an RTL module reference (create_bd_cell -type module).
#   - axi_dma S2MM only, 32-bit AXIS stream, SG enabled (cyclic is not
#     supported on SG cores and is not enabled), buffer length width 26.
#   - Payload (M_AXI_S2MM) and descriptor (M_AXI_SG) traffic both reach PS
#     DDR through S_AXI_HP0; AXI-Lite controls come from M_AXI_GP0.
#   - proc_sys_reset supplies synchronized resets; xlconcat fans the
#     analyzer and DMA interrupts into IRQ_F2P.
#   - write_hw_platform -fixed -include_bit produces build/release/
#     logic_analyzer.xsa; the bitstream is copied to build/release/.

if {$argc < 1} {
  error "usage: vivado -mode batch -source build.tcl -tclargs PROJECT_ROOT \[stage\]"
}
set project_root [file normalize [lindex $argv 0]]
set stage full
if {$argc > 1} {
  set stage [lindex $argv 1]
}
if {$stage ni {bd synth}} {
  set stage full
}

set work_dir [file normalize [file join $project_root build vivado]]
set release_dir [file normalize [file join $project_root build release]]
set board_root [file join $project_root fpga board_files]
file mkdir $work_dir
file mkdir $release_dir

set_param board.repoPaths [list $board_root]
puts "PROJECT_ROOT=$project_root"
puts "STAGE=$stage"
puts "Vivado version=[version -short]"

# ---------------------------------------------------------------------------
# Project: board part pinned by tools/board-lock.json.
# ---------------------------------------------------------------------------
set proj_dir [file join $work_dir la]
create_project -force la $proj_dir -part xc7z010clg400-1
set_property board_part digilentinc.com:zybo-z7-10:part0:1.2 [current_project]

set rtl_dir [file join $project_root fpga rtl]
set rtl_files [glob -nocomplain [file join $rtl_dir *.sv]]
if {[llength $rtl_files] == 0} {
  error "P3 blocked: no RTL sources found under $rtl_dir"
}
add_files -norecurse {*}$rtl_files
set xdc_file [file join $project_root fpga constraints zybo_z7_10.xdc]
if {[file exists $xdc_file]} {
  add_files -fileset constrs_1 $xdc_file
}
update_compile_order -fileset sources_1

# ---------------------------------------------------------------------------
# Helpers.
# ---------------------------------------------------------------------------
proc latest_vlnv {pattern} {
  set defs [get_ipdefs -quiet -filter "VLNV =~ xilinx.com:ip:${pattern}:*"]
  if {[llength $defs] == 0} {
    error "no installed IP definition for xilinx.com:ip:$pattern"
  }
  set versions [lsort -dictionary -unique [get_property VLNV $defs]]
  return [lindex $versions end]
}

proc bd_cell {short name} {
  set vlnv [latest_vlnv $short]
  puts "ADD IP $name = $vlnv"
  return [create_bd_cell -type ip -vlnv $vlnv $name]
}

proc try_net {src dst} {
  set sp [get_bd_pins -quiet $src]
  set dp [get_bd_pins -quiet $dst]
  if {[llength $sp] == 0 || [llength $dp] == 0} {
    puts "CONNECT SKIP (missing pin) $src -> $dst"
    return
  }
  if {[catch {connect_bd_net $sp $dp} msg]} {
    puts "CONNECT FAIL $src -> $dst : $msg"
  } else {
    puts "OK $src -> $dst"
  }
}

proc try_intf {src dst} {
  set sp [get_bd_intf_pins -quiet $src]
  set dp [get_bd_intf_pins -quiet $dst]
  if {[llength $sp] == 0 || [llength $dp] == 0} {
    puts "CONNECT SKIP (missing interface) $src -> $dst"
    return
  }
  if {[catch {connect_bd_intf_net $sp $dp} msg]} {
    puts "CONNECT FAIL $src -> $dst : $msg"
  } else {
    puts "OK $src -> $dst"
  }
}

# ---------------------------------------------------------------------------
# Block design.
# ---------------------------------------------------------------------------
create_bd_design system

# -- processing_system7: apply the Zybo board preset from the pinned board
# -- files (single source of truth), then P3.1 overrides. PCW_EN_TTC0 is
# -- forced on (board preset disables both TTCs) because the Vitis
# -- freertos BSP validation requires a ttcps hardware instance as the
# -- FreeRTOS tick source. QSPI is disabled: the Vitis FSBL build fails
# -- on the SDT-flow xqspips parameter set (missing QSPI_BUS_WIDTH) and
# -- this design boots from SD/JTAG, not the QSPI flash.
set ps7 [bd_cell processing_system7 ps7]
set preset_file [file join $board_root zybo-z7-10 A.0 preset.xml]
set preset_fd [open $preset_file r]
set preset_xml [read $preset_fd]
close $preset_fd
set pstart [string first "<ip_preset preset_proc_name=\"ps7_preset\">" $preset_xml]
set pend [string first "</ip_preset>" $preset_xml $pstart]
if {$pstart < 0 || $pend < 0} {
  error "ps7_preset not found in $preset_file"
}
set preset_slice [string range $preset_xml $pstart [expr {$pend - 1}]]
set pairs [regexp -all -inline {name="CONFIG\.[^"]+" value="[^"]*"} $preset_slice]
puts "Applying [expr {[llength $pairs]}] PS7 board-preset properties"
foreach pair $pairs {
  regexp {CONFIG\.([^"]+)" value="([^"]*)} $pair -> name value
  set_property -quiet CONFIG.$name $value $ps7
}

set_property -dict [list \
  CONFIG.PCW_UART0_PERIPHERAL_ENABLE {1} \
  CONFIG.PCW_UART0_UART0_IO {EMIO} \
  CONFIG.PCW_UART0_GRP_FULL_ENABLE {1} \
  CONFIG.PCW_UART0_BAUD_RATE {921600} \
  CONFIG.PCW_USE_S_AXI_HP0 {1} \
  CONFIG.PCW_S_AXI_HP0_DATA_WIDTH {64} \
  CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100} \
  CONFIG.PCW_EN_CLK0_PORT {1} \
  CONFIG.PCW_USE_FABRIC_INTERRUPT {1} \
  CONFIG.PCW_IRQ_F2P_INTR {1} \
  CONFIG.PCW_EN_TTC0 {1} \
  CONFIG.PCW_TTC0_PERIPHERAL_ENABLE {1} \
  CONFIG.PCW_EN_QSPI {0} \
  CONFIG.PCW_QSPI_PERIPHERAL_ENABLE {0} \
  CONFIG.PCW_GPIO_EMIO_GPIO_ENABLE {1} \
  CONFIG.PCW_GPIO_EMIO_GPIO_WIDTH {1} \
] $ps7

# -- la_top as RTL module reference (acquisition datapath).
set la_top [create_bd_cell -type module -reference la_top la_top_0]

# -- AXI DMA: S2MM only, SG enabled, 26-bit buffer length, 32-bit stream.
# -- The control/status stream is disabled: the acquisition datapath has no
# -- per-packet sideband status, and leaving C_SG_INCLUDE_STSCNTRL_STRM at its
# -- Vivado default (1) with S_AXIS_S2MM_STS tied off leaves the S2MM SG engine
# -- unable to complete descriptors (observed on bench 2026-09-10: channel
# -- running, no errors, BD never completed, no data written).
set dma [bd_cell axi_dma axi_dma_0]
set_property -dict [list \
  CONFIG.C_INCLUDE_MM2S {0} \
  CONFIG.C_INCLUDE_S2MM {1} \
  CONFIG.C_INCLUDE_SG {1} \
  CONFIG.C_SG_LENGTH_WIDTH {26} \
  CONFIG.C_M_AXI_S2MM_DATA_WIDTH {64} \
  CONFIG.C_SG_INCLUDE_STSCNTRL_STRM {0} \
] $dma

# -- P9.1 SPI slave: axi_quad_spi in slave mode on JD-7..10 with a PS EMIO
# -- GPIO data-ready on JA-1. The UART keeps commands/ACKs; this core carries
# -- the bulk Zynq->ESP32 direction (docs/spi-link.md).
set spi [bd_cell axi_quad_spi spi_probe]
set_property -dict [list \
  CONFIG.Master_mode {0} \
  CONFIG.C_SPI_MODE {0} \
  CONFIG.C_NUM_TRANSFER_BITS {8} \
  CONFIG.C_FIFO_DEPTH {256} \
  CONFIG.C_USE_STARTUP {0} \
  CONFIG.C_DUAL_QUAD_MODE {0} \
  CONFIG.C_SPI_MEMORY {0} \
] $spi

# -- AXI fabrics (SmartConnect: this Vivado 2026.1 install ships no
# -- axi_interconnect definition - verified catalog probe; the on-disk
# -- axi_interconnect_v2_1 IP is excluded from the served catalog).
puts "AXI fabric candidates checked on install: axi_interconnect absent; using smartconnect"
set lite_ic [bd_cell smartconnect sc_lite]
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {3}] $lite_ic
set dma_ic [bd_cell smartconnect sc_dma]
set_property -dict [list CONFIG.NUM_SI {2} CONFIG.NUM_MI {1}] $dma_ic

# -- Reset and clock plumbing.
set rst [bd_cell proc_sys_reset rst_ps7]
set inv_fclk [bd_cell util_vector_logic inv_fclk_rst]
set_property -dict [list CONFIG.C_SIZE {1} CONFIG.C_OPERATION {not}] $inv_fclk
set inv_srst [bd_cell util_vector_logic inv_srst]
set_property -dict [list CONFIG.C_SIZE {1} CONFIG.C_OPERATION {not}] $inv_srst
set const_hi [bd_cell xlconstant const_hi]
set_property -dict [list CONFIG.CONST_VAL {1}] $const_hi
set const_lo [bd_cell xlconstant const_lo]
set_property -dict [list CONFIG.CONST_VAL {0}] $const_lo

# -- Interrupt fan-in (analyzer IRQ, DMA S2MM IRQ, SPI slave IRQ).
set concat [bd_cell xlconcat xlconcat_irq]
set_property -dict [list CONFIG.NUM_PORTS {3}] $concat

# Clocks: everything PL runs on FCLK_CLK0 (100 MHz).
set fclk ps7/FCLK_CLK0
foreach pin {
  ps7/M_AXI_GP0_ACLK ps7/S_AXI_HP0_ACLK
  sc_lite/aclk
  sc_dma/aclk
  axi_dma_0/s_axi_lite_aclk axi_dma_0/m_axi_sg_aclk axi_dma_0/m_axi_s2mm_aclk
  rst_ps7/slowest_sync_clk
  la_top_0/clk
  spi_probe/s_axi_aclk spi_probe/ext_spi_clk
} {
  try_net $pin $fclk
}

# Resets: FCLK_RESET0_N (active low) inverted into proc_sys_reset; the
# synchronized peripheral_aresetn resets the AXI fabric; la_top's srst is
# the same reset inverted back to active-high (synchronous release).
try_net ps7/FCLK_RESET0_N inv_fclk_rst/Op1
try_net inv_fclk_rst/Res rst_ps7/ext_reset_in
try_net const_hi/dout rst_ps7/dcm_locked
try_net const_hi/dout rst_ps7/aux_reset_in
try_net const_lo/dout rst_ps7/mb_debug_sys_rst
try_net rst_ps7/peripheral_aresetn inv_srst/Op1
try_net inv_srst/Res la_top_0/srst
foreach pin {
  sc_lite/aresetn
  sc_dma/aresetn
  axi_dma_0/axi_resetn
  spi_probe/s_axi_aresetn
} {
  try_net $pin rst_ps7/peripheral_aresetn
}

# Interfaces.
try_intf ps7/M_AXI_GP0 sc_lite/S00_AXI
try_intf sc_lite/M00_AXI la_top_0/S_AXI
try_intf sc_lite/M01_AXI axi_dma_0/S_AXI_LITE
try_intf sc_lite/M02_AXI spi_probe/AXI_LITE
try_intf la_top_0/M_AXIS axi_dma_0/S_AXIS_S2MM
try_intf axi_dma_0/M_AXI_S2MM sc_dma/S00_AXI
try_intf axi_dma_0/M_AXI_SG sc_dma/S01_AXI
try_intf sc_dma/M00_AXI ps7/S_AXI_HP0

# Interrupts.
try_net la_top_0/irq_o xlconcat_irq/In0
try_net axi_dma_0/s2mm_introut xlconcat_irq/In1
try_net spi_probe/ip2intc_irpt xlconcat_irq/In2
try_net xlconcat_irq/dout ps7/IRQ_F2P

# External ports: JC channel inputs, JE pattern outputs, blink LED (LD0),
# JD UART0 EMIO, P9.1 SPI link (JD-7..10) and data-ready (JA-1).
# (The ps7 UART0 EMIO scalars belong to the inferred UART_0 interface; the
# override warnings below are expected and harmless: this script is the
# source of truth and the BD is never regenerated from writ_bd_tcl.)
create_bd_port -dir I -from 7 -to 0 ch_in
create_bd_port -dir O -from 7 -to 0 patgen_o
create_bd_port -dir O blink_o
create_bd_port -dir O uart0_tx
create_bd_port -dir I uart0_rx
create_bd_port -dir O uart0_rtsn
create_bd_port -dir I uart0_ctsn
create_bd_port -dir I spi_sck
create_bd_port -dir I spi_ss_n
create_bd_port -dir I spi_mosi
create_bd_port -dir O spi_miso
create_bd_port -dir O spi_ready
try_net la_top_0/ch_in ch_in
try_net la_top_0/patgen_o patgen_o
try_net la_top_0/blink_o blink_o
try_net ps7/UART0_TX uart0_tx
try_net ps7/UART0_RX uart0_rx
try_net ps7/UART0_RTSN uart0_rtsn
try_net ps7/UART0_CTSN uart0_ctsn
try_net spi_probe/sck_i spi_sck
try_net spi_probe/spisel spi_ss_n
try_net spi_probe/io0_i spi_mosi
try_net spi_probe/io1_o spi_miso
try_net {ps7/GPIO_O[0]} spi_ready
try_net const_hi/dout spi_probe/ss_i
try_net const_lo/dout spi_probe/io1_i
try_net const_lo/dout ps7/GPIO_I
# ps7/GPIO_T is driven by the PS (GPIO output-enable control); firmware sets
# the data-ready pin as an output through the GPIO direction register.

# Address assignment: control slaves at fixed GP0 offsets (la_top then DMA
# register space); DMA masters map the whole DDR low range through HP0.
# Address assignment: control slaves at fixed GP0 offsets (la_top registers
# at 0x4000_0000, DMA register space at 0x4040_0000, SPI slave at
# 0x4080_0000, all 64K); the DMA masters map the full DDR low range (1G at 0)
# through HP0.
set gp0_space [get_bd_addr_spaces -of_objects [get_bd_intf_pins ps7/M_AXI_GP0]]
puts "GP0 address space: $gp0_space"
if {[catch {
  assign_bd_address -offset 0x40000000 -range 64K -target_address_space $gp0_space \
    [get_bd_addr_segs {la_top_0/s_axi/reg0}]
  assign_bd_address -offset 0x40400000 -range 64K -target_address_space $gp0_space \
    [get_bd_addr_segs {axi_dma_0/S_AXI_LITE/Reg}]
} msg]} {
  puts "ADDR ASSIGN GP0 FAIL: $msg"
}
if {[catch {
  assign_bd_address -offset 0x40800000 -range 64K -target_address_space $gp0_space \
    [get_bd_addr_segs {spi_probe/AXI_LITE/Reg}]
} msg]} {
  puts "ADDR ASSIGN SPI FAIL: $msg"
}
foreach dma_master {axi_dma_0/M_AXI_S2MM axi_dma_0/M_AXI_SG} {
  set space [get_bd_addr_spaces -of_objects [get_bd_intf_pins $dma_master]]
  if {[catch {
    assign_bd_address -offset 0x00000000 -range 1G -target_address_space $space \
      [get_bd_addr_segs {ps7/S_AXI_HP0/HP0_DDR_LOWOCM}]
  } msg]} {
    puts "ADDR ASSIGN $dma_master FAIL: $msg"
  }
}
puts "Assigned address segments:"
foreach seg [get_bd_addr_segs -quiet] {
  set off [get_property -quiet OFFSET $seg]
  set rng [get_property -quiet RANGE $seg]
  puts "SEG $seg offset=$off range=$rng"
}
validate_bd_design
save_bd_design

if {$stage eq "bd"} {
  puts "STAGE bd complete: block design validated and saved."
  catch {close_design}
  close_project
  return
}

# ---------------------------------------------------------------------------
# Generate BD outputs, wrapper, synthesis.
# ---------------------------------------------------------------------------
generate_target all [get_files system.bd]
set wrapper_file [make_wrapper -files [get_files system.bd] -top]
add_files -norecurse $wrapper_file
update_compile_order -fileset sources_1
set_property top system_wrapper [current_fileset]

launch_runs synth_1 -jobs 2
wait_on_run synth_1
set synth_status [get_property STATUS [get_runs synth_1]]
puts "SYNTH STATUS: $synth_status"
if {[string first "Complete!" $synth_status] < 0} {
  error "synthesis failed: $synth_status"
}

if {$stage eq "synth"} {
  puts "STAGE synth complete: synthesis passed."
  catch {close_design}
  close_project
  return
}

# ---------------------------------------------------------------------------
# Implementation, reports and XSA.
# ---------------------------------------------------------------------------
launch_runs impl_1 -to_step write_bitstream -jobs 2
wait_on_run impl_1
set impl_status [get_property STATUS [get_runs impl_1]]
puts "IMPL STATUS: $impl_status"
if {[string first "Complete!" $impl_status] < 0} {
  error "implementation failed: $impl_status"
}

open_run impl_1
report_timing_summary -delay_type min_max -max_paths 10 \
  -file [file join $release_dir timing_summary.rpt]
report_utilization -file [file join $release_dir utilization.rpt]
report_drc -file [file join $release_dir drc.rpt]
set failing [get_timing_paths -quiet -nworst 1 -max_paths 1000 -filter {SLACK < 0}]
if {[llength $failing] > 0} {
  puts "TIMING FAIL: [llength $failing] failing endpoints"
  report_timing -nworst 10 -sort_by slack -file [file join $release_dir failing_paths.rpt]
  catch {close_design}
  close_project
  error "timing closure failed: [llength $failing] failing endpoints"
}
puts "TIMING OK: zero failing endpoints"

write_hw_platform -force -fixed -include_bit -file [file join $release_dir logic_analyzer.xsa]
catch {close_design}
set bit_file [file join $proj_dir la.runs impl_1 system_wrapper.bit]
if {[file exists $bit_file]} {
  file copy -force $bit_file [file join $release_dir logic_analyzer.bit]
  puts "BIT copied to [file join $release_dir logic_analyzer.bit]"
} else {
  puts "WARNING: bitstream not found at $bit_file"
}
close_project
puts "P3.1 full flow complete: XSA and reports under $release_dir"
