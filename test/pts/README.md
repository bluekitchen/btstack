# Testing tools

The provide Makefile and CMakeLists.txt create various tools used to test BTstack.
The test tools run on Linux and Mac systems with a USB Bluetooth Controller.

The Audio tests (`avdtp_source_test.c` and `avdtp_sink_test`) support the folowing audio codecs: SBC, AAC, aptX, and LDAC.
Support for non-mandatory codecs is only included in the CMake build. 
These audio codecs are provided by the open-source projects below, but cannot be used for products without licensing/testing by the respective owners.

## Dependencies

- [libusb](https://libusb.info)
- AAC: A2DP Sind + Source
  - [Debian package: libfdk-aac-dev](libfdk-aac-dev)
  - [Mac Homebrew: fdk-aac]
- aptX (HD): A2DP Sink + Source
  - [Github: libopenaptx](https://github.com/mringwal/libopenaptx)
  - CMake project
  - `mkdir build; cd build; cmake ..; make install`
- LDAC Encoder: A2DP Source
  - [Github: ldacBT](https://github.com/EHfive/ldacBT)
  - CMake project
  - `mkdir build; cd build; cmake ..; make install`
- LDAC Decoder: A2DP Sink
  - [Github: ldacdec](https://github.com/mringwal/libldacdec)
  - Automake project
  - `./bootstrap.sh; ./configure; make install`
 