#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the 
# src/ directory, compile them and link them into lib(subdirectory_name).a 
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#
BTSTACK_ROOT := ../../../../..

COMPONENT_ADD_INCLUDEDIRS := \
	${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include \
	${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include \
	$(BTSTACK_ROOT)/src/classic \
	$(BTSTACK_ROOT)/src/ble/gatt-service \
	$(BTSTACK_ROOT)/src/ble \
	$(BTSTACK_ROOT)/src/classic \
	$(BTSTACK_ROOT)/src \
	$(BTSTACK_ROOT)/platform/freertos \
	include \

COMPONENT_SRCDIRS := \
	$(BTSTACK_ROOT)/src/ble/gatt-service \
	$(BTSTACK_ROOT)/src/ble \
	$(BTSTACK_ROOT)/src/classic \
	$(BTSTACK_ROOT)/src/ \
	$(BTSTACK_ROOT)/platform/freertos \
	. \

CFLAGS += -Wno-format

