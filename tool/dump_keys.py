#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2022

# parse PacketLogger and dump encryption keys

import sys
import datetime
import struct


def as_hex(data):
	str_list = []
	for byte in data:
		str_list.append("{0:02x} ".format(byte))
	return ''.join(str_list)


def as_key(data):
	str_list = []
	for byte in data:
		str_list.append("{0:02x}".format(byte))
	return ''.join(str_list)


def as_bd_addr(data):
	str_list = []
	for byte in data:
		str_list.append("{0:02x}".format(byte))
	return ':'.join(str_list)


def read_header(f):
	bytes_read = f.read(13)
	if bytes_read:
		return struct.unpack(">IIIB", bytes_read)
	else:
		return (-1, 0, 0, 0)


def handle_at_offset(data, offset):
	return struct.unpack_from("<H", data, offset)[0];


def bd_addr_at_offset(data, offset):
	peer_addr = reversed(data[offset:offset + 6])
	return as_bd_addr(peer_addr)


class hci_connection:

	def __init__(self, bd_addr, con_handle):
		self.bd_addr = bd_addr
		self.con_handle = con_handle;


def connection_for_handle(con_handle):
	if con_handle in connections:
		return connections[con_handle]
	else:
		return None


def handle_cmd(packet):
	opcode = struct.unpack_from("<H", packet, 0)[0];
	if opcode == 0x201a:
		# LE Long Term Key Request Reply
		con_handle = handle_at_offset(packet, 3)
		conn = connection_for_handle(con_handle)
		print("LE LTK for %s - %s" % (conn.bd_addr, as_key(reversed(packet[5:5+16]))))
	elif opcode == 0x2019:
		# LE Enable Encryption command
		con_handle = handle_at_offset(packet, 3)
		conn = connection_for_handle(con_handle)
		print("LE LTK for %s - %s" % (conn.bd_addr, as_key(reversed(packet[15:15+16]))))
	elif opcode == 0x040b:
		# Link Key Request Reply
		bd_addr = bd_addr_at_offset(packet, 3)
		print("Link Key for %s - %s" % (bd_addr, as_key(reversed(packet[9:9+16]))))

def handle_evt(event):
	if event[0] == 0x05:
		# Disconnection Complete
		con_handle = handle_at_offset(event, 3)
		print("Disconnection Complete: handle 0x%04x" % con_handle)
		connections.pop(con_handle, None)
	elif event[0] == 0x18:
		# Link Key Notification
		bd_addr = bd_addr_at_offset(packet, 2)
		print("Link Key for %s - %s" % (bd_addr, as_key(reversed(packet[8:8+16]))))
	elif event[0] == 0x3e:
		if event[2] == 0x01:
			# LE Connection Complete
			con_handle = handle_at_offset(event, 4);
			peer_addr = bd_addr_at_offset(event, 8)
			connection = hci_connection(peer_addr, con_handle)
			connections[con_handle] = connection
			print("LE Connection Complete: %s handle 0x%04x" % (peer_addr, con_handle))


# globals
connections = {}

if len(sys.argv) == 1:
	print ('Dump encryptiong keys from PacketLogger trace file')
	print ('Copyright 2023, BlueKitchen GmbH')
	print ('')
	print ('Usage: ', sys.argv[0], 'hci_dump.pklg')
	exit(0)

infile = sys.argv[1]

with open (infile, 'rb') as fin:
	pos = 0
	try:
		while True:
			(entry_len, ts_sec, ts_usec, type) = read_header(fin)
			if entry_len < 0:
				break
			packet_len = entry_len - 9
			if packet_len > 66000:
				print ("Error parsing pklg at offset %u (%x)." % (pos, pos))
				break
			packet = fin.read(packet_len)
			pos = pos + 4 + entry_len
			if type == 0x00:
				handle_cmd(packet)
			elif type == 0x01:
				handle_evt(packet)
			elif type == 0x02:
				pass
			elif type == 0x03:
				pass

	except TypeError as e:
		print(e)
		print ("Error parsing pklg at offset %u (%x)." % (pos, pos))
