#!/usr/bin/env python3
#
# Simulate network of Bluetooth Controllers
#
# Each simulated controller has an HCI H4 interface
# Network configuration will be stored in a YAML file or similar
#
# Copyright 2017 BlueKitchen GmbH
#


import os
import pty
import select
import subprocess
import sys
import bisect
import time

# fallback: try to import PyCryptodome as (an almost drop-in) replacement for the PyCrypto library
try:
    from Cryptodome.Cipher import AES
    import Cryptodome.Random as Random
except ImportError:
    # fallback: try to import PyCryptodome as (an almost drop-in) replacement for the PyCrypto library
    try:
        from Crypto.Cipher import AES
        import Crypto.Random as Random
    except ImportError:
        print("\n[!] PyCryptodome required but not installed (using random value instead)")
        print("[!] Please install PyCryptodome, e.g. 'pip3 install pycryptodomex' or 'pip3 install pycryptodome'\n")


def little_endian_read_16(buffer, pos):
    return ord(buffer[pos]) + (ord(buffer[pos+1]) << 8)

def as_hex(data):
    str_list = []
    for byte in data:
        str_list.append("{0:02x} ".format(ord(byte)))
    return ''.join(str_list)

adv_type_names = ['ADV_IND', 'ADV_DIRECT_IND_HIGH', 'ADV_SCAN_IND', 'ADV_NONCONN_IND', 'ADV_DIRECT_IND_LOW']
timers_timeouts = []
timers_callbacks = []

class H4Parser:

    def __init__(self):
        self.packet_type = "NONE"
        self.reset()

    def set_packet_handler(self, handler):
        self.handler = handler

    def reset(self):
        self.bytes_to_read = 1
        self.buffer = ''
        self.state = "H4_W4_PACKET_TYPE"

    def parse(self, data):
        self.buffer += data
        self.bytes_to_read -= 1
        if self.bytes_to_read == 0:
            if self.state == "H4_W4_PACKET_TYPE":
                self.buffer = ''
                if data == chr(1):
                    # cmd
                    self.packet_type = "CMD"
                    self.state = "W4_CMD_HEADER"
                    self.bytes_to_read = 3
                if data == chr(2):
                    # acl 
                    self.packet_type = "ACL"
                    self.state = "W4_ACL_HEADER"
                    self.bytes_to_read = 4
                return
            if self.state == "W4_CMD_HEADER":
                self.bytes_to_read = ord(self.buffer[2])
                self.state = "H4_W4_PAYLOAD"
                if self.bytes_to_read > 0:
                    return
                # fall through to handle payload len = 0
            if self.state == "W4_ACL_HEADER":
                self.bytes_to_read = little_endian_read_16(buffer, 2)
                self.state = "H4_W4_PAYLOAD"
                if self.bytes_to_read > 0:
                    return
                # fall through to handle payload len = 0
            if self.state == "H4_W4_PAYLOAD":
                self.handler(self.packet_type, self.buffer)
                self.reset()
                return

class HCIController:

    def __init__(self):
        self.fd = -1
        self.random = Random.new()
        self.name = 'BTstack Mesh Simulator'
        self.bd_addr = 'aaaaaa'
        self.parser = H4Parser()
        self.parser.set_packet_handler(self.packet_handler)
        self.adv_enabled = 0
        self.adv_type = 0
        self.adv_interval_min = 0
        self.adv_interval_max = 0
        self.adv_data = ''
        self.scan_enabled = False

    def parse(self, data):
        self.parser.parse(data)

    def set_fd(self,fd):
        self.fd = fd

    def set_bd_addr(self, bd_addr):
        self.bd_addr = bd_addr

    def set_name(self, name):
        self.name = name

    def set_adv_handler(self, adv_handler, adv_handler_context):
        self.adv_handler         = adv_handler
        self.adv_handler_context = adv_handler_context

    def is_scanning(self):
        return self.scan_enabled

    def emit_command_complete(self, opcode, result):
        # type, event, len, num commands, opcode, result
        os.write(self.fd, '\x04\x0e' + chr(3 + len(result)) + chr(1) + chr(opcode & 255)  + chr(opcode >> 8) + result)
    
    def emit_adv_report(self, event_type, rssi):
        # type, event, len, Subevent_Code, Num_Reports, Event_Type[i], Address_Type[i], Address[i], Length[i], Data[i], RSSI[i]
        event = '\x04\x3e' + chr(12 + len(self.adv_data)) + chr(2) + chr(1) + chr(event_type) + chr(0) + self.bd_addr[::-1] + chr(len(self.adv_data)) + self.adv_data + chr(rssi)
        self.adv_handler(self.adv_handler_context, event)

    def handle_set_adv_enable(self, enable):
        self.adv_enabled = enable
        print('Node %s adv enable %u' % (self.name, self.adv_enabled))
        if self.adv_enabled:
            add_timer(1, self.handle_adv_timer, self)
        else:
            remove_timer(self.handle_adv_timer, self)

    def handle_set_adv_data(self, data):
        self.adv_data = data
        print('Node %s adv data %s' % (self.name, as_hex(self.adv_data)))

    def handle_set_adv_params(self, interval_min, interval_max, adv_type):
        self.adv_interval_min = interval_min * 0.625
        self.adv_interval_max = interval_max * 0.625
        self.adv_type         = adv_type
        print('Node %s adv interval min/max %u/%u ms, type %s' % (self.name, self.adv_interval_min, self.adv_interval_max, adv_type_names[self.adv_type]))

    def handle_adv_timer(self, context):
        if self.adv_enabled:
            self.emit_adv_report(0, 0)
            add_timer(self.adv_interval_min, self.handle_adv_timer, self)

    def packet_handler(self, packet_type, packet):
        opcode = little_endian_read_16(packet, 0)
        # print ("%s, opcode 0x%04x" % (self.name, opcode))
        if opcode == 0x0c03:
            self.emit_command_complete(opcode, '\x00')
            return
        if opcode == 0x1001:
            self.emit_command_complete(opcode, '\x00\x10\x00\x06\x86\x1d\x06\x0a\x00\x86\x1d')
            return
        if opcode == 0x0c14:
            self.emit_command_complete(opcode, '\x00' + self.name)
            return
        if opcode == 0x1002:
            self.emit_command_complete(opcode, '\x00\xff\xff\xff\x03\xfe\xff\xff\xff\xff\xff\xff\xff\xf3\x0f\xe8\xfe\x3f\xf7\x83\xff\x1c\x00\x00\x00\x61\xf7\xff\xff\x7f\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00')
            return
        if opcode == 0x1009:
            # read bd_addr
            self.emit_command_complete(opcode, '\x00' + self.bd_addr[::-1])            
            return
        if opcode == 0x1005:
            # read buffer size
            self.emit_command_complete(opcode, '\x00\x36\x01\x40\x0a\x00\x08\x00')
            return
        if opcode == 0x1003:
            # read local supported features
            self.emit_command_complete(opcode, '\x00\xff\xff\x8f\xfe\xf8\xff\x5b\x87')
            return
        if opcode == 0x0c01:
            self.emit_command_complete(opcode, '\x00')
            return
        if opcode == 0x2002:
            # le read buffer size
            self.emit_command_complete(opcode, '\x00\x00\x00\x00')
            return
        if opcode == 0x200f:
            # read whitelist size
            self.emit_command_complete(opcode, '\x00\x19')
            return
        if opcode == 0x200b:
            # set scan parameters
            self.emit_command_complete(opcode, '\x00')
            return
        if opcode == 0x200c:
            # set scan enabled
            self.scan_enabled = ord(packet[3])
            self.emit_command_complete(opcode, '\x00')
            return
        if opcode == 0x0c6d:
            # write le host supported
            self.emit_command_complete(opcode, '\x00')
            return
        if opcode == 0x2017:
            # LE Encrypt - key 16, data 16
            key = packet[18:2:-1]
            data = packet[35:18:-1]
            cipher = AES.new(key)
            result = cipher.encrypt(data)
            self.emit_command_complete(opcode, result[::-1])
            return
        if opcode == 0x2018:
            # LE Rand
            self.emit_command_complete(opcode, '\x00' + self.random.read(8))
            return
        if opcode == 0x2006:
            # Set Adv Params
            self.handle_set_adv_params(little_endian_read_16(packet,3), little_endian_read_16(packet,5), ord(packet[6]))
            self.emit_command_complete(opcode, '\x00')
            return
        if opcode == 0x2008:
            # Set Adv Data
            len = ord(packet[3])
            self.handle_set_adv_data(packet[4:4+len])
            self.emit_command_complete(opcode, '\x00')
            return
        if opcode == 0x200a:
            # Set Adv Enable
            self.handle_set_adv_enable(ord(packet[3]))
            self.emit_command_complete(opcode, '\x00')
            return
        print("Opcode 0x%0x not handled!" % opcode)

class Node:

    def __init__(self):
        self.name = 'node'
        self.master = -1
        self.slave  = -1
        self.slave_ttyname = ''
        self.controller = HCIController()

    def set_name(self, name):
        self.controller.set_name(name)
        self.name = name

    def get_name(self):
        return self.name

    def set_bd_addr(self, bd_addr):
        self.controller.set_bd_addr(bd_addr)

    def start_process(self):
        print('Node: %s' % self.name)
        (self.master, self.slave) = pty.openpty()
        self.slave_ttyname = os.ttyname(self.slave)
        print('- tty %s' % self.slave_ttyname)
        print('- fd %u' % self.master)
        self.controller.set_fd(self.master)
        subprocess.Popen(['./mesh', '-d', self.slave_ttyname])

    def get_master(self):
        return self.master

    def parse(self, c):
        self.controller.parse(c)

    def set_adv_handler(self, adv_handler, adv_handler_context):
        self.controller.set_adv_handler(adv_handler, adv_handler_context)

    def inject_packet(self, event):
        os.write(self.master, event)

    def is_scanning(self):
        return self.controller.is_scanning()

def get_time_millis():
    return int(round(time.time() * 1000))

def add_timer(timeout_ms, callback, context):
    global timers_timeouts
    global timers_callbacks

    timeout = get_time_millis() + timeout_ms;
    pos = bisect.bisect(timers_timeouts, timeout)
    timers_timeouts.insert(pos, timeout)
    timers_callbacks.insert(pos, (callback, context))

def remove_timer(callback, context):
    if (callback, context) in timers_callbacks:
        indices = [timers_callbacks.index(t) for t in timers_callbacks if t[0] == callback and t[1] == context]
        index = indices[0]
        timers_callbacks.pop(index)
        timers_timeouts.pop(index)

def run(nodes):
    # create map fd -> node
    nodes_by_fd = { node.get_master():node for node in nodes}
    read_fds = nodes_by_fd.keys()
    while True:
        # process expired timers
        time_ms = get_time_millis()
        while len(timers_timeouts) and timers_timeouts[0] < time_ms:
            timers_timeouts.pop(0)
            (callback,context) = timers_callbacks.pop(0)
            callback(context)
        # timer timers_timeouts?
        if len(timers_timeouts):
            timeout = (timers_timeouts[0] - time_ms) / 1000.0
            (read_ready, write_ready, exception_ready) = select.select(read_fds,[],[], timeout)
        else:
            (read_ready, write_ready, exception_ready) = select.select(read_fds,[],[])
        for fd in read_ready:
            node = nodes_by_fd[fd]
            c = os.read(fd, 1)
            node.parse(c)

def adv_handler(src_node, event):
    global nodes
    # print('adv from %s' % src_node.get_name())
    for dst_node in nodes:
        if src_node == dst_node:
            continue
        if not dst_node.is_scanning():
            continue
        print('Adv %s -> %s - %s' % (src_node.get_name(), dst_node.get_name(), as_hex(event[14:-1])))
        dst_node.inject_packet(event)

# parse configuration file passed in via cmd line args
# TODO

node1 = Node()
node1.set_name('node_1')
node1.set_bd_addr('aaaaaa')
node1.set_adv_handler(adv_handler, node1)
node1.start_process()

node2 = Node()
node2.set_name('node_2')
node2.set_bd_addr('bbbbbb')
node2.set_adv_handler(adv_handler, node2)
node2.start_process()

nodes = [node1, node2]

run(nodes)
