# BTstack Port for the Espressif ESP32 Platform

## Setup

- Follow [Espressif IoT Development Framework (ESP-IDF) setup](https://github.com/espressif/esp-idf) to install XTensa toolchain and the ESP-IDF. 
- Currently used for testing: ESP-IDF v5.4

## Usage

Please note: Since BTstack v1.7, BTstack sources are not and should not be copied into `${ESP_IDF}/components` folder. 
Please remove `${ESP_IDF}/components/btstack`

The current build system allows to build all provided examples for all supported ESP32 versions.
By default, the 'gatt_counter' example will be build for the original ESP32.

To select example 'gatt_streamer_server' and the ESP32-C3:

   EXAMPLE='gatt_streamer_server' idf.py set-target esp32c3

To get a list of valid targets:

   idf.py set-target

To get a list of BTstack examples:

    idf.py list-examples


To change the example:

    idf.py fullclean

To compile the example, run:

    idf.py build


To upload the binary to your device, run:

	idf.py -p PATH-TO-DEVICE flash


To get debug output, run:

	idf.py monitor

You can quit the monitor with CTRL-].


### Configuration

The configuration file 'btstack_config.h' is provided by the main project via the `btstack_config` component in
`components/btstack_config/btstack_config.h`

### Integration into custom projects

The esp32 port (this folder) contains the components 'btstack' and 'btstack_config'. 
It can be compiled out-of-tree by setting the environment variable BTSTACK_ROOT, example:

    BTSTACK_ROOT=/path/to/btstack EXAMPLE='gatt_streamer_server' idf.py build


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
i.e. 'do BTstack tasks' function, from the BTstack thread. The called function then checks for pending BTstack tasks and executes them.
