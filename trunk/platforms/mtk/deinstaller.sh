#!/bin/bash

echo "BTstack Deinstaller for RugGear/Mediatek devices"

# make /system writable
if adb shell mount | grep -q "/system ext4 ro" ; then 
	echo "- remounting /system as read/write"
	adb shell su root mount -o remount,rw /emmc@android /system
fi

if adb shell mount | grep -q "/system ext4 ro" ; then 
	echo "- remounting failed, abort"
fi
echo "- /system mounted as read/write"

if adb shell stat /system/bin/BTstackDaemon | grep -q "regular file" ; then
echo "- deleting BTstackDaemon and BTstackDaemonRespawn"
adb shell su root rm /system/bin/BTstackDaemon
adb shell su root rm /system/bin/BTstackDaemonRespawn
fi

echo "- stopping Bluetooth daemon"
adb shell su root setprop ctl.stop mtkbt

# put mtkbt back in place
echo "- restore mtkbk"
adb shell su root cp /system/bin/mtkbt_orig /system/bin/mtkbt

echo "- start Bluetooth daemon"
adb shell su root setprop ctl.start mtkbt

echo "DONE"
