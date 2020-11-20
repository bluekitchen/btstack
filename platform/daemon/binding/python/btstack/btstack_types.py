#!/usr/bin/env python3

import struct

BLUETOOTH_BASE_UUID = bytes ([ 0x00, 0x00, 0x00, 0x00,   0x00, 0x00,  0x10, 0x00,   0x80, 0x00,  0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB ]);

def hex_string(bytes):
    return " ".join([('%02x' % a) for a in bytes])

def unpack24(data):
    (upper, ) = struct.unpack("<H", data[1:])
    return data[0] | (upper << 8)

class BD_ADDR(object):
    # addr stored in big endian
    def __init__(self, addr):
        if isinstance(addr, str):
            parts = addr.split(':')
            if len(parts) != 6:
                return
            # list comprehension
            self.addr = bytes([int(a,16) for a in parts])
        elif isinstance(addr, bytes):
            self.addr = addr
        elif isinstance(addr, bytearray):
            self.addr = addr
        else:
            print('cannot use for bdaddr, len %u' % len(addr))
            print(addr)

    def get_bytes(self):
        data = bytearray(self.addr)
        data.reverse()
        return data

    def __repr__(self):
        return ":".join([('%02x' % a) for a in self.addr])

class BT_UUID(object):
    # uuid stored in big endian

    def __init__(self, uuid):
        global BLUETOOTH_BASE_UUID
        if isinstance(uuid,bytes):
            self.uuid = uuid
        if isinstance(uuid,int):
            self.uuid = struct.pack(">I", uuid) + BLUETOOTH_BASE_UUID[4:]
        if isinstance(uuid,str):
            parts = uuid.split('-')
            if len(parts) != 5:
                return
            # list comprehension
            self.uuid = bytes([int(a,16) for a in uuid.replace('-','')])

    def get_uuid32(self):
        global BLUETOOTH_BASE_UUID
        result = 0
        if self.uuid[4:] == BLUETOOTH_BASE_UUID[4:]:
            (result,) = struct.unpack(">I", self.uuid[:4])
        return result 

    def get_bytes(self):
        data = bytearray(self.uuid)
        data.reverse()
        return data

    def __repr__(self):
        return "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x" % (
                self.uuid[0], self.uuid[1], self.uuid[2], self.uuid[3],   self.uuid[4],  self.uuid[5],  self.uuid[6],  self.uuid[7],
                self.uuid[8], self.uuid[9], self.uuid[10], self.uuid[11], self.uuid[12], self.uuid[13], self.uuid[14], self.uuid[15]);


class GATTCharacteristic(object):

    # uint16_t start_handle;
    # uint16_t value_handle;
    # uint16_t end_handle;
    # uint16_t properties;
    # uint8_t  uuid128[16];

    def __init__(self, data):
        self.data = data

    def get_bytes(self):
        return self.data
    
    def get_start_handle(self):
        (result, ) = struct.unpack('<H', self.data[0:2])
        return result

    def get_value_handle(self):
        (result, ) = struct.unpack('<H', self.data[2:4])
        return result

    def get_ned_handle(self):
        (result, ) = struct.unpack('<H', self.data[4:6])
        return result

    def get_properties(self):
        (result, ) = struct.unpack('<H', self.data[6:8])
        return result
    
    def get_uuid(self):
        return BT_UUID(self.data[8:])

    def __repr__(self):
        return "GATTCharacteristic [start_handle={start_handle}, value_handle={value_handle}, end_handle={end_handle}, get_uuid={uuid}]".format(
            start_handle=self.get_start_handle(), value_handle=self.get_value_handle(), end_handle=self.get_end_handle(), uuid=self.get_uuid())



class GATTCharacteristicDescriptor(object):

    # uint16_t handle;
    # uint8_t  uuid128[16];
    
    def __init__(self, data):
        self.data = data

    def get_bytes(self):
        return self.data
    
    def get_handle(self):
        (result, ) = struct.unpack('<H', self.data[0:2])
        return result
    
    def get_uuid(self):
        return BT_UUID(self.data[2:])

    def __repr__(self):
        return "GATTCharacteristicDescriptor [handle={handle}, get_uuid={uuid}]".format(
            handle=self.get_handle(), uuid=self.get_uuid())


class GATTService(object):

    # uint16_t start_group_handle;
    # uint16_t end_group_handle;
    # uint8_t  uuid128[16];

    def __init__(self, data):
        self.data = data

    def get_bytes(self):
        return self.data
    
    def get_start_group_handle(self):
        (result, ) = struct.unpack('<H', self.data[0:2])
        return result
    
    def get_end_group_handle(self):
        (result, ) = struct.unpack('<H', self.data[2:4])
        return result

    def get_uuid(self):
        return BT_UUID(self.data[4:])

    def __repr__(self):
        return "GattService [start_group_handle={start_group_handle}, [end_group_handle={end_group_handle}, get_uuid={uuid}]".format(
            start_group_handle=self.get_start_group_handle(), end_group_handle=self.get_end_group_handle(), uuid=self.get_uuid())


class Packet(object):

    HCI_COMMAND_PACKET = 1;
    HCI_EVENT_PACKET   = 4;
    L2CAP_DATA_PACKET  = 6;
    RFCOMM_DATA_PACKET = 7;

    def __init__(self, packet_type, channel, payload):
        self.packet_type = packet_type
        self.channel = channel
        self.payload = payload
        
    def get_packet_type(self):
        return self.packet_type

    def get_channel(self):
        return self.channel

    def get_payload(self):
        return self.payload

    def __repr__(self):
        return "Packet type {packet_type}, channel {channel}, payload {payload}".format(packet_type={self.get_packet_type()},
            channel={self.get_channel()}, payload=hex_string(self.get_payload()))


class Event(Packet):

    def __init__(self, payload):
        self.packet_type = Packet.HCI_COMMAND_PACKET
        self.channel = 0
        self.payload = payload

    def get_event_type(self):
        return self.payload[0]

    def __repr__(self):
        return "Event type {event_type}, payload {payload}".format(event_type={self.get_event_type()},
         payload=hex_string(self.get_payload()))


class BTstackEventState(Packet):

    def __init__(self, payload):
        self.packet_type = Packet.HCI_COMMAND_PACKET
        self.channel = 0
        self.payload = payload

    def get_state(self):
        return self.payload[2]

