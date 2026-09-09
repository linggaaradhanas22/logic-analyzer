# Native Vivado programming entry point. Programming is a [HUMAN] action.
if {$argc < 1} {
  error "usage: vivado -mode batch -source program.tcl -tclargs PROJECT_ROOT"
}
set project_root [file normalize [lindex $argv 0]]
set bitstream [file normalize [file join $project_root build release logic_analyzer.bit]]
if {![file exists $bitstream]} {
  error "program blocked: bitstream is missing: $bitstream"
}
puts "Opening hardware manager; confirm the intended Zybo target before programming."
open_hw_manager
connect_hw_server
set targets [get_hw_targets -quiet]
if {[llength $targets] == 0} {
  error "program blocked: no JTAG target found; keep FTDI attached and JP5 set to JTAG"
}
open_hw_target [lindex $targets 0]
set devices [get_hw_devices -quiet]
if {[llength $devices] == 0} {
  error "program blocked: no hardware device found"
}
# Zynq chains expose the ARM DAP (arm_dap_0) before the PL part; it is not
# programmable, so select the first device whose part looks like the FPGA.
set device ""
foreach d $devices {
  set part [get_property PART $d]
  if {[string match {xc7*} $part]} {
    set device $d
    puts "Selected programmable device $d ($part)"
    break
  }
}
if {$device eq ""} {
  error "program blocked: no programmable (xc7*) device in [join $devices {, }]"
}
set_property PROGRAM.FILE $bitstream $device
program_hw_devices $device
puts "Programming completed; verify the board LED and console with the human operator."
