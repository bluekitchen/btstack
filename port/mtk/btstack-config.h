// btstack-config.h created by configure for BTstack  Thu Apr 24 14:34:39 CEST 2014

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

#define HAVE_TRANSPORT_H4
#define UART_DEVICE "/dev/ttyS0"
#define UART_SPEED 115200
#define USE_POWERMANAGEMENT
#define USE_POSIX_RUN_LOOP
#define HAVE_SDP
#define HAVE_RFCOMM
#define HAVE_BLE
#define REMOTE_DEVICE_DB remote_device_db_memory
// #define HAVE_SO_NOSIGPIPE
#define HAVE_TIME
#define HAVE_MALLOC
#define HAVE_BZERO
#define HAVE_HCI_DUMP
#define ENABLE_LOG_INFO 
#define ENABLE_LOG_ERROR
#define HCI_ACL_PAYLOAD_SIZE 1021
#define SDP_DES_DUMP

#define BTSTACK_UNIX "/data/btstack/BTstack"
#define BTSTACK_LOG_FILE "/data/btstack/hci_dump.pklg"

#endif
