#!/usr/bin/env python3

import struct
import math
import sys, os
import pickle

devices   = dict()
delays    = dict()
scan_start_timestamp = 0
scan_nr = -1

def delta(s): 
    rs = list()
    for i in range(len(s)-1):
        rs.append(s[i+1] - s[i])
    return rs

def normalize(s): 
    return map(lambda x: (x - s[0]), s)
   
def average(s): 
    if len(s) == 0: return 0
    return sum(s) * 1.0 / len(s)

def stder(s, mean):
    if len(s) == 0: return 0
    variance = map(lambda x: (x - mean)**2, s)
    return math.sqrt(average(variance))

       
def reset_timestamp(packet_type, packet, time_sec):
    global scan_start_timestamp, scan_nr
    if packet_type != 0x00 or packet[0] != 0x0C or packet[1] != 0x20:
        return
    if (int(packet[3])): 
        scan_start_timestamp = time_sec
        scan_nr = scan_nr + 1
        print("Scanning started at %u"%scan_start_timestamp)
        
    else:          
        print("Scanning stopped")

def read_scan(packet_type, packet, time_sec):
    if packet_type != 0x01 or packet[0] != 0x3E or packet[2] != 0x02:
        return
    
    if packet[3] != 1:
        print("More then one report")
        return

    (event_type, addr_type, addr, data_len) = struct.unpack('<BB6sB', packet[4:13])
    if event_type == 0x04:
        return

    unpack_format = '<%usB' % data_len
    (data, rssi) = struct.unpack(unpack_format,packet[13:])

    bt_addr = bytearray(addr)
    bt_addr_str = ''
    for b in bt_addr:
        bt_addr_str = '%02x:%s' % (b, bt_addr_str)

    if scan_start_timestamp <= 0:
        return
    
    normalized_timestamp = time_sec - scan_start_timestamp
    if not bt_addr_str in devices.keys():
        print("new device at %u %u" % (time_sec, scan_start_timestamp))
        devices[bt_addr_str] = list()
        delays[bt_addr_str] = list()
        
    devices[bt_addr_str].append(normalized_timestamp)
    if (len(delays[bt_addr_str]) == 0 or len(delays[bt_addr_str]) < scan_nr):
        delays[bt_addr_str].append(normalized_timestamp)

    #print("%03u, %08u, %08u, 0x%02X, adv: %s"%(length, time_sec, time_usec, packet_type, bt_addr_str))
    return

def process_pklg(exp_name, sensor_name, scanning_type, pklg_file_name):
    print("Opening %s" % pklg_file_name)
    with open(pklg_file_name, "rb") as f:
        while True:
            try:
                (length, time_sec, time_usec, packet_type) = struct.unpack('>IIIB',f.read(13))
            except:
                break
            packet = bytearray(f.read(length - 9))
            reset_timestamp(packet_type, packet, time_sec)
            read_scan(packet_type, packet, time_sec)
            
    f.close();
    
    prefix = '../data/processed/'
    for k in devices.keys():
        data_file_name = ''
        if k == '5c:f3:70:60:7b:87:': #BCM
            data_file_name = prefix + exp_name+'_'+scanning_type +'_mac.data'
        if k == '00:1a:7d:00:86:7c:': #neo
            data_file_name = prefix + exp_name+'_'+scanning_type +'_'+sensor_name+'.data'

        if k == '00:07:80:67:45:bc:': #xg 1
            data_file_name = prefix + exp_name+'_'+scanning_type +'_'+sensor_name+'1.data'

        if k == '00:07:80:67:46:00:': #xg 2
            data_file_name = prefix + exp_name+'_'+scanning_type +'_'+sensor_name+'2.data'

        if not data_file_name:
            continue
        
        pickle.dump(devices[k], open(data_file_name, 'wb'))
        mes_index = 0
        # take the last measurement
        for i in range(len(devices[k])-1):
            if devices[k][i] > devices[k][i+1]:
                mes_index = i+1
        pickle.dump(devices[k][mes_index:len(devices[k])], open(data_file_name, 'wb'))

def init():
    global devices, delays, scan_start_timestamp, scan_nr
    devices   = dict()
    delays    = dict()
    scan_start_timestamp = 0
    scan_nr = -1

init()
data_folder = "../data/processed"
if not os.access(data_folder, os.F_OK):
    os.mkdir(data_folder)

prefix = '../data/pklg/'
process_pklg('exp1','nio','continuous_mac', prefix+'BCM20702A0_nio_continuous_scanning.pklg')
process_pklg('exp1','nio','continuous_rug', prefix+'RugGear_nio_continuous_scanning.pklg')

init()
process_pklg('exp1','nio','normal_rug', prefix+'RugGear_nio_normal_scanning.pklg')
process_pklg('exp1','nio','normal_mac', prefix+'BCM20702A0_nio_normal_scanning.pklg')


init()
process_pklg('exp2','xg','continuous_mac', prefix+'BCM20702A0_xg_continuous_scanning.pklg')
process_pklg('exp2','xg','continuous_rug', prefix+'RugGear_xg_continuous_scanning.pklg')

init()
process_pklg('exp2','xg','normal_rug', prefix+'RugGear_xg_normal_scanning.pklg')
process_pklg('exp2','xg','normal_mac', prefix+'BCM20702A0_xg_normal_scanning.pklg')

