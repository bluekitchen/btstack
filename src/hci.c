/*
 *  hci.c
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "hci.h"
#include "hci_dump.h"

// the stack is here
static hci_stack_t       hci_stack;

/**
 * get link for given address
 *
 * @return connection OR NULL, if not found
 */
static hci_connection_t *link_for_addr(bd_addr_t addr){
    return NULL;
}

/** 
 * Dummy handler called by HCI
 */
static void dummy_handler(uint8_t *packet, uint16_t size){
}

/**
 * Dummy control handler
 */
static int null_control_function(void *config){
    return 0;
}
static const char * null_control_name(void *config){
    return "Hardware unknown";
}
static bt_control_t null_control = {
    null_control_function,
    null_control_function,
    null_control_function,
    null_control_name
}; 

static void acl_handler(uint8_t *packet, int size){
    hci_stack.acl_packet_handler(packet, size);
    
    // execute main loop
    hci_run();
}

static void event_handler(uint8_t *packet, int size){
    bd_addr_t addr;
    
    // Get Num_HCI_Command_Packets
    if (packet[0] == HCI_EVENT_COMMAND_COMPLETE ||
        packet[0] == HCI_EVENT_COMMAND_STATUS){
        hci_stack.num_cmd_packets = packet[2];
    }

    // handle BT initialization
    if (hci_stack.state == HCI_STATE_INITIALIZING){
        // handle H4 synchronization loss on restart
        // if (hci_stack.substate == 1 && packet[0] == HCI_EVENT_HARDWARE_ERROR){
        //    hci_stack.substate = 0;
        // }
        // handle normal init sequence
        if (hci_stack.substate % 2){
            // odd: waiting for event
            if (packet[0] == HCI_EVENT_COMMAND_COMPLETE){
                hci_stack.substate++;
            }
        }
    }

    // link key request
    if (packet[0] == HCI_EVENT_LINK_KEY_REQUEST){
        bt_flip_addr(addr, &packet[2]); 
        hci_send_cmd(&hci_link_key_request_negative_reply, &addr);
        return;
    }
    
    // pin code request
    if (packet[0] == HCI_EVENT_PIN_CODE_REQUEST){
        bt_flip_addr(addr, &packet[2]); 
        hci_send_cmd(&hci_pin_code_request_reply, &addr, 4, "1234");
    }
    
    hci_stack.event_packet_handler(packet, size);
	
	// execute main loop
	hci_run();
}

/** Register L2CAP handlers */
void hci_register_event_packet_handler(void (*handler)(uint8_t *packet, uint16_t size)){
    hci_stack.event_packet_handler = handler;
}
void hci_register_acl_packet_handler  (void (*handler)(uint8_t *packet, uint16_t size)){
    hci_stack.acl_packet_handler = handler;
}

void hci_init(hci_transport_t *transport, void *config, bt_control_t *control){
    
    // reference to use transport layer implementation
    hci_stack.hci_transport = transport;
    
    // references to used control implementation
    if (control) {
        hci_stack.control = control;
    } else {
        hci_stack.control = &null_control;
    }
    
    // reference to used config
    hci_stack.config = config;
    
    // empty cmd buffer
    hci_stack.hci_cmd_buffer = malloc(3+255);

    // higher level handler
    hci_stack.event_packet_handler = dummy_handler;
    hci_stack.acl_packet_handler = dummy_handler;

    // register packet handlers with transport
    transport->register_event_packet_handler( event_handler);
    transport->register_acl_packet_handler( acl_handler);
}

int hci_power_control(HCI_POWER_MODE power_mode){
    if (power_mode == HCI_POWER_ON) {
        
        // set up state machine
        hci_stack.num_cmd_packets = 1; // assume that one cmd can be sent
        hci_stack.state = HCI_STATE_INITIALIZING;
        hci_stack.substate = 0;
        
        // power on
        hci_stack.control->on(hci_stack.config);
        
        // open low-level device
        hci_stack.hci_transport->open(hci_stack.config);
        
    } else if (power_mode == HCI_POWER_OFF){
        
        // close low-level device
        hci_stack.hci_transport->close(hci_stack.config);

        // power off
        hci_stack.control->off(hci_stack.config);
    }
	
	// trigger next/first action
	hci_run();
	
    return 0;
}

uint32_t hci_run(){
    uint8_t micro_packet;
    switch (hci_stack.state){
        case HCI_STATE_INITIALIZING:
            if (hci_stack.substate % 2) {
                // odd: waiting for command completion
                return 0;
            }
            if (hci_stack.num_cmd_packets == 0) {
                // cannot send command yet
                return 0;
            }
            switch (hci_stack.substate/2){
                case 0:
                    hci_send_cmd(&hci_reset);
                    break;
				case 1:
					hci_send_cmd(&hci_read_bd_addr);
					break;
                case 2:
                    // ca. 15 sec
                    hci_send_cmd(&hci_write_page_timeout, 0x6000);
                    break;
				case 3:
					hci_send_cmd(&hci_write_scan_enable, 3); // 3 inq scan + page scan
					break;
                case 4:
                    // done.
                    hci_stack.state = HCI_STATE_WORKING;
                    micro_packet = HCI_EVENT_BTSTACK_WORKING;
                    hci_stack.event_packet_handler(&micro_packet, 1);
                    break;
                default:
                    break;
            }
            hci_stack.substate++;
            break;
        default:
            break;
    }
    
    // don't check for timetous yet
    return 0;
}


int hci_send_acl_packet(uint8_t *packet, int size){
    return hci_stack.hci_transport->send_acl_packet(packet, size);
}

int hci_send_cmd_packet(uint8_t *packet, int size){
    if (READ_CMD_OGF(packet) != OGF_BTSTACK) { 
        hci_stack.num_cmd_packets--;
        return hci_stack.hci_transport->send_cmd_packet(packet, size);
    }
    
    hci_dump_packet( HCI_COMMAND_DATA_PACKET, 1, packet, size);
    
    // BTstack internal commands
    uint8_t event[3];
    switch (READ_CMD_OCF(packet)){
        case HCI_BTSTACK_GET_STATE:
            event[0] = HCI_EVENT_BTSTACK_STATE;
            event[1] = 1;
            event[2] = hci_stack.state;
            hci_dump_packet( HCI_EVENT_PACKET, 0, event, 3);
            hci_stack.event_packet_handler(event, 3);
            break;
        default:
            // TODO log into hci dump as vendor specific "event"
            printf("Error: command %u not implemented\n:", READ_CMD_OCF(packet));
            break;
    }
    return 0;
}

/**
 * pre: numcmds >= 0 - it's allowed to send a command to the controller
 */
int hci_send_cmd(hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint8_t * hci_cmd_buffer = hci_stack.hci_cmd_buffer;
    uint16_t size = hci_create_cmd_internal(hci_stack.hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    return hci_send_cmd_packet(hci_cmd_buffer, size);
}