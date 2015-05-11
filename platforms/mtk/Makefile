# Toolchain config
NDK = /Projects/Android/android-ndk-r10d
ADB = adb
TOOLCHAIN = /usr/local/android-ndk
SYSROOT = $(TOOLCHAIN)/sysroot
GCC = $(TOOLCHAIN)/bin/arm-linux-androideabi-gcc
STRIP = $(TOOLCHAIN)/bin/arm-linux-androideabi-strip
CFLAGS = -march=armv7-a -mfloat-abi=softfp -I$(SYSROOT)/usr/include
LDFLAGS = -Wl,--fix-cortex-a8 -L$(SYSROOT)/usr/lib

BTSTACK_ROOT = ../..

CFLAGS += -I$(BTSTACK_ROOT)/include
CFLAGS += -I$(BTSTACK_ROOT)/src/
CFLAGS += -I$(BTSTACK_ROOT)/ble
CFLAGS += -I.

VPATH += $(BTSTACK_ROOT)/src
VPATH += $(BTSTACK_ROOT)/ble
VPATH += $(BTSTACK_ROOT)/platforms/posix/src
VPATH += $(BTSTACK_ROOT)/platforms/daemon/src
VPATH += $(BTSTACK_ROOT)/example/daemon

libBTstack_OBJS  = 		 \
	hci_cmds.o           \
	hci_dump.o           \
	linked_list.o        \
	run_loop.o           \
	sdp_util.o           \
	utils.o 		     \
	btstack.o            \
	run_loop_posix.o     \
	socket_connection.o  \

BTdaemon_OBJS = $(libBTstack_OBJS)   \
	att_dispatch.o              \
	le_device_db_memory.o        \
    att.o                       \
    att_server.o                \
	gatt_client.o               \
	sm.o                        \
	btstack_memory.o            \
	hci.o                       \
	l2cap.o                     \
	l2cap_signaling.o           \
	memory_pool.o               \
	remote_device_db_memory.o   \
	rfcomm.o                    \
	sdp.o                       \
    sdp_client.o                \
    sdp_parser.o                \
    sdp_query_rfcomm.o          \
    sdp_query_util.o            \
	daemon.o 				    \
	hci_transport_h4_mtk.o      \

all: $(TOOLCHAIN) version BTstackDaemon libBTstack.so BTstackDaemonRespawn inquiry le_scan rfcomm-echo dist

dist:
	rm -f *.o

clean:
	rm -f BTstackDaemon BTstackDaemonRespawn le_scan inquiry rfcomm-echo *.so *.o

$(TOOLCHAIN):
	$(NDK)/build/tools/make-standalone-toolchain.sh --install-dir=$@

libbluetoothdrv.so: bluetoothdrv-stub.o
	$(GCC) $(LDFLAGS) -shared -o libbluetoothdrv.so bluetoothdrv-stub.o

.c.o:
	$(GCC) $(CFLAGS) -o $@ -c $<

install:
	./installer.sh

run: 
	$(ADB) shell le_scan

shell: 
	$(ADB) shell

version:
	$(BTSTACK_ROOT)/tools/get_version.sh
	grep BTSTACK_DATE $(BTSTACK_ROOT)/include/btstack/version.h

BTstackDaemon: $(BTdaemon_OBJS) libbluetoothdrv.so
	$(GCC) $(LDFLAGS) -o $@ libbluetoothdrv.so $^

BTstackDaemonRespawn: BTstackDaemonRespawn.o
	$(GCC) $(LDFLAGS) -o $@ BTstackDaemonRespawn.o


libBTstack.so: $(libBTstack_OBJS)
	$(GCC) $(LDFLAGS) -shared -Wl,-soname,/system/lib/libBTstack.so -o $@ $^

inquiry: inquiry.o
	$(GCC) $(LDFLAGS) -Wl,-rpath,/system/lib libBTstack.so -o $@ inquiry.o

rfcomm-echo: rfcomm-echo.o
	$(GCC) $(LDFLAGS) -Wl,-rpath,/system/lib libBTstack.so -o $@ rfcomm-echo.o

le_scan: le_scan.o
	$(GCC) $(LDFLAGS) -Wl,-rpath,/system/lib libBTstack.so -o $@ le_scan.o

# minimal_peripheral: minimal_peripheral.o
# 	$(GCC) $(LDFLAGS) libs/libbluetoothdrv.so  -o $@ minimal_peripheral.o

hci_dump: 
	killall PacketLogger
	$(ADB) shell su root chmod 666 /data/btstack/hci_dump.pklg
	$(ADB) pull /data/btstack/hci_dump.pklg 2> /dev/null
	open hci_dump.pklg
	
log: hci_dump
