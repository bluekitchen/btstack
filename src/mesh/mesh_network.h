/*
 * Copyright (C) 2018 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#ifndef __MESH_NETWORK
#define __MESH_NETWORK

#include "btstack_linked_list.h"
#include "btstack_run_loop.h"

#include "mesh/provisioning.h"
#include "mesh/mesh_keys.h"

#if defined __cplusplus
extern "C" {
#endif

#define MESH_DEVICE_KEY_INDEX     0xffff

#define MESH_NETWORK_PAYLOAD_MAX      29
#define MESH_ACCESS_PAYLOAD_MAX      384
#define MESH_CONTROL_PAYLOAD_MAX     256

#define MESH_ADDRESS_UNSASSIGNED     0x0000u
#define MESH_ADDRESS_ALL_PROXIES     0xFFFCu
#define MESH_ADDRESS_ALL_FRIENDS     0xFFFDu
#define MESH_ADDRESS_ALL_RELAYS      0xFFFEu
#define MESH_ADDRESS_ALL_NODES       0xFFFFu

typedef enum {
    MESH_NETWORK_PDU_RECEIVED,
    MESH_NETWORK_PDU_SENT,
    MESH_NETWORK_PDU_ENCRYPTED,
    MESH_NETWORK_CAN_SEND_NOW,
} mesh_network_callback_type_t;

typedef enum {
    MESH_PDU_TYPE_INVALID,
    MESH_PDU_TYPE_NETWORK,
    MESH_PDU_TYPE_SEGMENT_ACKNOWLEDGMENT,
    MESH_PDU_TYPE_SEGMENTED,
    MESH_PDU_TYPE_UNSEGMENTED,
    MESH_PDU_TYPE_ACCESS,
    MESH_PDU_TYPE_CONTROL,
    MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS,
    MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS,
    MESH_PDU_TYPE_UPPER_SEGMENTED_CONTROL,
    MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL,
} mesh_pdu_type_t;

typedef struct mesh_pdu {
    // allow for linked lists
    btstack_linked_item_t item;
    // type
    mesh_pdu_type_t pdu_type;

} mesh_pdu_t;

// 
#define MESH_NETWORK_PDU_FLAGS_PROXY_CONFIGURATION 1
#define MESH_NETWORK_PDU_FLAGS_GATT_BEARER         2
#define MESH_NETWORK_PDU_FLAGS_RELAY               4

typedef struct mesh_network_pdu {
    mesh_pdu_t pdu_header;

    // meta data network layer
    uint16_t              netkey_index;
    // MESH_NETWORK_PDU_FLAGS
    uint16_t              flags;

    // pdu
    uint16_t              len;
    uint8_t               data[MESH_NETWORK_PAYLOAD_MAX];
} mesh_network_pdu_t;

#define MESH_TRANSPORT_FLAG_SEQ_RESERVED      1
#define MESH_TRANSPORT_FLAG_CONTROL           2
#define MESH_TRANSPORT_FLAG_TRANSMIC_64       4
#define MESH_TRANSPORT_FLAG_ACK_TIMER         8
#define MESH_TRANSPORT_FLAG_INCOMPLETE_TIMER 16

typedef struct {
    mesh_pdu_t pdu_header;
    // network header
    uint8_t               ivi_nid;
    uint8_t               ctl_ttl;
    uint16_t              src;
    uint16_t              dst;
    uint32_t              seq;

    // incoming: acknowledgement timer / outgoing: segment transmission timer
    btstack_timer_source_t acknowledgement_timer;
    // incoming: incomplete timer / outgoing: not used
    btstack_timer_source_t incomplete_timer;
    // block access
    uint32_t              block_ack;
    // meta data network layer
    uint16_t              netkey_index;
    // akf - aid for access, opcode for control
    uint8_t               akf_aid_control;
    // MESH_TRANSPORT_FLAG
    uint16_t              flags;
    // retry count
    uint8_t               retry_count;
    // pdu segments
    uint16_t              len;
    btstack_linked_list_t segments;
} mesh_segmented_pdu_t;

typedef struct {
    // generic pdu header
    mesh_pdu_t            pdu_header;
    // network header
    uint8_t               ivi_nid;
    uint8_t               ctl_ttl;
    uint16_t              src;
    uint16_t              dst;
    uint32_t              seq;
    // meta data network layer
    uint16_t              netkey_index;
    // meta data transport layer
    uint16_t              appkey_index;
    // akf - aid for access, opcode for control
    uint8_t               akf_aid_control;
    // MESH_TRANSPORT_FLAG
    uint16_t              flags;
    // payload
    uint16_t              len;
    uint8_t               data[MESH_ACCESS_PAYLOAD_MAX];

} mesh_access_pdu_t;

// for unsegmented + segmented access + segmented control pdus
typedef struct {
    // generic pdu header
    mesh_pdu_t            pdu_header;
    // network header
    uint8_t               ivi_nid;
    uint8_t               ctl_ttl;
    uint16_t              src;
    uint16_t              dst;
    uint32_t              seq;
    // meta data network layer
    uint16_t              netkey_index;
    // meta data transport layer
    uint16_t              appkey_index;
    // akf - aid for access, opcode for control
    uint8_t               akf_aid_control;
    // MESH_TRANSPORT_FLAG
    uint16_t              flags;
    // payload, single segmented or list of them
    uint16_t              len;
    btstack_linked_list_t segments;

    // access acknowledged message
    uint16_t retransmit_count;
    uint32_t retransmit_timeout_ms;
    uint32_t ack_opcode;

    // associated lower transport pdu
    mesh_pdu_t *          lower_pdu;
} mesh_upper_transport_pdu_t;

typedef struct {
    // generic pdu header
    mesh_pdu_t            pdu_header;
    // network header
    uint8_t               ivi_nid;
    uint8_t               ctl_ttl;
    uint16_t              src;
    uint16_t              dst;
    uint32_t              seq;
    // meta data network layer
    uint16_t              netkey_index;
    // akf - aid for access, opcode for control
    uint8_t               akf_aid_control;
    // MESH_TRANSPORT_FLAG
    uint16_t              flags;
    // payload
    uint16_t              len;
    uint8_t               data[MESH_CONTROL_PAYLOAD_MAX];
} mesh_control_pdu_t;

typedef enum {
    MESH_KEY_REFRESH_NOT_ACTIVE = 0,
    MESH_KEY_REFRESH_FIRST_PHASE,
    MESH_KEY_REFRESH_SECOND_PHASE
} mesh_key_refresh_state_t;

typedef enum {
    MESH_SECURE_NETWORK_BEACON_W2_AUTH_VALUE,
    MESH_SECURE_NETWORK_BEACON_W4_AUTH_VALUE,
    MESH_SECURE_NETWORK_BEACON_AUTH_VALUE,
    MESH_SECURE_NETWORK_BEACON_W2_SEND_ADV,
    MESH_SECURE_NETWORK_BEACON_ADV_SENT,
    MESH_SECURE_NETWORK_BEACON_W2_SEND_GATT,
    MESH_SECURE_NETWORK_BEACON_GATT_SENT,
    MESH_SECURE_NETWORK_BEACON_W4_INTERVAL
} mesh_secure_network_beacon_state_t;

typedef struct {
    btstack_linked_item_t item;

    // netkey index
    uint16_t              netkey_index;

    // current / old key
    mesh_network_key_t * old_key;

    // new key (only set during key refresh)
    mesh_network_key_t * new_key;

    // key refresh state
    mesh_key_refresh_state_t key_refresh;

    // advertisement using node id active
    uint8_t node_id_advertisement_running;


    // advertisement using network id (used by proxy)
    adv_bearer_connectable_advertisement_data_item_t advertisement_with_network_id;

    // advertising using node id (used by proxy)
    adv_bearer_connectable_advertisement_data_item_t advertisement_with_node_id;

    // secure network beacons
    mesh_secure_network_beacon_state_t beacon_state;
    uint32_t                           beacon_interval_ms;
    uint32_t                           beacon_observation_start_ms;
    uint16_t                           beacon_observation_counter;

} mesh_subnet_t;

typedef struct {
    btstack_linked_list_iterator_t it;
} mesh_subnet_iterator_t;

/**
 * @brief Init Mesh Network Layer
 */
void mesh_network_init(void);

/** 
 * @brief Set higher layer Network PDU handler
 * @param packet_handler
 */
void mesh_network_set_higher_layer_handler(void (*packet_handler)(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu));

/** 
 * @brief Set higher layer Proxy PDU handler
 * @param packet_handler
 */
void mesh_network_set_proxy_message_handler(void (*packet_handler)(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu));

/**
 * @brief Mark packet as processed
 * @param newtork_pdu received via call packet_handler
 */
void mesh_network_message_processed_by_higher_layer(mesh_network_pdu_t * network_pdu);

/**
 * @brief Send network_pdu after encryption
 * @param network_pdu
 */
void mesh_network_send_pdu(mesh_network_pdu_t * network_pdu);

/*
 * @brief Setup network pdu header
 * @param netkey_index
 * @param nid
 * @param ctl
 * @param ttl
 * @param seq
 * @param dst
 * @param transport_pdu_data
 * @param transport_pdu_len
 */
void mesh_network_setup_pdu(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint8_t nid, uint8_t ctl, uint8_t ttl, uint32_t seq, uint16_t src, uint16_t dst, const uint8_t * transport_pdu_data, uint8_t transport_pdu_len);

/**
 * Setup network pdu header without modifying len or payload
 * @param network_pdu
 * @param netkey_index
 * @param nid
 * @param ctl
 * @param ttl
 * @param seq
 * @param src
 * @param dest
 */
void mesh_network_setup_pdu_header(mesh_network_pdu_t * network_pdu, uint16_t netkey_index, uint8_t nid, uint8_t ctl, uint8_t ttl, uint32_t seq, uint16_t src, uint16_t dest);

/**
 * @brief Validate network addresses
 * @param ctl
 * @param src
 * @param dst
 * @return 1 if valid, 
 */
int mesh_network_addresses_valid(uint8_t ctl, uint16_t src, uint16_t dst);

/**
 * @brief Check if Unicast address
 * @param addr
 * @return 1 if unicast
 */
int mesh_network_address_unicast(uint16_t addr);

/**
 * @brief Check if Unicast address
 * @param addr
 * @return 1 if unicast
 */
int mesh_network_address_group(uint16_t addr);

/**
 * @brief Check if All Proxies address
 * @param addr
 * @return 1 if all proxies
 */
int mesh_network_address_all_proxies(uint16_t addr);

/**
 * @brief Check if All Nodes address
 * @param addr
 * @return 1 if all nodes
 */
int mesh_network_address_all_nodes(uint16_t addr);

/**
 * @brief Check if All Friends address
 * @param addr
 * @return 1 if all friends
 */
int mesh_network_address_all_friends(uint16_t addr);

/**
 * @brief Check if All Relays address
 * @param addr
 * @return 1 if all relays
 */
int mesh_network_address_all_relays(uint16_t addr);


/**
 * @brief Check if Virtual address
 * @param addr
 * @return 1 if virtual
 */
int mesh_network_address_virtual(uint16_t addr);


/**
 * @brief Add subnet to list
 * @param subnet
 */
void mesh_subnet_add(mesh_subnet_t * subnet);

/**
 * @brief Remove subnet from list
 * @param subnet
 */
void mesh_subnet_remove(mesh_subnet_t * subnet);

/**
 * @brief Get subnet for netkey_index
 * @param netkey_index
 * @return mesh_subnet_t or NULL
 */
mesh_subnet_t * mesh_subnet_get_by_netkey_index(uint16_t netkey_index);

/**
 * @brief Get number of stored subnets
 * @return count
 */
int mesh_subnet_list_count(void);

/**
 * @brief Iterate over all subnets
 * @param it
 */
void mesh_subnet_iterator_init(mesh_subnet_iterator_t *it);

/**
 * @brief Check if another subnet is available
 * @param it
 * @return
 */
int mesh_subnet_iterator_has_more(mesh_subnet_iterator_t *it);

/**
 * @brief Get next subnet
 * @param it
 * @return
 */
mesh_subnet_t * mesh_subnet_iterator_get_next(mesh_subnet_iterator_t *it);

/**
 * @brief Setup subnet for given netkey index
 */
void mesh_subnet_setup_for_netkey_index(uint16_t netkey_index);


/**
 * @brief Get outgoing network key for subnet based on key refresh phase
 */
mesh_network_key_t * mesh_subnet_get_outgoing_network_key(mesh_subnet_t * subnet);

// buffer pool
mesh_network_pdu_t * mesh_network_pdu_get(void);
void mesh_network_pdu_free(mesh_network_pdu_t * network_pdu);
void mesh_network_notify_on_freed_pdu(void (*callback)(void));

// Mesh Network PDU Getter
uint16_t  mesh_network_control(mesh_network_pdu_t * network_pdu);
uint8_t   mesh_network_nid(mesh_network_pdu_t * network_pdu);
uint8_t   mesh_network_ttl(mesh_network_pdu_t * network_pdu);
uint32_t  mesh_network_seq(mesh_network_pdu_t * network_pdu);
uint16_t  mesh_network_src(mesh_network_pdu_t * network_pdu);
uint16_t  mesh_network_dst(mesh_network_pdu_t * network_pdu);
int       mesh_network_segmented(mesh_network_pdu_t * network_pdu);
uint8_t   mesh_network_control_opcode(mesh_network_pdu_t * network_pdu);
uint8_t * mesh_network_pdu_data(mesh_network_pdu_t * network_pdu);
uint8_t   mesh_network_pdu_len(mesh_network_pdu_t * network_pdu);

// Mesh Network PDU Setter
void mesh_network_pdu_set_seq(mesh_network_pdu_t * network_pdu, uint32_t seq);

// Testing only
void mesh_network_received_message(const uint8_t * pdu_data, uint8_t pdu_len, uint8_t flags);
void mesh_network_process_proxy_configuration_message(const uint8_t * pdu_data, uint8_t pdu_len);
void mesh_network_encrypt_proxy_configuration_message(mesh_network_pdu_t * network_pdu);
void mesh_network_dump(void);
void mesh_network_reset(void);

#if defined __cplusplus
}
#endif

#endif
