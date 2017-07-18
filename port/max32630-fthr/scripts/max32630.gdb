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
	load build/max3263x.elf
	monitor reset halt
end

connect
settings
program
c
