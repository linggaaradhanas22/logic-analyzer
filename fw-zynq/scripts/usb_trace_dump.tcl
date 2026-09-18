# U03 USB trace dump over JTAG (read-only; the CPU keeps running).
#
# Usage:
#   xsdb usb_trace_dump.tcl HEAD_ADDR WORD_COUNT OUT_FILE
#
# HEAD_ADDR is the address of la_usb_trace_ring (resolved from the ELF by
# tools/usb_trace.py); WORD_COUNT covers the ring words only. The dump file
# contains one "HEAD <value>" line and one "WORDS <n> 0x........ ..." line per
# chunk. No console output is used, so the dump cannot perturb the USB link.
if {$argc < 3} {
  error "usage: xsdb usb_trace_dump.tcl HEAD_ADDR WORD_COUNT OUT_FILE"
}
set head_addr [lindex $argv 0]
set word_count [lindex $argv 1]
set out_file [lindex $argv 2]

connect
configparams force-mem-access 1
targets -set -nocase -filter {name =~ "*Cortex-A9*#0"} -timeout 10
set head [mrd -force -value $head_addr]
set fh [open $out_file w]
puts $fh [format "HEAD 0x%08X" $head]
set addr [expr {$head_addr + 4}]
set remaining $word_count
while {$remaining > 0} {
  set chunk [expr {$remaining > 64 ? 64 : $remaining}]
  set values [mrd -force $addr $chunk]
  puts -nonewline $fh "WORDS $chunk"
  foreach value $values {
    set clean [string trimright [string trim $value] ":"]
    if {[regexp {^[0-9A-Fa-f]+$} $clean]} {
      puts -nonewline $fh [format " 0x%08X" [expr "0x$clean"]]
    } else {
      puts -nonewline $fh " 0x00000000"
    }
  }
  puts $fh ""
  set addr [expr {$addr + $chunk * 4}]
  set remaining [expr {$remaining - $chunk}]
}
close $fh
configparams force-mem-access 0
disconnect
puts "USB-TRACE PASS: wrote $out_file"
