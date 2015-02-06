#!/bin/bash
export DIR=`dirname $0`

echo "BTstack Installer for RugGear/Mediatek devices"
echo "from: $DIR"

# make /system writable
if adb shell mount | grep -q "/system ext4 ro" ; then 
	echo "- remounting /system as read/write"
	adb shell su root mount -o remount,rw /emmc@android /system
fi

if adb shell mount | grep -q "/system ext4 ro" ; then 
	echo "- remounting failed, abort"
fi
echo "- /system mounted as read/write"

if adb shell stat /system/bin/mtkbt | grep -q "regular file" ; then
	echo "- backup mtkbt"
	adb shell su root mv /system/bin/mtkbt /system/bin/mtkbt_orig
fi
echo "- stopping Bluetooth daemon"
adb shell su root setprop ctl.stop mtkbt

echo "- transfer files to device"
adb shell su root mkdir -p         /system/btstack
adb shell su root chmod 777        /system/btstack
adb push $DIR/BTstackDaemon        /system/btstack
adb push $DIR/BTstackDaemonRespawn /system/btstack
adb push $DIR/libBTstack.so        /system/btstack
adb push $DIR/inquiry 	           /system/btstack
adb push $DIR/le_scan 	           /system/btstack
adb push $DIR/rfcomm-echo          /system/btstack

echo "- put files in place"
adb shell su root mv /system/btstack/BTstackDaemon /system/bin
adb shell su root chmod 755 /system/bin/BTstackDaemon
adb shell su root mv /system/btstack/BTstackDaemonRespawn /system/bin
adb shell su root chmod 755 /system/bin/BTstackDaemonRespawn
adb shell su root touch /system/bin/mtkbt
adb shell su root rm /system/bin/mtkbt
adb shell su root ln -s /system/bin/BTstackDaemonRespawn /system/bin/mtkbt

adb shell su root mkdir -p  /system/btstack
adb shell su root chmod 755 /system/btstack
adb shell su root chown bluetooth:bluetooth /system/btstack

adb shell su root mv /system/btstack/libBTstack.so /system/lib
adb shell su root chmod 755 /system/lib/libBTstack.so

adb shell su root mv /system/btstack/inquiry       /system/bin
adb shell su root chmod 755 /system/bin/inquiry

adb shell su root mv /system/btstack/le_scan       /system/bin
adb shell su root chmod 755 /system/bin/le_scan

adb shell su root mv /system/btstack/rfcomm-echo       /system/bin
adb shell su root chmod 755 /system/bin/rfcomm-echo

adb shell su root rm -r /system/btstack

echo "- create /data/bstack for unix socket and log files"
adb shell su root mkdir /data/btstack
adb shell su root chown bluetooth:bluetooth /data/btstack

echo "- start BTstack daemon"
adb shell su root setprop ctl.start mtkbt

echo "DONE"
