#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2025

import struct
import sys

baudrate_settings = {
	0x0252C014:  115200,
	0x0252C00A:  230400,
	0x05F75004:  921600,
	0x00005004: 1000000,
	0x04928002: 1500000,
	0x01128002: 1500000,
	0x00005002: 2000000,
	0x0000B001: 2500000,
	0x04928001: 3000000,
	0x052A6001: 3500000,
	0x00005001: 4000000,
}

def as_hex(data):
	return " ".join(f"{byte:02x}" for byte in data)

if len(sys.argv) == 1:
	print ('Dump Realtek Config file')
	print ('Copyright 2025, BlueKitchen GmbH')
	print ('')
	print ('Usage: ', sys.argv[0], 'config_file')
	exit(0)

def dump_file(infile):
	# Open the binary file in read mode
	with open(infile, "rb") as f:
		# verify magic
		magic, = struct.unpack("<I", f.read(4))
		if magic != 0x8723ab55:
			print("Magic not found")
			exit(1)

		print(f"Config: {infile}")
		data_len, = struct.unpack("<H", f.read(2))

		pos = 0
		while pos < data_len:
			offset,len, = struct.unpack("<HB", f.read(3))
			data = f.read(len)
			print(f"- {offset:04x} ({len:2} bytes): {as_hex(data)}")
			pos += 3 + len
			# pretty print
			if offset == 0x000c:
				if len >= 0x10 - offset:
					baudrate_setting, = struct.unpack("<I", data[0x00:0x04])
					if baudrate_setting in baudrate_settings:
						baudrate = baudrate_settings[baudrate_setting]
						print(f"    - baudrate    {baudrate:8}")
					else:
						print(f"    - baudrate    0x{baudrate_setting:08x}")
				if len >= 0x18 - offset:
					protocol, = struct.unpack("<I", data[0x14-offset:0x18-offset])
					hci_mode = "H5" if (protocol & 0x01) != 0 else "H4"
					print(f"    - hci_mode     {hci_mode}")
				if len >= 0x1c - offset:
					parity, = struct.unpack("<I", data[0x18-offset:0x1c-offset])
					parity_string = "disabled" if (parity & 0x01) == 0 else "even" if (parity & 0x01) != 0 else "odd"
					print(f"    - parity       {parity_string}")
					hw_fctrl = "enabled" if (parity & 0x01) != 0 else "disabled"
					print(f"    - flow control {hw_fctrl}")
		print("\n")

for config_file in sys.argv[1:]:
	dump_file(config_file)
