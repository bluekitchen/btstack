
# BTstack for iOS

BTstack for iOS provides an alternative Bluetooth stack for iOS devices with a public API.

It supports the following Bluetooth Classic protocols:
- L2CAP
- RFCOMM
- SDP

Based on these protocols, applications or daemons can implement various Bluetooth profiles.
Packages that already use BTstack are: BTstack GPS, Blutrol, WeBe++, and various game emulators. 

Note: As BTstack directly uses the Bluetooth hardware, the iOS Bluetooth is automatically disabled for BTstack applications & services. You can always turn BTstack off in Settings->BTstack.

Please visit the [project page at GitHub](https://github.com/bluekitchen/btstack/) for technical information and check the port/ios subfolder.

## How to develop

To write BTstack-based applications, you don't need to compile the BTstack Cydia package.
You can just install it on your JB iOS device and copy /usr/lib/libBTstack.dylib into your
project and add btstack/include to your project includes. 

While it's possible to use Xcode, I highly recommend to use theos to create apps & daemons for JB iOS devices instead.

## Compile Instructions for BTstack package

Install the following tools:
* Xcode
* [csu-ios libraries](https://github.com/mringwal/csu-ios) for deployment targets < 6.0 (BTstack compiles againt iOS 3.0 by default)
* [rpetrich's](https://github.com/rpetrich/theos) or [new official](https://github.com/theos/theos) theos fork with "lippoplastic" support


Set the $THEOS environment variable to the location of the theos checkout, e.g. like:

	export THEOS=/Projects/theos

Go to btstack/port/ios and run make

	cd btstack/port/ios
	make package

If everything went right, you'll end up with a .deb package that you could install via:

	make install


## Console Examples

With THEOS set-up as before, you can compile and install a set of command line examples in the example folder:

	cd btstack/port/ios/example
	make

You can copy the created examples to your device using scp and run it from there.


## Wii Mote CoocaTouch example

Similar as before, you can compile the WiiMoteOpenGLDemo by running make:

	cd btstack/port/ios/example/WiiMoteOpenGLDemo
	make package

You'll end up with a deb file that you can install with

 	make install

Note: as with any other JB application, you need to refresh the SpringBoard icon cache on the device to make the icon show up. After SSH to your device as root, you can execute these commands:
	
	su mobile
	uicache
	exit
