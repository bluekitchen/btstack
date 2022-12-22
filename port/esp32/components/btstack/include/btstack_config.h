//
// btstack_config.h for esp32 port
//
// Documentation: https://bluekitchen-gmbh.com/btstack/#how_to/
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_ASSERT
#define HAVE_BTSTACK_STDIN
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_FREERTOS_INCLUDE_PREFIX
#define HAVE_FREERTOS_TASK_NOTIFICATIONS
#define HAVE_MALLOC

// HCI Controller to Host Flow Control
#define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL

// BTstack features that can be enabled
#define ENABLE_PRINTF_HEXDUMP
#define ENABLE_LOG_ERROR
#define ENABLE_LOG_INFO

// Enable Classic/LE based on esp-idf sdkconfig
#include "sdkconfig.h"
#ifdef CONFIG_IDF_TARGET_ESP32
// ESP32 as dual-mode Controller
#define ENABLE_CLASSIC
#define ENABLE_BLE
#else /* CONFIG_IDF_TARGET_ESP32 */
// ESP32-C3 and ESP32-S3 with LE-only Controller
#define ENABLE_BLE
#endif

// Classic configuration
#ifdef ENABLE_CLASSIC

#define ENABLE_HFP_WIDE_BAND_SPEECH

#define ENABLE_SCO_OVER_HCI

// mainly needed for AVRCP Browsing, can be removed otherwise to reduce code size
#define ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE

// work around to link layer issues in ESP32
// https://github.com/espressif/esp-idf/issues/5494
#define ENABLE_CLASSIC_LEGACY_CONNECTIONS_FOR_SCO_DEMOS

// support CTKD if LE is available, too
#ifdef ENABLE_BLE
#define ENABLE_CROSS_TRANSPORT_KEY_DERIVATION
#endif

#define NVM_NUM_LINK_KEYS 16

#endif

// LE configuration
#ifdef ENABLE_BLE

#define ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
#define ENABLE_LE_CENTRAL
#define ENABLE_LE_DATA_LENGTH_EXTENSION
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_SECURE_CONNECTIONS
// ESP32 supports ECDH HCI Commands, but micro-ecc lib is already provided anyway
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS

#define NVM_NUM_DEVICE_DB_ENTRIES 16

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

// BTstack configuration. buffers, sizes, ...

#ifdef ENABLE_CLASSIC

// ACL buffer large enough for Ethernet frame in BNEP/PAN
#define HCI_ACL_PAYLOAD_SIZE (1691 + 4)

#define HCI_HOST_ACL_PACKET_LEN 1024
#define HCI_HOST_ACL_PACKET_NUM 20
#define HCI_HOST_SCO_PACKET_LEN 60
#define HCI_HOST_SCO_PACKET_NUM 10

#else

// ACL buffer large enough to allow for 512 byte Characteristic
#define HCI_ACL_PAYLOAD_SIZE (512 + 4 + 3)

#define HCI_HOST_ACL_PACKET_LEN HCI_ACL_PAYLOAD_SIZE
#define HCI_HOST_ACL_PACKET_NUM 20
#define HCI_HOST_SCO_PACKET_LEN 0
#define HCI_HOST_SCO_PACKET_NUM 0

#endif


#endif
