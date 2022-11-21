# BTstack Port for Windows Systems with DA14585 Controller connected via Serial Port

This port allows to use the DA14585 connected via Serial Port with BTstack running on a Win32 host system.

It first downloads the hci_585.hex firmware from the 6.0.8.509 SDK, before BTstack starts up.

Please note that it does not detect if the firmware has already been downloaded, so you need to reset the DA14585 before starting an example.

For production use, the HCI firmware could be flashed into the OTP and the firmware download could be skipped.

Tested with the official DA14585 Dev Kit Basic on OS X and Windows 10.

The port provides both a regular Makefile as well as a CMake build file. It uses native Win32 APIs for file access and does not require the Cygwin or mingw64 build/runtine. All examples can also be build with Visual Studio 2022 (e.g. Community Edition).

## Visual Studio 2022

Visual Studio can directly open the provided `port/windows-windows-h4-da14585/CMakeLists.txt` and allows to compile and run all examples.

## mingw64 

It can also be compiles with a regular Unix-style toolchain like [mingw-w64](https://www.mingw-w64.org).
mingw64-w64 is based on [MinGW](https://en.wikipedia.org/wiki/MinGW), which '...provides a complete Open Source programming tool set which is suitable for the development of native MS-Windows applications, and which do not depend on any 3rd-party C-Runtime DLLs.'

We've used the Msys2 package available from the [downloads page](https://www.mingw-w64.org/downloads/) on Windows 10, 64-bit and use the MSYS2 MinGW 64-bit start menu item to compile 64-bit binaries.

In the MSYS2 shell, you can install everything with pacman:

    $ pacman -S git
    $ pacman -S cmake
    $ pacman -S make
    $ pacman -S mingw-w64-x86_64-toolchain
    $ pacman -S mingw-w64-x86_64-portaudio
    $ pacman -S python
    $ pacman -S winpty

### Compilation with CMake

With mingw64-w64 installed, just go to the port/windows-h4 directory and use CMake as usual

    $ cd port/windows-h4
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make

Note: When compiling with msys2-32 bit and/or the 32-bit toolchain, compilation fails
as `conio.h` seems to be mission. Please use msys2-64 bit with the 64-bit toolchain for now.

## Console Output

When running the examples in the MSYS2 shell, the console input (via btstack_stdin_support) doesn't work. It works in the older MSYS and also the regular CMD.exe environment. Another option is to install WinPTY and then start the example via WinPTY like this:

    $ winpty ./gatt_counter.exe

The packet log will be written to hci_dump.pklg
