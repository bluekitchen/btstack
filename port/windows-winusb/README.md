# BTstack Port for Windows Systems using the WinUSB Driver

Although libusb basically works with the POSIX Run Loop on Windows, we recommend to use the Windows-WinUSB port that uses the native run loop and WinUSB API to access a USB Bluetooth dongle.

To allow libusb or WinUSB to access an USB Bluetooth dongle, you need to install a special device driver to make it accessible to user space processes. 

It works like this:
-  Start [Zadig](http://zadig.akeo.ie)
-  Select Options -> “List all devices”
-  Select USB Bluetooth dongle in the big pull down list
-  Select WinUSB (libusb) in the right pull pull down list
-  Select “Replace Driver”

When running the examples in the MSYS2 shell, the console input (via btstack_stdin_support) doesn't work. It works in the older MSYS and also the regular CMD.exe environment. Another option is to install WinPTY and then start the example via WinPTY like this:

    $ winpty ./hfp_hf_demo.exe

