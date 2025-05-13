# Needed to include the lwIP's HTTP app, which is not included by default.
set(IDF_PATH $ENV{IDF_PATH})
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)

include(BTstackUtils)

if( NOT DEFINED BTSTACK_ROOT )
    if( NOT DEFINED ENV{BTSTACK_ROOT} )
        set(BTSTACK_ROOT ${CMAKE_SOURCE_DIR}/../..)
    else()
        set(BTSTACK_ROOT $ENV{BTSTACK_ROOT})
    endif()
    set(BTSTACK_ROOT "${BTSTACK_ROOT}" PARENT_SCOPE)
endif()
message( "btstack supposed location: \"${BTSTACK_ROOT}\"" )

if (CMAKE_HOST_WIN32)
    string(REPLACE "\\" "/" IDF_PATH $ENV{IDF_PATH})
endif()

set(include_dirs
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
    ${BTSTACK_ROOT}/3rd-party/hxcmod-player
    ${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods
    ${BTSTACK_ROOT}/3rd-party/lc3-google/include
    ${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server
    ${BTSTACK_ROOT}/3rd-party/md5
    ${BTSTACK_ROOT}/3rd-party/qr-code-generator
    ${BTSTACK_ROOT}/3rd-party/yxml
    ${BTSTACK_ROOT}/src/classic
    ${BTSTACK_ROOT}/src/ble/gatt-service
    ${BTSTACK_ROOT}/src/ble
    ${BTSTACK_ROOT}/src/le-audio
    ${BTSTACK_ROOT}/src/le-audio/gatt-service
    ${BTSTACK_ROOT}/src/classic
    ${BTSTACK_ROOT}/src
    ${BTSTACK_ROOT}/platform/embedded
    ${BTSTACK_ROOT}/platform/freertos
    ${BTSTACK_ROOT}/platform/lwip
    ${IDF_PATH}/components/lwip/lwip/src/include
    include)

set(src_dirs
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce
    ${BTSTACK_ROOT}/3rd-party/hxcmod-player
    ${BTSTACK_ROOT}/3rd-party/hxcmod-player/mods
    ${BTSTACK_ROOT}/3rd-party/lc3-google/src
    ${BTSTACK_ROOT}/3rd-party/lwip/dhcp-server
    ${BTSTACK_ROOT}/3rd-party/md5
    ${BTSTACK_ROOT}/3rd-party/micro-ecc
    ${BTSTACK_ROOT}/3rd-party/qr-code-generator
    ${BTSTACK_ROOT}/3rd-party/yxml
    ${BTSTACK_ROOT}/src/ble/gatt-service
    ${BTSTACK_ROOT}/src/ble
    ${BTSTACK_ROOT}/src/le-audio
    ${BTSTACK_ROOT}/src/le-audio/gatt-service
    ${BTSTACK_ROOT}/src/mesh
    ${BTSTACK_ROOT}/src
#    ${BTSTACK_ROOT}/platform/embedded
    ${BTSTACK_ROOT}/platform/freertos
    ${BTSTACK_ROOT}/platform/lwip
    ${IDF_PATH}/components/lwip/lwip/src/apps/http
    .)

set( exclude_srcs )
if(${CONFIG_BTSTACK_AUDIO})
    if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
        list(APPEND exclude_srcs "btstack_audio_esp32_v4.c")
    else()
        list(APPEND exclude_srcs "btstack_audio_esp32_v5.c")
    endif()
else()
    list(APPEND exclude_srcs "btstack_audio_esp32_v4.c" "btstack_audio_esp32_v5.c")
endif()

if(CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID)
    list(APPEND exclude_srcs "btstack_transport_vhci.c")
else()
    list(APPEND exclude_srcs "btstack_transport_hosted.c")
endif()

if(CONFIG_IDF_TARGET_ESP32)
    list(APPEND src_dirs ${BTSTACK_ROOT}/src/classic)
endif()

set(priv_include_dirs
    ${BTSTACK_ROOT}/3rd-party/micro-ecc
)

set(priv_requires
        "nvs_flash"
        "bt"
        "driver"
        "lwip"
        "vfs"
        )

idf_component_register(
    SRC_DIRS "${src_dirs}"
    EXCLUDE_SRCS "${exclude_srcs}"
    INCLUDE_DIRS "${include_dirs}"
    PRIV_INCLUDE_DIRS "${priv_include_dirs}"
    PRIV_REQUIRES "${priv_requires}"
    REQUIRES btstack_config
)

