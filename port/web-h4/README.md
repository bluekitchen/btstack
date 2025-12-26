# BTstack on Web using WebSerial

This ports compiles BTstack into WebAssembly and allows to run it in a Chrome-based Browser.
It communicates to the Bluetooth Controller via the WebSerial API.

## Requirements
- Emscripten from https://emscripten.org

## Build instructions
- Get Emscripten repository somewhere:
    - `git clone https://github.com/emscripten-core/emsdk.git`
- make sure EMSDK points to the root of your Emscripten installation:
    - `export EMSDK=<path/to/your/emscripten/repository>`
- create build folder and change to it:
    - `mkdir ninja`
    - `cd ninja`
- configure:
    - `cmake .. \
        -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
        -DCMAKE_BUILD_TYPE=Release`
- build using your default CMake generator:
    - `ninja` or `make`

## Run Local Web Server
- `python3 -m http.server 8080`

## Test
- [Open local URL](http://localhost:8080/myprog.html)
- Force refresh of .js files: CMD+Alt+R on Safari, CMD+Shift+R on Chrome

## Supported Bluetooth Controllers
The baudrate is currently set to 115200 and there's no chipset detection.
Suitable Controllers are therefore:
- Controllers with USB CDC and no need for firmware upload:
  - e.g. BL654 with Bluetooth SIG PTS Firmware
- Controllers with regular UART that start up at 115200 and no need for firmware upload:
  - e.g. Ezurio IF820/CYW20820 with HCI Firmware.

## Status
Supported Controller start up and all examples work as expected. The 115200 baud are to low for A2DP.
There's no audio driver yet as well as no support for STDIO. In addition, TLV is not supported, so
bonding information cannot be stored. A better UI will come, too. :)
