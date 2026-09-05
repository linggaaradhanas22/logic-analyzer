# Print installed Zybo board parts without assuming a board-file version.
if {$argc > 0} {
  set project_root [file normalize [lindex $argv 0]]
  set board_root [file join $project_root fpga board_files]
  if {[file isdirectory $board_root]} {
    set_param board.repoPaths [list $board_root]
  }
}
puts "Vivado version=[version -short]"
set parts [get_board_parts -quiet *zybo*]
if {[llength $parts] == 0} {
  puts "NO_ZYBO_BOARD_PART"
  exit 1
}
foreach part $parts {
  puts "BOARD_PART=$part"
}
