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



/**
 * @title BTstack Memory Management
 *
 * Manage BTstack objects using configurable pools or dynamic allocation.
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
#include "ble/gatt-service/hids_host.h"
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

/**
 * @brief Allocate a hci_connection_t object, or return NULL if unavailable.
 */
hci_connection_t * btstack_memory_hci_connection_get(void);
/**
 * @brief Release a hci_connection_t object.
 */
void   btstack_memory_hci_connection_free(hci_connection_t *hci_connection);

/**
 * @brief Allocate a l2cap_service_t object, or return NULL if unavailable.
 */
l2cap_service_t * btstack_memory_l2cap_service_get(void);
/**
 * @brief Release a l2cap_service_t object.
 */
void   btstack_memory_l2cap_service_free(l2cap_service_t *l2cap_service);
/**
 * @brief Allocate a l2cap_channel_t object, or return NULL if unavailable.
 */
l2cap_channel_t * btstack_memory_l2cap_channel_get(void);
/**
 * @brief Release a l2cap_channel_t object.
 */
void   btstack_memory_l2cap_channel_free(l2cap_channel_t *l2cap_channel);

#ifdef ENABLE_CLASSIC
/**
 * @brief Allocate a rfcomm_multiplexer_t object, or return NULL if unavailable.
 */
rfcomm_multiplexer_t * btstack_memory_rfcomm_multiplexer_get(void);
/**
 * @brief Release a rfcomm_multiplexer_t object.
 */
void   btstack_memory_rfcomm_multiplexer_free(rfcomm_multiplexer_t *rfcomm_multiplexer);
/**
 * @brief Allocate a rfcomm_service_t object, or return NULL if unavailable.
 */
rfcomm_service_t * btstack_memory_rfcomm_service_get(void);
/**
 * @brief Release a rfcomm_service_t object.
 */
void   btstack_memory_rfcomm_service_free(rfcomm_service_t *rfcomm_service);
/**
 * @brief Allocate a rfcomm_channel_t object, or return NULL if unavailable.
 */
rfcomm_channel_t * btstack_memory_rfcomm_channel_get(void);
/**
 * @brief Release a rfcomm_channel_t object.
 */
void   btstack_memory_rfcomm_channel_free(rfcomm_channel_t *rfcomm_channel);

/**
 * @brief Allocate a btstack_link_key_db_memory_entry_t object, or return NULL if unavailable.
 */
btstack_link_key_db_memory_entry_t * btstack_memory_btstack_link_key_db_memory_entry_get(void);
/**
 * @brief Release a btstack_link_key_db_memory_entry_t object.
 */
void   btstack_memory_btstack_link_key_db_memory_entry_free(btstack_link_key_db_memory_entry_t *btstack_link_key_db_memory_entry);

/**
 * @brief Allocate a bnep_service_t object, or return NULL if unavailable.
 */
bnep_service_t * btstack_memory_bnep_service_get(void);
/**
 * @brief Release a bnep_service_t object.
 */
void   btstack_memory_bnep_service_free(bnep_service_t *bnep_service);
/**
 * @brief Allocate a bnep_channel_t object, or return NULL if unavailable.
 */
bnep_channel_t * btstack_memory_bnep_channel_get(void);
/**
 * @brief Release a bnep_channel_t object.
 */
void   btstack_memory_bnep_channel_free(bnep_channel_t *bnep_channel);

/**
 * @brief Allocate a goep_server_service_t object, or return NULL if unavailable.
 */
goep_server_service_t * btstack_memory_goep_server_service_get(void);
/**
 * @brief Release a goep_server_service_t object.
 */
void   btstack_memory_goep_server_service_free(goep_server_service_t *goep_server_service);
/**
 * @brief Allocate a goep_server_connection_t object, or return NULL if unavailable.
 */
goep_server_connection_t * btstack_memory_goep_server_connection_get(void);
/**
 * @brief Release a goep_server_connection_t object.
 */
void   btstack_memory_goep_server_connection_free(goep_server_connection_t *goep_server_connection);

/**
 * @brief Allocate a hfp_connection_t object, or return NULL if unavailable.
 */
hfp_connection_t * btstack_memory_hfp_connection_get(void);
/**
 * @brief Release a hfp_connection_t object.
 */
void   btstack_memory_hfp_connection_free(hfp_connection_t *hfp_connection);

/**
 * @brief Allocate a hid_host_connection_t object, or return NULL if unavailable.
 */
hid_host_connection_t * btstack_memory_hid_host_connection_get(void);
/**
 * @brief Release a hid_host_connection_t object.
 */
void   btstack_memory_hid_host_connection_free(hid_host_connection_t *hid_host_connection);

/**
 * @brief Allocate a service_record_item_t object, or return NULL if unavailable.
 */
service_record_item_t * btstack_memory_service_record_item_get(void);
/**
 * @brief Release a service_record_item_t object.
 */
void   btstack_memory_service_record_item_free(service_record_item_t *service_record_item);

/**
 * @brief Allocate a avdtp_stream_endpoint_t object, or return NULL if unavailable.
 */
avdtp_stream_endpoint_t * btstack_memory_avdtp_stream_endpoint_get(void);
/**
 * @brief Release a avdtp_stream_endpoint_t object.
 */
void   btstack_memory_avdtp_stream_endpoint_free(avdtp_stream_endpoint_t *avdtp_stream_endpoint);

/**
 * @brief Allocate a avdtp_connection_t object, or return NULL if unavailable.
 */
avdtp_connection_t * btstack_memory_avdtp_connection_get(void);
/**
 * @brief Release a avdtp_connection_t object.
 */
void   btstack_memory_avdtp_connection_free(avdtp_connection_t *avdtp_connection);

/**
 * @brief Allocate a avrcp_connection_t object, or return NULL if unavailable.
 */
avrcp_connection_t * btstack_memory_avrcp_connection_get(void);
/**
 * @brief Release a avrcp_connection_t object.
 */
void   btstack_memory_avrcp_connection_free(avrcp_connection_t *avrcp_connection);

/**
 * @brief Allocate a avrcp_browsing_connection_t object, or return NULL if unavailable.
 */
avrcp_browsing_connection_t * btstack_memory_avrcp_browsing_connection_get(void);
/**
 * @brief Release a avrcp_browsing_connection_t object.
 */
void   btstack_memory_avrcp_browsing_connection_free(avrcp_browsing_connection_t *avrcp_browsing_connection);

#endif
#ifdef ENABLE_BLE
/**
 * @brief Allocate a battery_service_client_t object, or return NULL if unavailable.
 */
battery_service_client_t * btstack_memory_battery_service_client_get(void);
/**
 * @brief Release a battery_service_client_t object.
 */
void   btstack_memory_battery_service_client_free(battery_service_client_t *battery_service_client);
/**
 * @brief Allocate a gatt_client_t object, or return NULL if unavailable.
 */
gatt_client_t * btstack_memory_gatt_client_get(void);
/**
 * @brief Release a gatt_client_t object.
 */
void   btstack_memory_gatt_client_free(gatt_client_t *gatt_client);
/**
 * @brief Allocate a hids_host_t object, or return NULL if unavailable.
 */
hids_host_t * btstack_memory_hids_host_get(void);
/**
 * @brief Release a hids_host_t object.
 */
void   btstack_memory_hids_host_free(hids_host_t *hids_host);
/**
 * @brief Allocate a sm_lookup_entry_t object, or return NULL if unavailable.
 */
sm_lookup_entry_t * btstack_memory_sm_lookup_entry_get(void);
/**
 * @brief Release a sm_lookup_entry_t object.
 */
void   btstack_memory_sm_lookup_entry_free(sm_lookup_entry_t *sm_lookup_entry);
/**
 * @brief Allocate a whitelist_entry_t object, or return NULL if unavailable.
 */
whitelist_entry_t * btstack_memory_whitelist_entry_get(void);
/**
 * @brief Release a whitelist_entry_t object.
 */
void   btstack_memory_whitelist_entry_free(whitelist_entry_t *whitelist_entry);
/**
 * @brief Allocate a periodic_advertiser_list_entry_t object, or return NULL if unavailable.
 */
periodic_advertiser_list_entry_t * btstack_memory_periodic_advertiser_list_entry_get(void);
/**
 * @brief Release a periodic_advertiser_list_entry_t object.
 */
void   btstack_memory_periodic_advertiser_list_entry_free(periodic_advertiser_list_entry_t *periodic_advertiser_list_entry);

#endif
#ifdef ENABLE_MESH
/**
 * @brief Allocate a mesh_network_pdu_t object, or return NULL if unavailable.
 */
mesh_network_pdu_t * btstack_memory_mesh_network_pdu_get(void);
/**
 * @brief Release a mesh_network_pdu_t object.
 */
void   btstack_memory_mesh_network_pdu_free(mesh_network_pdu_t *mesh_network_pdu);
/**
 * @brief Allocate a mesh_segmented_pdu_t object, or return NULL if unavailable.
 */
mesh_segmented_pdu_t * btstack_memory_mesh_segmented_pdu_get(void);
/**
 * @brief Release a mesh_segmented_pdu_t object.
 */
void   btstack_memory_mesh_segmented_pdu_free(mesh_segmented_pdu_t *mesh_segmented_pdu);
/**
 * @brief Allocate a mesh_upper_transport_pdu_t object, or return NULL if unavailable.
 */
mesh_upper_transport_pdu_t * btstack_memory_mesh_upper_transport_pdu_get(void);
/**
 * @brief Release a mesh_upper_transport_pdu_t object.
 */
void   btstack_memory_mesh_upper_transport_pdu_free(mesh_upper_transport_pdu_t *mesh_upper_transport_pdu);
/**
 * @brief Allocate a mesh_network_key_t object, or return NULL if unavailable.
 */
mesh_network_key_t * btstack_memory_mesh_network_key_get(void);
/**
 * @brief Release a mesh_network_key_t object.
 */
void   btstack_memory_mesh_network_key_free(mesh_network_key_t *mesh_network_key);
/**
 * @brief Allocate a mesh_transport_key_t object, or return NULL if unavailable.
 */
mesh_transport_key_t * btstack_memory_mesh_transport_key_get(void);
/**
 * @brief Release a mesh_transport_key_t object.
 */
void   btstack_memory_mesh_transport_key_free(mesh_transport_key_t *mesh_transport_key);
/**
 * @brief Allocate a mesh_virtual_address_t object, or return NULL if unavailable.
 */
mesh_virtual_address_t * btstack_memory_mesh_virtual_address_get(void);
/**
 * @brief Release a mesh_virtual_address_t object.
 */
void   btstack_memory_mesh_virtual_address_free(mesh_virtual_address_t *mesh_virtual_address);
/**
 * @brief Allocate a mesh_subnet_t object, or return NULL if unavailable.
 */
mesh_subnet_t * btstack_memory_mesh_subnet_get(void);
/**
 * @brief Release a mesh_subnet_t object.
 */
void   btstack_memory_mesh_subnet_free(mesh_subnet_t *mesh_subnet);

#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
/**
 * @brief Allocate a hci_iso_stream_t object, or return NULL if unavailable.
 */
hci_iso_stream_t * btstack_memory_hci_iso_stream_get(void);
/**
 * @brief Release a hci_iso_stream_t object.
 */
void   btstack_memory_hci_iso_stream_free(hci_iso_stream_t *hci_iso_stream);

#endif


#ifdef ENABLE_MALLOC_TEST
/**
 * @brief Enable or disable simulated allocation failures for testing.
 */
void btstack_memory_simulate_malloc_failure(bool out_of_memory);
#endif

#if defined __cplusplus
}
#endif

#endif // BTSTACK_MEMORY_H

