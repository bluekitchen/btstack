#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

# micro-ecc of ESP32 tree was moved into components/bootloader/micro-ecc in v3.3
# we use our copy, but keep it private to not clash compiling the bootloader

COMPONENT_ADD_INCLUDEDIRS := \
	3rd-party/bluedroid/decoder/include \
	3rd-party/bluedroid/encoder/include \
	3rd-party/hxcmod-player \
	3rd-party/hxcmod-player/mods \
	../lwip/lwip/src/include \
	3rd-party/lwip/dhcp-server \
	3rd-party/md5 \
	3rd-party/yxml \
	src/classic \
	src \
	platform/embedded \
	platform/freertos \
	platform/lwip \
	include \

COMPONENT_PRIV_INCLUDEDIRS := \
	3rd-party/micro-ecc \

COMPONENT_SRCDIRS := \
	3rd-party/bluedroid/decoder/srce \
	3rd-party/bluedroid/encoder/srce \
	3rd-party/hxcmod-player \
	3rd-party/hxcmod-player/mods \
	../lwip/lwip/src/apps/http \
	3rd-party/lwip/dhcp-server \
	3rd-party/micro-ecc \
	3rd-party/md5 \
	src/ble/gatt-service \
	src/ble \
	src/mesh \
	src/ \
	platform/embedded \
	platform/freertos \
	platform/lwip \
	. \

ifdef CONFIG_IDF_TARGET_ESP32
	COMPONENT_SRCDIRS += src/classic 
endif

CFLAGS += -Wno-format
