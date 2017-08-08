define connect
	set trace-commands on
	set remotetimeout 1000000000
	set logging on
	target remote localhost:3333
end

define reset
	monitor reset halt
	c
end

define settings
	# set unlimited string size while print
	set print elements 0
end

define program
	monitor reset halt
	load build/max32630-fthr-mbed.elf
	monitor reset halt
end

connect
settings
program
b ../mbed-os/platform/retarget.cpp:251
b _write
b src/btstack_port.cpp:415
b ../src/spp_counter.c:262
b mnt/sda1/work/sandbox/mbed_btstack_2/btstack/src/hci.c:2596
c

