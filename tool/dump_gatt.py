#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2022

# parse PacketLogger and reconstruct GATT DB

import sys
import datetime
import struct

def as_hex(data):
	str_list = []
	for byte in data:
	    str_list.append("{0:02x} ".format(byte))
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

def uuid16_at_offset(data, offset):
	return "%04x" % struct.unpack_from("<H", data, offset)[0]

def uuid128_at_offset(data, offset):
	uuid128 = bytes(reversed(data[offset:offset+16]))
	return uuid128[0:4].hex() + "-" + uuid128[4:6].hex() + "-" + uuid128[6:8].hex() + "-" + uuid128[8:10].hex() + "-" + uuid128[10:].hex()

def handle_at_offset(data, offset):
	return struct.unpack_from("<H", data, offset)[0];

def bd_addr_at_offset(data, offset):
	peer_addr = reversed(data[8:8 + 6])
	return as_bd_addr(peer_addr)

class gatt_characteristic:
	def __init__(self, uuid, properties, characteristic_handle, value_handle):
		self.uuid = uuid
		self.properties = properties
		self.characteristic_handle = characteristic_handle
		self.value_handle = value_handle
	def report(self, prefix):
		print("%sUUID %-36s, Handle %04x, Properties %02x, Value Handle %04x" % (prefix, self.uuid, self.characteristic_handle, self.properties, self.value_handle))

class gatt_service:

	def __init__(self, uuid, start_handle, end_handle):
		self.uuid = uuid
		self.start_handle = start_handle
		self.end_handle = end_handle
		self.characteristics = []

	def report(self, prefix):
		print("%sUUID: %-36s, Start Handle %04x, End Handle %04x" % (prefix, self.uuid, self.start_handle, self.end_handle))
		print("  %sCharacteristics:" % prefix)
		for characteristic in self.characteristics:
			characteristic.report("   " + prefix)

class gatt_server:

	primary_services = []

	client_opcode = 0
	group_type = 0
	read_type = 0
	mtu = 23

	def __init__(self, bd_addr):
		self.bd_addr = bd_addr

	def service_for_handle(self, handle):
		for service in self.primary_services:
			if service.start_handle <= handle and handle <= service.end_handle:
				return service
		return None

	def handle_pdu(self, pdu):
		opcode = pdu[0]
		if opcode == 0x01:
			pass
		elif opcode == 0x02:
			# exchange mtu
			pass
		elif opcode == 0x03:
			# exchange mtu
			self.mtu = struct.unpack_from("<H", pdu, 1)[0]
		elif opcode == 0x08:
			# read by type request
			if len(pdu) == 7:
				(_,_,self.read_type) = struct.unpack_from("<HHH", pdu, 1)
		elif opcode == 0x09:
			# read by type response
			if self.read_type == 0x2803:
				item_len = pdu[1]
				pos = 2
				while pos < len(pdu):
					(characteristic_handle, properties, value_handle) = struct.unpack_from("<HBH", pdu, pos)
					if item_len == 11:
						uuid = uuid16_at_offset(pdu, pos + 5)
					elif item_len == 21:
						uuid = uuid128_at_offset(pdu, pos + 5)
					service = self.service_for_handle(characteristic_handle)
					if service:
						service.characteristics.append(gatt_characteristic(uuid, properties, characteristic_handle, value_handle))
					pos += item_len
		elif opcode == 0x10:
			# read by group type request
			if len(pdu) == 7:
				(_,_,self.group_type) = struct.unpack_from("<HHH", pdu, 1)
		elif opcode == 0x11:
			# read by group type response
			item_len = pdu[1]
			pos = 2
			while pos < len(pdu):
				(start, end) = struct.unpack_from("<HH", pdu, pos)
				if self.group_type == 0x2800:
					# primary service
					if item_len == 6:
						uuid = uuid16_at_offset(pdu, pos+4)
					elif item_len == 20:
						uuid = uuid128_at_offset(pdu, pos+4)
					self.primary_services.append(gatt_service(uuid, start, end))
				pos += item_len
		else:
			# print(self.bd_addr, "ATT PDU:", as_hex(pdu))
			pass

	def report(self):
		print("GATT Server on", self.bd_addr)
		print("- MTU", self.mtu)
		print("- Primary Services:")
		for service in self.primary_services:
			service.report("  - ")

class l2cap_reassembler:

	payload_data = bytes()
	payload_len = 0
	channel = 0;

	def handle_acl(self, pb, data):
		if pb in [0, 2]:
			(self.payload_len, self.channel) = struct.unpack("<HH", data[0:4])
			self.payload_data = data[4:]
		if pb == 0x01:
			self.payload_data += data[4:]

	def l2cap_complete(self):
		return len(self.payload_data) == self.payload_len

	def l2cap_packet(self):
		return (self.channel, self.payload_data)

class hci_connection:

	l2cap_in = l2cap_reassembler()
	l2cap_out = l2cap_reassembler()

	def __init__(self, bd_addr, con_handle):
		self.bd_addr = bd_addr
		self.con_handle = con_handle;
		self.remote_gatt_server = gatt_server(bd_addr)

	def handle_att_pdu(self, direction_in, pdu):
		opcode = pdu[0]
		remote_server = ((opcode & 1) == 1) == direction_in
		if (remote_server):
			self.remote_gatt_server.handle_pdu(pdu)
		else:
			local_gatt_server.handle_pdu(pdu)

	def handle_acl(self, direction_in, pb, data):
		if direction_in:
			self.l2cap_in.handle_acl(pb, data)
			if self.l2cap_in.l2cap_complete():
				(channel, l2cap_data) = self.l2cap_in.l2cap_packet()
				if channel == 0x004:
					self.handle_att_pdu(direction_in, l2cap_data)
		else:
			self.l2cap_out.handle_acl(pb, data)
			if self.l2cap_out.l2cap_complete():
				(channel, l2cap_data) = self.l2cap_out.l2cap_packet()
				if channel == 0x004:
					self.handle_att_pdu(direction_in, l2cap_data)

def connection_for_handle(con_handle):
	if con_handle in connections:
		return connections[con_handle]
	else:
		return None

def handle_cmd(packet):
	pass

def handle_evt(event):
	if event[0] == 0x05:
		# Disconnection Complete
		con_handle = handle_at_offset(event, 3)
		print("Disconnection Complete: handle 0x%04x" % con_handle)
		connection = connections.pop(con_handle, None)
		connection.remote_gatt_server.report()

	if event[0] == 0x3e:
		if event[2] == 0x01:
			# LE Connection Complete
			con_handle = handle_at_offset(event, 4);
			peer_addr = bd_addr_at_offset(event, 8)
			connection = hci_connection(peer_addr, con_handle)
			connections[con_handle] = connection
			print("LE Connection Complete: %s handle 0x%04x" % (peer_addr, con_handle))

def handle_acl(data, direction_in):
	(header, hci_len) = struct.unpack("<HH", data[0:4])
	pb = (header >> 12) & 0x03
	con_handle = header & 0x0FFF
	connection_for_handle(con_handle).handle_acl(direction_in, pb, data[4:])

# globals
connections = {}
local_gatt_server = gatt_server("00:00:00:00:00:00")

if len(sys.argv) == 1:
	print ('Reconstruct GATT interactions from PacketLogger trace file')
	print ('Copyright 2022, BlueKitchen GmbH')
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
			packet_len = entry_len - 9;
			if (packet_len > 66000):
				print ("Error parsing pklg at offset %u (%x)." % (pos, pos))
				break
			packet  = fin.read(packet_len)
			pos     = pos + 4 + entry_len
			if type == 0x00:
				handle_cmd(packet)
			elif type == 0x01:
				handle_evt(packet)
			elif type == 0x02:
				handle_acl(packet, False)
			elif type == 0x03:
				handle_acl(packet, True)

	except TypeError as e:
		print(e)
		print ("Error parsing pklg at offset %u (%x)." % (pos, pos))

for connection in connections:
	connection.remote_gatt_server.report()
