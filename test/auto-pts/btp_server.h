/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

/**
 *  @brief TODO
 */

#ifndef BTP_SERVER_H
#define BTP_SERVER_H

#include <stdint.h>
#include "btstack_linked_list.h"
#include "bluetooth.h"
#include "le-audio/gatt-service/coordinated_set_identification_service_client.h"
#include "le-audio/gatt-service/published_audio_capabilities_service_client.h"

#if defined __cplusplus
extern "C" {
#endif

#define MAX_NUM_SERVERS 3

typedef struct {
    btstack_linked_item_t item;
    uint8_t server_id;

    bd_addr_t address;
    bd_addr_type_t address_type;

    // hci
    hci_con_handle_t acl_con_handle;

    // csis
    csis_client_connection_t csis_connection;
    uint16_t csis_cid;
    uint8_t  sirk[16];
    uint8_t  coordinated_set_size;
    uint8_t  coordinated_set_rank;

    // pacs client
    pacs_client_connection_t pacs_connection;
    uint16_t pacs_cid;
    uint8_t  pacs_state;
} server_t;

/**
 * @brief Initialize server structures
 */
void btp_server_init(void);

/**
 * @brief Setup server struct for active hci connection
 * @param con_handle
 * @return
 */
server_t * btp_server_initialize(hci_con_handle_t con_handle);

/**
 * @brief Lookup server by index, used to iterate over servers
 * @param index
 * @return
 */
server_t * btp_server_for_index(uint8_t index);

/**
 * @brief Free server
 */
void btp_server_finalize(server_t * server);


/**
 * @brief Lookup server by Address
 * @param address_type
 * @param address
 * @return
 */
server_t * btp_server_for_address(bd_addr_type_t address_type, bd_addr_t address);

/**
 * @brief Lookup server by ACL Connection Handle
 * @param acl_con_handle
 * @return
 */
server_t * btp_server_for_acl_con_handle(hci_con_handle_t acl_con_handle);

/**
 * @brief Lookup server by csis_cid
 * @param csis_cid
 * @return
 */
server_t * btp_server_for_csis_cid(uint16_t csis_cid);

/**
 * @brief Lookup server by pacs_cid
 * @param pacs_cid
 * @return
 */
server_t * btp_server_for_pacs_cid(uint16_t csis_cid);

#if defined __cplusplus
}
#endif
#endif // BTP_SERVER_H