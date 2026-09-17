cmake_minimum_required(VERSION 3.20)
project(dialer-btstack C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-function)
endif()

# ---------------------------------------------------------------------------
# BTSTACK_ROOT points at the BTstack library that hosts this dialer port.
# This port lives inside the BTstack fork at port/windows-dialer, so the
# library sources are two levels up. This replaces the old third_party/btstack
# git submodule the dialer-btstack repo used to carry.
# ---------------------------------------------------------------------------
get_filename_component(BTSTACK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Common Include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${BTSTACK_ROOT}/src
    ${BTSTACK_ROOT}/chipset/realtek
    ${BTSTACK_ROOT}/3rd-party/rijndael
    ${BTSTACK_ROOT}/3rd-party/yxml
    ${BTSTACK_ROOT}/3rd-party/md5
    ${BTSTACK_ROOT}/3rd-party/tinydir
    ${BTSTACK_ROOT}/3rd-party/micro-ecc
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include
)

# Core BTstack sources common across all operating systems
set(BTSTACK_CORE_SOURCES
    ${BTSTACK_ROOT}/src/btstack_memory.c
    ${BTSTACK_ROOT}/src/btstack_memory_pool.c
    ${BTSTACK_ROOT}/src/btstack_linked_list.c
    ${BTSTACK_ROOT}/src/btstack_ring_buffer.c
    ${BTSTACK_ROOT}/src/btstack_util.c
    ${BTSTACK_ROOT}/src/btstack_crypto.c
    ${BTSTACK_ROOT}/src/hci_event.c
    ${BTSTACK_ROOT}/src/btstack_tlv.c
    ${BTSTACK_ROOT}/src/btstack_audio.c
    ${BTSTACK_ROOT}/src/btstack_audio_generator.c
    ${BTSTACK_ROOT}/src/btstack_run_loop.c
    ${BTSTACK_ROOT}/src/btstack_run_loop_base.c
    ${BTSTACK_ROOT}/src/hci.c
    ${BTSTACK_ROOT}/src/hci_cmd.c
    ${BTSTACK_ROOT}/src/hci_dump.c
    ${BTSTACK_ROOT}/src/ad_parser.c
    ${BTSTACK_ROOT}/src/l2cap.c
    ${BTSTACK_ROOT}/src/l2cap_signaling.c
    ${BTSTACK_ROOT}/src/classic/btstack_link_key_db_tlv.c
    ${BTSTACK_ROOT}/src/classic/btstack_cvsd_plc.c
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_bluedroid.c
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_decoder_bluedroid.c
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_encoder_bluedroid.c
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_plc.c
    ${BTSTACK_ROOT}/src/classic/hfp_msbc.c
    ${BTSTACK_ROOT}/src/classic/sdp_util.c
    ${BTSTACK_ROOT}/src/classic/sdp_server.c
    ${BTSTACK_ROOT}/src/classic/sdp_client.c
    ${BTSTACK_ROOT}/src/classic/sdp_client_rfcomm.c
    ${BTSTACK_ROOT}/src/classic/rfcomm.c
    ${BTSTACK_ROOT}/src/classic/hfp.c
    ${BTSTACK_ROOT}/src/classic/hfp_hf.c
    ${BTSTACK_ROOT}/src/classic/hfp_codec.c
    # BLE Layer
    ${BTSTACK_ROOT}/src/ble/sm.c
    ${BTSTACK_ROOT}/src/ble/att_db.c
    ${BTSTACK_ROOT}/src/ble/att_server.c
    ${BTSTACK_ROOT}/src/ble/att_dispatch.c
    ${BTSTACK_ROOT}/src/ble/le_device_db_tlv.c
    ${BTSTACK_ROOT}/chipset/realtek/btstack_chipset_realtek.c
    ${BTSTACK_ROOT}/3rd-party/rijndael/rijndael.c
    ${BTSTACK_ROOT}/3rd-party/yxml/yxml.c
    ${BTSTACK_ROOT}/3rd-party/md5/md5.c
    ${BTSTACK_ROOT}/3rd-party/micro-ecc/uECC.c
    # Bluedroid SBC Decoder
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/alloc.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/bitalloc-sbc.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/bitalloc.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/bitstream-decode.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/decoder-oina.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/decoder-private.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/decoder-sbc.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/dequant.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/framing-sbc.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/framing.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/oi_codec_version.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/synthesis-8-generated.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/synthesis-dct8.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/synthesis-sbc.c
    # Bluedroid SBC Encoder
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_analysis.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_dct.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_dct_coeffs.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_encoder.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_mono.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_ste.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_enc_coeffs.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_packing.c
)

# Common BT Layer sources (dialer application glue)
set(BT_LAYER_SOURCES
    src/diag_logger.c
    src/bt/bt_controller.c
    src/bt/hfp_hf.c
    src/bt/sco_audio.c
)

# OS-Specific Audio & Bluetooth Transport Layer
if(WIN32)
    include_directories(
        ${BTSTACK_ROOT}/platform/windows
    )
    set(PLATFORM_BTSTACK_SOURCES
        ${BTSTACK_ROOT}/platform/windows/btstack_run_loop_windows.c
        ${BTSTACK_ROOT}/platform/windows/hci_transport_h2_winusb.c
        ${BTSTACK_ROOT}/platform/windows/btstack_stdin_windows.c
        ${BTSTACK_ROOT}/platform/windows/btstack_tlv_windows.c
        ${BTSTACK_ROOT}/platform/windows/hci_dump_windows_fs.c
    )
    set(AUDIO_LAYER_SOURCES
        src/audio/wasapi_render.cpp
        src/audio/wasapi_capture.cpp
        src/audio/call_recorder.c
    )
    set(PLATFORM_LIBS
        winusb.lib
        setupapi.lib
        ole32.lib
        avrt.lib
        ksuser.lib
        ws2_32.lib
    )
elseif(APPLE)
    include_directories(
        ${BTSTACK_ROOT}/platform/posix
        ${BTSTACK_ROOT}/platform/libusb
    )
    set(PLATFORM_BTSTACK_SOURCES
        ${BTSTACK_ROOT}/platform/posix/btstack_run_loop_posix.c
        ${BTSTACK_ROOT}/platform/libusb/hci_transport_h2_libusb.c
        ${BTSTACK_ROOT}/platform/posix/btstack_stdin_posix.c
        ${BTSTACK_ROOT}/platform/posix/btstack_tlv_posix.c
        ${BTSTACK_ROOT}/platform/posix/hci_dump_posix_fs.c
    )
    set(AUDIO_LAYER_SOURCES
        src/audio/coreaudio_render.c
        src/audio/coreaudio_capture.c
        src/audio/call_recorder.c
    )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    include_directories(${LIBUSB_INCLUDE_DIRS})
    link_directories(${LIBUSB_LIBRARY_DIRS})

    find_library(COREAUDIO_LIB CoreAudio REQUIRED)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox REQUIRED)
    find_library(CORESERVICES_LIB CoreServices REQUIRED)
    find_package(Threads REQUIRED)

    set(PLATFORM_LIBS
        ${LIBUSB_LIBRARIES}
        ${COREAUDIO_LIB}
        ${AUDIOTOOLBOX_LIB}
        ${CORESERVICES_LIB}
        ${CMAKE_THREAD_LIBS_INIT}
        m
    )
else() # Other POSIX (Linux)
    include_directories(
        ${BTSTACK_ROOT}/platform/posix
        ${BTSTACK_ROOT}/platform/libusb
    )
    set(PLATFORM_BTSTACK_SOURCES
        ${BTSTACK_ROOT}/platform/posix/btstack_run_loop_posix.c
        ${BTSTACK_ROOT}/platform/libusb/hci_transport_h2_libusb.c
        ${BTSTACK_ROOT}/platform/posix/btstack_stdin_posix.c
        ${BTSTACK_ROOT}/platform/posix/btstack_tlv_posix.c
        ${BTSTACK_ROOT}/platform/posix/hci_dump_posix_fs.c
    )
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    find_package(Threads REQUIRED)

    set(PLATFORM_LIBS
        ${LIBUSB_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        m
    )
endif()

# -------------------------------------------------------------
# Target 1: Diagnostic Test Tool (dialer-btstack-test)
# -------------------------------------------------------------
add_executable(dialer-btstack-test
    src/test_main.c
    ${BT_LAYER_SOURCES}
    ${AUDIO_LAYER_SOURCES}
    ${BTSTACK_CORE_SOURCES}
    ${PLATFORM_BTSTACK_SOURCES}
)
target_link_libraries(dialer-btstack-test PRIVATE ${PLATFORM_LIBS})

# -------------------------------------------------------------
# Target 2: Production Dialer Application (dialer-btstack)
# -------------------------------------------------------------
add_executable(dialer-btstack
    src/main.c
    ${BT_LAYER_SOURCES}
    ${AUDIO_LAYER_SOURCES}
    ${BTSTACK_CORE_SOURCES}
    ${PLATFORM_BTSTACK_SOURCES}
)
target_link_libraries(dialer-btstack PRIVATE ${PLATFORM_LIBS})

# -------------------------------------------------------------
# Target 3: Audio Extraction Tool (export_call_audio)
# -------------------------------------------------------------
add_executable(export_call_audio
    tools/export_call_audio.c
    ${BTSTACK_ROOT}/src/btstack_util.c
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_bluedroid.c
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_decoder_bluedroid.c
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_encoder_bluedroid.c
    ${BTSTACK_ROOT}/src/classic/btstack_sbc_plc.c
    # Bluedroid Decoder
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/alloc.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/bitalloc-sbc.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/bitalloc.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/bitstream-decode.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/decoder-oina.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/decoder-private.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/decoder-sbc.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/dequant.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/framing-sbc.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/framing.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/oi_codec_version.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/synthesis-8-generated.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/synthesis-dct8.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/srce/synthesis-sbc.c
    # Bluedroid Encoder
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_analysis.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_dct.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_dct_coeffs.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_encoder.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_mono.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_enc_bit_alloc_ste.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_enc_coeffs.c
    ${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/srce/sbc_packing.c
)
target_link_libraries(export_call_audio PRIVATE ${PLATFORM_LIBS})

# Copy firmware files to output directory on build
add_custom_command(TARGET dialer-btstack POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/firmware
        $<TARGET_FILE_DIR:dialer-btstack>/firmware
)

add_custom_command(TARGET dialer-btstack-test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/firmware
        $<TARGET_FILE_DIR:dialer-btstack-test>/firmware
)
