cmake_minimum_required(VERSION 3.18)
project(BTstack-windows-h4)
set(BTSTACK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
include(${BTSTACK_ROOT}/tool/cmake/BTstack.cmake)
include(${BTSTACK_ROOT}/tool/cmake/BTstackFirmware.cmake)
btstack_declare_port(windows-h4)
btstack_collect_public_include_directories(BTSTACK_PUBLIC_INCLUDE_DIRECTORIES)
set(SOURCES_PORT main.c)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
    if(PORTAUDIO_FOUND)
        message("HAVE_PORTAUDIO")
        add_compile_definitions(HAVE_PORTAUDIO)
        btstack_declare_port_capability(windows-h4 CAPABILITY audio
            GROUPS platform-posix-audio-portaudio platform-posix-wav)
    endif()
endif()
btstack_declare_port_capability(windows-h4 CAPABILITY led
    GROUPS platform-posix-hal-led
    PUBLIC_INCLUDE_DIRECTORIES "${BTSTACK_ROOT}/platform/embedded")
set(CC256X_INIT_SCRIPT bluetooth_init_cc2564C_1.5.c)
include(${BTSTACK_ROOT}/chipset/cc256x/cc256x.cmake)
list(APPEND SOURCES_PORT ${CC256X_INIT_SCRIPT_SOURCES})
include(${BTSTACK_ROOT}/chipset/bcm/btstack_bcm_patchram.cmake)
btstack_add_firmware_download(TARGET bcm43430a1_patchram
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${BTSTACK_BCM43430A1_PATCHRAM_FILE}"
    URL "${BTSTACK_BCM43430A1_PATCHRAM_URL}" SHA256 "${BTSTACK_BCM43430A1_PATCHRAM_SHA256}")
btstack_add_firmware_download(TARGET cyw55310_patchram
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${BTSTACK_CYW55310_PATCHRAM_FILE}"
    URL "${BTSTACK_CYW55310_PATCHRAM_URL}" SHA256 "${BTSTACK_CYW55310_PATCHRAM_SHA256}")
set(BTSTACK_WINDOWS_H4_FIRMWARE_TARGETS bcm43430a1_patchram cyw55310_patchram)
set(BTSTACK_CONFIG_DEFINITIONS)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_CONFIG_DEFINITIONS HAVE_PORTAUDIO)
endif()
btstack_collect_port_configuration_macros(BTSTACK_CONFIG_MACROS
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/btstack_config.h" DEFINITIONS ${BTSTACK_CONFIG_DEFINITIONS})
btstack_set_port_configuration_macros(windows-h4 ${BTSTACK_CONFIG_MACROS})
include(${BTSTACK_ROOT}/example/requirements.cmake)
set(EXAMPLES ${btstack_declared_applications})
btstack_filter_examples_for_port(EXAMPLES PORT windows-h4 CANDIDATES ${EXAMPLES})
list(REMOVE_ITEM EXAMPLES mesh_node_demo)
set(BTSTACK_WINDOWS_H4_BASE_GROUPS
    ble le-device-db-tlv link-key-db-tlv tlv-none transport-h4 platform-windows
    chipset-bcm chipset-cc256x chipset-csr chipset-em9301 chipset-realtek
    chipset-stlc2500d chipset-tc3566x chipset-zephyr
    third-party-micro-ecc third-party-rijndael)
if(PORTAUDIO_FOUND)
    list(APPEND BTSTACK_WINDOWS_H4_BASE_GROUPS platform-posix-audio-portaudio)
endif()
foreach(EXAMPLE IN LISTS EXAMPLES)
    btstack_get_application_property(APPLICATION_SOURCES "${EXAMPLE}" SOURCES)
    btstack_get_application_property(APPLICATION_GATT_DB "${EXAMPLE}" GATT_DB)
    btstack_add_application_library(
        TARGET btstack_${EXAMPLE}_library
        PORT windows-h4 APPLICATION ${EXAMPLE}
        BASE_GROUPS ${BTSTACK_WINDOWS_H4_BASE_GROUPS}
        PORT_SOURCES ${SOURCES_PORT}
        PUBLIC_INCLUDE_DIRECTORIES ${BTSTACK_PUBLIC_INCLUDE_DIRECTORIES}
            "${CMAKE_CURRENT_SOURCE_DIR}" ${PORTAUDIO_INCLUDE_DIRS}
        PRIVATE_INCLUDE_DIRECTORIES
            "${BTSTACK_ROOT}/chipset/cc256x"
        LINK_DIRECTORIES ${PORTAUDIO_LIBRARY_DIRS}
        LINK_LIBRARIES ${PORTAUDIO_LIBRARIES})
    add_dependencies(btstack_${EXAMPLE}_library ${CC256X_INIT_SCRIPT_TARGETS})
    set(SOURCES_EXAMPLE)
    foreach(EXAMPLE_SOURCE IN LISTS APPLICATION_SOURCES)
        list(APPEND SOURCES_EXAMPLE "${EXAMPLE_SOURCE}")
    endforeach()
    add_executable(${EXAMPLE} ${SOURCES_EXAMPLE})
    if(APPLICATION_GATT_DB)
        message("example ${EXAMPLE} -- with GATT DB")
        btstack_target_gatt(${EXAMPLE} "${APPLICATION_GATT_DB}")
    else()
        message("example ${EXAMPLE}")
    endif()
    btstack_target_warnings(${EXAMPLE} STRICT)
    target_link_libraries(${EXAMPLE} PRIVATE btstack_${EXAMPLE}_library)
    add_dependencies(${EXAMPLE} ${BTSTACK_WINDOWS_H4_FIRMWARE_TARGETS})
endforeach()
btstack_add_info_targets(PORT windows-h4)
