#!/usr/bin/env python3
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
import datetime

io_capabilities = [
'IO_CAPABILITY_DISPLAY_ONLY',
'IO_CAPABILITY_DISPLAY_YES_NO',
'IO_CAPABILITY_KEYBOARD_ONLY',
'IO_CAPABILITY_NO_INPUT_NO_OUTPUT',
'IO_CAPABILITY_KEYBOARD_DISPLAY']

SM_AUTHREQ_NO_BONDING        = 0x00
SM_AUTHREQ_BONDING           = 0x01
SM_AUTHREQ_MITM_PROTECTION   = 0x04
SM_AUTHREQ_SECURE_CONNECTION = 0x08
SM_AUTHREQ_KEYPRESS          = 0x10

failures = [
'',
'PASSKEY_ENTRY_FAILED',
'OOB_NOT_AVAILABLE',
'AUTHENTHICATION_REQUIREMENTS',
'CONFIRM_VALUE_FAILED',
'PAIRING_NOT_SUPPORTED',
'ENCRYPTION_KEY_SIZE',
'COMMAND_NOT_SUPPORTED',
'UNSPECIFIED_REASON',
'REPEATED_ATTEMPTS',
'INVALID_PARAMETERS',
'DHKEY_CHECK_FAILED',
'NUMERIC_COMPARISON_FAILED',
]

# tester config
debug      = False
regenerate = False
usb_paths = ['02-04', '02-03']

class Node:

    def __init__(self):
        self.name = 'node'
        self._got_line = False
        self.peer_addr = None
        self.failure   = None

    def get_name(self):
        return self.name

    def set_name(self, name):
        self.name = name

    def set_auth_req(self, auth_req):
        self.auth_req = auth_req

    def set_io_capabilities(self, io_capabilities):
        self.io_capabilities = io_capabilities

    def set_oob_data(self, oob_data):
        self.oob_data = oob_data

    def set_failure(self, failure):
        self.failure = failure

    def set_usb_path(self, path):
        self.usb_path = path

    def get_stdout_fd(self):
        return self.stdout.fileno()

    def read_stdout(self):
        c = os.read(self.stdout.fileno(), 1).decode("utf-8")
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
        args = ['build-coverage/sm_test', '-u', self.usb_path, '-c']
        if self.peer_addr != None:
            args.append('-a')
            args.append(self.peer_addr)
        if self.failure != None:
            args.append('-f')
            args.append(self.failure)
        args.append('-i')
        args.append(self.io_capabilities)
        args.append('-r')
        args.append(self.auth_req)
        args.append('-o')
        args.append(self.oob_data)
        print('%s - "%s"' % (self.name, ' '.join(args)))
        self.p = subprocess.Popen(args, bufsize=0, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (self.stdin, self.stdout) = (self.p.stdin, self.p.stdout)
        self.linebuffer = ''

    def set_packet_log(self, path):
        self.packet_log = path

    def get_packet_log(self):
        return self.packet_log

    def set_bd_addr(self, addr):
        self.bd_addr = addr

    def get_bd_addr(self):
        return self.bd_addr

    def set_peer_addr(self, addr):
        self.peer_addr = addr

    def write(self, string):
        print("CMD -> %s: %s" % (self.name, string))
        self.stdin.write(string.encode('utf-8'))

    def terminate(self):
        self.write('x')
        # wait for 'EXIT' message indicating coverage data was written
        while not self.got_line():
            self.read_stdout()
        self.p.terminate()

def run(test_descriptor, nodes):
    state = 'W4_SLAVE_OOB_RANDOM'
    pairing_complete = []
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
                if debug:
                    print('%s: %s' % (node.get_name(), line))
                if line.startswith('Packet Log: '):
                    path = line.split(': ')[1]
                    node.set_packet_log(path)
                    print('%s log %s' % (node.get_name(), path))
                elif line.startswith('BD_ADDR: '):
                    addr = line.split(': ')[1]
                    node.set_bd_addr(addr)
                    print('%s started' % node.get_name())
                elif line.startswith('LOCAL_OOB_CONFIRM:'):
                    confirm = line.split('OOB_CONFIRM: ')[1]
                    test_descriptor[node.get_name()+'_oob_confirm'] = confirm
                elif line.startswith('LOCAL_OOB_RANDOM:'):
                    random = line.split('OOB_RANDOM: ')[1]
                    test_descriptor[node.get_name()+'_oob_random'] = random
                    print('%s OOB Random: %s' % (node.get_name(), random))
                    if state == 'W4_SLAVE_OOB_RANDOM':
                        # peripheral started, start central
                        state = 'W4_MASTER_OOB_RANDOM'
                        master_role = test_descriptor['master_role']
                        master = Node()
                        # configure master
                        master.set_name(master_role)
                        master.usb_path = usb_paths[1]
                        master.set_peer_addr(addr)
                        master.set_auth_req(test_descriptor[master_role + '_auth_req'])
                        master.set_io_capabilities(test_descriptor[master_role + '_io_capabilities'])
                        master.set_oob_data(test_descriptor[master_role + '_oob_data'])
                        if master_role == 'tester':
                            master.set_failure(test_descriptor['tester_failure'])
                        master.start_process()
                        nodes.append(master)
                        #
                        if node.get_name() == 'iut':
                            iut_node = node
                            tester_node = master
                        else:
                            iut_node = master
                            tester_node = node
                    elif state == 'W4_MASTER_OOB_RANDOM':
                        # central started, start connecting
                        node.write('c')
                        print('start to connect')
                        state = 'W4_CONNECTED'
                elif line.startswith('CONNECTED:'):
                    print('%s connected' % node.get_name())
                    if state == 'W4_CONNECTED' and node == nodes[1]:
                        # simulate OOK exchange if requested
                        if test_descriptor['tester_oob_data'] == '1':
                            print('Simulate IUT -> Tester OOB')
                            tester_node.write('o' + test_descriptor['iut_oob_confirm'])
                            tester_node.write('r' + test_descriptor['iut_oob_random'])
                            test_descriptor['method'] = 'OOB'
                        if test_descriptor['iut_oob_data'] == '1':
                            print('Simulate Tester -> IUT OOB')
                            iut_node.write('o' + test_descriptor['tester_oob_confirm'])
                            iut_node.write('r' + test_descriptor['tester_oob_random'])
                            test_descriptor['method'] = 'OOB'
                        node.write('p')
                        state = 'W4_PAIRING'
                elif line.startswith('JUST_WORKS_REQUEST'):
                    print('%s just works requested' % node.get_name())
                    test_descriptor['method'] = 'Just Works'
                    if node.get_name() == 'tester' and  test_descriptor['tester_failure'] == '12':
                        print('Decline bonding')
                        node.write('d')
                    else:
                        print('Accept bonding')
                        node.write('a')
                elif line.startswith('NUMERIC_COMPARISON_REQUEST'):
                    print('%s numeric comparison requested' % node.get_name())
                    test_descriptor['method'] = 'Numeric Comparison'
                    if node.get_name() == 'tester' and  test_descriptor['tester_failure'] == '12':
                        print('Decline bonding')
                        node.write('d')
                    else:
                        print('Accept bonding')
                        node.write('a')
                elif line.startswith('PASSKEY_DISPLAY_NUMBER'):
                    passkey = line.split(': ')[1]
                    print('%s passkey display %s' % (node.get_name(), passkey))
                    test_descriptor['passkey'] = passkey
                    if node.get_name() == 'tester' and  test_descriptor['tester_failure'] == '1':
                        print('Decline bonding')
                        node.write('d')
                    if state == 'W4_PAIRING':
                        state = 'W4_PASSKEY_INPUT'
                    else:
                        test_descriptor['waiting_node'].write(test_descriptor['passkey'])
                elif line.startswith('PASSKEY_INPUT_NUMBER'):
                    test_descriptor['method'] = 'Passkey Entry'
                    if node.get_name() == 'tester' and  test_descriptor['tester_failure'] == '1':
                        print('Decline bonding')
                        node.write('d')
                    elif state == 'W4_PASSKEY_INPUT':
                        node.write(test_descriptor['passkey'])
                    else:
                        test_descriptor['waiting_node'] = node
                        state = 'W4_PASSKEY_DISPLAY'
                elif line.startswith('PAIRING_COMPLETE'):
                    result = line.split(': ')[1]
                    (status,reason) = result.split(',')
                    test_descriptor[node.get_name()+'_pairing_complete_status'] = status
                    test_descriptor[node.get_name()+'_pairing_complete_reason'] = reason
                    print('%s pairing complete: status %s, reason %s' % (node.get_name(), status, reason))
                    pairing_complete.append(node.get_name())
                    # pairing complete?
                    if len(pairing_complete) == 2:
                        # on error, test is finished, else wait for notify
                        if status != '0':
                            return
                elif line.startswith('DISCONNECTED'):
                    # Abort on unexpected disconnect
                    print('%s unexpected disconnect' % node.get_name())
                    test_descriptor['error'] = 'DISCONNECTED'
                    return
                elif line.startswith('COUNTER'):
                    print('%s notification received' % node.get_name())
                    return;

def write_config(fout, test_descriptor):
    attributes = [
        'header',
        '---',
        'bd_addr',
        'role',
        'failure',
        'io_capabilities',
        'mitm',
        'secure_connection',
        'keypress',
        'rfu',
        'oob_data',
        'method',
        'passkey',
        'pairing_complete_status',
        'pairing_complete_reason']

    # header
    fout.write('Test: %s\n' % test_descriptor['name'])
    fout.write('Date: %s\n' % str(datetime.datetime.now()))
    fout.write('\n')
    attribute_len = 28
    value_len     = 35
    format_string = '%%-%us|%%-%us|%%-%us\n' % (attribute_len, value_len, value_len)
    for attribute in attributes:
        name = attribute
        if attribute == 'header':
            name  = 'Attribute'
            iut   = 'IUT'
            tester = 'Tester'
        elif attribute == '---':
            name   = '-' * attribute_len
            iut    = '-' * value_len
            tester = '-' * value_len
        elif attribute == 'io_capabilities':
            iut    = io_capabilities[int(test_descriptor['iut_io_capabilities'   ])]
            tester = io_capabilities[int(test_descriptor['tester_io_capabilities'])]
        elif attribute == 'mitm':
            iut    = (int(test_descriptor['iut_auth_req'   ]) & SM_AUTHREQ_MITM_PROTECTION)   >> 2
            tester = (int(test_descriptor['tester_auth_req']) & SM_AUTHREQ_MITM_PROTECTION)   >> 2
        elif attribute == 'secure_connection':
            iut    = (int(test_descriptor['iut_auth_req'   ]) & SM_AUTHREQ_SECURE_CONNECTION) >> 3
            tester = (int(test_descriptor['tester_auth_req']) & SM_AUTHREQ_SECURE_CONNECTION) >> 3
        elif attribute == 'keypress':
            iut    = (int(test_descriptor['iut_auth_req'   ]) & SM_AUTHREQ_KEYPRESS)          >> 4
            tester = (int(test_descriptor['tester_auth_req']) & SM_AUTHREQ_KEYPRESS)          >> 4
        elif attribute == 'rfu':
            iut    = (int(test_descriptor['iut_auth_req'   ]) & 192) >> 6
            tester = (int(test_descriptor['tester_auth_req']) & 192) >> 6
        elif attribute == 'passkey':
            if not 'passkey' in test_descriptor:
                continue
            iut    = test_descriptor['passkey']
            tester = test_descriptor['passkey']
        elif attribute == 'method':
            if not 'method' in test_descriptor:
                continue
            iut    = test_descriptor['method']
            tester = test_descriptor['method']
        elif attribute == 'failure':
            iut    = ''
            tester = failures[int(test_descriptor['tester_failure'])]
        else:
            iut    = test_descriptor['iut_'    + attribute]
            tester = test_descriptor['tester_' + attribute]
        fout.write(format_string % (name, iut, tester))

def run_test(test_descriptor):
    # shutdown previous sm_test instances
    try:
        subprocess.run(['killall', 'sm_test'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except:
        pass

    # trash all bonding informatino
    try:
        subprocess.call(['rm', '-f', '/tmp/btstack_*'])
    except:
        pass

    test_name = test_descriptor['name']
    print('Test: %s' % test_name,  file=sys.stderr)

    if '/SLA/' in test_descriptor['name']:
        iut_role    = 'responder'
        tester_role = 'initiator'
        slave_role  = 'iut'
        master_role = 'tester'
    else:
        iut_role    = 'initiator'
        tester_role = 'responder'
        slave_role  = 'tester'
        master_role = 'iut'

    test_descriptor['iut_role'   ] = iut_role
    test_descriptor['tester_role'] = tester_role
    test_descriptor['master_role'] = master_role
    test_descriptor['slave_role']  = slave_role

    slave = Node()

    # configure slave
    slave.set_name(slave_role)
    slave.usb_path = usb_paths[0]
    slave.set_auth_req(test_descriptor[slave_role + '_auth_req'])
    slave.set_io_capabilities(test_descriptor[slave_role + '_io_capabilities'])
    slave.set_oob_data(test_descriptor[slave_role + '_oob_data'])
    if slave_role == 'tester':
        slave.set_failure(test_descriptor['tester_failure'])

    # start up slave
    slave.start_process()

    nodes = [slave]

    # run test
    try:
        run(test_descriptor, nodes)
        if 'error' in test_descriptor:
            sys.exit()
            raise NameError(test_descriptor['error'])

        # identify iut and tester
        if iut_role == 'responder':
            iut    = nodes[0]
            tester = nodes[1]
        else:
            iut    = nodes[1]
            tester = nodes[0]

        test_folder =  test_descriptor['test_folder']

        # check result
        test_ok = True
        if  test_descriptor['tester_failure'] != '0':
            # expect status != 0 if tester_failure set
            test_ok &= test_descriptor['iut_pairing_complete_status'] != '0'
            test_ok &= test_descriptor['iut_pairing_complete_reason'] == test_descriptor['tester_failure']
        else:
            test_ok &= test_descriptor['iut_pairing_complete_status'] == '0' 

        # check pairing method
        if 'method' in test_descriptor:
            method = test_descriptor['method']
            if 'SCJW' in test_name and (method != 'Just Works' and method != 'Numeric Comparison'):
                test_ok = False
            if 'SCPK' in test_name and method != 'Passkey Entry':
                test_ok = False
            if 'SCOB' in test_name and method != 'OOB':
                test_ok = False

        # rename folder if test not ok
        if not test_ok:
            test_folder = 'TEST_FAIL-' + test_folder

        # move hci logs into result folder
        os.makedirs(test_folder)
        shutil.move(iut.get_packet_log(),    test_folder + '/iut.pklg')
        shutil.move(tester.get_packet_log(), test_folder + '/tester.pklg')

        # write config
        with open (test_folder + '/config.txt', "wt") as fout:
            test_descriptor['iut_bd_addr']    = iut.get_bd_addr()
            test_descriptor['tester_bd_addr'] = tester.get_bd_addr()
            write_config(fout, test_descriptor)

    except KeyboardInterrupt:
        print('Interrupted')
        test_descriptor['interrupted'] = 'EXIT'

    except NameError:
        print('Run-time error')

    # shutdown
    for node in nodes:
        node.terminate()
    print("Done\n")


# read tests
with open('sm_test.csv') as csvfile:
    reader = csv.DictReader(csvfile)
    for test_descriptor in reader:
        test_name = test_descriptor['name']

        if test_name.startswith('#'):
            continue
        if len(test_name) == 0:
            continue

        test_folder = test_name.replace('/', '_')
        test_descriptor['test_folder'] = test_folder

        # skip test if regenerate not requested
        if os.path.exists(test_folder):
            if regenerate:
                shutil.rmtree(test_folder)
            else: 
                print('Test: %s (completed)' % test_name)
                continue

        # run test (max 10 times)
        tries = 10
        done = False
        while not done:
            print(test_descriptor)
            run_test(test_descriptor)
            tries = tries - 1
            done = True

            # escalate CTRL-C
            if 'interrupted' in test_descriptor:
                break

            # repeat on 'error'
            if 'error' in test_descriptor:
                del test_descriptor['error']
                if tries > 0:
                    done = False

        if 'interrupted' in test_descriptor:
            break
