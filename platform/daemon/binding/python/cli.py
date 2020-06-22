#!/usr/bin/env python3

from btstack import btstack_client, event_factory, btstack_types
#import btstack.command_builder as cb
import sys
import time
import struct
from enum import Enum

class STATE(Enum):
	w4_btstack_working = 0
	w4_query_result = 1
	w4_connected = 2
	active = 3

btstack = None
state = -1
testHandle = -1

remote = btstack_types.BD_ADDR("84:CF:BF:8D:D8:F7")
rfcommServiceUUID = 0x1002
btIncomingChannelNr = 3

rfcommChannelID = 0
mtu = 0

services = []
counter = -1

def service_search_pattern(uuid):
	return struct.pack("BBBBB", 0x35, 0x03, 0x19, (uuid >> 8), (uuid & 0xff))

def packet_handler(packet):
	global state
	global btstack_client
	if isinstance(packet, event_factory.HCIEventDisconnectionComplete):
		event = packet
		testHandle = event.get_connection_handle()
		print("Received disconnect, status" + str(event.get_status()) + ", handle" + str(testHandle))
		#return

	# this is a switch statement
	if state == STATE.w4_btstack_working:
		if isinstance(packet, event_factory.BTstackEventState):
			event = packet
			if event.get_state() == 2:
				print("BTstack working. Start SDP inquiry.")
				state = STATE.w4_query_result
				serviceSearchPattern = service_search_pattern(rfcommServiceUUID)
				btstack_client.sdp_client_query_rfcomm_services(remote, serviceSearchPattern)
	elif state == STATE.w4_query_result:
		if isinstance(packet, event_factory.SDPEventQueryRFCOMMService):
			service = packet
			services.append(service.get_rfcomm_channel())
		if isinstance(packet, event_factory.SDPEventQueryComplete):
			for channel_nr in services:
				print("Found rfcomm channel nr: " + str(channel_nr))
				if channel_nr == btIncomingChannelNr:
					state = STATE.w4_connected
					print("Connect to channel nr " + str(channel_nr))
					btstack_client.rfcomm_create_channel(remote, 3)
	elif state == STATE.w4_connected:
		if isinstance(packet, event_factory.RFCOMMEventChannelOpened):
			e = packet
			print("RFCOMMEventChannelOpened with status " + str(e.get_status()))
			if e.get_status():
				print("RFCOMM channel open failed, status " + str(e.get_status()))
			else:
				state = STATE.active
				rfcommChannelID = e.get_rfcomm_cid()
				mtu = e.get_max_frame_size()
				print("RFCOMM channel open succeeded. New RFCOMM Channel ID %s, max frame size %s" % (str(rfcommChannelID), str(mtu)))
				counter = 0
				while state == STATE.active:
					time.sleep(1)
					data = ("BTstack SPP Counter %d\n" % counter).encode()
					print("would send now")
					#btstack_client.rfcomm_send_data(rfcommChannelID, data)
	elif state == STATE.active:
		if isinstance(packet, event_factory.RFCOMM_Data_Packet):
			print("Received RFCOMM data packet: " + packet.toString())

# check version
if sys.version_info < (3, 0):
	print('BTstack Server Client library, requires Python 3.x or higher.\n')
	sys.exit(10)

# Client for BTstack Server
btstack_client = btstack_client.BTstackClient()

# connect to slient, register for HCI packets and power up
ok = btstack_client.connect()
if ok:
	btstack_client.register_packet_handler(packet_handler)
	state = STATE.w4_btstack_working
	btstack_client.btstack_set_power_mode(1)
	btstack_client.run()

