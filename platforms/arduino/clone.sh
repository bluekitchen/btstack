#!/bin/sh
DIR=`dirname $0`
BTSTACK_ROOT=$DIR/../..
BTSTACK_PACKAGE=$DIR/btstack

echo Update version.h file
$BTSTACK_ROOT/tools/get_version.sh

pushd .
cd $BTSTACK_ROOT
VERSION=`sed -n -e 's/^.*BTSTACK_VERSION \"\(.*\)\"/\1/p' include/btstack/version.h`
ARCHIVE=btstack-arduino-$VERSION.zip
popd

echo Prepare lib at $BTSTACK_PACKAGE

rm -rf $BTSTACK_PACKAGE
mkdir $BTSTACK_PACKAGE


# <btstack/...> headers
cp -r $BTSTACK_ROOT/include/btstack $BTSTACK_PACKAGE

# other headers 
cp $BTSTACK_ROOT/ble/*.h $BTSTACK_PACKAGE
cp $BTSTACK_ROOT/src/*.h $BTSTACK_PACKAGE

# src files 
SRC_FILES="btstack_memory.c linked_list.c memory_pool.c run_loop.c run_loop_embedded.c "
SRC_FILES+="hci_dump.c hci.c hci_cmds.c hci_transport_h4_dma.c sdp_util.c utils.c "
for i in $SRC_FILES
do
	cp $BTSTACK_ROOT/src/$i $BTSTACK_PACKAGE
done

# ble files
BLE_FILES="ad_parser.c att.c att_server.c att_dispatch.c le_device_db_memory.c gatt_client.c "
BLE_FILES+="sm.c l2cap_le.c ancs_client_lib.h ancs_client_lib.c"
for i in $BLE_FILES
do
	cp $BTSTACK_ROOT/ble/$i $BTSTACK_PACKAGE
done

# em9301 chipset support
cp $BTSTACK_ROOT/chipset-em9301/* $BTSTACK_PACKAGE

# Configuration
cp $DIR/btstack-config.h $BTSTACK_PACKAGE

# BSP Arduino
cp bsp_arduino_em9301.cpp $BTSTACK_PACKAGE

# Arduino c++ API
cp $DIR/BTstack.cpp $DIR/BTstack.h $BTSTACK_PACKAGE

# Arduino examples
cp -r $BTSTACK_ROOT/platforms/arduino/examples $BTSTACK_PACKAGE

echo "Create Archive $ARCHIVE"
rm -f $ARCHIVE btstack.zip
zip -r $ARCHIVE btstack
