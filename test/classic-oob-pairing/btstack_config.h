//
// btstack_config.h
//

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// Port related features
#define HAVE_EMBEDDED_TIME_MS

// BTstack features that can be enabled
#define ENABLE_CLASSIC

#define ENABLE_BTSTACK_ASSERT

// BTstack configuration. buffers, sizes, ...
#define HCI_ACL_PAYLOAD_SIZE                        (676 + 4)
/// Provide 1 byte of data for H4 TL type
#define HCI_OUTGOING_PRE_BUFFER_SIZE                (1)

#define HAVE_HOST_CONTROLLER_API

// Internal ring buffer: 21 kB
#define HCI_HOST_ACL_PACKET_NUM                     (20)
#define HCI_HOST_ACL_PACKET_LEN                     (1024)
#define HCI_HOST_SCO_PACKET_NUM                     (10)
#define HCI_HOST_SCO_PACKET_LEN                     (60)

// Link Key DB and LE Device DB using TLV on top of Flash Sector interface
#define NVM_NUM_LINK_KEYS                           (0)
#define NVM_NUM_DEVICE_DB_ENTRIES                   (0)

// BTstack configuration. buffers, sizes, ...

#define MAX_NR_HCI_CONNECTIONS                      (4)
#define MAX_NR_L2CAP_SERVICES                       (6)
#define MAX_NR_L2CAP_CHANNELS                       (6)

#define MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES   (0)
#define MAX_NR_BNEP_SERVICES                        (0)
#define MAX_NR_BNEP_CHANNELS                        (0)
#define MAX_NR_WHITELIST_ENTRIES                    (0)
#define MAX_NR_SM_LOOKUP_ENTRIES                    (0)
#define MAX_NR_SERVICE_RECORD_ITEMS                 (8)
#define MAX_NR_AVDTP_STREAM_ENDPOINTS               (4)
#define MAX_NR_AVDTP_CONNECTIONS                    (2)
#define MAX_NR_AVRCP_CONNECTIONS                    (2)

#define MAX_NR_HFP_CONNECTIONS                      (1)
#define MAX_NR_RFCOMM_MULTIPLEXERS                  (1)
#define MAX_NR_RFCOMM_SERVICES                      (1)
#define MAX_NR_RFCOMM_CHANNELS                      (1)

/// Enable SCO over HCI
#define ENABLE_SCO_OVER_HCI
/// Ensure that BK doesn't start implicitly discoverable and connectable mode
#define ENABLE_EXPLICIT_CONNECTABLE_MODE_CONTROL
/// Add support of BT classic OOB pairing
#define ENABLE_CLASSIC_PAIRING_OOB
/// Handle IOCAP by application
#define ENABLE_EXPLICIT_IO_CAPABILITIES_REPLY
/// Enable A2DP codec config
#define ENABLE_A2DP_EXPLICIT_CONFIG
/// Let app delay stream configurartion
#define ENABLE_AVDTP_ACCEPTOR_EXPLICIT_START_STREAM_CONFIRMATION
/// Allow defer of LINK Key Reply
#define ENABLE_EXPLICIT_LINK_KEY_REPLY
///  Trigger L2CAP Information Requests to get supported fixed channels
#define ENABLE_L2CAP_INFORMATION_REQUESTS_ON_CONNECT

#define ENABLE_HFP_WIDE_BAND_SPEECH
#define ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
#define ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE_FOR_RFCOMM
#define ENABLE_GOEP_L2CAP


// cannot be used yet - mere inclusion of <stdio.h> causes compile errors
// <stdio.h> is used at various places for snprintf
// abort on use of printf in main library
// #define printf() no_printf_please()

#endif

