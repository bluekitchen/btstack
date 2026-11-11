# Standalone POSIX H4 GATT counter

This small CMake project uses `btstack_add_library` to build the BTstack
groups needed by `gatt_counter.c` and its GATT database. It
uses a POSIX serial port and requires a C compiler, CMake, and Python 3; it
does not use libusb or audio libraries.

From the BTstack repository root:

```sh
cmake -S port/posix-h4/gatt-counter-standalone -B build-standalone
cmake --build build-standalone
./build-standalone/gatt_counter /dev/ttyACM0
```

Pass the serial device for an HCI controller already configured for H4 at
115200 baud with hardware flow control. This minimal example does not download
controller firmware or perform chipset-specific initialization. Adapt the
serial configuration in `main.c` for your controller. Pairing data is stored
in memory and is lost when the program exits.

The `CMakeLists.txt` selects the BTstack groups, creates the executable from
its own `main.c` and the existing GATT counter, and generates its GATT header.
It does not need an application declaration or port registration.
