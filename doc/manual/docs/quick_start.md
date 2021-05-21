#

## General Tools

Most ports use a regular Makefile to build the examples.

On Unix-based systems, git, make, and Python are usually installed. If
not, use the system’s packet manager to install them.

On Windows, there is no packet manager, but it's easy to download and install all requires development packets quickly by hand. You'll need:

- [Python](http://www.python.org/getit/) for Windows. When using the official installer, please confirm adding Python to the Windows Path.
- [MSYS2](https://msys2.github.io) is used to provide the bash shell and most standard POSIX command line tools.
- [MinGW64](https://mingw-w64.org/doku.php) GCC for Windows 64 & 32 bits incl. make. To install with MSYS2: pacman -S mingw-w64-x86_64-gcc
- [git](https://git-scm.com) is used to download BTstack source code. To install with MSYS2: pacman -S git
- [winpty](https://github.com/rprichard/winpty) a wrapper to allow for console input when running in MSYS2: To install with MSYS2: pacman -S winpty

## Getting BTstack from GitHub

Use git to clone the latest version:

    git clone https://github.com/bluekitchen/btstack.git
        
Alternatively, you can download it as a ZIP archive from
[BTstack’s page](https://github.com/bluekitchen/btstack/archive/master.zip) on
GitHub.

## Let's Go

The easiest way to try BTstack is on a regular desktop setup like macOS, Linux or Windows together with a standard USB Bluetooth Controller. Running BTstack on desktop speeds up the development cycle a lot and also provides direct access to full packet log files in cases something doesn't work as expected. The same code can then later be run unmodified on an embedded target.

For macOS and Linux, please see [libusb](ports/existing_ports.md#libusbPort) port.
For Windows, please see [windows-winusb](ports/existing_ports.md#windows-winusbPort) port.

Or checkout the [list of existing ports]()ports/existing_ports.md)
