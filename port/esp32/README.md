# BTstack Port for the Espressif ESP32 Platform

Status: Basic port incl. all examples. BTstack runs on dedicated FreeRTOS thread. Multi threading (calling BTstack functions from a different thread) is not supported.

## Setup

- Follow [Espressif IoT Development Framework (ESP-IDF) setup](https://github.com/espressif/esp-idf) to install XTensa toolchain and the ESP-IDF. 
- Make sure your checkout is newer than 4654278b1bd6b7f7f55013f7edad76109f7ee944 from Aug 25th, 2017
- In port/esp32/template, configure the serial port for firmware upload as described in the ESP-IDF setup guides.

## Usage

In port/esp32, run

	./integrate_btstack.py

The script will copy parts of the BTstack tree into the ESP-IDF as $IDF_PATH/components/btstack and then create project folders for all examples.

Each example project folder, e.g. port/esp32/examples/spp_and_le_counter, contains a Makefile. Please run the command again after updating the BTstack tree (e.g. by git pull) to also update the copy in the ESP-IDF.

To compile an example, run:

	idf.py build


To upload the binary to your device, run:

	idf.py flash


To get the debug output, run:

	idf.py monitor

You can quit the monitor with CTRL-].

## Old Make Versions

Compilation fails with older versions of the make tool, e.g. make 3.8.1 (from 2006) provided by the current Xcode 9 on macOS or Ubuntu 14.04.1 LTS.

Interestingly, if you run make a second time, it completes the compilation.

## Configuration

The sdkconfig of the example template disables the original Bluedroid stack by disabling the `CONFIG_BT_BLUEDROID_ENABLED` and enabling `CONFIG_BT_CONTROLLER_ONLY` kconfig option.

## Limitations

### Issues with the Bluetooth Controller Implementation

There are different issues in the Bluetooth Controller of the ESP32 that is provided in binary. We've submitted appropriate issues on the GitHub Issues page here: https://github.com/espressif/esp-idf/issues/created_by/mringwal

### Audio playback

Audio playback is implemented by `btstack_audio_esp32.c` and supports generic I2S codecs as well as the ES8388 on the [ESP32 LyraT v4.3](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/board-esp32-lyrat-v4.3.html) devkit.

It uses the first I2S interface with the following pin out:

ESP32 pin | I2S Pin
----------|---------
GPIO0     | MCK
GPIO5     | BCLK
GPIO25    | LRCK
GPIO26    | DOUT
GPIO35    | DIN

If support for the LyraT v4.3 is enabled, e.g. via menuconfig - Example Board Configuration --> ESP32 board --> ESP32-LyraT V4.3, CONFIG_ESP_LYRAT_V4_3_BOARD gets defined and the ES8388 will be configured as well.

We've also used the MAX98357A on the [Adafruit breakout board](https://www.adafruit.com/product/3006). The simplest example is the mod_player, which plays back an 8 kB sound file and the a2dp_sink_demo that implements a basic Bluetooth loudspeaker.


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
