# program_fpga.tcl
# Usage:
#   vivado -mode batch -source program_fpga.tcl -tclargs /path/to/file.bit

if { [llength $argv] < 1 } {
    puts "ERROR: No bitfile passed. Usage: vivado -mode batch -source program_fpga.tcl -tclargs <bitfile>"
    exit 1
}

set bitfile [lindex $argv 0]
puts "Programming FPGA with bitfile: $bitfile"

open_hw
connect_hw_server
open_hw_target

set hw_device [lindex [get_hw_devices] 0]
current_hw_device $hw_device

refresh_hw_device -update_hw_probes false $hw_device

set_property PROGRAM.FILE $bitfile $hw_device
program_hw_devices $hw_device

puts "Done."
close_hw
exit