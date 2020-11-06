//
// btstack_config.h for iOS port in Cydia
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_MALLOC
#define HAVE_PLATFORM_IPHONE_OS
#define HAVE_POSIX_FILE_IO
#define HAVE_POSIX_TIME

// BTstack features that can be enabled
#define ENABLE_CLASSIC
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_PRINTF_HEXDUMP
#define ENABLE_SDP_DES_DUMP

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 1021
#define MAX_NR_LE_DEVICE_DB_ENTRIES 1

// Daemon configuration
#define BTSTACK_DEVICE_NAME_DB_INSTANCE btstack_device_name_db_corefoundation_instance
#define BTSTACK_LINK_KEY_DB_INSTANCE btstack_link_key_db_corefoundation_instance
#define ENABLE_RFCOMM
#define ENABLE_SDP
#define HAVE_TRANSPORT_H4
#define UART_DEVICE "/dev/tty.bluetooth"
#define UART_SPEED 921600
#define USE_LAUNCHD
#define USE_SPRINGBOARD

#endif
