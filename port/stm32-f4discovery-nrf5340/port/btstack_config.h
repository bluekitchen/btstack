//
// btstack_config.h for STM32 F4 Discovery + TI CC256x port
//
// Documentation: https://bluekitchen-gmbh.com/btstack/#how_to/
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_BTSTACK_STDIN
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_HAL_AUDIO
#define HAVE_HAL_AUDIO_SINK_STEREO_ONLY
#define HAVE_BTSTACK_AUDIO_EFFECTIVE_SAMPLERATE

// BTstack features that can be enabled
#define ENABLE_BLE
#define ENABLE_LE_CENTRAL
#define ENABLE_L2CAP_LE_CREDIT_BASED_FLOW_CONTROL_MODE
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_EXTENDED_ADVERTISING
#define ENABLE_LE_PERIODIC_ADVERTISING
#define ENABLE_LE_ISOCHRONOUS_STREAMS
//#define ENABLE_LE_SECURE_CONNECTIONS
#define ENABLE_ATT_DELAYED_RESPONSE
#define ENABLE_LOG_ERROR
#define ENABLE_PRINTF_HEXDUMP
#define ENABLE_SEGGER_RTT

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE (300 + 4)
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS 2
#define MAX_NR_L2CAP_CHANNELS  4
#define MAX_NR_L2CAP_SERVICES  3
#define MAX_NR_SM_LOOKUP_ENTRIES 3
#define MAX_NR_WHITELIST_ENTRIES 1
#define MAX_NR_HCI_ISO_STREAMS 2
#define MAX_NR_PERIODIC_ADVERTISER_LIST_ENTRIES 2
#define MAX_ATT_DB_SIZE 200

// Link Key DB and LE Device DB using TLV on top of Flash Sector interface
#define NVM_NUM_DEVICE_DB_ENTRIES 16
#define NVM_NUM_LINK_KEYS 16

#endif
