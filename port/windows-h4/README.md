# BTstack Port for Windows Systems with Bluetooth Controller connected via Serial Port

The Windows-H4 port uses the native run loop and allows to use Bluetooth Controllers connected via Serial Port.

Make sure to manually reset the Bluetooth Controller before starting any of the examples.

## Toolchain

The port requires a Unix-like toolchain. We successfully used [mingw-w64](https://mingw-w64.org/doku.php) to compile and run the examples. mingw64-w64 is based on [MinGW](https://en.wikipedia.org/wiki/MinGW), which '...provides a complete Open Source programming tool set which is suitable for the development of native MS-Windows applications, and which do not depend on any 3rd-party C-Runtime DLLs.'

We've used the Msys2 package available from the [downloads page](https://mingw-w64.org/doku.php/download) on Windows 10, 64-bit and use the MSYS2 MinGW 64-bit start menu item to compile 64-bit binaries.

In the MSYS2 shell, you can install everything with pacman:

    $ pacman -S git
    $ pacman -S make
    $ pacman -S mingw-w64-x86_64-toolchain
    $ pacman -S python
    $ pacman -S winpty

## Compilation

With mingw64-w64 installed, just go to the port/windows-winusb directory and run make

    $ cd btstack/port/windows-winusb
    $ make

Note: When compiling with msys2-32 bit and/or the 32-bit toolchain, compilation fails
as `conio.h` seems to be mission. Please use msys2-64 bit with the 64-bit toolchain for now.

## Console Output

When running the examples in the MSYS2 shell, the console input (via btstack_stdin_support) doesn't work. It works in the older MSYS and also the regular CMD.exe environment. Another option is to install WinPTY and then start the example via WinPTY like this:

    $ winpty ./spp_and_le_counter.exe

