# BTstack Port for the Espressif ESP32 Platform

Status: Basic port incl. all examples. BTstack runs on dedicated FreeRTOS thread. Multi threading (calling BTstack functions from a different thread) is not supported.

## Setup

- Follow [Espressif IoT Development Framework (ESP-IDF) setup](https://github.com/espressif/esp-idf) to install XTensa toolchain and the ESP-IDF. 
- Currently used for testing: ESP-IDF v4.4 or v5.0

## Usage

In port/esp32, run

	./integrate_btstack.py

The script will copy parts of the BTstack tree into the ESP-IDF as $IDF_PATH/components/btstack and then create project folders for all examples.

Each example project folder, e.g. port/esp32/examples/spp_and_le_counter, contains a CMake project file. Please run the `integrate_btstack.py` 
command again after updating the BTstack tree (e.g. by git pull) to also update the copy in the ESP-IDF.

The examples are configure for the original ESP32. IF you want to use the newer ESP32-C3 or ESP32-S3 - both only support Bluetooth LE - you need to:
1. set target:

    `idf.py set-target esp32c3`

    or 

    `idf.py set-target esp32s3`

2. re-enable Bluetooth Controller via menuconfig
   
   1. `idf.py menuconfig`
   2. select `Component Config`
   3. select `Bluetooth` and enable
   4. select `Bluetooth Host`
   5. select `Controller Only`
   6. exit and save config

To compile an example, run:

    idf.py


To upload the binary to your device, run:

	idf.py -p PATH-TO-DEVICE flash


To get debug output, run:

	idf.py monitor

You can quit the monitor with CTRL-].

## Configuration

The sdkconfig of the example template disables the original Bluedroid stack by disabling the CONFIG_BLUEDROID_ENABLED kconfig option.

## Limitations

### Issues with the Bluetooth Controller Implementation

There are different issues in the Bluetooth Controller of the ESP32 that is provided in binary. We've submitted appropriate issues on the GitHub Issues page here: https://github.com/espressif/esp-idf/issues/created_by/mringwal

### Audio playback

Audio playback is implemented by `btstack_audio_esp32_v4.c` resp. `btstack_audio_esp32_v5.c` and supports generic I2S codecs as well as the ES8388 on the 
[ESP32 LyraT v4.3](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/board-esp32-lyrat-v4.3.html) devkit.

Due to the various ESP32 variants, please double check the I2S Configuration in the mentioned audio file and verify that the correct menuconfig options are set.

For a generic ESP32, the first I2S interface is used with with the following pin out:

ESP32 pin | I2S Pin
----------|---------
GPIO0     | MCK
GPIO5     | BCLK
GPIO25    | LRCK
GPIO26    | DOUT
GPIO35    | DIN

If support for the LyraT v4.3 is enabled via menuconfig - Example Board Configuration --> ESP32 board --> ESP32-LyraT V4.3, CONFIG_ESP_LYRAT_V4_3_BOARD gets defined and the ES8388 will be configured as well.

We've also used the MAX98357A on the [Adafruit breakout board](https://www.adafruit.com/product/3006). 
The simplest audio example is the `mod_player`, which plays back an 8 kB sound file and the a2dp_sink_demo that implements a basic Bluetooth loudspeaker.
You can test the audio input with the `audio_duplex` example.

## ESP32 printf/log

The BTstack port setups a buffered output for printf/ESP log macros. However, if that gets full, the main thread will get blocked. If you're playing audio, e.g. a2dp_sink_demo, and a lot of text
is sent over the UART, this will cause audio glitches as the I2S DMA driver isn't re-filled on time. If this happens, you can increase the UART baudrate, reduce the debug output or increase the output
buffer size - or any combination of these. :)

### Multi-Threading

BTstack is not thread-safe, but you're using a multi-threading OS. Any function that is called from BTstack, e.g. packet handlers, can directly call into BTstack without issues. For other situations, you need to provide some general 'do BTstack tasks' function and trigger BTstack to execute it on its own thread.
To call a function from the BTstack thread, you can use *btstack_run_loop_execute_on_main_thread* allows to directly schedule a function callback, 
i.e. 'do BTstack tasks' function, from the BTstack thread. The called function should check if there are any pending BTstack tasks and execute them.

We're considering different options to make BTstack thread-safe, but for now, please use one of the suggested options.

### Acknowledgments

First HCI Reset was sent to Bluetooth chipset by [@mattkelly](https://github.com/mattkelly)
