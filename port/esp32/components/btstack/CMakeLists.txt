set(COMPONENT_ADD_INCLUDEDIRS "3rd-party/bluedroid/decoder/include"
                              "3rd-party/bluedroid/encoder/include"
                              "3rd-party/hxcmod-player"
                              "3rd-party/hxcmod-player/mods"
                              "3rd-party/md5"
                              "3rd-party/yxml"
                              "src/classic"
                              "src/ble/gatt-service"
                              "src/ble"
                              "src/classic"
                              "src"
                              "platform/embedded"
                              "platform/freertos"
                              "include")

set(COMPONENT_SRCDIRS "3rd-party/bluedroid/decoder/srce"
                      "3rd-party/bluedroid/encoder/srce"
                      "3rd-party/hxcmod-player"
                      "3rd-party/hxcmod-player/mods"
                      "3rd-party/md5"
                      "src/ble/gatt-service"
                      "src/ble"
                      "src/classic"
                      "src/"
                      "platform/freertos"
                      ".")

set(COMPONENT_REQUIRES "micro-ecc" "nvs_flash" "bt")
register_component()

