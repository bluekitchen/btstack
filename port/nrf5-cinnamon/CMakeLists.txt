cmake_minimum_required(VERSION 3.18)

# Select the bare-metal toolchain before project() probes the compiler.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
if(NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER arm-none-eabi-gcc)
endif()
if(NOT CMAKE_ASM_COMPILER)
    set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
endif()
project(btstack-nrf5-cinnamon C ASM)

set(NRF5_SDK_ROOT "$ENV{NRF5_SDK_ROOT}" CACHE PATH "Path to the Nordic nRF5 SDK")
if(NOT EXISTS "${NRF5_SDK_ROOT}/modules/nrfx/mdk/system_nrf52.c")
    message(FATAL_ERROR "Set NRF5_SDK_ROOT to the nRF5 SDK directory")
endif()
get_filename_component(BTSTACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)

# Match the existing PCA10040 Makefile's ABI, memory layout and optimization.
add_compile_definitions(BOARD_PCA10040 BSP_DEFINES_ONLY CONFIG_GPIO_AS_PINRESET
    FLOAT_ABI_HARD NRF52 NRF52832_XXAA NRF52_PAN_74)
add_compile_options(-mcpu=cortex-m4 -mthumb -mabi=aapcs -mfloat-abi=hard
    -mfpu=fpv4-sp-d16 -g -ffunction-sections -fdata-sections)
add_compile_options("$<$<COMPILE_LANGUAGE:C>:-Os;-fno-strict-aliasing;-fno-builtin;-fshort-enums>")
add_link_options(-mcpu=cortex-m4 -mthumb -mabi=aapcs -mfloat-abi=hard
    -mfpu=fpv4-sp-d16 --specs=nano.specs -Wl,--gc-sections -Wl,-u,_write
    "-L${NRF5_SDK_ROOT}/modules/nrfx/mdk"
    "-T${CMAKE_CURRENT_SOURCE_DIR}/pca10040/armgcc/nrf52832_xxaa.ld")

btstack_declare_port(nrf5-cinnamon)
btstack_configure_port(CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/port/btstack_config.h")


set(PORT_INCLUDES
    "${BTSTACK_ROOT}/platform/embedded"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/port"
    "${CMAKE_CURRENT_SOURCE_DIR}/pca10040/config"
    "${NRF5_SDK_ROOT}/integration/nrfx" "${NRF5_SDK_ROOT}/integration/nrfx/legacy"
    "${NRF5_SDK_ROOT}/modules/nrfx" "${NRF5_SDK_ROOT}/modules/nrfx/hal"
    "${NRF5_SDK_ROOT}/modules/nrfx/mdk" "${NRF5_SDK_ROOT}/modules/nrfx/drivers/include"
    "${NRF5_SDK_ROOT}/components/boards" "${NRF5_SDK_ROOT}/components/drivers_nrf/nrf_soc_nosd"
    "${NRF5_SDK_ROOT}/components/libraries/bsp" "${NRF5_SDK_ROOT}/components/toolchain/cmsis/include"
    "${NRF5_SDK_ROOT}/components/libraries/util" "${NRF5_SDK_ROOT}/components/libraries/delay"
    "${NRF5_SDK_ROOT}/components/libraries/log" "${NRF5_SDK_ROOT}/components/libraries/log/src"
    "${NRF5_SDK_ROOT}/components/libraries/experimental_section_vars")
set(PORT_SOURCES main.c port/hal_timer_nrf5.c port/ll_nrf5.c port/radio_nrf5.c
    "${NRF5_SDK_ROOT}/modules/nrfx/mdk/system_nrf52.c"
    "${NRF5_SDK_ROOT}/components/boards/boards.c")
set(BASE_GROUPS core ble le-device-db-tlv tlv-none chipset-cinnamon
    platform-embedded-run-loop third-party-segger-rtt third-party-micro-ecc third-party-rijndael)

# Cinnamon currently supports only the examples built by the existing Makefile;
# encryption and central connections are not implemented by its controller.
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES gap_le_advertisements gatt_counter gatt_streamer_server)
btstack_filter_examples_for_port(EXAMPLES CANDIDATES ${EXAMPLES})
foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
 APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BASE_GROUPS} PORT_SOURCES ${PORT_SOURCES}
        # Match the legacy Makefile warning policy for the Cinnamon port.
        PRIVATE_COMPILE_OPTIONS -Wno-extra -Wno-switch-default
        PUBLIC_INCLUDE_DIRECTORIES ${PORT_INCLUDES})
    btstack_add_application_executable(${EXAMPLE} APPLICATION ${EXAMPLE}
        SOURCES port/syscalls.c "${NRF5_SDK_ROOT}/modules/nrfx/mdk/gcc_startup_nrf52.S")
    set_target_properties(${EXAMPLE} PROPERTIES SUFFIX .elf LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/pca10040/armgcc/nrf52832_xxaa.ld")
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} btstack_${EXAMPLE}_library c nosys m)
endforeach()
btstack_add_info_targets()
