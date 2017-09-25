#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

# micro-ecc of WICED tree used for SECP256R1 in LE Secure Connections
COMPONENT_DEPENDS += micro-ecc

COMPONENT_ADD_INCLUDEDIRS := \
	3rd-party/bluedroid/decoder/include \
	3rd-party/bluedroid/encoder/include \
	3rd-party/hxcmod-player \
	3rd-party/hxcmod-player/mods \
	src/classic \
	src/ble/gatt-service \
	src/ble \
	src/classic \
	src \
	platform/embedded \
	platform/freertos \
	include \

COMPONENT_SRCDIRS := \
	3rd-party/bluedroid/decoder/srce \
	3rd-party/bluedroid/encoder/srce \
	3rd-party/hxcmod-player \
	3rd-party/hxcmod-player/mods \
	src/ble/gatt-service \
	src/ble \
	src/classic \
	src/ \
	platform/freertos \
	. \

CFLAGS += -Wno-format
