# Hand-maintained platform source groups shared by CMake ports.
#
# Unlike BTstackSources.cmake, this file is not generated. Platform source
# selection is port-specific and includes implementation-provider decisions.

set(btstack_platform_source_groups
    platform-posix
    platform-windows
    platform-libusb-h2
    platform-winusb-h2
)

# Common POSIX support. le_device_db_fs.c is deprecated; ports select the
# le-device-db-tlv group from src/ instead when persistent LE storage is used.
set(btstack_platform_sources_platform_posix
    platform/posix/btstack_audio_portaudio.c
    platform/posix/btstack_audio_sdl2.c
    platform/posix/btstack_link_key_db_fs.c
    platform/posix/btstack_main_config.c
    platform/posix/btstack_network_posix.c
    platform/posix/btstack_run_loop_posix.c
    platform/posix/btstack_sco_transport_posix_i2s_test_bridge.c
    platform/posix/btstack_signal.c
    platform/posix/btstack_stdin_posix.c
    platform/posix/btstack_tlv_posix.c
    platform/posix/btstack_uart_posix.c
    platform/posix/hci_dump_posix_fs.c
    platform/posix/hci_dump_posix_stdout.c
    platform/posix/wav_util.c
)

# Common Windows support. The WinUSB H2 transport is deliberately separate.
set(btstack_platform_sources_platform_windows
    platform/windows/btstack_run_loop_windows.c
    platform/windows/btstack_stdin_windows.c
    platform/windows/btstack_tlv_windows.c
    platform/windows/btstack_uart_block_windows.c
    platform/windows/hci_dump_windows_fs.c
    platform/windows/hci_dump_windows_stdout.c
)

set(btstack_platform_sources_platform_libusb_h2
    platform/libusb/hci_transport_h2_libusb.c
)

set(btstack_platform_sources_platform_winusb_h2
    platform/windows/hci_transport_h2_winusb.c
)
