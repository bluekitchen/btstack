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

#define BTSTACK_FILE__ "le_data_channel_client.c"

/*
 * le_data_channel_client.c
 */

// *****************************************************************************
/* EXAMPLE_START(le_data_channel_client): Connects to 'LE Data Channel' and streams data 
 * via LE Data Channel == LE Connection-Oriented Channel == LE Credit-based Connection
 */
// *****************************************************************************

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

#define TEST_STREAM_DATA
#define TEST_PACKET_SIZE 1000

static enum {
    TC_OFF,
    TC_IDLE,
    TC_W4_SCAN_RESULT,
    TC_W4_CONNECT,
    TC_W4_CHANNEL,
    TC_TEST_DATA
} state = TC_OFF;

const uint16_t TSPX_le_psm = 0x25;

static bd_addr_t cmdline_addr;
static int cmdline_addr_found = 0;

// addr and type of device with correct name
static bd_addr_t      le_data_channel_addr;
static bd_addr_type_t le_data_channel_addr_type;

static hci_con_handle_t connection_handle;
static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint8_t data_channel_buffer[TEST_PACKET_SIZE];

/*
 * @section Track throughput
 * @text We calculate the throughput by setting a start time and measuring the amount of 
 * data sent. After a configurable REPORT_INTERVAL_MS, we print the throughput in kB/s
 * and reset the counter and start time.
 */

/* LISTING_START(tracking): Tracking throughput */

#define REPORT_INTERVAL_MS 3000

// support for multiple clients
typedef struct {
    char name;
    hci_con_handle_t connection_handle;
    uint16_t cid;
    int  counter;
    char test_data[TEST_PACKET_SIZE];
    int  test_data_len;
    uint32_t test_data_sent;
    uint32_t test_data_start;
} le_data_channel_connection_t;

static le_data_channel_connection_t le_data_channel_connection;

static void test_reset(le_data_channel_connection_t * context){
    context->test_data_start = btstack_run_loop_get_time_ms();
    context->test_data_sent = 0;
}

static void test_track_data(le_data_channel_connection_t * context, int bytes_transferred){
    context->test_data_sent += bytes_transferred;
    // evaluate
    uint32_t now = btstack_run_loop_get_time_ms();
    uint32_t time_passed = now - context->test_data_start;
    if (time_passed < REPORT_INTERVAL_MS) return;
    // print speed
    int bytes_per_second = context->test_data_sent * 1000 / time_passed;
    printf("%c: %"PRIu32" bytes -> %u.%03u kB/s\n", context->name, context->test_data_sent, bytes_per_second / 1000, bytes_per_second % 1000);

    // restart
    context->test_data_start = now;
    context->test_data_sent  = 0;
}
/* LISTING_END(tracking): Tracking throughput */


// returns 1 if name is found in advertisement
static int advertisement_report_contains_name(const char * name, uint8_t * advertisement_report){
    // get advertisement from report event
    const uint8_t * adv_data = gap_event_advertising_report_get_data(advertisement_report);
    uint16_t        adv_len  = gap_event_advertising_report_get_data_length(advertisement_report);
    int             name_len = strlen(name);

    // iterate over advertisement data
    ad_context_t context;
    for (ad_iterator_init(&context, adv_len, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type    = ad_iterator_get_data_type(&context);
        uint8_t data_size    = ad_iterator_get_data_len(&context);
        const uint8_t * data = ad_iterator_get_data(&context);
        int i;
        switch (data_type){
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                // compare common prefix
                for (i=0; i<data_size && i<name_len;i++){
                    if (data[i] != name[i]) break;
                }
                // prefix match
                return 1;
            default:
                break;
        }
    }
    return 0;
}

#ifdef TEST_STREAM_DATA
/* LISTING_END */
/*
 * @section Streamer
 *
 * @text The streamer function checks if notifications are enabled and if a notification can be sent now.
 * It creates some test data - a single letter that gets increased every time - and tracks the data sent.
 */

 /* LISTING_START(streamer): Streaming code */
static void streamer(void){

    // create test data
    le_data_channel_connection.counter++;
    if (le_data_channel_connection.counter > 'Z') le_data_channel_connection.counter = 'A';
    memset(le_data_channel_connection.test_data, le_data_channel_connection.counter, le_data_channel_connection.test_data_len);

    // send
    l2cap_le_send_data(le_data_channel_connection.cid, (uint8_t *) le_data_channel_connection.test_data, le_data_channel_connection.test_data_len);

    // track
    test_track_data(&le_data_channel_connection, le_data_channel_connection.test_data_len);

    // request another packet
    l2cap_le_request_can_send_now_event(le_data_channel_connection.cid);
} 
/* LISTING_END */
#endif

// Either connect to remote specified on command line or start scan for device with "LE Data Channel" in advertisement
static void le_data_channel_client_start(void){
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

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    bd_addr_t event_address;
    uint16_t psm;
    uint16_t cid;
    uint16_t conn_interval;
    hci_con_handle_t handle;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    // BTstack activated, get started
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                        le_data_channel_client_start();
                    } else {
                        state = TC_OFF;
                    }
                    break;
                case GAP_EVENT_ADVERTISING_REPORT:
                    if (state != TC_W4_SCAN_RESULT) return;
                    // check name in advertisement
                    if (!advertisement_report_contains_name("LE Data Channel", packet)) return;
                    // store address and type
                    gap_event_advertising_report_get_address(packet, le_data_channel_addr);
                    le_data_channel_addr_type = gap_event_advertising_report_get_address_type(packet);
                    // stop scanning, and connect to the device
                    state = TC_W4_CONNECT;
                    gap_stop_scan();
                    printf("Stop scan. Connect to device with addr %s.\n", bd_addr_to_str(le_data_channel_addr));
                    gap_connect(le_data_channel_addr,le_data_channel_addr_type);
                    break;
                case HCI_EVENT_LE_META:
                    // wait for connection complete
                    if (hci_event_le_meta_get_subevent_code(packet) !=  HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
                    if (state != TC_W4_CONNECT) return;
                    connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    // print connection parameters (without using float operations)
                    conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
                    printf("Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                    printf("Connection Latency: %u\n", hci_subevent_le_connection_complete_get_conn_latency(packet));  
                    // initialize gatt client context with handle, and add it to the list of active clients
                    // query primary services
                    printf("Connect to performance test channel.\n");
                    state = TC_W4_CHANNEL;
                    l2cap_le_create_channel(&packet_handler, connection_handle, TSPX_le_psm, data_channel_buffer, 
                                            sizeof(data_channel_buffer), L2CAP_LE_AUTOMATIC_CREDITS, LEVEL_0, &le_data_channel_connection.cid);
                    break;
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    if (cmdline_addr_found){
                        printf("Disconnected %s\n", bd_addr_to_str(cmdline_addr));
                        return;
                    }
                    printf("Disconnected %s\n", bd_addr_to_str(le_data_channel_addr));
                    if (state == TC_OFF) break;
                    le_data_channel_client_start();
                    break;
                case L2CAP_EVENT_LE_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    l2cap_event_le_channel_opened_get_address(packet, event_address);
                    psm = l2cap_event_le_channel_opened_get_psm(packet); 
                    cid = l2cap_event_le_channel_opened_get_local_cid(packet); 
                    handle = l2cap_event_le_channel_opened_get_handle(packet);
                    if (packet[2] == 0) {
                        printf("L2CAP: LE Data Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                               bd_addr_to_str(event_address), handle, psm, cid,  little_endian_read_16(packet, 15));
                        le_data_channel_connection.cid = cid;
                        le_data_channel_connection.connection_handle = handle;
                        le_data_channel_connection.test_data_len = btstack_min(l2cap_event_le_channel_opened_get_remote_mtu(packet), sizeof(le_data_channel_connection.test_data));
                        state = TC_TEST_DATA;
                        printf("Test packet size: %u\n", le_data_channel_connection.test_data_len);
                        test_reset(&le_data_channel_connection);
#ifdef TEST_STREAM_DATA
                        l2cap_le_request_can_send_now_event(le_data_channel_connection.cid);
#endif
                    } else {
                        printf("L2CAP: LE Data Channel connection to device %s failed. status code %u\n", bd_addr_to_str(event_address), packet[2]);
                    }
                    break;

#ifdef TEST_STREAM_DATA
                case L2CAP_EVENT_LE_CAN_SEND_NOW:
                    streamer();
                    break;
#endif

                case L2CAP_EVENT_LE_CHANNEL_CLOSED:
                    cid = l2cap_event_le_channel_closed_get_local_cid(packet);
                    printf("L2CAP: LE Data Channel closed 0x%02x\n", cid); 
                    break;

                default:
                    break;
            }
            break;

        case L2CAP_DATA_PACKET:
            test_track_data(&le_data_channel_connection, size);
            break;

        default:
            break;
    }
}

#ifdef HAVE_BTSTACK_STDIN
static void usage(const char *name){
    fprintf(stderr, "Usage: %s [-a|--address aa:bb:cc:dd:ee:ff]\n", name);
    fprintf(stderr, "If no argument is provided, LE Data Channel Client will start scanning and connect to the first device named 'LE Data Channel'.\n");
    fprintf(stderr, "To connect to a specific device use argument [-a].\n\n");
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

#ifdef HAVE_BTSTACK_STDIN
    int arg = 1;
    cmdline_addr_found = 0;
    
    while (arg < argc) {
        if(!strcmp(argv[arg], "-a") || !strcmp(argv[arg], "--address")){
            arg++;
            cmdline_addr_found = sscanf_bd_addr(argv[arg], cmdline_addr);
            arg++;
            if (!cmdline_addr_found) exit(1);
            continue;
        }
        usage(argv[0]);
        return 0;
    }
#else
    (void)argc;
    (void)argv;
#endif

    l2cap_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
