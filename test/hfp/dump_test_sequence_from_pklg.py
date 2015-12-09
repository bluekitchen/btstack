#!/usr/bin/env python
# BlueKitchen GmbH (c) 2014

# primitive dump for PacketLogger format

# APPLE PacketLogger
# typedef struct {
#   uint32_t    len;
#   uint32_t    ts_sec;
#   uint32_t    ts_usec;
#   uint8_t     type;   // 0xfc for note
# }

import re
import sys
import time
import datetime

packet_types = [ "CMD =>", "EVT <=", "ACL =>", "ACL <="]

def read_net_32(f):
    a = f.read(1)
    b = f.read(1)
    c = f.read(1)
    d = f.read(1)
    return ord(a) << 24 | ord(b) << 16 | ord(c) << 8 | ord(d)

def as_hex(data):
    str_list = []
    for byte in data:
        str_list.append("{0:02x} ".format(ord(byte)))
    return ''.join(str_list)

if len(sys.argv) < 2:
    print 'Dump PacketLogger file'
    print 'Copyright 2014, BlueKitchen GmbH'
    print ''
    print 'Usage: ', sys.argv[0], 'hci_dump.pklg test_name'
    exit(0)

infile = sys.argv[1]
test_name = sys.argv[2]
separator = ""
spaces = "    "
print "const char * "+test_name+"[] = {"


with open (infile, 'rb') as fin:
    try:
        while True:
            len     = read_net_32(fin)
            ts_sec  = read_net_32(fin)
            ts_usec = read_net_32(fin)
            type    = ord(fin.read(1))
            packet_len = len - 9;
            packet  = fin.read(packet_len)
            time    = "[%s.%03u]" % (datetime.datetime.fromtimestamp(ts_sec).strftime("%Y-%m-%d %H:%M:%S"), ts_usec / 1000)
            if type == 0xfc:
                packet = packet.replace("\n","\\n")
                packet = packet.replace("\r","\\r")
                packet = packet.replace("\"","\\\"")
                
                parts = re.match('HFP_RX(.*)',packet)
                if not parts:
                    parts = re.match('HFP_TX(.*)',packet)
                
                cmd = 0
                if parts:
                    hfp_cmds = parts.groups()[0].split('\\r\\n')
                    for cmd in hfp_cmds:
                        cmd = cmd.strip()
                        if cmd <> "":
                            cmd = cmd.replace("\\r","")
                            print separator+spaces+"\""+cmd+"\"",
                            separator = ",\n"
                        
                else:
                    parts = re.match('USER:\'(.*)\'.*',packet)
                    if parts:
                        cmd = 'USER:'+parts.groups()[0]
                        print separator+spaces+"\""+cmd+"\"",
                        separator = ",\n"


    except TypeError:
        print "\n};\n"
        exit(0) 

print "\n};\n"
