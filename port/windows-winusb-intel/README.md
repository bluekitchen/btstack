# BTstack port for Windows Systems with Intel Wireless 8260/8265 Controllers

Same as port/windows-winusb, but customized for Intel Wireless 8260 and 8265 Controllers.
These controller require firmware upload and configuration to work. Firmware and config is downloaded from the Linux firmware repository.

## Access to Bluetooth USB Dongle with Zadig

To allow libusb or WinUSB to access an USB Bluetooth dongle, you need to install a special device driver to make it accessible to user space processes. 

It works like this:

-  Download [Zadig](http://zadig.akeo.ie)
-  Start Zadig
-  Select Options -> “List all devices”
-  Select USB Bluetooth dongle in the big pull down list
-  Select WinUSB (libusb) in the right pull pull down list
-  Select “Replace Driver”

## Toolchain

The port requires a Unix-like toolchain. We successfully used [mingw-w64](https://mingw-w64.org/doku.php) to compile and run the examples. mingw64-w64 is based on [MinGW](mingw.org), which '...provides a complete Open Source programming tool set which is suitable for the development of native MS-Windows applications, and which do not depend on any 3rd-party C-Runtime DLLs.'

We've used the Msys2 package available from the [downloads page](https://mingw-w64.org/doku.php/download) on Windows 10, 64-bit and use the MSYS2 MinGW 32-bit start menu item to compile 32-bit binaries that run on both 32/64-bit systems.

In the MSYS2 shell, you can install git, python, and, winpty with pacman:

    $ pacman -S git
    $ pacman -S python
    $ pacman -S winpty

## Compilation

With mingw64-w64 installed, just go to the port/windows-winusb directory and run make

    $ cd btstack/port/windows-winusb
    $ make

## Console Output

When running the examples in the MSYS2 shell, the console input (via btstack_stdin_support) doesn't work. It works in the older MSYS and also the regular CMD.exe environment. Another option is to install WinPTY and then start the example via WinPTY like this:

    $ winpty ./spp_and_le_counter.exe

