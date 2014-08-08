//*****************************************************************************
//
// minimal setup for HCI code
//
//*****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack-config.h"
#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>
#include <btstack/utils.h>
#include <btstack/utils.h>

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#define OPCODE(ogf, ocf) (ocf | ogf << 10)

enum STATE {INIT, W4_1, W4_2, W4_3, DONE} ;
enum STATE state = INIT;

static const hci_cmd_t hci_write_linkkey = {
    OPCODE(OGF_CONTROLLER_BASEBAND, 0x11), "1BP"
};

static const hci_cmd_t hci_read_linkkeys = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x0D), "B1"
};

static void packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
    bd_addr_t a = {1,2,3,4,5,6};
    uint8_t linkkey[16];
                
    // printf("packet_handler: pt: 0x%02x, packet[0]: 0x%02x\n", packet_type, packet[0]);
    if (packet_type != HCI_EVENT_PACKET) return;

    switch(state){

        case INIT: 
            if (packet[2] == HCI_STATE_WORKING) {
                // hci_send_cmd(&hci_write_inquiry_mode, 0x01); // with RSSI
                hci_send_cmd(&hci_write_linkkey, 1, a, linkkey);
                state = W4_1;
            }
            break;

        case W4_1:
            if ( COMMAND_COMPLETE_EVENT(packet, hci_write_linkkey) ) {
                a[0] = 2;
                hci_send_cmd(&hci_write_linkkey, 1, a, linkkey);
                state = W4_2;
            }
            break;
        case W4_2:
            if ( COMMAND_COMPLETE_EVENT(packet, hci_write_linkkey) ) {
                a[0] = 4;
                hci_send_cmd(&hci_write_linkkey, 1, a, linkkey);
                state = W4_3;
            }
            break;
        case W4_3:
            if ( COMMAND_COMPLETE_EVENT(packet, hci_write_linkkey) ) {
                hci_send_cmd(&hci_read_linkkeys, a, 1);
                state = DONE;
            }
            break;
        case DONE:
            break;
        default:
            break;
    }
}

void setup(void){
	/// GET STARTED with BTstack ///
	btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
	    
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);

    // init HCI
	hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t * config = NULL;
	bt_control_t       * control   = NULL;
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
        
	hci_init(transport, config, control, remote_db);
    hci_register_packet_handler(packet_handler);
}

// main == setup
int main(void)
{
    setup();

    // turn on!
	hci_power_control(HCI_POWER_ON);
	
    // go!
    run_loop_execute();	
    
    // happy compiler!
    return 0;
}

