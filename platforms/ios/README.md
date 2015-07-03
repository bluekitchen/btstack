=== BTstack for iOS ===

BTstack for iOS provides an alternative Bluetooth stack for iOS devices with a public API.

It supports the following Bluetooth Classic protocols:
- L2CAP
- RFCOMM
- SDP

Based on these protocols, applications or daemons can implement various Bluetooth profiles.
Packages that already use BTstack are: BTstack GPS, Blutrol, WeBe++, and various game emulators. 

Note: As BTstack directly uses the Bluetooth hardware, the iOS Bluetooth is automatically disabled for BTstack applications & services. You can always turn BTstack off in Settings->BTstack.

Please visit the [project page at GitHub](https://github.com/bluekitchen/btstack/) for technical information and check the platform/ios subfolder.

