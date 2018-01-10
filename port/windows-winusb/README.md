# BTstack port for Windows Systems using the WinUSB Driver

The Windows-WinUSB port uses the native run loop and WinUSB API to access a USB Bluetooth dongle.

To allow libusb or WinUSB to access an USB Bluetooth dongle, you need to install a special device driver to make it accessible to user space processes. 

It works like this:
-  Download [Zadig](http://zadig.akeo.ie)
-  Start Zadig
-  Select Options -> “List all devices”
-  Select USB Bluetooth dongle in the big pull down list
-  Select WinUSB (libusb) in the right pull pull down list
-  Select “Replace Driver”

When running the examples in the MSYS2 shell, the console input (via btstack_stdin_support) doesn't work. It works in the older MSYS and also the regular CMD.exe environment. Another option is to install WinPTY and then start the example via WinPTY like this:

    $ winpty ./hfp_hf_demo.exe

