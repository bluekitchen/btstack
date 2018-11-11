import socket
import struct
import btstack.command_builder

BTSTACK_SERVER_HOST = "localhost"
BTSTACK_SERVER_TCP_PORT = 13333

# temp
OGF_BTSTACK = 0x3d;
BTSTACK_SET_POWER_MODE = 0x02

# utils
def print_hex(data):
    print(" ".join("{:02x}".format(c) for c in data))

class BTstackClient(btstack.command_builder.CommandBuilder):

    #
    btstack_server_socket = None

    #
    packet_handler = None

    def __init__(self):
        pass

    def connect(self):
        global BTSTACK_SERVER_TCP_PORT
        global BTSTACK_SERVER_HOST

        print("[+] Connect to server on port %u" % BTSTACK_SERVER_TCP_PORT)
        self.btstack_server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.btstack_server_socket.connect((BTSTACK_SERVER_HOST, BTSTACK_SERVER_TCP_PORT))
        # TODO: handle connection failure

    def register_packet_handler(self, callback):
        print("[+] Register packet handler")
        self.packet_handler = callback

    def send_hci_command(self, command):
        packet_type = 1
        channel = 0
        length = len(command)
        header = struct.pack("<HHH", packet_type, channel, length)
        self.btstack_server_socket.sendall(header + command)

    def run(self):
        print("[+] Run")
        while True:
            # receive packet header: packet type, channel, len
            header = self.btstack_server_socket.recv(6)
            (packet_type, channel, length) = struct.unpack("<HHH", header)
            print_hex(header)
            payload = self.btstack_server_socket.recv(length)
            print_hex(payload)
