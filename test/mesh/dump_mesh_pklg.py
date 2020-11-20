#!/usr/bin/env python3
# BlueKitchen GmbH (c) 2019

# primitive dump for PacketLogger format

# APPLE PacketLogger
# typedef struct {
#   uint32_t    len;
#   uint32_t    ts_sec;
#   uint32_t    ts_usec;
#   uint8_t     type;   // 0xfc for note
# }

#define BLUETOOTH_DATA_TYPE_PB_ADV                                             0x29 // PB-ADV
#define BLUETOOTH_DATA_TYPE_MESH_MESSAGE                                       0x2A // Mesh Message
#define BLUETOOTH_DATA_TYPE_MESH_BEACON                                        0x2B // Mesh Beacon

import re
import sys
import time
import datetime
import struct
from mesh_crypto import *

# state
netkeys = {}
appkeys = {}
devkey = b''
ivi = b'\x00'
segmented_messages = {}

access_messages = {
# Foundation
0x0000: 'Config Appkey Add',
0x0001: 'Config Appkey Update',
0x0002: 'Config Composition Data Status',
0x0003: 'Config Model Publication Set',
0x0004: 'Config Health Current Status',
0x0005: 'Config Health Fault Status',
0x0006: 'Config Heartbeat Publication Status',
0x8000: 'Config Appkey Delete',
0x8001: 'Config Appkey Get',
0x8002: 'Config Appkey List',
0x8003: 'Config Appkey Status',
0x8004: 'Config Health Attention Get',
0x8005: 'Config Health Attention Set',
0x8006: 'Config Health Attention Set Unacknowledged',
0x8007: 'Config Health Attention Status',
0x8008: 'Config Composition Data Get',
0x8009: 'Config Beacon Get',
0x800a: 'Config Beacon Set',
0x800b: 'Config Beacon Status',
0x800c: 'Config Default Ttl Get',
0x800d: 'Config Default Ttl Set',
0x800e: 'Config Default Ttl Status',
0x800f: 'Config Friend Get',
0x8010: 'Config Friend Set',
0x8011: 'Config Friend Status',
0x8012: 'Config Gatt Proxy Get',
0x8013: 'Config Gatt Proxy Set',
0x8014: 'Config Gatt Proxy Status',
0x8015: 'Config Key Refresh Phase Get',
0x8016: 'Config Key Refresh Phase Set',
0x8017: 'Config Key Refresh Phase Status',
0x8018: 'Config Model Publication Get',
0x8019: 'Config Model Publication Status',
0x801a: 'Config Model Publication Virtual Address Set',
0x801b: 'Config Model Subscription Add',
0x801c: 'Config Model Subscription Delete',
0x801d: 'Config Model Subscription Delete All',
0x801e: 'Config Model Subscription Overwrite',
0x801f: 'Config Model Subscription Status',
0x8020: 'Config Model Subscription Virtual Address Add',
0x8021: 'Config Model Subscription Virtual Address Delete',
0x8022: 'Config Model Subscription Virtual Address Overwrite',
0x8023: 'Config Network Transmit Get',
0x8024: 'Config Network Transmit Set',
0x8025: 'Config Network Transmit Status',
0x8026: 'Config Relay Get',
0x8027: 'Config Relay Set',
0x8028: 'Config Relay Status',
0x8029: 'Config Sig Model Subscription Get',
0x802a: 'Config Sig Model Subscription List',
0x802b: 'Config Vendor Model Subscription Get',
0x802c: 'Config Vendor Model Subscription List',
0x802d: 'Config Low Power Node Poll Timeout Get',
0x802e: 'Config Low Power Node Poll Timeout Status',
0x802f: 'Config Health Fault Clear',
0x8030: 'Config Health Fault Clear Unacknowledged',
0x8031: 'Config Health Fault Get',
0x8032: 'Config Health Fault Test',
0x8033: 'Config Health Fault Test Unacknowledged',
0x8034: 'Config Health Period Get',
0x8035: 'Config Health Period Set',
0x8036: 'Config Health Period Set Unacknowledged',
0x8037: 'Config Health Period Status',
0x8038: 'Config Heartbeat Publication Get',
0x8039: 'Config Heartbeat Publication Set',
0x803a: 'Config Heartbeat Subscription Get',
0x803b: 'Config Heartbeat Subscription Set',
0x803c: 'Config Heartbeat Subscription Status',
0x803d: 'Config Model App Bind',
0x803e: 'Config Model App Status',
0x803f: 'Config Model App Unbind',
0x8040: 'Config Netkey Add',
0x8041: 'Config Netkey Delete',
0x8042: 'Config Netkey Get',
0x8043: 'Config Netkey List',
0x8044: 'Config Netkey Status',
0x8045: 'Config Netkey Update',
0x8046: 'Config Node Identity Get',
0x8047: 'Config Node Identity Set',
0x8048: 'Config Node Identity Status',
0x8049: 'Config Node Reset',
0x804a: 'Config Node Reset Status',
0x804b: 'Config Sig Model App Get',
0x804c: 'Config Sig Model App List',
0x804d: 'Config Vendor Model App Get',
0x804e: 'Config Vendor Model App List',    
# Generic
0x8201: 'Generic On Off Get',
0x8202: 'Generic On Off Set',
0x8203: 'Generic On Off Set Unacknowledged',
0x8204: 'Generic On Off Status',
0x8205: 'Generic Level Get',
0x8206: 'Generic Level Set',
0x8207: 'Generic Level Set Unacknowledged',
0x8208: 'Generic Level Status',
0x8209: 'Generic Delta Set',
0x820A: 'Generic Delta Set Unacknowledged',
0x820B: 'Generic Move Set',
0x820C: 'Generic Move Set Unacknowledged',
}

# quick hack to convert Access message opcode + names
tmp_messages =[ 
# 'GENERIC_ON_OFF_GET                     :0x8201',
]
for message in tmp_messages:
    parts = message.split(':')
    print ("%s: '%s'," % (parts[1].strip(),parts[0].strip().title().replace('_',' ')))

# helpers
def read_net_32_from_file(f):
    data = f.read(4)
    if len(data) < 4:
        return -1
    return struct.unpack('>I', data)[0]

def as_hex(data):
    return ''.join(["{0:02x} ".format(byte) for byte in data])

def as_big_endian32(value):
    return struct.pack('>I', value)

def read_net_16(data):
    return struct.unpack('>H', data)[0]

def read_net_24(data):
    return data[0] << 16 | struct.unpack('>H', data[1:3])[0]

def read_net_32(data):
    return struct.unpack('>I', data)[0]

# log engine - simple pretty printer
max_indent = 10
def log_pdu(pdu, indent = 0, in_hide_properties = []):
    hide_properties = list(in_hide_properties)
    spaces = '    ' * indent
    print(spaces + "%-20s %s" % (pdu.type, pdu.summary))
    if indent >= max_indent:
        return
    for property in pdu.properties:
        if property.key in hide_properties:
            continue
        if isinstance( property.value, int):
            print (spaces + "|%20s: 0x%x (%u)" % (property.key, property.value, property.value))
        elif isinstance( property.value, bytes):
            print (spaces + "|%20s: %s" % (property.key, as_hex(property.value)))
        else:
            print (spaces + "|%20s: %s" % (property.key, str(property.value)))
        hide_properties.append(property.key)
    print (spaces + '|           data: ' + as_hex(pdu.data))
    print (spaces + '----')
    for origin in pdu.origins:
        log_pdu(origin, indent + 1, hide_properties)

# classes

class network_key(object):
    def __init__(self, index, netkey):
        self.index = index
        self.netkey = netkey
        (self.nid, self.encryption_key, self.privacy_key) = k2(netkey, b'\x00')

    def __repr__(self):
        return ("NetKey-%04x %s: NID %02x Encryption %s Privacy %s" % (self.index, self.netkey.hex(), self.nid, self.encryption_key.hex(), self.privacy_key.hex()))

class application_key(object):
    def __init__(self, index, appkey):
        self.index = index
        self.appkey = appkey
        self.aid = k4(self.appkey)

    def __repr__(self):
        return ("AppKey-%04x %s: AID %02x" % (self.index, self.appkey.hex(), self.aid))

class property(object):
    def __init__(self, key, value):
        self.key = key
        self.value = value

class layer_pdu(object):
    def __init__(self, pdu_type, pdu_data):
        self.summary = ''
        self.src = None
        self.dst = None
        self.type = pdu_type
        self.data = pdu_data
        self.origins = []
        self.properties = []

    def add_property(self, key, value):
        self.properties.append(property(key, value))

class network_pdu(layer_pdu):
    def __init__(self, pdu_data):
        super().__init__("Network(unencrpyted)", pdu_data)

        # parse pdu
        self.ivi = (self.data[1] & 0x80) >> 7
        self.nid = self.data[0] & 0x7f
        self.ctl = (self.data[1] & 0x80) == 0x80
        self.ttl = self.data[1] & 0x7f
        self.seq = read_net_24(self.data[2:5])
        self.src = read_net_16(self.data[5:7])
        self.dst = read_net_16(self.data[7:9])
        self.lower_transport = self.data[9:]

        # set properties
        self.add_property('ivi', self.ivi)
        self.add_property('nid', self.nid)
        self.add_property('ctl', self.ctl)
        self.add_property('ttl', self.ttl)
        self.add_property('seq', self.seq)
        self.add_property('src', self.src)
        self.add_property('dst', self.dst)
        self.add_property('lower_transport', self.lower_transport)

class lower_transport_pdu(layer_pdu):
    def __init__(self, network_pdu):
        super().__init__('Lower Transport', network_pdu.lower_transport)

        # inherit properties
        self.ctl = network_pdu.ctl
        self.seq = network_pdu.seq
        self.src = network_pdu.src
        self.dst = network_pdu.dst
        self.add_property('ctl', self.ctl)
        self.add_property('seq', self.seq)
        self.add_property('src', self.src)
        self.add_property('dst', self.dst)

        # parse pdu and set propoerties
        self.seg = (self.data[0] & 0x80) == 0x80
        self.add_property('seg', self.seg)
        self.szmic = False
        if self.ctl:
            self.opcode = self.data[0] & 0x7f
            self.add_property('opcode', self.opcode)
        else:
            self.aid = self.data[0] & 0x3f 
            self.add_property('aid', self.aid)
            self.akf = self.data[0] & 0x40 == 0x040
            self.add_property('akf', self.akf)
        if self.seg:
            if not self.ctl:
                self.szmic = self.data[1] & 0x80 == 0x80
                self.add_property('szmic', self.szmic)
            temp_12 = struct.unpack('>H', self.data[1:3])[0]
            self.seq_zero = (temp_12 >> 2) & 0x1fff
            self.add_property('seq_zero', self.seq_zero)
            temp_23 = struct.unpack('>H', self.data[2:4])[0]
            self.seg_o = (temp_23 >> 5) & 0x1f
            self.add_property('seg_o', self.seg_o)
            self.seg_n =  temp_23 & 0x1f
            self.add_property('seg_n', self.seg_n)
            self.segment = self.data[4:]
            self.add_property('segment', self.segment)
        else:
            self.seq_auth = self.seq
            self.upper_transport = self.data[1:]
            self.add_property('upper_transport', self.upper_transport)

class upper_transport_pdu(layer_pdu):
    def __init__(self, segment):
        if segment.ctl:
            super().__init__('Segmented Control', b'')
        else:
            super().__init__('Segmented Transport', b'')
        self.ctl      = segment.ctl
        self.src      = segment.src
        self.dst      = segment.dst
        self.seq      = segment.seq
        self.akf      = segment.akf
        self.aid      = segment.aid
        self.szmic    = segment.szmic
        self.seg_n    = segment.seg_n
        self.seq_zero = segment.seq_zero
        self.direction = segment.direction
        # TODO handle seq_zero overrun
        self.seq_auth = segment.seq & 0xffffe000 | segment.seq_zero
        self.add_property('seq_auth', self.seq_auth)
        self.missing  = (1 << (segment.seg_n+1)) - 1
        self.data = b''
        self.processed = False
        self.origins = []
        if self.ctl:
            self.segment_len = 8
        else:
            self.segment_len = 12

        self.add_property('src', self.src)
        self.add_property('dst', self.dst)
        self.add_property('aid', self.aid)
        self.add_property('akf', self.akf)
        self.add_property('segment_len', self.segment_len)
        self.add_property('dir', self.direction)

    def add_segment(self, network_pdu):
        self.origins.append(network_pdu)
        self.missing &= ~ (1 << network_pdu.seg_o)
        if network_pdu.seg_o == self.seg_n:
            # last segment, set len
            self.len = (self.seg_n * self.segment_len) + len(network_pdu.segment)
        if len(self.data) == 0 and self.complete():
            self.reassemble()

    def complete(self):
        return self.missing == 0

    def reassemble(self):
        self.data = bytearray(self.len)
        missing  = (1 << (self.seg_n+1)) - 1
        for pdu in self.origins:
            if pdu.ctl:
                continue
            # copy data
            pos = pdu.seg_o * self.segment_len
            self.data[pos:pos+len(pdu.segment)] = pdu.segment
            # done?
            missing &= ~ (1 << pdu.seg_o)
            if missing == 0:
                break

class access_pdu(layer_pdu):
    def __init__(self, lower_pdu, data):
        super().__init__('Access', b'')
        self.src       = lower_pdu.src
        self.dst       = lower_pdu.dst
        self.akf       = lower_pdu.akf
        self.aid       = lower_pdu.aid
        self.seq_auth  = lower_pdu.seq_auth
        self.data      = data
        self.direction = lower_pdu.direction
        self.add_property('src', self.src)
        self.add_property('dst', self.dst)
        self.add_property('akf', self.akf)
        self.add_property('aid', self.aid)
        self.add_property('seq_auth', self.seq_auth)
        self.add_property('dir', self.direction)

        upper_bits = data[0] >> 6
        if upper_bits == 0 or upper_bits == 1:
            self.opcode = data[0]
            self.opcode_len = 1
        elif upper_bits == 2:
            self.opcode = read_net_16(data[0:2])
            self.opcode_len = 2
        elif upper_bits == 3:
            self.opcode = read_net_24(data[0:3])
            self.opcode_len = 3
        self.add_property('opcode', self.opcode)
        self.params = data[self.opcode_len:]
        self.add_property('params', self.params)
        if self.opcode in access_messages:
            self.summary = access_messages[self.opcode]

def segmented_message_tag(src, direction):
    tag = str(src) + direction
    return tag

def segmented_message_for_pdu(pdu):
    tag = segmented_message_tag(pdu.src, pdu.direction)
    if tag in segmented_messages:
        seg_message = segmented_messages[tag]
        # check seq zero
        if pdu.seq_zero  == seg_message.seq_zero:
            return seg_message
    # print("new segmented message: src %04x, seq_zero %04x" % (pdu.src, pdu.seq_zero))
    seg_message = upper_transport_pdu(pdu)
    segmented_messages[tag] = seg_message
    return seg_message

def mesh_set_iv_index(iv_index):
    global ivi
    ivi = iv_index
    print ("IV-Index: " + as_big_endian32(ivi).hex())

# key management
def mesh_add_netkey(index, netkey):
    key = network_key(index, netkey)
    print (key)
    netkeys[index] = key

def mesh_network_keys_for_nid(nid):
    for (index, key) in netkeys.items():
        if key.nid == nid:
            yield key

def mesh_set_device_key(key):
    global devkey
    print ("DevKey: " + key.hex())
    devkey = key

def mesh_add_application_key(index, appkey):
    key = application_key(index, appkey)
    print (key)
    appkeys[index] = key

def mesh_application_keys_for_aid(aid):
    for (index, key) in appkeys.items():
        if key.aid == aid:
            yield key

def mesh_transport_nonce(pdu, nonce_type):
    if pdu.szmic:
        aszmic = 0x80
    else:
        aszmic = 0x00
    return bytes( [nonce_type, aszmic, pdu.seq_auth >> 16, (pdu.seq_auth >> 8) & 0xff, pdu.seq_auth & 0xff, pdu.src >> 8, pdu.src & 0xff, pdu.dst >> 8, pdu.dst & 0xff]) + as_big_endian32(ivi)

def mesh_application_nonce(pdu):
    return mesh_transport_nonce(pdu, 0x01)

def mesh_device_nonce(pdu):
    return mesh_transport_nonce(pdu, 0x02)

def mesh_upper_transport_decrypt(message, data):
    if message.szmic:
        trans_mic_len = 8
    else:
        trans_mic_len = 4
    ciphertext = data[:-trans_mic_len]
    trans_mic  = data[-trans_mic_len:]
    decrypted = None
    if message.akf:
        nonce = mesh_application_nonce(message)
        # print( as_hex(nonce))
        for key in mesh_application_keys_for_aid(message.aid):
            decrypted = aes_ccm_decrypt(key.appkey, nonce, ciphertext, b'', trans_mic_len, trans_mic)
            if decrypted != None:
                break
    else:
        nonce = mesh_device_nonce(message)
        decrypted =  aes_ccm_decrypt(devkey, nonce, ciphertext, b'', trans_mic_len, trans_mic)
    return decrypted

def mesh_process_control(control_pdu):
    # TODO decode control message
    # TODO add Seg Ack to sender access message origins
    control_pdu.opcode = control_pdu.data[0]
    control_pdu.add_property('opcode', control_pdu.opcode)
    control_pdu.obo = (control_pdu.data[1] & 0x80) >> 7
    control_pdu.add_property('obo', control_pdu.obo)
    temp_12 = read_net_16(control_pdu.data[1:3])
    control_pdu.seq_zero = (temp_12 >> 2) & 0x1fff
    control_pdu.add_property('seq_zero', control_pdu.seq_zero)
    control_pdu.block_ack = read_net_32(control_pdu.data[3:7])
    control_pdu.add_property('block_ack', control_pdu.block_ack)

    # try to add it to access message
    inverse_direction = 'RX'
    if control_pdu.direction == 'RX':
        inverse_direcgtion = 'TX'
    tag = segmented_message_tag(control_pdu.dst, inverse_direction)
    if tag in segmented_messages:
        seg_message = segmented_messages[tag]
        seg_message.origins.append(control_pdu)
    else:
        log_pdu(control_pdu, 0, [])

def mesh_process_access(access_pdu):
    log_pdu(access_pdu, 0, [])

def mesh_process_network_pdu_tx(network_pdu_encrypted):

    # network layer - decrypt pdu
    nid = network_pdu_encrypted.data[0] & 0x7f
    for key in mesh_network_keys_for_nid(nid):
        network_pdu_decrypted_data = network_decrypt(network_pdu_encrypted.data, as_big_endian32(ivi), key.encryption_key, key.privacy_key)
        if network_pdu_decrypted_data != None:
            break
    if network_pdu_decrypted_data == None:
        network_pdu_encrypted.summary = 'No encryption key found'
        log_pdu(network_pdu_encrypted, 0, [])
        return

    # decrypted network pdu
    network_pdu_decrypted = network_pdu(network_pdu_decrypted_data)
    network_pdu_decrypted.direction = network_pdu_encrypted.direction
    network_pdu_decrypted.add_property('dir', network_pdu_decrypted.direction)
    network_pdu_decrypted.origins.append(network_pdu_encrypted)

    # print("network pdu (enc)" + network_pdu_encrypted.data.hex())
    # print("network pdu (dec)" + network_pdu_decrypted_data.hex())

    # lower transport - reassemble
    lower_transport = lower_transport_pdu(network_pdu_decrypted)
    lower_transport.direction = network_pdu_decrypted.direction
    lower_transport.add_property('dir', lower_transport.direction)
    lower_transport.origins.append(network_pdu_decrypted)

    if lower_transport.seg:
        message = segmented_message_for_pdu(lower_transport)
        message.add_segment(lower_transport)
        if not message.complete():
            return
        if message.processed:
            return

        message.processed = True
        if message.ctl:
            mesh_process_control(message)
        else:
            access_payload = mesh_upper_transport_decrypt(message, message.data)
            if access_payload == None:
                message.summary = 'No encryption key found'
                log_pdu(message, 0, [])
            else:
                access = access_pdu(message, access_payload)
                access.direction = message.direction
                access.origins.append(message)
                mesh_process_access(access)

    else:
        # print("lower_transport.ctl = " + str(lower_transport.ctl))
        if lower_transport.ctl:
            control = layer_pdu('Unsegmented Control', lower_transport.data)
            control.direction = lower_transport.direction
            control.seq = lower_transport.seq
            control.src = lower_transport.src
            control.dst = lower_transport.dst
            control.ctl = True
            control.add_property('seq', lower_transport.seq)
            control.add_property('src', lower_transport.src)
            control.add_property('dst', lower_transport.dst)
            control.origins.append(lower_transport)
            mesh_process_control(control)
        else:
            access_payload = mesh_upper_transport_decrypt(lower_transport, lower_transport.upper_transport)
            if access_payload == None:
                lower_transport.summary = 'No encryption key found'
                log_pdu(lower_transport, 0, [])
            else:
                access = access_pdu(lower_transport, access_payload)
                access.add_property('seq_auth', lower_transport.seq)
                access.direction = lower_transport.direction
                access.origins.append(lower_transport)
                mesh_process_access(access)


def mesh_process_beacon_pdu(adv_pdu):
    log_pdu(adv_pdu, 0, [])

def mesh_process_adv(adv_pdu):
    ad_len  = adv_pdu.data[0] - 1
    ad_type = adv_pdu.data[1]
    if ad_type == 0x2A:
        network_pdu_encrypted = layer_pdu("Network(encrypted)", adv_data[2:2+ad_len])
        network_pdu_encrypted.add_property('ivi', adv_data[2] >> 7)
        network_pdu_encrypted.add_property('nid', adv_data[2] & 0x7f)
        network_pdu_encrypted.add_property('dir', adv_pdu.direction)
        network_pdu_encrypted.direction = adv_pdu.direction
        network_pdu_encrypted.origins.append(adv_pdu)
        mesh_process_network_pdu_tx(network_pdu_encrypted)
    if ad_type == 0x2b:
        beacon_pdu = layer_pdu("Beacon", adv_data[2:2+ad_len])
        beacon_pdu.origins.append(adv_pdu)
        mesh_process_beacon_pdu(beacon_pdu)

def mesh_log_completed():
    # log left-overs
    print("\n\nLOG COMPLETE - unfinished segmented messages:")
    for tag in segmented_messages:
        message = segmented_messages[tag]
        if message.processed:
            continue
        log_pdu(message, 0, [])

if len(sys.argv) == 1:
    print ('Dump Mesh PacketLogger file')
    print ('Copyright 2019, BlueKitchen GmbH')
    print ('')
    print ('Usage: ' + sys.argv[0] + ' hci_dump.pklg')
    exit(0)

infile = sys.argv[1]

with open (infile, 'rb') as fin:
    pos = 0
    while True:
        payload_length  = read_net_32_from_file(fin)
        if payload_length < 0:
            break
        ts_sec  = read_net_32_from_file(fin)
        ts_usec = read_net_32_from_file(fin)
        type    = ord(fin.read(1))
        packet_len = payload_length - 9;
        if (packet_len > 66000):
            print ("Error parsing pklg at offset %u (%x)." % (pos, pos))
            break

        packet  = fin.read(packet_len)
        pos     = pos + 4 + payload_length
        # time    = "[%s.%03u] " % (datetime.datetime.fromtimestamp(ts_sec).strftime("%Y-%m-%d %H:%M:%S"), ts_usec / 1000)

        if type == 0:
            # CMD
            if packet[0] != 0x08:
                continue
            if packet[1] != 0x20:
                continue
            adv_data = packet[4:]
            adv_pdu = layer_pdu("ADV(TX)", adv_data)
            adv_pdu.add_property('dir', 'TX')
            adv_pdu.direction = 'TX'
            mesh_process_adv(adv_pdu)

        elif type == 1:
            # EVT
            event = packet[0]
            if event != 0x3e:
                continue
            event_len = packet[1]
            if event_len < 14:
                continue
            adv_data = packet[13:-1]
            adv_pdu = layer_pdu("ADV(RX)", adv_data)
            adv_pdu.add_property('dir', 'RX')
            adv_pdu.direction = 'RX'
            mesh_process_adv(adv_pdu)

        elif type == 0xfc:
            # LOG
            log = packet.decode("utf-8")
            parts = re.match('mesh-iv-index: (.*)', log)
            if parts and len(parts.groups()) == 1:
                mesh_set_iv_index(int(parts.groups()[0], 16))
                continue
            parts = re.match('mesh-devkey: (.*)', log)
            if parts and len(parts.groups()) == 1:
                mesh_set_device_key(bytes.fromhex(parts.groups()[0]))
                continue
            parts = re.match('mesh-appkey-(.*): (.*)', log)
            if parts and len(parts.groups()) == 2:
                mesh_add_application_key(int(parts.groups()[0], 16), bytes.fromhex(parts.groups()[1]))
                continue
            parts = re.match('mesh-netkey-(.*): (.*)', log)
            if parts and len(parts.groups()) == 2:
                mesh_add_netkey(int(parts.groups()[0], 16), bytes.fromhex(parts.groups()[1]))
                continue

    mesh_log_completed()
