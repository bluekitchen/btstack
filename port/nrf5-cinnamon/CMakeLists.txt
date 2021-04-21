cmake_minimum_required(VERSION 3.14)
project(n5rf-ll)

# nrf5 sdk root with nrfx
set(NRF5_SDK_ROOT /Users/mringwal/Projects/Nordic/nRF5_SDK_17.0.2_d674dde)

# list all sources
file(GLOB_RECURSE btstack_src_src ../../src/*.c)
file(GLOB_RECURSE btstack_src_hdr ../../src/*.h)
file(GLOB_RECURSE btstack_embedded_src ../../platform/embedded/src/*.c)
file(GLOB_RECURSE btstack_embedded_hdr ../../platform/embedded/src/*.h)
file(GLOB_RECURSE btstack_port_src *.c)
file(GLOB_RECURSE btstack_port_hdr *.h)
file(GLOB_RECURSE nrfx_src ${NRF5_SDK_ROOT}/modules/*.c)
file(GLOB_RECURSE nrfx_hdr ${NRF5_SDK_ROOT}/modules/*.h)

# Compile pca10040 port using Make
find_program(MAKE make)
add_custom_target(pca10040
        ALL
        WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/pca10040/armgcc
        COMMAND ${MAKE}
        SOURCES ${btstack_src_src} ${btstack_embedded_src} ${btstack_port_src} ${nrfx_src})

# fake executable for navigation in IDE
add_executable(port EXCLUDE_FROM_ALL
        ${btstack_src_src}
        ${btstack_src_hdr}
        ${btstack_embedded_src}
        ${btstack_embedded_hdr}
        ${btstack_port_src}
        ${btstack_port_hdr}
        ${nrfx_src}
        ${nrfx_hdr}
)
target_include_directories(port SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/port
        ../../3rd-party/segger-rtt
        ../../platform/embedded
        ../../chipset/cinnamon
        ../../src
        ${NRF5_SDK_ROOT}/components/boards
        ${NRF5_SDK_ROOT}/components/libraries/delay
        ${NRF5_SDK_ROOT}/modules/nrfx/drivers/include
        ${NRF5_SDK_ROOT}/modules/nrfx/mdk
        ${NRF5_SDK_ROOT}/modules/nrfx/hal
        ${NRF5_SDK_ROOT}/modules/nrfx
)
