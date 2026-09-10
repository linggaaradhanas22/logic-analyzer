# Native XSDB development loader for Zybo Z7-10.
# Programs the PL, runs the generated FSBL to initialize the PS/DDR, then
# downloads and starts the FreeRTOS application on Cortex-A9 #0.
if {$argc < 1} {
  error "usage: xsdb jtag_load.tcl PROJECT_ROOT"
}

set project_root [file normalize [lindex $argv 0]]
set bitstream [file normalize [file join $project_root build release logic_analyzer.bit]]
set fsbl [file normalize [file join $project_root build vitis ws lap zynq_fsbl build fsbl.elf]]
set application [file normalize [file join $project_root build vitis ws la_app build la_app.elf]]

foreach {label artifact} [list bitstream $bitstream FSBL $fsbl application $application] {
  if {![file exists $artifact]} {
    error "jtag-zynq blocked: $label is missing: $artifact"
  }
}

proc recover_dap {} {
  if {[catch {targets -set -nocase -filter {name =~ "APU*"} -timeout 3}]} {
    targets -set -nocase -filter {name =~ "DAP*"} -timeout 3
  }
  rst -dap
  after 1000
}

proc halt_a9_cores {} {
  foreach index {0 1} {
    targets -set -nocase \
      -filter [format {name =~ "*Cortex-A9*#%d"} $index] -timeout 3
    catch {stop}
  }
}

puts "Connecting to the local hardware server..."
connect
configparams force-mem-access 1
recover_dap
puts "XSDB targets:\n[targets]"

targets -set -nocase -filter {name =~ "APU*"} -timeout 10
set boot_mode [expr {[mrd -value 0xF800025C] & 0x7}]
puts "Latched boot mode: $boot_mode (0 = JTAG)"
if {$boot_mode != 0} {
  error "jtag-zynq blocked: JP5 is not latched in JTAG mode; set JP5=JTAG and power-cycle the board"
}
puts "Resetting the Zynq processing system through JTAG SRST..."
halt_a9_cores
targets -set -nocase -filter {name =~ "APU*"} -timeout 10
rst -srst
after 5000
recover_dap

targets -set -nocase -filter {name =~ "xc7z010*"} -timeout 10
puts "Programming PL: $bitstream"
fpga -file $bitstream

targets -set -nocase -filter {name =~ "*Cortex-A9*#0"} -timeout 10
rst -processor -clear-registers
puts [format "PS SLCR IDCODE: 0x%08X" [mrd -value 0xF8000530]]
puts [format "OCM probe: 0x%08X" [mrd -value 0x00000000]]
puts [format "DevCfg INT_STS: 0x%08X" [mrd -value 0xF800700C]]
puts "Downloading FSBL: $fsbl"
dow -force $fsbl
set fsbl_breakpoint [bpadd -addr &FsblHandoffJtagExit]
puts "Running FSBL to the JTAG handoff..."
con -block -timeout 30

set level_shifters [expr {[mrd -value 0xF8000900] & 0xF}]
puts [format "PS/PL level shifters: 0x%X" $level_shifters]
if {$level_shifters != 0xF} {
  error "jtag-zynq blocked: FSBL did not enable the PS/PL level shifters"
}
bpremove $fsbl_breakpoint

rst -processor -clear-registers
puts "Downloading application: $application"
dow -force $application
puts "Starting FreeRTOS on Cortex-A9 #0..."
con
configparams force-mem-access 0
disconnect
puts "JTAG-ZYNQ PASS: PL programmed and application started"
