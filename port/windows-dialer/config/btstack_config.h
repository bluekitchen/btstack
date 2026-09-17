//
// btstack_config.h for dialer-btstack (Windows WinUSB + Realtek RTL8761BU)
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

// Port related features
#define HAVE_ASSERT
#define HAVE_BTSTACK_STDIN
#define HAVE_MALLOC
#define HAVE_POSIX_FILE_IO
#define HAVE_BTSTACK_TLV

// BTstack features enabled
#define ENABLE_CLASSIC
#define ENABLE_BLE
#define ENABLE_LE_PERIPHERAL
#define ENABLE_SCO_OVER_HCI
// #define ENABLE_HFP_WIDE_BAND_SPEECH  // CVSD hardware codec is used for stability across USB Bluetooth dongles
#define ENABLE_LOG_INFO
#define ENABLE_LOG_ERROR
#define ENABLE_PRINTF_HEXDUMP

// Buffers and limits
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)
#define HCI_INCOMING_PRE_BUFFER_SIZE 14

#define NVM_NUM_DEVICE_DB_ENTRIES 16
#define NVM_NUM_LINK_KEYS 16

#define MAX_NR_BTM_SEC_SERVICES 8
#define MAX_NR_BTM_SEC_ATTRIBUTES 8
#define MAX_NR_HCI_CONNECTIONS 4
#define MAX_NR_L2CAP_CHANNELS 8
#define MAX_NR_L2CAP_SERVICES 8
#define MAX_NR_RFCOMM_CHANNELS 8
#define MAX_NR_RFCOMM_MULTIPLEXERS 4
#define MAX_NR_RFCOMM_SERVICES 8
#define MAX_NR_SERVICE_RECORD_ITEMS 16
#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES 16
#define MAX_NR_LE_DEVICE_DB_ENTRIES 16
#define MAX_NR_GATT_CLIENTS 2
#define MAX_NR_WHITELIST_ENTRIES 4

#endif // BTSTACK_CONFIG_H
