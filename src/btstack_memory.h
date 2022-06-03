/*
 * Copyright (C) 2014 BlueKitchen GmbH
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



/*
 * btstack_memory.h
 *
 * @brief BTstack memory management using configurable memory pools
 *
 */

#ifndef BTSTACK_MEMORY_H
#define BTSTACK_MEMORY_H

#if defined __cplusplus
extern "C" {
#endif

#include "btstack_config.h"
    
// Core
#include "hci.h"
#include "l2cap.h"

// Classic
#ifdef ENABLE_CLASSIC
#include "classic/avdtp_sink.h"
#include "classic/avdtp_source.h"
#include "classic/avrcp.h"
#include "classic/bnep.h"
#include "classic/btstack_link_key_db.h"
#include "classic/btstack_link_key_db_memory.h"
#include "classic/goep_server.h"
#include "classic/hfp.h"
#include "classic/hid_host.h"
#include "classic/rfcomm.h"
#include "classic/sdp_server.h"
#endif

// BLE
#ifdef ENABLE_BLE
#include "ble/gatt-service/battery_service_client.h"
#include "ble/gatt-service/hids_client.h"
#include "ble/gatt-service/scan_parameters_service_client.h"
#include "ble/gatt_client.h"
#include "ble/sm.h"
#endif

#ifdef ENABLE_MESH
#include "mesh/mesh_network.h"
#include "mesh/mesh_keys.h"
#include "mesh/mesh_virtual_addresses.h"
#endif

/* API_START */

/**
 * @brief Initializes BTstack memory pools.
 */
void btstack_memory_init(void);

/**
 * @brief Deinitialize BTstack memory pools
 * @note if HAVE_MALLOC is defined, all previously allocated buffers are free'd
 */
void btstack_memory_deinit(void);

/* API_END */

hci_connection_t * btstack_memory_hci_connection_get(void);
void   btstack_memory_hci_connection_free(hci_connection_t *hci_connection);

l2cap_service_t * btstack_memory_l2cap_service_get(void);
void   btstack_memory_l2cap_service_free(l2cap_service_t *l2cap_service);
l2cap_channel_t * btstack_memory_l2cap_channel_get(void);
void   btstack_memory_l2cap_channel_free(l2cap_channel_t *l2cap_channel);

#ifdef ENABLE_CLASSIC
rfcomm_multiplexer_t * btstack_memory_rfcomm_multiplexer_get(void);
void   btstack_memory_rfcomm_multiplexer_free(rfcomm_multiplexer_t *rfcomm_multiplexer);
rfcomm_service_t * btstack_memory_rfcomm_service_get(void);
void   btstack_memory_rfcomm_service_free(rfcomm_service_t *rfcomm_service);
rfcomm_channel_t * btstack_memory_rfcomm_channel_get(void);
void   btstack_memory_rfcomm_channel_free(rfcomm_channel_t *rfcomm_channel);

btstack_link_key_db_memory_entry_t * btstack_memory_btstack_link_key_db_memory_entry_get(void);
void   btstack_memory_btstack_link_key_db_memory_entry_free(btstack_link_key_db_memory_entry_t *btstack_link_key_db_memory_entry);

bnep_service_t * btstack_memory_bnep_service_get(void);
void   btstack_memory_bnep_service_free(bnep_service_t *bnep_service);
bnep_channel_t * btstack_memory_bnep_channel_get(void);
void   btstack_memory_bnep_channel_free(bnep_channel_t *bnep_channel);

goep_server_service_t * btstack_memory_goep_server_service_get(void);
void   btstack_memory_goep_server_service_free(goep_server_service_t *goep_server_service);
goep_server_connection_t * btstack_memory_goep_server_connection_get(void);
void   btstack_memory_goep_server_connection_free(goep_server_connection_t *goep_server_connection);

hfp_connection_t * btstack_memory_hfp_connection_get(void);
void   btstack_memory_hfp_connection_free(hfp_connection_t *hfp_connection);

hid_host_connection_t * btstack_memory_hid_host_connection_get(void);
void   btstack_memory_hid_host_connection_free(hid_host_connection_t *hid_host_connection);

service_record_item_t * btstack_memory_service_record_item_get(void);
void   btstack_memory_service_record_item_free(service_record_item_t *service_record_item);

avdtp_stream_endpoint_t * btstack_memory_avdtp_stream_endpoint_get(void);
void   btstack_memory_avdtp_stream_endpoint_free(avdtp_stream_endpoint_t *avdtp_stream_endpoint);

avdtp_connection_t * btstack_memory_avdtp_connection_get(void);
void   btstack_memory_avdtp_connection_free(avdtp_connection_t *avdtp_connection);

avrcp_connection_t * btstack_memory_avrcp_connection_get(void);
void   btstack_memory_avrcp_connection_free(avrcp_connection_t *avrcp_connection);

avrcp_browsing_connection_t * btstack_memory_avrcp_browsing_connection_get(void);
void   btstack_memory_avrcp_browsing_connection_free(avrcp_browsing_connection_t *avrcp_browsing_connection);

#endif
#ifdef ENABLE_BLE
battery_service_client_t * btstack_memory_battery_service_client_get(void);
void   btstack_memory_battery_service_client_free(battery_service_client_t *battery_service_client);
gatt_client_t * btstack_memory_gatt_client_get(void);
void   btstack_memory_gatt_client_free(gatt_client_t *gatt_client);
hids_client_t * btstack_memory_hids_client_get(void);
void   btstack_memory_hids_client_free(hids_client_t *hids_client);
scan_parameters_service_client_t * btstack_memory_scan_parameters_service_client_get(void);
void   btstack_memory_scan_parameters_service_client_free(scan_parameters_service_client_t *scan_parameters_service_client);
sm_lookup_entry_t * btstack_memory_sm_lookup_entry_get(void);
void   btstack_memory_sm_lookup_entry_free(sm_lookup_entry_t *sm_lookup_entry);
whitelist_entry_t * btstack_memory_whitelist_entry_get(void);
void   btstack_memory_whitelist_entry_free(whitelist_entry_t *whitelist_entry);
periodic_advertiser_list_entry_t * btstack_memory_periodic_advertiser_list_entry_get(void);
void   btstack_memory_periodic_advertiser_list_entry_free(periodic_advertiser_list_entry_t *periodic_advertiser_list_entry);

#endif
#ifdef ENABLE_MESH
mesh_network_pdu_t * btstack_memory_mesh_network_pdu_get(void);
void   btstack_memory_mesh_network_pdu_free(mesh_network_pdu_t *mesh_network_pdu);
mesh_segmented_pdu_t * btstack_memory_mesh_segmented_pdu_get(void);
void   btstack_memory_mesh_segmented_pdu_free(mesh_segmented_pdu_t *mesh_segmented_pdu);
mesh_upper_transport_pdu_t * btstack_memory_mesh_upper_transport_pdu_get(void);
void   btstack_memory_mesh_upper_transport_pdu_free(mesh_upper_transport_pdu_t *mesh_upper_transport_pdu);
mesh_network_key_t * btstack_memory_mesh_network_key_get(void);
void   btstack_memory_mesh_network_key_free(mesh_network_key_t *mesh_network_key);
mesh_transport_key_t * btstack_memory_mesh_transport_key_get(void);
void   btstack_memory_mesh_transport_key_free(mesh_transport_key_t *mesh_transport_key);
mesh_virtual_address_t * btstack_memory_mesh_virtual_address_get(void);
void   btstack_memory_mesh_virtual_address_free(mesh_virtual_address_t *mesh_virtual_address);
mesh_subnet_t * btstack_memory_mesh_subnet_get(void);
void   btstack_memory_mesh_subnet_free(mesh_subnet_t *mesh_subnet);

#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
hci_iso_stream_t * btstack_memory_hci_iso_stream_get(void);
void   btstack_memory_hci_iso_stream_free(hci_iso_stream_t *hci_iso_stream);

#endif

#if defined __cplusplus
}
#endif

#endif // BTSTACK_MEMORY_H

