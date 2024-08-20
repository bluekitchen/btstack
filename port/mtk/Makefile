# Toolchain config
NDK = /Projects/Android/android-ndk-r10d
ADB = adb
TOOLCHAIN = /usr/local/android-ndk
SYSROOT = $(TOOLCHAIN)/sysroot
GCC = $(TOOLCHAIN)/bin/arm-linux-androideabi-gcc
STRIP = $(TOOLCHAIN)/bin/arm-linux-androideabi-strip
CFLAGS = -march=armv7-a -mfloat-abi=softfp -I$(SYSROOT)/usr/include -fpie -fpic -fPIE -pie#
LDFLAGS = -Wl,--fix-cortex-a8 -L$(SYSROOT)/usr/lib -fpie -fpic -fPIE -pie

BTSTACK_ROOT ?= ../..

CFLAGS += -I$(BTSTACK_ROOT)/include
CFLAGS += -I$(BTSTACK_ROOT)/src/
CFLAGS += -I$(BTSTACK_ROOT)/ble
CFLAGS += -I$(BTSTACK_ROOT)/platform/daemon/src
CFLAGS += -I$(BTSTACK_ROOT)/platform/posix
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include
CFLAGS += -I${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
CFLAGS += -I$(BTSTACK_ROOT)/3rd-party/md5
CFLAGS += -I$(BTSTACK_ROOT)/3rd-party/micro-ecc
CFLAGS += -I$(BTSTACK_ROOT)/3rd-party/rijndael
CFLAGS += -I$(BTSTACK_ROOT)/3rd-party/yxml
CFLAGS += -I.

VPATH += $(BTSTACK_ROOT)/src
VPATH += $(BTSTACK_ROOT)/src/ble
VPATH += $(BTSTACK_ROOT)/src/classic
VPATH += $(BTSTACK_ROOT)/platform/posix
VPATH += $(BTSTACK_ROOT)/platform/daemon/src
VPATH += $(BTSTACK_ROOT)/platform/daemon/example
VPATH += $(BTSTACK_ROOT)/3rd-party/md5

libBTstack_OBJS  = 		           \
	btstack.o                      \
	btstack_linked_list.o          \
	btstack_run_loop.o             \
	btstack_run_loop_base.o        \
	btstack_run_loop_posix.o       \
    btstack_tlv.o                  \
	btstack_util.o 	               \
	hci_cmd.o                      \
	hci_event_builder.o            \
	daemon_cmds.o                  \
	hci_dump.o                     \
	hci_dump_posix_fs.o            \
	sdp_util.o                     \
	spp_server.o                   \
	socket_connection.o            \

BTdaemon_OBJS = $(libBTstack_OBJS) \
	ad_parser.o 			       \
	att_dispatch.o                 \
	btstack_link_key_db_tlv.o      \
	btstack_memory.o               \
	btstack_memory_pool.o          \
	btstack_tlv.o       		   \
	btstack_tlv_posix.o 		   \
	btstack_crypto.o               \
	daemon.o 				       \
	gatt_client.o                  \
	hci.o                          \
	hci_transport_h4_mtk.o         \
	btstack_uart_posix.o           \
	l2cap.o                        \
	l2cap_signaling.o              \
	le_device_db_tlv.o             \
	md5.o                          \
	rfcomm.o                       \
	rfcomm_service_db_hash.o       \
	sdp_server.o                   \
	sm.o                           \
    att_db.o                       \
    att_server.o                   \
    sdp_client.o                   \
    sdp_client_rfcomm.o            \

all: $(TOOLCHAIN) version BTstackDaemon libBTstack.so BTstackDaemonRespawn inquiry le_scan rfcomm_echo dist

dist:
	rm -f *.o

clean:
	rm -f BTstackDaemon BTstackDaemonRespawn le_scan inquiry rfcomm_echo *.so *.o

$(TOOLCHAIN):
	$(NDK)/build/tools/make-standalone-toolchain.sh --install-dir=$@

libbluetoothdrv.so: bluetoothdrv_stub.o
	$(GCC) $(LDFLAGS) -shared -o libbluetoothdrv.so bluetoothdrv_stub.o

.c.o:
	$(GCC) $(CFLAGS) -o $@ -c $<

install:
	./installer.sh

run: 
	$(ADB) shell le_scan

shell: 
	$(ADB) shell

version:
	$(BTSTACK_ROOT)/tool/get_version.sh
	grep BTSTACK_DATE $(BTSTACK_ROOT)/platform/daemon/src/btstack_version.h

BTstackDaemon: $(BTdaemon_OBJS) libbluetoothdrv.so
	$(GCC) $(LDFLAGS) -o $@ libbluetoothdrv.so $^

BTstackDaemonRespawn: BTstackDaemonRespawn.o
	$(GCC) $(LDFLAGS) -o $@ BTstackDaemonRespawn.o


libBTstack.so: $(libBTstack_OBJS)
	$(GCC) $(LDFLAGS) -shared -Wl,-soname,/system/lib/libBTstack.so -o $@ $^

inquiry: inquiry.o
	$(GCC) $(LDFLAGS) -Wl,-rpath,/system/lib libBTstack.so -o $@ inquiry.o

rfcomm_echo: rfcomm_echo.o
	$(GCC) $(LDFLAGS) -Wl,-rpath,/system/lib libBTstack.so -o $@ rfcomm_echo.o

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
