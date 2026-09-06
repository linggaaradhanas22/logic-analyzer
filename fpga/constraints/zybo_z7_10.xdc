# P3.2 Zybo Z7-10 constraints for the RTOS logic analyzer.
#
# Header contract: docs/wiring.md
#   JC-1/2/3/4/7/8/9/10  -> ch_in[0..7]   (acquisition inputs, LVCMOS33)
#   JE-1/2/3/4/7/8/9/10  -> patgen_o[0..7] (pattern outputs, LVCMOS33)
#   JD-1 TX (out), JD-2 RX (in), JD-3 RTS_N (out), JD-4 CTS_N (in)
# Package pin sites come from the pinned board files
#   fpga/board_files/zybo-z7-10/A.0/part0_pins.xml.
#
# CDC note: ch_in are asynchronous digital inputs. Each channel feeds only
# the two-flop synchronizer in la_input_sync (ASYNC_REG attributes in RTL);
# the pads-to-first-stage path carries no timing requirement and is
# intentionally exempted below. Everything after ch_sync is synchronous to
# FCLK_CLK0 and timing-closed internally.

# ---- JC acquisition inputs (CH0..CH7) ------------------------------------
set_property -dict [list PACKAGE_PIN V15 IOSTANDARD LVCMOS33] [get_ports {ch_in[0]}]
set_property -dict [list PACKAGE_PIN W15 IOSTANDARD LVCMOS33] [get_ports {ch_in[1]}]
set_property -dict [list PACKAGE_PIN T11 IOSTANDARD LVCMOS33] [get_ports {ch_in[2]}]
set_property -dict [list PACKAGE_PIN T10 IOSTANDARD LVCMOS33] [get_ports {ch_in[3]}]
set_property -dict [list PACKAGE_PIN W14 IOSTANDARD LVCMOS33] [get_ports {ch_in[4]}]
set_property -dict [list PACKAGE_PIN Y14 IOSTANDARD LVCMOS33] [get_ports {ch_in[5]}]
set_property -dict [list PACKAGE_PIN T12 IOSTANDARD LVCMOS33] [get_ports {ch_in[6]}]
set_property -dict [list PACKAGE_PIN U12 IOSTANDARD LVCMOS33] [get_ports {ch_in[7]}]

# ---- JE pattern-generator outputs (PATGEN0..PATGEN7) ---------------------
set_property -dict [list PACKAGE_PIN V12 IOSTANDARD LVCMOS33] [get_ports {patgen_o[0]}]
set_property -dict [list PACKAGE_PIN W16 IOSTANDARD LVCMOS33] [get_ports {patgen_o[1]}]
set_property -dict [list PACKAGE_PIN J15 IOSTANDARD LVCMOS33] [get_ports {patgen_o[2]}]
set_property -dict [list PACKAGE_PIN H15 IOSTANDARD LVCMOS33] [get_ports {patgen_o[3]}]
set_property -dict [list PACKAGE_PIN V13 IOSTANDARD LVCMOS33] [get_ports {patgen_o[4]}]
set_property -dict [list PACKAGE_PIN U17 IOSTANDARD LVCMOS33] [get_ports {patgen_o[5]}]
set_property -dict [list PACKAGE_PIN T17 IOSTANDARD LVCMOS33] [get_ports {patgen_o[6]}]
set_property -dict [list PACKAGE_PIN Y17 IOSTANDARD LVCMOS33] [get_ports {patgen_o[7]}]

# ---- JD UART0 on EMIO (Zynq <-> ESP32 link) -------------------------------
set_property -dict [list PACKAGE_PIN T14 IOSTANDARD LVCMOS33] [get_ports uart0_tx]
set_property -dict [list PACKAGE_PIN T15 IOSTANDARD LVCMOS33] [get_ports uart0_rx]
set_property -dict [list PACKAGE_PIN P14 IOSTANDARD LVCMOS33] [get_ports uart0_rtsn]
set_property -dict [list PACKAGE_PIN R14 IOSTANDARD LVCMOS33] [get_ports uart0_ctsn]

# ---- Asynchronous-input timing exceptions ---------------------------------
# Acquisition pads are asynchronous with respect to FCLK_CLK0: their only
# consumer is the la_input_sync synchronizer chain (ASYNC_REG) described in
# the CDC note above, so no setup/hold contract is applied at the pads.
set_false_path -from [get_ports {ch_in[*]}]
