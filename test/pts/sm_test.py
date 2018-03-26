#!/usr/bin/env python
#
# Perform Security Manager Test Cases using two BTstack instances
#
# Copyright 2018 BlueKitchen GmbH
#

import os
import subprocess
import sys
import time
import signal
import select
import fcntl
import csv
import shutil

usb_paths = ['3', '5']

class Node:

    def __init__(self):
        self.name = 'node'
        self._got_line = False
        self.peer_addr = None

    def get_name(self):
        return self.name

    def set_name(self, name):
        self.name = name

    def set_usb_path(self, path):
        self.usb_path = path

    def get_stdout_fd(self):
        return self.stdout.fileno()

    def read_stdout(self):
        c = os.read(self.stdout.fileno(), 1)
        if len(c) == 0: 
            return
        if c in '\n\r':
            if len(self.linebuffer) > 0:
                self._got_line = True
        else: 
            self.linebuffer += c

    def got_line(self):
        return self._got_line

    def fetch_line(self):
        line = self.linebuffer
        self.linebuffer = ''
        self._got_line = False
        return line

    def start_process(self):
        args = ['./sm_test', '-u', self.usb_path]
        if self.peer_addr != None:
            args.append('-a')
            args.append(self.peer_addr)
        print('%s - "%s"' % (self.name, ' '.join(args)))
        self.p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (self.stdin, self.stdout) = (self.p.stdin, self.p.stdout)
        self.linebuffer = ''

    def set_packet_log(self, path):
        self.packet_log = path

    def get_packet_log(self):
        return self.packet_log

    def set_bd_addr(self, addr):
        self.bd_addr = addr

    def get_bd_addr(self, addr):
        return self.bd_addr

    def set_peer_addr(self, addr):
        self.peer_addr = addr

    def write(self, string):
        self.stdin.write(string)

    def terminate(self):
        self.write('x')
        self.p.terminate()

def run(nodes):
    state = 'W4_SLAVE_BD_ADDR'

    while True:
        # create map fd -> node
        nodes_by_fd = { node.get_stdout_fd():node for node in nodes}
        read_fds = nodes_by_fd.keys()
        (read_ready, write_ready, exception_ready) = select.select(read_fds,[],[])
        for fd in read_ready:
            node = nodes_by_fd[fd]
            node.read_stdout()
            if node.got_line():
                line = node.fetch_line()
                # print('%s: %s' % (node.get_name(), line))
                if line.startswith('Packet Log: '):
                    path = line.split(': ')[1]
                    node.set_packet_log(path)
                    print('%s log %s' % (node.get_name(), path))
                if line.startswith('BD_ADDR: '):
                    addr = line.split(': ')[1]
                    node.set_bd_addr(addr)
                    print('%s started' % node.get_name())
                    if state == 'W4_SLAVE_BD_ADDR':
                        # peripheral started, start central
                        state = 'W4_CONNECTED'
                        node = Node()
                        node.set_name('Master')
                        node.usb_path = usb_paths[1]
                        node.set_peer_addr(addr)
                        node.start_process()
                        nodes.append(node)
                if line.startswith('CONNECTED:'):
                    print('%s connected' % node.get_name())
                    if state == 'W4_CONNECTED':
                        state = 'W4_PAIRING'
                if line.startswith('JUST_WORKS_REQUEST'):
                    print('%s just works requested' % node.get_name())
                    node.write('a')
                if line.startswith('PAIRING_COMPLETE'):
                    print('%s pairing complete' % node.get_name())
                    return

def run_test(test_descriptor):
    # shutdown previous sm_test instances
    try:
        subprocess.call("killall sm_test", shell = True)
    except:
        pass

    test_name = test_descriptor['name']
    print('Test: %s' % test_name)

    # start up slave process
    slave = Node()
    slave.set_name('Slave')
    slave.usb_path = usb_paths[0]
    slave.start_process()

    nodes = [slave]

    # run test
    try:
        run(nodes)
    except KeyboardInterrupt:
        pass

    # identify iut and tester
    if '/SLA/' in test_descriptor['name']:
        iut    = nodes[0]
        tester = nodes[1]
    else:
        iut    = nodes[1]
        tester = nodes[0]

    # move hci logs into result folder
    test_folder =  test_descriptor['test_folder']
    os.makedirs(test_folder)
    shutil.move(iut.get_packet_log(), test_folder + '/iut.pklg')
    shutil.move(tester.get_packet_log(), test_folder + '/tester.pklg')

    # shutdown
    for node in nodes:
        node.terminate()
    print("Done\n")

# read tests
with open('sm_test.csv') as csvfile:
    reader = csv.DictReader(csvfile)
    for test_descriptor in reader:
        test_name = test_descriptor['name']
        test_folder = test_name.replace('/', '_')
        test_descriptor['test_folder'] = test_folder

        # check if result folder exists
        if os.path.exists(test_folder):
            print('Test: %s (completed)' % test_name)
            continue

        # run test
        run_test(test_descriptor)

