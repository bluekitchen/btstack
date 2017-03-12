//
// btstack_config.h for Android mtk port
//

#ifndef __BTSTACK_CONFIG
#define __BTSTACK_CONFIG

// Port related features
#define HAVE_MALLOC
#define HAVE_POSIX_FILE_IO
#define HAVE_POSIX_TIME

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_CLASSIC
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_CENTRAL
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO 
#define ENABLE_SDP_DES_DUMP

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE 1021
#define MAX_NR_LE_DEVICE_DB_ENTRIES 1

// Daemon configuration
#define ENABLE_SDP
#define ENABLE_RFCOMM
#define HAVE_TRANSPORT_H4
#define BTSTACK_LOG_FILE "/data/btstack/hci_dump.pklg"
#define BTSTACK_UNIX "/data/btstack/BTstack"
#define REMOTE_DEVICE_DB remote_device_db_memory
#define UART_DEVICE "/dev/ttyS0"
#define UART_SPEED 115200

#endif
