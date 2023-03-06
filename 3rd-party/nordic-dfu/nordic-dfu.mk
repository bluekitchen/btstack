# EDIT HERE:
MK_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
CURDIR := $(patsubst %/, %, $(dir $(MK_PATH)))

C_SOURCES_NORDIC_DFU := $(CURDIR)/port/nrf_dfu_ble.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/port/nrf_dfu_flash_port.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/src/nrf_dfu.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/src/nrf_dfu_transport.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/src/nrf_dfu_req_handler.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/src/nrf_dfu_settings.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/src/nrf_dfu_utils.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/src/dfu-cc.pb.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/src/nrf_dfu_flash.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/src/nrf_dfu_validation.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/src/nrf_dfu_handling_error.c
C_SOURCES_NORDIC_DFU += $(CURDIR)/src/crc32.c

C_INCLUDES_NORDIC_DFU := -I$(CURDIR)/port
C_INCLUDES_NORDIC_DFU += -I$(CURDIR)/src
C_INCLUDES_NORDIC_DFU += -I$(CURDIR)/src/nano-pb
C_INCLUDES_NORDIC_DFU += -I$(CURDIR)/tools
################################################################################