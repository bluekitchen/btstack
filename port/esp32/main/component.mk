#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the 
# src/ directory, compile them and link them into lib(subdirectory_name).a 
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#
BTSTACK_ROOT := ../../..

# Examples
#include ${BTSTACK_ROOT}/example/Makefile.inc

COMPONENT_ADD_LDFLAGS := -l$(COMPONENT_NAME) $(COMPONENT_PATH)/../components/libbtdm_app/libbtdm_app.a $(COMPONENT_PATH)/../components/libcoexist/libcoexist.a

COMPONENT_ADD_INCLUDEDIRS := $(BTSTACK_ROOT)/src/ble $(BTSTACK_ROOT)/src $(BTSTACK_ROOT)/platform/embedded .

COMPONENT_SRCDIRS := $(BTSTACK_ROOT)/src/ble $(BTSTACK_ROOT)/src/ $(BTSTACK_ROOT)/platform/embedded .

CFLAGS += -Wno-format

