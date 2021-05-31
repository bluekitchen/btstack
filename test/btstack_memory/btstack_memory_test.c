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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// malloc hook
static int simulate_no_memory;
extern "C" void * test_malloc(size_t size);
void * test_malloc(size_t size){
    if (simulate_no_memory) return NULL;
    return malloc(size);
}

#include "btstack_config.h"

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "bluetooth_data_types.h"
#include "btstack_util.h"
#include "btstack_memory.h"


TEST_GROUP(btstack_memory){
    void setup(void){
        btstack_memory_init();
        simulate_no_memory = 0;
    }
};

#ifdef HAVE_MALLOC
TEST(btstack_memory, deinit){
    // alloc buffers 1,2,3
    hci_connection_t * buffer_1 = btstack_memory_hci_connection_get();
    hci_connection_t * buffer_2 = btstack_memory_hci_connection_get();
    hci_connection_t * buffer_3 = btstack_memory_hci_connection_get();
    // free first one in list
    btstack_memory_hci_connection_free(buffer_3);
    // free second one in list
    btstack_memory_hci_connection_free(buffer_1);
    // leave buffer in list
    (void) buffer_2;
    btstack_memory_deinit();
}
#endif




TEST(btstack_memory, hci_connection_GetAndFree){
    hci_connection_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_hci_connection_get();
    CHECK(context != NULL);
    btstack_memory_hci_connection_free(context);
#else
#ifdef MAX_NR_HCI_CONNECTIONS
    // single
    context = btstack_memory_hci_connection_get();
    CHECK(context != NULL);
    btstack_memory_hci_connection_free(context);
#else
    // none
    context = btstack_memory_hci_connection_get();
    CHECK(context == NULL);
    btstack_memory_hci_connection_free(context);
#endif
#endif
}

TEST(btstack_memory, hci_connection_NotEnoughBuffers){
    hci_connection_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_HCI_CONNECTIONS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_HCI_CONNECTIONS; i++){
        context = btstack_memory_hci_connection_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_hci_connection_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, l2cap_service_GetAndFree){
    l2cap_service_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_l2cap_service_get();
    CHECK(context != NULL);
    btstack_memory_l2cap_service_free(context);
#else
#ifdef MAX_NR_L2CAP_SERVICES
    // single
    context = btstack_memory_l2cap_service_get();
    CHECK(context != NULL);
    btstack_memory_l2cap_service_free(context);
#else
    // none
    context = btstack_memory_l2cap_service_get();
    CHECK(context == NULL);
    btstack_memory_l2cap_service_free(context);
#endif
#endif
}

TEST(btstack_memory, l2cap_service_NotEnoughBuffers){
    l2cap_service_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_L2CAP_SERVICES
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_L2CAP_SERVICES; i++){
        context = btstack_memory_l2cap_service_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_l2cap_service_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, l2cap_channel_GetAndFree){
    l2cap_channel_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_l2cap_channel_get();
    CHECK(context != NULL);
    btstack_memory_l2cap_channel_free(context);
#else
#ifdef MAX_NR_L2CAP_CHANNELS
    // single
    context = btstack_memory_l2cap_channel_get();
    CHECK(context != NULL);
    btstack_memory_l2cap_channel_free(context);
#else
    // none
    context = btstack_memory_l2cap_channel_get();
    CHECK(context == NULL);
    btstack_memory_l2cap_channel_free(context);
#endif
#endif
}

TEST(btstack_memory, l2cap_channel_NotEnoughBuffers){
    l2cap_channel_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_L2CAP_CHANNELS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_L2CAP_CHANNELS; i++){
        context = btstack_memory_l2cap_channel_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_l2cap_channel_get();
    CHECK(context == NULL);
}

#ifdef ENABLE_CLASSIC


TEST(btstack_memory, rfcomm_multiplexer_GetAndFree){
    rfcomm_multiplexer_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_rfcomm_multiplexer_get();
    CHECK(context != NULL);
    btstack_memory_rfcomm_multiplexer_free(context);
#else
#ifdef MAX_NR_RFCOMM_MULTIPLEXERS
    // single
    context = btstack_memory_rfcomm_multiplexer_get();
    CHECK(context != NULL);
    btstack_memory_rfcomm_multiplexer_free(context);
#else
    // none
    context = btstack_memory_rfcomm_multiplexer_get();
    CHECK(context == NULL);
    btstack_memory_rfcomm_multiplexer_free(context);
#endif
#endif
}

TEST(btstack_memory, rfcomm_multiplexer_NotEnoughBuffers){
    rfcomm_multiplexer_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_RFCOMM_MULTIPLEXERS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_RFCOMM_MULTIPLEXERS; i++){
        context = btstack_memory_rfcomm_multiplexer_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_rfcomm_multiplexer_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, rfcomm_service_GetAndFree){
    rfcomm_service_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_rfcomm_service_get();
    CHECK(context != NULL);
    btstack_memory_rfcomm_service_free(context);
#else
#ifdef MAX_NR_RFCOMM_SERVICES
    // single
    context = btstack_memory_rfcomm_service_get();
    CHECK(context != NULL);
    btstack_memory_rfcomm_service_free(context);
#else
    // none
    context = btstack_memory_rfcomm_service_get();
    CHECK(context == NULL);
    btstack_memory_rfcomm_service_free(context);
#endif
#endif
}

TEST(btstack_memory, rfcomm_service_NotEnoughBuffers){
    rfcomm_service_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_RFCOMM_SERVICES
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_RFCOMM_SERVICES; i++){
        context = btstack_memory_rfcomm_service_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_rfcomm_service_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, rfcomm_channel_GetAndFree){
    rfcomm_channel_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_rfcomm_channel_get();
    CHECK(context != NULL);
    btstack_memory_rfcomm_channel_free(context);
#else
#ifdef MAX_NR_RFCOMM_CHANNELS
    // single
    context = btstack_memory_rfcomm_channel_get();
    CHECK(context != NULL);
    btstack_memory_rfcomm_channel_free(context);
#else
    // none
    context = btstack_memory_rfcomm_channel_get();
    CHECK(context == NULL);
    btstack_memory_rfcomm_channel_free(context);
#endif
#endif
}

TEST(btstack_memory, rfcomm_channel_NotEnoughBuffers){
    rfcomm_channel_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_RFCOMM_CHANNELS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_RFCOMM_CHANNELS; i++){
        context = btstack_memory_rfcomm_channel_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_rfcomm_channel_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, btstack_link_key_db_memory_entry_GetAndFree){
    btstack_link_key_db_memory_entry_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_btstack_link_key_db_memory_entry_get();
    CHECK(context != NULL);
    btstack_memory_btstack_link_key_db_memory_entry_free(context);
#else
#ifdef MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES
    // single
    context = btstack_memory_btstack_link_key_db_memory_entry_get();
    CHECK(context != NULL);
    btstack_memory_btstack_link_key_db_memory_entry_free(context);
#else
    // none
    context = btstack_memory_btstack_link_key_db_memory_entry_get();
    CHECK(context == NULL);
    btstack_memory_btstack_link_key_db_memory_entry_free(context);
#endif
#endif
}

TEST(btstack_memory, btstack_link_key_db_memory_entry_NotEnoughBuffers){
    btstack_link_key_db_memory_entry_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES; i++){
        context = btstack_memory_btstack_link_key_db_memory_entry_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_btstack_link_key_db_memory_entry_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, bnep_service_GetAndFree){
    bnep_service_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_bnep_service_get();
    CHECK(context != NULL);
    btstack_memory_bnep_service_free(context);
#else
#ifdef MAX_NR_BNEP_SERVICES
    // single
    context = btstack_memory_bnep_service_get();
    CHECK(context != NULL);
    btstack_memory_bnep_service_free(context);
#else
    // none
    context = btstack_memory_bnep_service_get();
    CHECK(context == NULL);
    btstack_memory_bnep_service_free(context);
#endif
#endif
}

TEST(btstack_memory, bnep_service_NotEnoughBuffers){
    bnep_service_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_BNEP_SERVICES
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_BNEP_SERVICES; i++){
        context = btstack_memory_bnep_service_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_bnep_service_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, bnep_channel_GetAndFree){
    bnep_channel_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_bnep_channel_get();
    CHECK(context != NULL);
    btstack_memory_bnep_channel_free(context);
#else
#ifdef MAX_NR_BNEP_CHANNELS
    // single
    context = btstack_memory_bnep_channel_get();
    CHECK(context != NULL);
    btstack_memory_bnep_channel_free(context);
#else
    // none
    context = btstack_memory_bnep_channel_get();
    CHECK(context == NULL);
    btstack_memory_bnep_channel_free(context);
#endif
#endif
}

TEST(btstack_memory, bnep_channel_NotEnoughBuffers){
    bnep_channel_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_BNEP_CHANNELS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_BNEP_CHANNELS; i++){
        context = btstack_memory_bnep_channel_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_bnep_channel_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, hfp_connection_GetAndFree){
    hfp_connection_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_hfp_connection_get();
    CHECK(context != NULL);
    btstack_memory_hfp_connection_free(context);
#else
#ifdef MAX_NR_HFP_CONNECTIONS
    // single
    context = btstack_memory_hfp_connection_get();
    CHECK(context != NULL);
    btstack_memory_hfp_connection_free(context);
#else
    // none
    context = btstack_memory_hfp_connection_get();
    CHECK(context == NULL);
    btstack_memory_hfp_connection_free(context);
#endif
#endif
}

TEST(btstack_memory, hfp_connection_NotEnoughBuffers){
    hfp_connection_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_HFP_CONNECTIONS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_HFP_CONNECTIONS; i++){
        context = btstack_memory_hfp_connection_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_hfp_connection_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, hid_host_connection_GetAndFree){
    hid_host_connection_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_hid_host_connection_get();
    CHECK(context != NULL);
    btstack_memory_hid_host_connection_free(context);
#else
#ifdef MAX_NR_HID_HOST_CONNECTIONS
    // single
    context = btstack_memory_hid_host_connection_get();
    CHECK(context != NULL);
    btstack_memory_hid_host_connection_free(context);
#else
    // none
    context = btstack_memory_hid_host_connection_get();
    CHECK(context == NULL);
    btstack_memory_hid_host_connection_free(context);
#endif
#endif
}

TEST(btstack_memory, hid_host_connection_NotEnoughBuffers){
    hid_host_connection_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_HID_HOST_CONNECTIONS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_HID_HOST_CONNECTIONS; i++){
        context = btstack_memory_hid_host_connection_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_hid_host_connection_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, service_record_item_GetAndFree){
    service_record_item_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_service_record_item_get();
    CHECK(context != NULL);
    btstack_memory_service_record_item_free(context);
#else
#ifdef MAX_NR_SERVICE_RECORD_ITEMS
    // single
    context = btstack_memory_service_record_item_get();
    CHECK(context != NULL);
    btstack_memory_service_record_item_free(context);
#else
    // none
    context = btstack_memory_service_record_item_get();
    CHECK(context == NULL);
    btstack_memory_service_record_item_free(context);
#endif
#endif
}

TEST(btstack_memory, service_record_item_NotEnoughBuffers){
    service_record_item_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_SERVICE_RECORD_ITEMS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_SERVICE_RECORD_ITEMS; i++){
        context = btstack_memory_service_record_item_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_service_record_item_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, avdtp_stream_endpoint_GetAndFree){
    avdtp_stream_endpoint_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_avdtp_stream_endpoint_get();
    CHECK(context != NULL);
    btstack_memory_avdtp_stream_endpoint_free(context);
#else
#ifdef MAX_NR_AVDTP_STREAM_ENDPOINTS
    // single
    context = btstack_memory_avdtp_stream_endpoint_get();
    CHECK(context != NULL);
    btstack_memory_avdtp_stream_endpoint_free(context);
#else
    // none
    context = btstack_memory_avdtp_stream_endpoint_get();
    CHECK(context == NULL);
    btstack_memory_avdtp_stream_endpoint_free(context);
#endif
#endif
}

TEST(btstack_memory, avdtp_stream_endpoint_NotEnoughBuffers){
    avdtp_stream_endpoint_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_AVDTP_STREAM_ENDPOINTS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_AVDTP_STREAM_ENDPOINTS; i++){
        context = btstack_memory_avdtp_stream_endpoint_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_avdtp_stream_endpoint_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, avdtp_connection_GetAndFree){
    avdtp_connection_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_avdtp_connection_get();
    CHECK(context != NULL);
    btstack_memory_avdtp_connection_free(context);
#else
#ifdef MAX_NR_AVDTP_CONNECTIONS
    // single
    context = btstack_memory_avdtp_connection_get();
    CHECK(context != NULL);
    btstack_memory_avdtp_connection_free(context);
#else
    // none
    context = btstack_memory_avdtp_connection_get();
    CHECK(context == NULL);
    btstack_memory_avdtp_connection_free(context);
#endif
#endif
}

TEST(btstack_memory, avdtp_connection_NotEnoughBuffers){
    avdtp_connection_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_AVDTP_CONNECTIONS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_AVDTP_CONNECTIONS; i++){
        context = btstack_memory_avdtp_connection_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_avdtp_connection_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, avrcp_connection_GetAndFree){
    avrcp_connection_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_avrcp_connection_get();
    CHECK(context != NULL);
    btstack_memory_avrcp_connection_free(context);
#else
#ifdef MAX_NR_AVRCP_CONNECTIONS
    // single
    context = btstack_memory_avrcp_connection_get();
    CHECK(context != NULL);
    btstack_memory_avrcp_connection_free(context);
#else
    // none
    context = btstack_memory_avrcp_connection_get();
    CHECK(context == NULL);
    btstack_memory_avrcp_connection_free(context);
#endif
#endif
}

TEST(btstack_memory, avrcp_connection_NotEnoughBuffers){
    avrcp_connection_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_AVRCP_CONNECTIONS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_AVRCP_CONNECTIONS; i++){
        context = btstack_memory_avrcp_connection_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_avrcp_connection_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, avrcp_browsing_connection_GetAndFree){
    avrcp_browsing_connection_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_avrcp_browsing_connection_get();
    CHECK(context != NULL);
    btstack_memory_avrcp_browsing_connection_free(context);
#else
#ifdef MAX_NR_AVRCP_BROWSING_CONNECTIONS
    // single
    context = btstack_memory_avrcp_browsing_connection_get();
    CHECK(context != NULL);
    btstack_memory_avrcp_browsing_connection_free(context);
#else
    // none
    context = btstack_memory_avrcp_browsing_connection_get();
    CHECK(context == NULL);
    btstack_memory_avrcp_browsing_connection_free(context);
#endif
#endif
}

TEST(btstack_memory, avrcp_browsing_connection_NotEnoughBuffers){
    avrcp_browsing_connection_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_AVRCP_BROWSING_CONNECTIONS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_AVRCP_BROWSING_CONNECTIONS; i++){
        context = btstack_memory_avrcp_browsing_connection_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_avrcp_browsing_connection_get();
    CHECK(context == NULL);
}

#endif
#ifdef ENABLE_BLE


TEST(btstack_memory, battery_service_client_GetAndFree){
    battery_service_client_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_battery_service_client_get();
    CHECK(context != NULL);
    btstack_memory_battery_service_client_free(context);
#else
#ifdef MAX_NR_BATTERY_SERVICE_CLIENTS
    // single
    context = btstack_memory_battery_service_client_get();
    CHECK(context != NULL);
    btstack_memory_battery_service_client_free(context);
#else
    // none
    context = btstack_memory_battery_service_client_get();
    CHECK(context == NULL);
    btstack_memory_battery_service_client_free(context);
#endif
#endif
}

TEST(btstack_memory, battery_service_client_NotEnoughBuffers){
    battery_service_client_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_BATTERY_SERVICE_CLIENTS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_BATTERY_SERVICE_CLIENTS; i++){
        context = btstack_memory_battery_service_client_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_battery_service_client_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, gatt_client_GetAndFree){
    gatt_client_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_gatt_client_get();
    CHECK(context != NULL);
    btstack_memory_gatt_client_free(context);
#else
#ifdef MAX_NR_GATT_CLIENTS
    // single
    context = btstack_memory_gatt_client_get();
    CHECK(context != NULL);
    btstack_memory_gatt_client_free(context);
#else
    // none
    context = btstack_memory_gatt_client_get();
    CHECK(context == NULL);
    btstack_memory_gatt_client_free(context);
#endif
#endif
}

TEST(btstack_memory, gatt_client_NotEnoughBuffers){
    gatt_client_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_GATT_CLIENTS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_GATT_CLIENTS; i++){
        context = btstack_memory_gatt_client_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_gatt_client_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, hids_client_GetAndFree){
    hids_client_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_hids_client_get();
    CHECK(context != NULL);
    btstack_memory_hids_client_free(context);
#else
#ifdef MAX_NR_HIDS_CLIENTS
    // single
    context = btstack_memory_hids_client_get();
    CHECK(context != NULL);
    btstack_memory_hids_client_free(context);
#else
    // none
    context = btstack_memory_hids_client_get();
    CHECK(context == NULL);
    btstack_memory_hids_client_free(context);
#endif
#endif
}

TEST(btstack_memory, hids_client_NotEnoughBuffers){
    hids_client_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_HIDS_CLIENTS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_HIDS_CLIENTS; i++){
        context = btstack_memory_hids_client_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_hids_client_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, scan_parameters_service_client_GetAndFree){
    scan_parameters_service_client_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_scan_parameters_service_client_get();
    CHECK(context != NULL);
    btstack_memory_scan_parameters_service_client_free(context);
#else
#ifdef MAX_NR_SCAN_PARAMETERS_SERVICE_CLIENTS
    // single
    context = btstack_memory_scan_parameters_service_client_get();
    CHECK(context != NULL);
    btstack_memory_scan_parameters_service_client_free(context);
#else
    // none
    context = btstack_memory_scan_parameters_service_client_get();
    CHECK(context == NULL);
    btstack_memory_scan_parameters_service_client_free(context);
#endif
#endif
}

TEST(btstack_memory, scan_parameters_service_client_NotEnoughBuffers){
    scan_parameters_service_client_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_SCAN_PARAMETERS_SERVICE_CLIENTS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_SCAN_PARAMETERS_SERVICE_CLIENTS; i++){
        context = btstack_memory_scan_parameters_service_client_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_scan_parameters_service_client_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, sm_lookup_entry_GetAndFree){
    sm_lookup_entry_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_sm_lookup_entry_get();
    CHECK(context != NULL);
    btstack_memory_sm_lookup_entry_free(context);
#else
#ifdef MAX_NR_SM_LOOKUP_ENTRIES
    // single
    context = btstack_memory_sm_lookup_entry_get();
    CHECK(context != NULL);
    btstack_memory_sm_lookup_entry_free(context);
#else
    // none
    context = btstack_memory_sm_lookup_entry_get();
    CHECK(context == NULL);
    btstack_memory_sm_lookup_entry_free(context);
#endif
#endif
}

TEST(btstack_memory, sm_lookup_entry_NotEnoughBuffers){
    sm_lookup_entry_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_SM_LOOKUP_ENTRIES
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_SM_LOOKUP_ENTRIES; i++){
        context = btstack_memory_sm_lookup_entry_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_sm_lookup_entry_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, whitelist_entry_GetAndFree){
    whitelist_entry_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_whitelist_entry_get();
    CHECK(context != NULL);
    btstack_memory_whitelist_entry_free(context);
#else
#ifdef MAX_NR_WHITELIST_ENTRIES
    // single
    context = btstack_memory_whitelist_entry_get();
    CHECK(context != NULL);
    btstack_memory_whitelist_entry_free(context);
#else
    // none
    context = btstack_memory_whitelist_entry_get();
    CHECK(context == NULL);
    btstack_memory_whitelist_entry_free(context);
#endif
#endif
}

TEST(btstack_memory, whitelist_entry_NotEnoughBuffers){
    whitelist_entry_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_WHITELIST_ENTRIES
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_WHITELIST_ENTRIES; i++){
        context = btstack_memory_whitelist_entry_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_whitelist_entry_get();
    CHECK(context == NULL);
}

#endif
#ifdef ENABLE_MESH


TEST(btstack_memory, mesh_network_pdu_GetAndFree){
    mesh_network_pdu_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_mesh_network_pdu_get();
    CHECK(context != NULL);
    btstack_memory_mesh_network_pdu_free(context);
#else
#ifdef MAX_NR_MESH_NETWORK_PDUS
    // single
    context = btstack_memory_mesh_network_pdu_get();
    CHECK(context != NULL);
    btstack_memory_mesh_network_pdu_free(context);
#else
    // none
    context = btstack_memory_mesh_network_pdu_get();
    CHECK(context == NULL);
    btstack_memory_mesh_network_pdu_free(context);
#endif
#endif
}

TEST(btstack_memory, mesh_network_pdu_NotEnoughBuffers){
    mesh_network_pdu_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_MESH_NETWORK_PDUS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_MESH_NETWORK_PDUS; i++){
        context = btstack_memory_mesh_network_pdu_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_mesh_network_pdu_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, mesh_segmented_pdu_GetAndFree){
    mesh_segmented_pdu_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_mesh_segmented_pdu_get();
    CHECK(context != NULL);
    btstack_memory_mesh_segmented_pdu_free(context);
#else
#ifdef MAX_NR_MESH_SEGMENTED_PDUS
    // single
    context = btstack_memory_mesh_segmented_pdu_get();
    CHECK(context != NULL);
    btstack_memory_mesh_segmented_pdu_free(context);
#else
    // none
    context = btstack_memory_mesh_segmented_pdu_get();
    CHECK(context == NULL);
    btstack_memory_mesh_segmented_pdu_free(context);
#endif
#endif
}

TEST(btstack_memory, mesh_segmented_pdu_NotEnoughBuffers){
    mesh_segmented_pdu_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_MESH_SEGMENTED_PDUS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_MESH_SEGMENTED_PDUS; i++){
        context = btstack_memory_mesh_segmented_pdu_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_mesh_segmented_pdu_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, mesh_upper_transport_pdu_GetAndFree){
    mesh_upper_transport_pdu_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_mesh_upper_transport_pdu_get();
    CHECK(context != NULL);
    btstack_memory_mesh_upper_transport_pdu_free(context);
#else
#ifdef MAX_NR_MESH_UPPER_TRANSPORT_PDUS
    // single
    context = btstack_memory_mesh_upper_transport_pdu_get();
    CHECK(context != NULL);
    btstack_memory_mesh_upper_transport_pdu_free(context);
#else
    // none
    context = btstack_memory_mesh_upper_transport_pdu_get();
    CHECK(context == NULL);
    btstack_memory_mesh_upper_transport_pdu_free(context);
#endif
#endif
}

TEST(btstack_memory, mesh_upper_transport_pdu_NotEnoughBuffers){
    mesh_upper_transport_pdu_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_MESH_UPPER_TRANSPORT_PDUS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_MESH_UPPER_TRANSPORT_PDUS; i++){
        context = btstack_memory_mesh_upper_transport_pdu_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_mesh_upper_transport_pdu_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, mesh_network_key_GetAndFree){
    mesh_network_key_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_mesh_network_key_get();
    CHECK(context != NULL);
    btstack_memory_mesh_network_key_free(context);
#else
#ifdef MAX_NR_MESH_NETWORK_KEYS
    // single
    context = btstack_memory_mesh_network_key_get();
    CHECK(context != NULL);
    btstack_memory_mesh_network_key_free(context);
#else
    // none
    context = btstack_memory_mesh_network_key_get();
    CHECK(context == NULL);
    btstack_memory_mesh_network_key_free(context);
#endif
#endif
}

TEST(btstack_memory, mesh_network_key_NotEnoughBuffers){
    mesh_network_key_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_MESH_NETWORK_KEYS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_MESH_NETWORK_KEYS; i++){
        context = btstack_memory_mesh_network_key_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_mesh_network_key_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, mesh_transport_key_GetAndFree){
    mesh_transport_key_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_mesh_transport_key_get();
    CHECK(context != NULL);
    btstack_memory_mesh_transport_key_free(context);
#else
#ifdef MAX_NR_MESH_TRANSPORT_KEYS
    // single
    context = btstack_memory_mesh_transport_key_get();
    CHECK(context != NULL);
    btstack_memory_mesh_transport_key_free(context);
#else
    // none
    context = btstack_memory_mesh_transport_key_get();
    CHECK(context == NULL);
    btstack_memory_mesh_transport_key_free(context);
#endif
#endif
}

TEST(btstack_memory, mesh_transport_key_NotEnoughBuffers){
    mesh_transport_key_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_MESH_TRANSPORT_KEYS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_MESH_TRANSPORT_KEYS; i++){
        context = btstack_memory_mesh_transport_key_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_mesh_transport_key_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, mesh_virtual_address_GetAndFree){
    mesh_virtual_address_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_mesh_virtual_address_get();
    CHECK(context != NULL);
    btstack_memory_mesh_virtual_address_free(context);
#else
#ifdef MAX_NR_MESH_VIRTUAL_ADDRESSS
    // single
    context = btstack_memory_mesh_virtual_address_get();
    CHECK(context != NULL);
    btstack_memory_mesh_virtual_address_free(context);
#else
    // none
    context = btstack_memory_mesh_virtual_address_get();
    CHECK(context == NULL);
    btstack_memory_mesh_virtual_address_free(context);
#endif
#endif
}

TEST(btstack_memory, mesh_virtual_address_NotEnoughBuffers){
    mesh_virtual_address_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_MESH_VIRTUAL_ADDRESSS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_MESH_VIRTUAL_ADDRESSS; i++){
        context = btstack_memory_mesh_virtual_address_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_mesh_virtual_address_get();
    CHECK(context == NULL);
}



TEST(btstack_memory, mesh_subnet_GetAndFree){
    mesh_subnet_t * context;
#ifdef HAVE_MALLOC
    context = btstack_memory_mesh_subnet_get();
    CHECK(context != NULL);
    btstack_memory_mesh_subnet_free(context);
#else
#ifdef MAX_NR_MESH_SUBNETS
    // single
    context = btstack_memory_mesh_subnet_get();
    CHECK(context != NULL);
    btstack_memory_mesh_subnet_free(context);
#else
    // none
    context = btstack_memory_mesh_subnet_get();
    CHECK(context == NULL);
    btstack_memory_mesh_subnet_free(context);
#endif
#endif
}

TEST(btstack_memory, mesh_subnet_NotEnoughBuffers){
    mesh_subnet_t * context;
#ifdef HAVE_MALLOC
    simulate_no_memory = 1;
#else
#ifdef MAX_NR_MESH_SUBNETS
    int i;
    // alloc all static buffers
    for (i = 0; i < MAX_NR_MESH_SUBNETS; i++){
        context = btstack_memory_mesh_subnet_get();
        CHECK(context != NULL);
    }
#endif
#endif
    // get one more
    context = btstack_memory_mesh_subnet_get();
    CHECK(context == NULL);
}

#endif

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

