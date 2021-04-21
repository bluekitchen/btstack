
NRF5_SDK_ROOT ?= /Users/mringwal/Projects/Nordic/nRF5_SDK_17.0.2_d674dde

# unset GNU_INSTALL_ROOT will select arm-none-eabi-gcc from your PATH
export GNU_INSTALL_ROOT ?=

OUTPUT_DIRECTORY := build

SDK_ROOT := $(NRF5_SDK_ROOT)
PROJ_DIR := ../..

BTSTACK_ROOT = ../../../..

LINKER_SCRIPT  := nrf52832_xxaa.ld

# Source files common to all targets
C_SOURCES = \
  $(SDK_ROOT)/modules/nrfx/mdk/system_nrf52.c \
  $(SDK_ROOT)/components/boards/boards.c \
  $(PROJ_DIR)/main.c \
  $(PROJ_DIR)/port/hal_timer_nrf5.c \
  $(PROJ_DIR)/port/ll_nrf5.c \
  $(PROJ_DIR)/port/radio_nrf5.c \
  ${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c \
  ${BTSTACK_ROOT}/3rd-party/rijndael/rijndael.c \
  ${BTSTACK_ROOT}/3rd-party/segger-rtt/SEGGER_RTT.c \
  ${BTSTACK_ROOT}/3rd-party/segger-rtt/SEGGER_RTT_printf.c \
  ${BTSTACK_ROOT}/3rd-party/segger-rtt/SEGGER_RTT_Syscalls_GCC.c \
  ${BTSTACK_ROOT}/chipset/controller/controller.c \
  ${BTSTACK_ROOT}/chipset/controller/hci_event.c \
  ${BTSTACK_ROOT}/chipset/controller/hopping.c \
  ${BTSTACK_ROOT}/platform/embedded/btstack_run_loop_embedded.c \
  ${BTSTACK_ROOT}/platform/embedded/hci_dump_embedded_stdout.c \
  ${BTSTACK_ROOT}/platform/embedded/hci_dump_segger_rtt_stdout.c \
  ${BTSTACK_ROOT}/platform/embedded/hci_dump_segger_rtt_binary.c \
  ${BTSTACK_ROOT}/src/ad_parser.c \
  ${BTSTACK_ROOT}/src/ble/att_db.c \
  ${BTSTACK_ROOT}/src/ble/att_dispatch.c \
  ${BTSTACK_ROOT}/src/ble/att_server.c \
  ${BTSTACK_ROOT}/src/ble/gatt-service/ancs_client.c \
  ${BTSTACK_ROOT}/src/ble/gatt-service/battery_service_client.c \
  ${BTSTACK_ROOT}/src/ble/gatt-service/battery_service_server.c \
  ${BTSTACK_ROOT}/src/ble/gatt-service/device_information_service_client.c \
  ${BTSTACK_ROOT}/src/ble/gatt-service/device_information_service_server.c \
  ${BTSTACK_ROOT}/src/ble/gatt-service/hids_device.c \
  ${BTSTACK_ROOT}/src/ble/gatt_client.c \
  ${BTSTACK_ROOT}/src/ble/le_device_db_memory.c \
  ${BTSTACK_ROOT}/src/ble/le_device_db_tlv.c \
  ${BTSTACK_ROOT}/src/ble/sm.c \
  ${BTSTACK_ROOT}/src/btstack_audio.c \
  ${BTSTACK_ROOT}/src/btstack_crypto.c \
  ${BTSTACK_ROOT}/src/btstack_hid_parser.c \
  ${BTSTACK_ROOT}/src/btstack_linked_list.c \
  ${BTSTACK_ROOT}/src/btstack_linked_queue.c \
  ${BTSTACK_ROOT}/src/btstack_memory.c \
  ${BTSTACK_ROOT}/src/btstack_memory_pool.c \
  ${BTSTACK_ROOT}/src/btstack_resample.c \
  ${BTSTACK_ROOT}/src/btstack_ring_buffer.c \
  ${BTSTACK_ROOT}/src/btstack_run_loop.c \
  ${BTSTACK_ROOT}/src/btstack_tlv.c \
  ${BTSTACK_ROOT}/src/btstack_tlv_none.c \
  ${BTSTACK_ROOT}/src/btstack_util.c \
  ${BTSTACK_ROOT}/src/hci.c \
  ${BTSTACK_ROOT}/src/hci_cmd.c \
  ${BTSTACK_ROOT}/src/hci_dump.c \
  ${BTSTACK_ROOT}/src/hci_transport_h4.c \
  ${BTSTACK_ROOT}/src/l2cap.c \
  ${BTSTACK_ROOT}/src/l2cap_signaling.c \

ASM_SOURCES= \
  $(SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52.S \

# Include folders common to all targets
INC_FOLDERS += \
  $(PROJ_DIR) \
  $(SDK_ROOT)/integration/nrfx \
  $(SDK_ROOT)/integration/nrfx/legacy \
  $(SDK_ROOT)/modules/nrfx \
  $(SDK_ROOT)/modules/nrfx/hal \
  $(SDK_ROOT)/modules/nrfx/mdk \
  $(SDK_ROOT)/modules/nrfx/drivers/include/ \
  $(SDK_ROOT)/components/boards \
  $(SDK_ROOT)/components/drivers_nrf/nrf_soc_nosd \
  $(SDK_ROOT)/components/libraries/bsp \
  $(SDK_ROOT)/components/toolchain/cmsis/include \
  $(SDK_ROOT)/components/libraries/util \
  $(SDK_ROOT)/components/libraries/delay \
  $(SDK_ROOT)/components/libraries/log/ \
  $(SDK_ROOT)//components/libraries/log/src \
  $(SDK_ROOT)/components/libraries/experimental_section_vars/ \
  $(BTSTACK_ROOT)/3rd-party/segger-rtt \
  $(BTSTACK_ROOT)/chipset/controller \
  $(BTSTACK_ROOT)/src/ \
  $(BTSTACK_ROOT)/platform/embedded \
  ../config \
  ../../port \
  $(OUTPUT_DIRECTORY)\

# Libraries common to all targets
LIB_FILES += \

# Optimization flags
# OPT = -O3 -g3
# Uncomment the line below to enable link time optimization
#OPT += -flto
OPT = -Os -g

# C flags common to all targets
CFLAGS += $(OPT)
CFLAGS += -DBOARD_PCA10040
CFLAGS += -DBSP_DEFINES_ONLY
CFLAGS += -DCONFIG_GPIO_AS_PINRESET
CFLAGS += -DFLOAT_ABI_HARD
CFLAGS += -DNRF52
CFLAGS += -DNRF52832_XXAA
CFLAGS += -DNRF52_PAN_74
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -Wall -Werror
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in a separate section, this allows linker to discard unused ones
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums

# C++ flags common to all targets
CXXFLAGS += $(OPT)
# Assembler flags common to all targets
ASMFLAGS += -g3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
ASMFLAGS += -DBOARD_PCA10040
ASMFLAGS += -DBSP_DEFINES_ONLY
ASMFLAGS += -DCONFIG_GPIO_AS_PINRESET
ASMFLAGS += -DFLOAT_ABI_HARD
ASMFLAGS += -DNRF52
ASMFLAGS += -DNRF52832_XXAA
ASMFLAGS += -DNRF52_PAN_74

# Linker flags
LDFLAGS += $(OPT)
LDFLAGS += -mthumb -mabi=aapcs -L$(SDK_ROOT)/modules/nrfx/mdk -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# let linker dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs

# Add standard libraries at the very end of the linker input, after all objects
# that may need symbols provided by these libraries.
LIB_FILES += -lc -lnosys -lm

# default action: build all
EXAMPLES = 					\
	gap_le_advertisements   \
	gatt_counter            \
	gatt_streamer_server    \
# require encryption:
#	hog_keyboard_demo       \
#	hog_mouse_demo          \
#	sm_pairing_peripheral   \

GATT_FILES = \
	gatt_counter.gatt \
	gatt_streamer_server.gatt \
# require encryption:
#	hog_keyboard_demo.gatt \
#	hog_mouse_demo.gatt \
#	sm_pairing_peripheral.gatt \

# list of objects
OBJECTS = $(addprefix $(OUTPUT_DIRECTORY)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(OUTPUT_DIRECTORY)/,$(notdir $(ASM_SOURCES:.S=.o)))
vpath %.S $(sort $(dir $(ASM_SOURCES)))

.PHONY: default help

# VPATH to find .gatt files
VPATH += ${BTSTACK_ROOT}/example

# Default target - first one defined
default: \
	$(OBJECTS) \
	$(addprefix $(OUTPUT_DIRECTORY)/,$(GATT_FILES:.gatt=.h)) \
	$(addprefix $(OUTPUT_DIRECTORY)/,$(EXAMPLES:=.elf)) \
	$(addprefix $(OUTPUT_DIRECTORY)/,$(EXAMPLES:=.jdebug)) \

$(OUTPUT_DIRECTORY)/%.h: %.gatt | $(OUTPUT_DIRECTORY)
	python3 ${BTSTACK_ROOT}/tool/compile_gatt.py $< $@

$(OUTPUT_DIRECTORY)/%.o: %.S Makefile | $(OUTPUT_DIRECTORY)
	$(CC) -c $(ASMFLAGS) $< -o $@

$(OUTPUT_DIRECTORY)/%.o: %.c Makefile | $(OUTPUT_DIRECTORY)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(OUTPUT_DIRECTORY)/$(notdir $(<:.c=.lst)) $< -o $@

$(OUTPUT_DIRECTORY)/%.elf: Makefile $(OBJECTS) $(OUTPUT_DIRECTORY)/%.o
	$(CC) $(filter-out Makefile,$^)  $(LDFLAGS) -o $@
	$(SIZE) $@

$(OUTPUT_DIRECTORY)/%.jdebug: ozone.jdebug | $(OUTPUT_DIRECTORY)
	sed -e "s|EXAMPLE|$(basename $(notdir $@))|" $<  > $@

TEMPLATE_PATH := $(SDK_ROOT)/components/toolchain/gcc

include $(TEMPLATE_PATH)/Makefile.common

# Convert INC_FOLDERS into CFLAGS
$(eval INC_PATHS := $(call get_inc_paths, $(INC_FOLDERS) $(call target_specific, INC_FOLDERS, $(1))))
CFLAGS += ${INC_PATHS}
