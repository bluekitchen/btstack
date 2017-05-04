# BTstack Port for the Espressif ESP32 Platform

Status: Basic port incl. all examples. BTstack runs on dedicated FreeRTOS thread. Multi threading not supported.

## Setup

- Follow [Espressif IoT Development Framework (IDF) setup](https://github.com/espressif/esp-idf) to install xtensa toolchain.
- Checkout our [esp-idf fork](https://github.com/mringwal/esp-idf) make IDF_PATH point to it.
- In port/esp32, configure serial port for firmware upload as described before
	
## Usage

In port/esp32, run

	./create_examples.py

Now, it creates project folders for all examples. Inside an example, e.g. in port/esp32/spp_and_le_counter, run

	make

to compile the example

Run 
	
	make flash

to upload it to your device.

Run

	make monitor

to get the debug output. You can quit the monitor with CTRL-]

## Limitations

### Broken Host Controller to Host Flow Control
There's currently no way to let the virtual HCI (VHCI) of the ESP32 know when data cannot be processed fast enough. For most applications, this won't be an issue, but please keep it in mind. See https://github.com/espressif/esp-idf/issues/480

### Multi-threading

BTstack is not thread-safe, but you're using a multi-threading OS. To call the BTstack API from other threads, there are currently two options:

- *btstack_run_loop_freertos_single_threaded_execute_code_on_main_thread* allows to schedule a function callback from the BTstack thread. This function can call any BTstack function. Also, any function that is called from BTstack e.g. packet handler can directly call into BTstack without issues.
- Setup a BTstack Data Source (btstack_data_source_t) and enable the polling callback (DATA_SOURCE_CALLBACK_POLL). The process function set in the data source will be called in every iteration of the BTstack Run Loop. To trigger a run loop iteration from a different thread, you can call *btstack_run_loop_freertos_trigger*

With both options, the called function should check if there are any pending BTstack calls and execute them. The 'run on main thread' method is only provided by a few ports and requires a queue to store the calls. This should be used with care, since calling it multiple times could cause the queue to overflow. 
We're considering different options to make BTstack thread-safe, but for now, please use one of the suggested options.

### Acknowledgments

First HCI Reset was sent to Bluetooth chipset by [@mattkelly](https://github.com/mattkelly)
