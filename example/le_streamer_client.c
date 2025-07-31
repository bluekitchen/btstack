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

#define BTSTACK_FILE__ "le_streamer_client.c"

/*
 * le_streamer_client.c
 */

// *****************************************************************************
/* EXAMPLE_START(le_streamer_client): Performance - Stream Data over GATT (Client)
 * 
 * @text Connects to 'LE Streamer' and subscribes to test characteristic
 *
 */
// *****************************************************************************

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack.h"

#define LE_STREAMER_SERVICE_CLIENT_NUM_CHARACTERISTICS 2

typedef struct {
    btstack_linked_item_t item;
    gatt_service_client_connection_t basic_connection;
    gatt_service_client_characteristic_t characteristics_storage[LE_STREAMER_SERVICE_CLIENT_NUM_CHARACTERISTICS];
    btstack_context_callback_registration_t write_without_response_request;

    char name;
    int le_notification_enabled;
    int  counter;
    char test_data[200];
    uint16_t test_data_len;
    uint32_t test_data_sent;
    uint32_t test_data_start;
} le_streamer_client_connection_t;

typedef enum {
    TC_OFF,
    TC_IDLE,
    TC_W4_SCAN_RESULT,
    TC_W4_CONNECT,
    TC_W4_SERVICE_CONNECTED,
    TC_W4_TEST_DATA
} gc_state_t;

// On the GATT Server, RX Characteristic is used for receive data via Write, and TX Characteristic is used to send data via Notifications
static uuid128_t LE_STREAMER_SERVICE_UUID           = { 0x00, 0x00, 0xFF, 0x10, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
static const uuid128_t le_streamer_uuid128s[LE_STREAMER_SERVICE_CLIENT_NUM_CHARACTERISTICS] = {
        { 0x00, 0x00, 0xFF, 0x11, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB}, // rx
        { 0x00, 0x00, 0xFF, 0x12, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB}  // tx
};

static char *const le_streamer_server_name = "LE Streamer";
static gatt_service_client_t le_streamer_client;
static le_streamer_client_connection_t le_streamer_connection_storage;

static bd_addr_t cmdline_addr;
static int cmdline_addr_found = 0;

// addr and type of device with correct name
static bd_addr_t      le_streamer_addr;
static bd_addr_type_t le_streamer_addr_type;

static hci_con_handle_t connection_handle;
static gc_state_t state = TC_OFF;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static void le_streamer_handle_can_write_without_response(void * context);
static void le_streamer_client_request_to_send(le_streamer_client_connection_t * connection);
static void le_streamer_client_connection_and_notification_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

/*
 * @section Track throughput
 * @text We calculate the throughput by setting a start time and measuring the amount of 
 * data sent. After a configurable REPORT_INTERVAL_MS, we print the throughput in kB/s
 * and reset the counter and start time.
 */

/* LISTING_START(tracking): Tracking throughput */

#define TEST_MODE_WRITE_WITHOUT_RESPONSE 1
#define TEST_MODE_DUPLEX                 3

// configure test mode: send only, receive only, full duplex
#define TEST_MODE TEST_MODE_DUPLEX

#define REPORT_INTERVAL_MS 3000

static void test_reset(le_streamer_client_connection_t * context){
    context->test_data_start = btstack_run_loop_get_time_ms();
    context->test_data_sent = 0;
}

static void test_track_data(le_streamer_client_connection_t * context, int bytes_sent){
    context->test_data_sent += bytes_sent;
    // evaluate
    uint32_t now = btstack_run_loop_get_time_ms();
    uint32_t time_passed = now - context->test_data_start;
    if (time_passed < REPORT_INTERVAL_MS) return;
    // print speed
    uint32_t bytes_per_second = context->test_data_sent * 1000 / time_passed;
    printf("%c: %"PRIu32" bytes -> %u.%03u kB/s\n", context->name, context->test_data_sent, bytes_per_second / 1000, bytes_per_second % 1000);

    // restart
    context->test_data_start = now;
    context->test_data_sent  = 0;
}
/* LISTING_END(tracking): Tracking throughput */

static void le_streamer_client_request_to_send(le_streamer_client_connection_t * connection){
    connection->write_without_response_request.callback = &le_streamer_handle_can_write_without_response;
    connection->write_without_response_request.context = connection;
    gatt_client_request_to_write_without_response(&connection->write_without_response_request, connection_handle);
}

// returns true if name is found in advertisement
static bool advertisement_contains_name(const char * name, uint8_t adv_len, const uint8_t * adv_data){
    // get advertisement from report event
    uint16_t        name_len = (uint8_t) strlen(name);

    // iterate over advertisement data
    ad_context_t context;
    for (ad_iterator_init(&context, adv_len, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type    = ad_iterator_get_data_type(&context);
        uint8_t data_size    = ad_iterator_get_data_len(&context);
        const uint8_t * data = ad_iterator_get_data(&context);
        switch (data_type){
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                // compare prefix
                if (data_size < name_len) break;
                if (memcmp(data, name, name_len) == 0) return true;
                break;
            default:
                break;
        }
    }
    return false;
}

// Either connect to remote specified on command line or start scan for device with "LE Streamer" in advertisement
static void le_streamer_client_start(void){
    if (cmdline_addr_found){
        printf("Connect to %s\n", bd_addr_to_str(cmdline_addr));
        state = TC_W4_CONNECT;
        gap_connect(cmdline_addr, 0);
    } else {
        printf("Start scanning!\n");
        state = TC_W4_SCAN_RESULT;
        gap_set_scan_parameters(0,0x0030, 0x0030);
        gap_start_scan();
    }
}

static void le_stream_server_found(void) {
    // stop scanning, and connect to the device
    state = TC_W4_CONNECT;
    gap_stop_scan();
    printf("Stop scan. Connect to device with addr %s.\n", bd_addr_to_str(le_streamer_addr));
    gap_connect(le_streamer_addr,le_streamer_addr_type);
}

// streamer
static void le_streamer_handle_can_write_without_response(void * context){
    le_streamer_client_connection_t * connection = (le_streamer_client_connection_t *) context;

    // create test data
    connection->counter++;
    if (connection->counter > 'Z') connection->counter = 'A';
    memset(connection->test_data, connection->counter, connection->test_data_len);

    uint8_t status = gatt_client_write_value_of_characteristic_without_response(connection_handle, connection->characteristics_storage[0].value_handle, connection->test_data_len, (uint8_t*) connection->test_data);
    if (status){
        printf("Write without response failed, status 0x%02x.\n", status);
        return;
    } else {
        test_track_data(connection, connection->test_data_len);
    }
    // request again
    le_streamer_client_request_to_send(connection);
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    static const char * const phy_names[] = {
            "1 M", "2 M", "Codec"
    };

    uint16_t conn_interval;
    hci_con_handle_t con_handle;
    const uint8_t * adv_data;
    uint8_t         adv_len;
    uint8_t status;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                le_streamer_client_start();
            } else {
                state = TC_OFF;
            }
            break;
        case GAP_EVENT_ADVERTISING_REPORT:
            if (state != TC_W4_SCAN_RESULT) return;
            // check name in advertisement
            adv_data = gap_event_advertising_report_get_data(packet);
            adv_len  = gap_event_advertising_report_get_data_length(packet);
            if (!advertisement_contains_name(le_streamer_server_name, adv_len, adv_data)) return;
            // match connecting phy
            gap_set_connection_phys(1);
            // store address and type
            gap_event_advertising_report_get_address(packet, le_streamer_addr);
            le_streamer_addr_type = gap_event_advertising_report_get_address_type(packet);
            le_stream_server_found();
            break;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
            if (state != TC_W4_SCAN_RESULT) return;
            // check name in advertisement
            adv_data = gap_event_extended_advertising_report_get_data(packet);
            adv_len  = gap_event_extended_advertising_report_get_data_length(packet);
            if (!advertisement_contains_name(le_streamer_server_name, adv_len, adv_data)) return;
            // match connecting phy
            if (gap_event_extended_advertising_report_get_primary_phy(packet) == 1){
                // LE 1M PHY => use 1M or 2M PHY
                gap_set_connection_phys(3);
            } else {
                // Coded PHY => use Coded PHY
                gap_set_connection_phys(4);
            }
            // store address and type
            gap_event_extended_advertising_report_get_address(packet, le_streamer_addr);
            le_streamer_addr_type = gap_event_extended_advertising_report_get_address_type(packet);
            le_stream_server_found();
            break;
#endif
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)) {
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    if (state != TC_W4_CONNECT) return;
                    connection_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    // print connection parameters (without using float operations)
                    conn_interval = gap_subevent_le_connection_complete_get_conn_interval(packet);
                    printf("Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                    printf("Connection Latency: %u\n", gap_subevent_le_connection_complete_get_conn_latency(packet));
                    // initialize gatt client context with handle, and add it to the list of active clients
                    // query primary services
                    printf("Search for LE Streamer service .\n");
                    state = TC_W4_SERVICE_CONNECTED;
                    status = gatt_service_client_connect_primary_service_with_uuid128(connection_handle, &le_streamer_client, &le_streamer_connection_storage.basic_connection,
                                                                                              &LE_STREAMER_SERVICE_UUID, &le_streamer_connection_storage.characteristics_storage[0],
                                                                                              LE_STREAMER_SERVICE_CLIENT_NUM_CHARACTERISTICS);
                    if (status != ERROR_CODE_SUCCESS){
                        state = TC_OFF;
                        gap_disconnect(connection_handle);
                        printf("GATT Service Client connection failed %02x\n", status);
                    } else {
                        printf("GATT Service Client discovery process started\n");
                    }
                    break;

                default:
                    break;
            }
            break;
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE:
                    con_handle = hci_subevent_le_data_length_change_get_connection_handle(packet);
                    printf("- LE Connection 0x%04x: data length change - max %u bytes per packet\n", con_handle,
                           hci_subevent_le_data_length_change_get_max_tx_octets(packet));
                    break;
                case HCI_SUBEVENT_LE_PHY_UPDATE_COMPLETE:
                    con_handle = hci_subevent_le_phy_update_complete_get_connection_handle(packet);
                    printf("- LE Connection 0x%04x: PHY update - using LE %s PHY now\n", con_handle,
                           phy_names[hci_subevent_le_phy_update_complete_get_tx_phy(packet) - 1]);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void le_streamer_client_connection_and_notification_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    if (hci_event_packet_get_type(packet) == HCI_EVENT_GATTSERVICE_META) {
        switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
            case GATTSERVICE_SUBEVENT_CLIENT_CONNECTED: {
                uint16_t cid = gattservice_subevent_client_connected_get_cid(packet);
                le_streamer_client_connection_t *connection = (le_streamer_client_connection_t *) gatt_service_client_get_connection_for_cid(&le_streamer_client, cid);
                btstack_assert(connection != NULL);

                uint8_t status = gattservice_subevent_client_connected_get_status(packet);
                if (status != ERROR_CODE_SUCCESS) {
                    printf("Finalize connection");
                    state = TC_OFF;
                    gap_disconnect(connection->basic_connection.con_handle);
                    return;
                }

                uint16_t mtu;
                connection->name = 'A';
                connection->test_data_len = ATT_DEFAULT_MTU - 3;
                test_reset(connection);
                gatt_client_get_mtu(connection_handle, &mtu);
                connection->test_data_len = btstack_min(mtu - 3, sizeof(connection->test_data));
                printf("%c: ATT MTU = %u => use test data of len %u\n", connection->name, mtu, connection->test_data_len);
                state = TC_W4_TEST_DATA;
#if (TEST_MODE & TEST_MODE_WRITE_WITHOUT_RESPONSE)
                printf("Start streaming - request can send now.\n");
                le_streamer_client_request_to_send(connection);
#endif
                break;
            }
            case GATTSERVICE_SUBEVENT_CLIENT_DISCONNECTED:
                connection_handle = HCI_CON_HANDLE_INVALID;
                printf("Disconnected %s\n", bd_addr_to_str(le_streamer_addr));
                if (state == TC_OFF) break;
                le_streamer_client_start();
                break;
            default:
                break;
        }
    }

    if (hci_event_packet_get_type(packet) == GATT_EVENT_NOTIFICATION) {
        uint16_t cid = gatt_event_notification_get_connection_id(packet);
        le_streamer_client_connection_t * connection = (le_streamer_client_connection_t *) gatt_service_client_get_connection_for_cid(&le_streamer_client, cid);
        btstack_assert(connection != NULL);
        test_track_data(connection, gatt_event_notification_get_value_length(packet));
    }
}

#ifdef HAVE_BTSTACK_STDIN
static void usage(const char *name){
    fprintf(stderr, "Usage: %s [-a|--address aa:bb:cc:dd:ee:ff]\n", name);
    fprintf(stderr, "If no argument is provided, LE Streamer Client will start scanning and connect to the first device named 'LE Streamer'.\n");
    fprintf(stderr, "To connect to a specific device use argument [-a].\n\n");
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

#ifdef HAVE_BTSTACK_STDIN
    int arg;
    cmdline_addr_found = 0;
    
    for (arg = 1; arg < argc; arg++) {
        if(!strcmp(argv[arg], "-a") || !strcmp(argv[arg], "--address")){
            if (arg + 1 < argc) {
                arg++;
                cmdline_addr_found = sscanf_bd_addr(argv[arg], cmdline_addr);
            }
            if (!cmdline_addr_found) {
                usage(argv[0]);
                return 1;
            }
        }
    }
    if (!cmdline_addr_found) {
        fprintf(stderr, "No specific address specified or found; start scanning for 'LE Streamer' advertisement.\n");
    }
#else
    (void)argc;
    (void)argv;
#endif
    l2cap_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    // sm_init needed before gatt_client_init
    gatt_client_init();
    gatt_service_client_init();
    gatt_service_client_register_client_with_uuid128s(&le_streamer_client, &le_streamer_client_connection_and_notification_handler, &le_streamer_uuid128s[0], LE_STREAMER_SERVICE_CLIENT_NUM_CHARACTERISTICS);

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // use different connection parameters: conn interval min/max (* 1.25 ms), slave latency, supervision timeout, CE len min/max (* 0.6125 ms) 
    // gap_set_connection_parameters(0x06, 0x06, 4, 1000, 0x01, 0x06 * 2);

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    // scan on Coded and 1M PHYs
    gap_set_scan_phys(5);
#endif

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* EXAMPLE_END */