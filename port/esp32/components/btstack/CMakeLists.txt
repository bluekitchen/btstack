# Needed to include the lwIP's HTTP app, which is not included by default.
set(IDF_PATH $ENV{IDF_PATH})

if (CMAKE_HOST_WIN32)
    string(REPLACE "\\" "/" IDF_PATH $ENV{IDF_PATH})
endif()

set(include_dirs
    "3rd-party/bluedroid/decoder/include"
    "3rd-party/bluedroid/encoder/include"
    "3rd-party/hxcmod-player"
    "3rd-party/hxcmod-player/mods"
    "3rd-party/lc3-google/include"
    "3rd-party/lwip/dhcp-server"
    "3rd-party/md5"
    "3rd-party/yxml"
    "src/classic"
    "src/ble/gatt-service"
    "src/ble"
    "src/classic"
    "src"
    "platform/embedded"
    "platform/freertos"
    "platform/lwip"
    "${IDF_PATH}/components/lwip/lwip/src/include"
    "include")

set(src_dirs
    "3rd-party/bluedroid/decoder/srce"
    "3rd-party/bluedroid/encoder/srce"
    "3rd-party/hxcmod-player"
    "3rd-party/hxcmod-player/mods"
    "3rd-party/lwip/dhcp-server"
    "3rd-party/md5"
    "3rd-party/micro-ecc"
    "3rd-party/yxml"
    "src/ble/gatt-service"
    "src/ble"
    "src/mesh"
    "src/"
    "platform/embedded"
    "platform/freertos"
    "platform/lwip"
    "${IDF_PATH}/components/lwip/lwip/src/apps/http"
    ".")

if(${CONFIG_BTSTACK_AUDIO})
    if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.0")
        set(exclude_srcs "btstack_audio_esp32_v4.c")
    else()
        set(exclude_srcs "btstack_audio_esp32_v5.c")
    endif()
else()
    set(exclude_srcs "btstack_audio_esp32_v4.c" "btstack_audio_esp32_v5.c")
endif()

if(CONFIG_IDF_TARGET_ESP32)
        list(APPEND src_dirs "src/classic")
endif()

set(priv_include_dirs
        "3rd-party/micro-ecc"
        )

set(priv_requires
        "nvs_flash"
        "bt"
        "driver"
        "lwip"
        "vfs"
        )

idf_component_register(SRC_DIRS "${src_dirs}"
                    EXCLUDE_SRCS "${exclude_srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    PRIV_INCLUDE_DIRS "${priv_include_dirs}"
                    PRIV_REQUIRES ${priv_requires})
