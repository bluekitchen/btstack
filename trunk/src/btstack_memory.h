
/*
 * Copyright (C) 2009 by BlueKitchen GmbH
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



/*
 *  btstsack_memory.h
 *
 *  @brief BTstack memory management via configurable memory pools
 *
 */

#ifndef __BTSTACK_MEMORY_H
#define __BTSTACK_MEMORY_H

#if defined __cplusplus
extern "C" {
#endif

#include "btstack-config.h"
    
#include "hci.h"
#include "l2cap.h"
#include "rfcomm.h"
#include "bnep.h"
#include "remote_device_db.h"

#ifdef HAVE_BLE
#include "gatt_client.h"
#endif

void btstack_memory_init(void);

// hci_connection
hci_connection_t * btstack_memory_hci_connection_get(void);
void   btstack_memory_hci_connection_free(hci_connection_t *hci_connection);

// l2cap_service, l2cap_channel
l2cap_service_t * btstack_memory_l2cap_service_get(void);
void   btstack_memory_l2cap_service_free(l2cap_service_t *l2cap_service);
l2cap_channel_t * btstack_memory_l2cap_channel_get(void);
void   btstack_memory_l2cap_channel_free(l2cap_channel_t *l2cap_channel);

// rfcomm_multiplexer, rfcomm_service, rfcomm_channel
rfcomm_multiplexer_t * btstack_memory_rfcomm_multiplexer_get(void);
void   btstack_memory_rfcomm_multiplexer_free(rfcomm_multiplexer_t *rfcomm_multiplexer);
rfcomm_service_t * btstack_memory_rfcomm_service_get(void);
void   btstack_memory_rfcomm_service_free(rfcomm_service_t *rfcomm_service);
rfcomm_channel_t * btstack_memory_rfcomm_channel_get(void);
void   btstack_memory_rfcomm_channel_free(rfcomm_channel_t *rfcomm_channel);

// db_mem_device_name, db_mem_device_link_key, db_mem_service
db_mem_device_name_t * btstack_memory_db_mem_device_name_get(void);
void   btstack_memory_db_mem_device_name_free(db_mem_device_name_t *db_mem_device_name);
db_mem_device_link_key_t * btstack_memory_db_mem_device_link_key_get(void);
void   btstack_memory_db_mem_device_link_key_free(db_mem_device_link_key_t *db_mem_device_link_key);
db_mem_service_t * btstack_memory_db_mem_service_get(void);
void   btstack_memory_db_mem_service_free(db_mem_service_t *db_mem_service);

// bnep_service, bnep_channel
bnep_service_t * btstack_memory_bnep_service_get(void);
void   btstack_memory_bnep_service_free(bnep_service_t *bnep_service);
bnep_channel_t * btstack_memory_bnep_channel_get(void);
void   btstack_memory_bnep_channel_free(bnep_channel_t *bnep_channel);

#ifdef HAVE_BLE
// gatt_client, gatt_subclient
gatt_client_t * btstack_memory_gatt_client_get(void);
void   btstack_memory_gatt_client_free(gatt_client_t *gatt_client);
gatt_subclient_t * btstack_memory_gatt_subclient_get(void);
void   btstack_memory_gatt_subclient_free(gatt_subclient_t *gatt_subclient);
#endif

#if defined __cplusplus
}
#endif

#endif // __BTSTACK_MEMORY_H

