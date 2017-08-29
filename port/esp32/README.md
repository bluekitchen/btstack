# BTstack Port for the Espressif ESP32 Platform

Status: Basic port incl. all examples. BTstack runs on dedicated FreeRTOS thread. Multi threading (calling BTstack functions from a different thread) is not supported.

## Setup

- Follow [Espressif IoT Development Framework (ESP-IDF) setup](https://github.com/espressif/esp-idf) to install XTensa toolchain and the ESP-IDF. 
- Make sure your checkout is newer than 4654278b1bd6b7f7f55013f7edad76109f7ee944 from Aug 25th, 2017
- In port/esp32/template, configure the serial port for firmware upload as described in the ESP-IDF setup guides.

## Usage

In port/esp32, run

	./create_examples.py

The script will create project folders for all examples. Each example project folder, e.g. port/esp32/spp_and_le_counter, contains a Makefile.  

To compile the example, run:

	make


To upload the binary to your device, run:

	make flash


To get the debug output, run:

	make monitor

You can quit the monitor with CTRL-].

## Configuration

The sdkconfig of the example template disables the original Bluedroid stack by disabling the CONFIG_BLUEDROID_ENABLED kconfig option.

## Limitations

### Bug in Host Controller to Host Flow Control implementation
The Link Layer in the ESP32 does not reset the supervision timeout while the Host does not accept new packets (due to slow processing etc..), which will triggger a disconnect based on Connection Timeout (reason 8). See https://github.com/espressif/esp-idf/issues/644#issuecomment-325251315 

For most applications, this won't be an issue, but please keep it in mind. 

### Multi-Threading

BTstack is not thread-safe, but you're using a multi-threading OS. Any function that is called from BTstack, e.g. packet handlers, can directly call into BTstack without issues. For other situations, you need to provide some general 'do BTstack tasks' function and trigger BTstack to execute it on its own thread.
To call a function from the BTstack thread, there are currently two options:

- *btstack_run_loop_freertos_execute_code_on_main_thread* allows to directly schedule a function callback, i.e. 'do BTstack tasks' function, from the BTstack thread.
- Setup a BTstack Data Source (btstack_data_source_t):
 Set 'do BTstack tasks' function as its process function and enable its polling callback (DATA_SOURCE_CALLBACK_POLL). The process function will be called in every iteration of the BTstack Run Loop. To trigger a run loop iteration, you can call *btstack_run_loop_freertos_trigger*.

With both options, the called function should check if there are any pending BTstack tasks and execute them.

The 'run on main thread' method is only provided by a few ports and requires a queue to store the calls. This should be used with care, since calling it multiple times could cause the queue to overflow.

We're considering different options to make BTstack thread-safe, but for now, please use one of the suggested options.

### Acknowledgments

First HCI Reset was sent to Bluetooth chipset by [@mattkelly](https://github.com/mattkelly)
