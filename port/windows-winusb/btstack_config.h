//
// btstack_config.h for Windows-winusb port
//
// Documentation: https://bluekitchen-gmbh.com/btstack/#how_to/
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_ASSERT
#define HAVE_BTSTACK_STDIN
#define HAVE_MALLOC
#define HAVE_POSIX_FILE_IO
#define HAVE_POSIX_TIME

// BTstack features that can be enabled
#define ENABLE_AVRCP_COVER_ART
#define ENABLE_BLE
#define ENABLE_CLASSIC
#define ENABLE_CROSS_TRANSPORT_KEY_DERIVATION
// traditionally OBEX is sent over RFCOM but this is an additonal layer, diable for better wireshark debugging
// #define ENABLE_GOEP_L2CAP // PTS test Case MAP/MSE/GOEP/ROB/BV-02-C needs this to be enabled to pass the test
#define ENABLE_HFP_WIDE_BAND_SPEECH
#define ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
#define ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_LOG_PREFIXES
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO
#define ENABLE_LOG_DEBUG
#define HCI_LOG_MESSAGE_BUFFER_WINUSB 500
#define ENABLE_HCI_LOG_FUNCTION_NAME
//#define ENABLE_HCI_LOG_TO_CONSOLE
#define ENABLE_LOG_APP_MESSAGING
#define ENABLE_LOG_MAP_ACCESS_SERVER
#define ENABLE_LOG_OBEX
#define ENABLE_DBG_PRINTF
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS
#define ENABLE_PRINTF_HEXDUMP
#define ENABLE_SCO_OVER_HCI
#define ENABLE_SDP_DES_DUMP
#define ENABLE_SOFTWARE_AES128

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)
#define HCI_INCOMING_PRE_BUFFER_SIZE 14 // sizeof BNEP header, avoid memcpy

#define NVM_NUM_DEVICE_DB_ENTRIES      16
#define NVM_NUM_LINK_KEYS              16

// Mesh Configuration
#define ENABLE_MESH
#define ENABLE_MESH_ADV_BEARER
#define ENABLE_MESH_GATT_BEARER
#define ENABLE_MESH_PB_ADV
#define ENABLE_MESH_PB_GATT
#define ENABLE_MESH_PROVISIONER
#define ENABLE_MESH_PROXY_SERVER

#define MAX_NR_MESH_SUBNETS            2
#define MAX_NR_MESH_TRANSPORT_KEYS    16
#define MAX_NR_MESH_VIRTUAL_ADDRESSES 16

// allow for one NetKey update
#define MAX_NR_MESH_NETWORK_KEYS      (MAX_NR_MESH_SUBNETS+1)

#endif
