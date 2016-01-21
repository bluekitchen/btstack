//
// btstack_config.h
// created by configure for BTstack 
// Tue Oct 14 21:52:44 CEST 2014
//

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

#define HAVE_TRANSPORT_H4
#define UART_DEVICE "/dev/tty.bluetooth"
#define UART_SPEED 921600
#define ENABLE_SDP
#define ENABLE_RFCOMM
#define REMOTE_DEVICE_DB remote_device_db_iphone
#define HAVE_SO_NOSIGPIPE
#define HAVE_TIME
#define HAVE_MALLOC
#define HAVE_BZERO
#define ENABLE_LOG_INTO_HCI_DUMP
#define ENABLE_LOG_INFO 
#define ENABLE_LOG_ERROR
#define HCI_ACL_PAYLOAD_SIZE 1021
#define ENABLE_SDP_DES_DUMP

#define USE_SPRINGBOARD
#define USE_LAUNCHD
#define HAVE_PLATFORM_IPHONE_OS

#endif