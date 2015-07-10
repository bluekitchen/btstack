
# BTstack for iOS

BTstack for iOS provides an alternative Bluetooth stack for iOS devices with a public API.

It supports the following Bluetooth Classic protocols:
- L2CAP
- RFCOMM
- SDP

Based on these protocols, applications or daemons can implement various Bluetooth profiles.
Packages that already use BTstack are: BTstack GPS, Blutrol, WeBe++, and various game emulators. 

Note: As BTstack directly uses the Bluetooth hardware, the iOS Bluetooth is automatically disabled for BTstack applications & services. You can always turn BTstack off in Settings->BTstack.

Please visit the [project page at GitHub](https://github.com/bluekitchen/btstack/) for technical information and check the platform/ios subfolder.

## How to develop

To write BTstack-based applications, you don't need to compile the BTstack Cydia package. You can just install it on your 
JB iOS device and copy /usr/lib/libBTstack.dylib into your project and add btstack/include to your project includes. 
See btstack/platforms/example/WiiMoteOpenGLDemo for a Xcode-base example. In general, I highly recommend to use theos
to create apps & daemons for JB iOS devices instead of using Xcode.

## Compile Instructions

Install the following tools:
* Xcode 6.4 as /Applications/Xcode.app
* Xcode 4.4.1 as /Applications/Xcode-4.4.1.app/
* [rpetrich's theos fork](https://github.com/rpetrich/theos) with "lippoplastic" support

Set the $THEOS environment variable to the location of theos, e.g. like:

	export THEOS=/Projects/theos

Go to btstack/platforms/ios and run make

	cd btstack
	make package

If everything went right, you'll end up with a .deb package that you can install via:

	make install



