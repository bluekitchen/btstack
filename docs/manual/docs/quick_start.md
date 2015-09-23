
## General Tools

On Unix-based systems, git, make, and Python are usually installed. If
not, use the system’s packet manager to install them.

On Windows, you need to manually install and configure GNU Make, Python,
and optionally git :

-   [GNU Make](http://gnuwin32.sourceforge.net/packages/make.htm)
    for Windows: Add its bin folder to the Windows Path in Environment
    Variables. The bin folder is where make.exe resides, and it’s
    usually located in [C:\Program Files\GnuWin32\bin]().

-   [Python](http://www.python.org/getit/) for
    Windows: Add Python installation folder to the Windows Path in
    Environment Variables.

### Adding paths to the Windows Path variable {#sec:windowsPathQuickStart}

-   Go to: Control Panel->System->Advanced tab->Environment Variables.

-   The top part contains a list of User variables.

-   Click on the Path variable and then click edit.

-   Go to the end of the line, then append the path to the list, for
    example, [C:\Program Files\GnuWin32\bin]() for GNU Make.

-   Ensure that there is a semicolon before and after [C:\Program Files\GnuWin32\bin]().

## Getting BTstack from GitHub

Use git to clone the latest version:

    git clone https://github.com/bluekitchen/btstack.git
        

Alternatively, you can download it as a ZIP archive from
[BTstack’s page](https://github.com/bluekitchen/btstack/archive/master.zip) on
GitHub.

## Compiling the examples and loading firmware

This step is platform specific. To compile and run the examples, you
need to download and install the platform specific toolchain and a flash
tool. For TI’s CC256x chipsets, you also need the correct init script,
or “Service Pack” in TI nomenclature. Assuming that these are provided,
go to folder [btstack/platforms/$PLATFORM$]() in command prompt and run make. 
If all the paths are correct, it will generate several firmware files. These firmware files
can be loaded onto the device using platform specific flash programmer.
For the PIC32-Harmony platform, a project file for the MPLAB X IDE is
provided, too.

## Run the Example

As a first test, we recommend the [SPP Counter example](examples/generated/#sec:sppcounterExample). 
During the startup, for TI chipsets, the init
script is transferred, and the Bluetooth stack brought up. After that,
the development board is discoverable as “BTstack SPP Counter” and
provides a single virtual serial port. When you connect to it, you’ll
receive a counter value as text every second.

## Platform specifics

In the following, we provide more information on specific platform
setups, toolchains, programmers, and init scripts.

### libusb

The quickest way to try BTstack is on a Linux or OS X system with an
additional USB Bluetooth module. The Makefile [platforms/libusb]() in requires
[pkg-config](http://www.freedesktop.org/wiki/Software/pkg-config/)
and [libusb-1.0](http://libusb.info) or higher to be
installed.

On Linux, it’s usually necessary to run the examples as root as the
kernel needs to detach from the USB module.

On OS X, it’s necessary to tell the OS to only use the internal
Bluetooth. For this, execute:

    sudo nvram bluetoothHostControllerSwitchBehavior=never

It’s also possible to run the examples on Win32 systems. For this:

-   Install [MSYS](http://www.mingw.org/wiki/msys) and
    [MINGW32](http://www.mingw.org) using the MINGW installer

-   Compile and install libusb-1.0.19 to [/usr/local/]() in msys command
    shell

-   Setup a USB Bluetooth dongle for use with libusb-1.0:

    -   Start [Zadig](http://zadig.akeo.ie)

    -   Select Options -> “List all devices”

    -   Select USB Bluetooth dongle in the big pull down list

    -   Select WinUSB (libusb) in the right pull pull down list

    -   Select “Replace Driver”

Now, you can run the examples from the *msys* shell the same way as on
Linux/OS X.

### Texas Instruments MSP430-based boards

**Compiler Setup.** The MSP430 port of BTstack is developed using the
Long Term Support (LTS) version of mspgcc. General information about it
and installation instructions are provided on the 
[MSPGCC Wiki](http://sourceforge.net/apps/mediawiki/mspgcc/index.php?title=MSPGCC_Wiki).
On Windows, you need to download and extract 
[mspgcc](http://sourceforge.net/projects/mspgcc/files/Windows/mingw32/)
to [C:\mspgcc](). Add [C:\mspgcc\bin]() folder to the Windows Path in Environment 
variable as explained [here](#sec:windowsPathQuickStart).

**Loading Firmware.** To load firmware files onto the MSP430 MCU for the
MSP-EXP430F5438 Experimeneter board, you need a programmer like the
MSP430 MSP-FET430UIF debugger or something similar. The eZ430-RF2560 and
MSP430F5529LP contain a basic debugger. Now, you can use one of
following software tools:

-   [MSP430Flasher](http://processors.wiki.ti.com/index.php/MSP430_Flasher_Command_Line_Programmer)
    (windows-only):

    -   Use the following command, where you need to replace the [BINARY_FILE_NAME.hex]() with
        the name of your application:

<!-- -->

    MSP430Flasher.exe -n MSP430F5438A -w "BINARY_FILE_NAME.hex" -v -g -z [VCC]
           

-   [MSPDebug](http://mspdebug.sourceforge.net/):
    An example session with the MSP-FET430UIF connected on OS X is given
    in following listing:

<!-- -->

    mspdebug -j -d /dev/tty.FET430UIFfd130 uif
    ... 
    prog blink.hex
    run

### Texas Instruments CC256x-based chipsets

**CC256x Init Scripts.** In order to use the CC256x chipset on the
PAN13xx modules and others, an initialization script must be obtained.
Due to licensing restrictions, this initialization script must be
obtained separately as follows:

-   Download the [BTS file](http://processors.wiki.ti.com/index.php/CC256x_Downloads)
    for your CC256x-based module.

-   Copy the included .bts file into

-   In [chipset-cc256x](), run the Python script: 

<!-- -->

    ./convert_bts_init_scripts.py

The common code for all CC256x chipsets is provided by
*bt_control_cc256x.c*. During the setup,
*bt_control_cc256x_instance* function is used to get a
*bt_control_t* instance and passed to *hci_init* function.

**Note:** Depending on the CC256x-based module you’re using, you’ll need
to update the reference in the Makefile to match the downloaded file.

**Update:** For the latest revision of the CC256x chipsets, the CC2560B
and CC2564B, TI decided to split the init script into a main part and
the BLE part. The conversion script has been updated to detect
*bluetooth_init_cc256x_1.2.bts* and adds *BLE_init_cc256x_1.2.bts*
if present and merges them into a single .c file.

**Update 2:** In May 2015, TI renamed the init scripts to match
the naming scheme previously used on Linux systems. The conversion
script has been updated to also detect *initscripts_TIInit_6.7.16_bt_spec_4.1.bts*
and integrates *initscripts_TIInit_6.7.16_ble_add-on.bts* if present.

### MSP-EXP430F5438 + CC256x Platform {#sec:platformMSP430QuickStart}

**Hardware Setup.** We assume that a PAN1315, PAN1317, or PAN1323 module
is plugged into RF1 and RF2 of the MSP-EXP430F5438 board and the “RF3
Adapter board” is used or at least simulated. See [User
Guide](http://processors.wiki.ti.com/index.php/PAN1315EMK_User_Guide#RF3_Connector).

### STM32F103RB Nucleo + CC256x Platform

To try BTstack on this platform, you’ll need a simple adaptor board. For
details, please read the documentation in [platforms/stm32-f103rb-nucleo/README.md]().

### PIC32 Bluetooth Audio Development Kit

The PIC32 Bluetooth Audio Development Kit comes with the CSR8811-based
BTM805 Bluetooth module. In the port, the UART on the DAC daughter board
was used for the debug output. Please remove the DAC board and connect a
3.3V USB-2-UART converter to GND and TX to get the debug output.

In [platforms/pic32-harmony](), a project file for the MPLAB X IDE 
is provided as well as a regular
Makefile. Both assume that the MPLAB XC32 compiler is installed. The
project is set to use -Os optimization which will cause warnings if you
only have the Free version. It will still compile a working example. For
this platform, we only provide the SPP and LE Counter example directly.
Other examples can be run by replacing the *spp_and_le_counter.c* file
with one of the other example files.
