//
// btstack_config.h for Android mtk port
//
// Documentation: https://bluekitchen-gmbh.com/btstack/#how_to/
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_MALLOC
#define HAVE_POSIX_FILE_IO
#define HAVE_POSIX_TIME
#define HAVE_UNIX_SOCKETS

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_CLASSIC
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_PRINTF_HEXDUMP
#define ENABLE_SDP_DES_DUMP

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 1021
#define MAX_NR_LE_DEVICE_DB_ENTRIES 1

#define NVM_NUM_DEVICE_DB_ENTRIES      16
#define NVM_NUM_LINK_KEYS              16

// Daemon configuration
#define BTSTACK_LOG_FILE "/data/btstack/hci_dump.pklg"
#define BTSTACK_UNIX "/data/btstack/BTstack"
#define ENABLE_RFCOMM
#define ENABLE_SDP
#define HAVE_TRANSPORT_H4
#define REMOTE_DEVICE_DB remote_device_db_memory
#define UART_DEVICE "/dev/ttyS0"
#define UART_SPEED 115200

#endif
