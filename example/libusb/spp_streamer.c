
// *****************************************************************************
//
// minimal setup for SDP client over USB or UART
//
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "sdp_query_rfcomm.h"
#include "rfcomm.h"

#define NUM_ROWS 25
#define NUM_COLS 40

typedef enum {
    W4_SDP_RESULT,
    W4_SDP_COMPLETE,
    W4_RFCOMM_CHANNEL,
    DONE
} state_t;

#define DATA_VOLUME (1000 * 1000)

// configuration area {
static bd_addr_t remote = {0x84, 0x38, 0x35, 0x65, 0xD1, 0x15};     // address of remote device
static const char * spp_service_name_prefix = "Bluetooth-Incoming"; // default on OS X
// configuration area }

static uint8_t  test_data[NUM_ROWS * NUM_COLS];
static uint16_t test_data_len = sizeof(test_data);
static uint8_t  channel_nr = 0;
static uint16_t mtu;
static uint16_t rfcomm_cid = 0;
static uint32_t data_to_send =  DATA_VOLUME;
static state_t state = W4_SDP_RESULT;

static void create_test_data(void){
    int x,y;
    for (y=0;y<NUM_ROWS;y++){
        for (x=0;x<NUM_COLS-2;x++){
            test_data[y*NUM_COLS+x] = '0' + (x % 10);
        }
        test_data[y*NUM_COLS+NUM_COLS-2] = '\n';
        test_data[y*NUM_COLS+NUM_COLS-1] = '\r';
    }
}

static void try_send(){
    if (!rfcomm_cid) return;
    int err = rfcomm_send_internal(rfcomm_cid, (uint8_t*) test_data, test_data_len);
    if (err) return;
    if (data_to_send < test_data_len){
        rfcomm_disconnect_internal(rfcomm_cid);
        rfcomm_cid = 0;
        state = DONE;
        printf("SPP Streamer: enough data send, closing DLC\n");
        return;
    }
    data_to_send -= test_data_len;
}

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    // printf("packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);

    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = packet[0];

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                sdp_query_rfcomm_channel_and_name_for_uuid(remote, 0x1002);
            }
            break;
        case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
            // data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)
            if (packet[2]) {
                printf("RFCOMM channel open failed, status %u\n", packet[2]);
            } else {
                // data: event(8), len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
                rfcomm_cid = READ_BT_16(packet, 12);
                mtu = READ_BT_16(packet, 14);
                printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_cid, mtu);
                if ((test_data_len > mtu)) {
                    test_data_len = mtu;
                }
                try_send();
                break;
            }
            break;
        case DAEMON_EVENT_HCI_PACKET_SENT:
        case RFCOMM_EVENT_CREDITS:
            try_send();
            break;
        default:
            break;
    }
}

void handle_found_service(char * name, uint8_t port){
    printf("APP: Service name: '%s', RFCOMM port %u\n", name, port);

    if (strncmp(name, spp_service_name_prefix, strlen(spp_service_name_prefix)) != 0) return;

    printf("APP: matches requested SPP Service Name\n");
    channel_nr = port;
    state = W4_SDP_COMPLETE;
}

void handle_query_rfcomm_event(sdp_query_event_t * event, void * context){
    sdp_query_rfcomm_service_event_t * ve;
            
    switch (event->type){
        case SDP_QUERY_RFCOMM_SERVICE:
            ve = (sdp_query_rfcomm_service_event_t*) event;
            handle_found_service((char*) ve->service_name, ve->channel_nr);
            break;
        case SDP_QUERY_COMPLETE:
            if (state != W4_SDP_COMPLETE){
                printf("Requested SPP Service %s not found \n", spp_service_name_prefix);
                break;
            }
            // connect
            printf("Requested SPP Service found, creating RFCOMM channel\n");
            state = W4_RFCOMM_CHANNEL;
            rfcomm_create_channel_internal(NULL, &remote, channel_nr); 
            break;
        default: 
            break;
    }
}

static void btstack_setup(){
    /// GET STARTED ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
    
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
   
    hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t  * config = NULL;
    bt_control_t       * control   = NULL;    

    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, config, control, remote_db);
    printf("Client HCI init done\r\n");
    
    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);

    rfcomm_register_packet_handler(packet_handler);

    sdp_query_rfcomm_register_callback(handle_query_rfcomm_event, NULL);

    // turn on!
    hci_power_control(HCI_POWER_ON);
}

int main(void){
    create_test_data();
    btstack_setup();
    // go!
    run_loop_execute(); 
    return 0;
}
