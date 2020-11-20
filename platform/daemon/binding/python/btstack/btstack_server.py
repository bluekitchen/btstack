#!/usr/bin/env python3

from threading import Thread
from ctypes import *

# global variables
btstack_server_library = None

class btstackServerThread (Thread):
    def run(self):
        global btstack_server_library
        print ("[+] BTstack Server thread started")
        btstack_server_library.btstack_server_run_tcp()
        print ("[+] BTstack Server thread quit")

class BTstackServer(object):
    def __init__(self):
        pass

    def load(self, path = None):
        global btstack_server_library
        if path == None:
            path = "../../../../port/daemon/src/" 
        print("[+] Load BTstack Server from %s" % path)
        path += "libBTstackServer.dylib"
        # TODO: check OS (mac,linux,windows)
        # TODO: construct path to load library from
        btstack_server_library = cdll.LoadLibrary("../../../../port/daemon/src/libBTstackServer.dylib")

    def set_storage_path(self, path):
        global btstack_server_library
        print("[+] Set storage path to %s" % path)
        btstack_server_library.btstack_server_set_storage_path("/tmp")

    def run_tcp(self):
        print("[+] Run TCP server")
        btstackServerThread(name="BTstackServerThread").start()
