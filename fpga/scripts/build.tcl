# Native Vivado batch scaffold. Full RTL integration starts in P3.
if {$argc < 1} {
  error "usage: vivado -mode batch -source build.tcl -tclargs PROJECT_ROOT"
}

set project_root [file normalize [lindex $argv 0]]
set work_dir [file normalize [file join $project_root build vivado]]
set release_dir [file normalize [file join $project_root build release]]
file mkdir $work_dir
file mkdir $release_dir

puts "PROJECT_ROOT=$project_root"
puts "Vivado version=[version -short]"
set board_root [file join $project_root fpga board_files]
if {[file isdirectory $board_root]} {
  set_param board.repoPaths [list $board_root]
}
set discovered_parts [get_board_parts -quiet *zybo*]
if {[llength $discovered_parts] == 0} {
  puts "WARNING: no installed Zybo board part matched; use the pinned board lock and query installed parts."
} else {
  puts "Discovered board parts: $discovered_parts"
}

set top [file join $project_root fpga rtl la_top.sv]
if {![file exists $top]} {
  error "P3 blocked: RTL top is not present yet: $top"
}

create_project -force la $work_dir -part xc7z010clg400-1
add_files -norecurse $top
set_property top la_top [current_fileset]
update_compile_order -fileset sources_1
launch_runs synth_1 -jobs 1
wait_on_run synth_1
if {[get_property STATUS [get_runs synth_1]] ne " Design is clean" &&
    [get_property STATUS [get_runs synth_1]] ne "synth_design Complete!"} {
  puts "WARNING: inspect synthesis status in the generated Vivado log."
}
puts "P3 scaffold reached; constraints and block design are still required."
