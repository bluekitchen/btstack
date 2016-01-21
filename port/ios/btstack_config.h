//
// btstack_config.h for iOS port in Cydia
//

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

// Port related features
#define HAVE_BZERO
#define HAVE_MALLOC
#define HAVE_PLATFORM_IPHONE_OS
#define HAVE_POSIX_FILE_IO
#define HAVE_SO_NOSIGPIPE
#define HAVE_TIME

// BTstack features that can be enabled
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO 
#define ENABLE_LOG_INTO_HCI_DUMP
#define ENABLE_SDP_DES_DUMP

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 1021

// Daemon configuration
#define ENABLE_RFCOMM
#define ENABLE_SDP
#define HAVE_TRANSPORT_H4
#define UART_DEVICE "/dev/tty.bluetooth"
#define UART_SPEED 921600
#define REMOTE_DEVICE_DB remote_device_db_iphone
#define USE_SPRINGBOARD
#define USE_LAUNCHD

#endif