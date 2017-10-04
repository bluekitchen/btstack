#!/bin/sh
DIR=`dirname $0`
BTSTACK_ROOT=$DIR/../
SRC_FOLDERS="src src/classic src/ble src/ble/gatt-service 3rd-party/bluedroid/encoder/srce 3rd-party/bluedroid/decoder/srce 3rd-party/hxcmod-player 3rd-party/hxcmod-player/mods"
for folder in $SRC_FOLDERS
do
	rm -f $BTSTACK_ROOT/$folder/*.o
	rm -f $BTSTACK_ROOT/$folder/*.d
done
