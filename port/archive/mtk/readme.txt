//
// BTstack port for RugGear Anrdroid 4.x devices with MediaTek Chipsets
//

It replaces MediaTek's Bluetooth Stack but doesn't integrate with the OS. BTstack libbtstack is required to communicate with the BTstack Server. It supports the LE Central and SPP.

Technical details:
- the binary for the Mediatek Bluetooth Server is replaced by the BTstackDaemonRespawn binary
- It links against MediaTeks's libbluetoothdrv.so that provides a standard HCI Transport implementation via POSIX API


Compilation:
It depends on the Android NDK. Please update NDK, ADB at the beginning of the Makefile
$ make

Install:
$ ./installer.sh
It will try to backup the original mtkbt Bluetooth server. Use at your own risk resp. double check the script first.

Deinstall:
$ ./deinstaller.sh
This tries to put mtkbk back in place.

Quick test:
$ adb shell le_scan
To start an LE discovery

Getting the packet log:
$ make hci_dump

