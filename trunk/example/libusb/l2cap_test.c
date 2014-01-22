
//*****************************************************************************
//
// minimal setup for SDP client over USB or UART
//
//*****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "gap.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"

// static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};
static bd_addr_t remote = {0x84, 0x38, 0x35, 0x65, 0xD1, 0x15};

typedef enum {
    W4_OPEN, SEND_ECHO, SEND_DATA, CLOSE_CHANNEL
} state_t;

static state_t state = W4_OPEN;
static uint16_t handle;
static uint16_t local_cid;

static void test_run(){
    int result;
    switch (state){
        case SEND_ECHO:
            // send echo request
            result = l2cap_send_echo_request(handle, (uint8_t *)  "Hello World!", 13);
            printf("L2CAP ECHO Request Sent\n");
            if (result) break;
            state++;
            break;
        case SEND_DATA:
            result = l2cap_send_internal(local_cid, (uint8_t *) "0123456789", 10);
            if (result) break;
            printf("L2CAP Data Sent\n");
            state++;
            break;
        case CLOSE_CHANNEL:
            l2cap_disconnect_internal(local_cid, 0);
            printf("L2CAP Channel Closed\n");
            state++;
            break;
        default:
            break;
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    bd_addr_t event_addr;
    uint16_t psm;

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                l2cap_create_channel_internal(NULL, packet_handler, remote, PSM_SDP, 100);
            }
            break;
        case L2CAP_EVENT_CHANNEL_OPENED:
            // inform about new l2cap connection
            bt_flip_addr(event_addr, &packet[3]);
            psm = READ_BT_16(packet, 11); 
            local_cid = READ_BT_16(packet, 13); 
            handle = READ_BT_16(packet, 9);
            if (packet[2] == 0) {
                printf("Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                       bd_addr_to_str(event_addr), handle, psm, local_cid,  READ_BT_16(packet, 15));
            } else {
                printf("L2CAP connection to device %s failed. status code %u\n", bd_addr_to_str(event_addr), packet[2]);
            }

            // let's do our thing
            state = SEND_ECHO;

            break;

        case DAEMON_EVENT_HCI_PACKET_SENT:
            test_run();
            break;

        default:
            break;
    }
}

static void packet_handler2 (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    packet_handler(packet_type, 0, packet, size);
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

    l2cap_init();
    l2cap_register_packet_handler(&packet_handler2);

    // turn on!
    hci_power_control(HCI_POWER_ON);
}

int main(void){
    btstack_setup();
    run_loop_execute(); 
    return 0;
}
