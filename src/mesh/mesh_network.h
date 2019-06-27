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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
#include "provisioning.h"
#include "btstack_run_loop.h"

#if defined __cplusplus
extern "C" {
#endif

#define MESH_DEVICE_KEY_INDEX     0xffff

#define MESH_NETWORK_PAYLOAD_MAX      29
#define MESH_ACCESS_PAYLOAD_MAX      384

#define MESH_ADDRESS_UNSASSIGNED     0x0000u
#define MESH_ADDRESS_ALL_PROXIES     0xFFFCu
#define MESH_ADDRESS_ALL_FRIENDS     0xFFFDu
#define MESH_ADDRESS_ALL_RELAYS      0xFFFEu
#define MESH_ADDRESS_ALL_NODES       0xFFFFu

typedef enum {
    MESH_NETWORK_PDU_RECEIVED,
    MESH_NETWORK_PDU_SENT,
} mesh_network_callback_type_t;

typedef enum {
    MESH_PDU_TYPE_NETWORK = 0,
    MESH_PDU_TYPE_TRANSPORT,
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

    // callback
    void (*callback)(struct mesh_network_pdu * network_pdu);

    // meta data network layer
    uint16_t              netkey_index;
    // meta data transport layer
    uint16_t              appkey_index;
    // flags: bit 0 indicates Proxy PDU
    uint16_t              flags;

    // pdu
    uint16_t              len;
    uint8_t               data[MESH_NETWORK_PAYLOAD_MAX];
} mesh_network_pdu_t;

typedef struct {
    mesh_pdu_t pdu_header;

    // rx/tx: acknowledgement timer / segment transmission timer
    btstack_timer_source_t acknowledgement_timer;
    // rx: incomplete timer / tx: resend timer
    btstack_timer_source_t incomplete_timer;
    // block access
    uint32_t              block_ack;
    // meta data network layer
    uint16_t              netkey_index;
    // meta data transport layer
    uint16_t              appkey_index;
    // transmic size
    uint8_t               transmic_len;
    // akf - aid
    uint8_t               akf_aid;
    // network pdu header
    uint8_t               network_header[9];
    // acknowledgement timer active
    uint8_t               acknowledgement_timer_active;
    // incomplete timer active
    uint8_t               incomplete_timer_active;
    // message complete
    uint8_t               message_complete;
    // seq_zero for segmented messages
    uint16_t              seq_zero;
    // pdu
    uint16_t              len;
    uint8_t               data[MESH_ACCESS_PAYLOAD_MAX];
} mesh_transport_pdu_t;


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
 * @brief Configure address filter
 */
void mesh_network_set_primary_element_address(uint16_t addr);

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
 * @returns 1 if valid, 
 */
int mesh_network_addresses_valid(uint8_t ctl, uint16_t src, uint16_t dst);

/**
 * @brief Check if Unicast address
 * @param addr
 * @returns 1 if unicast
 */
int mesh_network_address_unicast(uint16_t addr);

/**
 * @brief Check if Unicast address
 * @param addr
 * @returns 1 if unicast
 */
int mesh_network_address_group(uint16_t addr);

/**
 * @brief Check if All Proxies address
 * @param addr
 * @returns 1 if all proxies
 */
int mesh_network_address_all_proxies(uint16_t addr);

/**
 * @brief Check if All Nodes address
 * @param addr
 * @returns 1 if all nodes
 */
int mesh_network_address_all_nodes(uint16_t addr);

/**
 * @brief Check if All Friends address
 * @param addr
 * @returns 1 if all friends
 */
int mesh_network_address_all_friends(uint16_t addr);

/**
 * @brief Check if All Relays address
 * @param addr
 * @returns 1 if all relays
 */
int mesh_network_address_all_relays(uint16_t addr);


/**
 * @brief Check if Virtual address
 * @param addr
 * @returns 1 if virtual
 */
int mesh_network_address_virtual(uint16_t addr);

// buffer pool
mesh_network_pdu_t * mesh_network_pdu_get(void);
void mesh_network_pdu_free(mesh_network_pdu_t * network_pdu);

// Mesh Network PDU Getter
uint16_t  mesh_network_control(mesh_network_pdu_t * network_pdu);
uint8_t   mesh_network_nid(mesh_network_pdu_t * network_pdu);
uint8_t   mesh_network_ttl(mesh_network_pdu_t * network_pdu);
uint32_t  mesh_network_seq(mesh_network_pdu_t * network_pdu);
uint16_t  mesh_network_src(mesh_network_pdu_t * network_pdu);
uint16_t  mesh_network_dst(mesh_network_pdu_t * network_pdu);
int       mesh_network_segmented(mesh_network_pdu_t * network_pdu);
uint8_t * mesh_network_pdu_data(mesh_network_pdu_t * network_pdu);
uint8_t   mesh_network_pdu_len(mesh_network_pdu_t * network_pdu);

void     mesh_set_iv_index(uint32_t iv_index);
uint32_t mesh_get_iv_index(void);

// Testing only
void mesh_network_received_message(const uint8_t * pdu_data, uint8_t pdu_len, uint8_t flags);
void mesh_network_process_proxy_configuration_message(const uint8_t * pdu_data, uint8_t pdu_len);
void mesh_network_encrypt_proxy_configuration_message(mesh_network_pdu_t * network_pdu, void (* callback)(mesh_network_pdu_t * callback));
void mesh_network_dump(void);
void mesh_network_reset(void);

#if defined __cplusplus
}
#endif

#endif
