#!/usr/bin/env python3

from btstack import btstack_server, btstack_client, event_factory
import sys

def packet_handler(packet):
    global btstack_client
    if isinstance(packet, event_factory.BTstackEventState):
        print("BTstack state: %u" % packet.get_state())
        if packet.get_state() == 2:
            print('BTstack up and running, starting scan')
            btstack_client.gap_le_scan_start()
    if isinstance(packet, event_factory.GAPEventAdvertisingReport):
        print(packet)

# check version
if sys.version_info < (3, 0):
    print('BTstack Server Client library, requires Python 3.x or higher.\n')
    sys.exit(10)

# Conrtrol for BTstack Server
btstack_server = btstack_server.BTstackServer()

# start BTstack Server from .dll
btstack_server.load()
# btstack_server.set_storage_path("/tmp")
btstack_server.run_tcp()

# Client for BTstack Server
btstack_client = btstack_client.BTstackClient()

# connect to slient, register for HCI packets and power up
ok = btstack_client.connect()
if ok:
    btstack_client.register_packet_handler(packet_handler)
    btstack_client.btstack_set_power_mode(1)
    btstack_client.run()
