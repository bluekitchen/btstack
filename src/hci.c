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

// calculate combined ogf/ocf value
#define OPCODE(ogf, ocf) (ocf | ogf << 10)
#define OGF_LINK_CONTROL 0x01
#define OGF_CONTROLLER_BASEBAND 0x03
#define OGF_INFORMATIONAL_PARAMETERS 0x04

hci_cmd_t hci_inquiry = {
    OPCODE(OGF_LINK_CONTROL, 0x01), "311"
    // LAP, Inquiry length, Num_responses
};

hci_cmd_t hci_inquiry_cancel = {
OPCODE(OGF_LINK_CONTROL, 0x02), ""
// no params
};

hci_cmd_t hci_link_key_request_negative_reply = {
    OPCODE(OGF_LINK_CONTROL, 0x0c), "B"
};

hci_cmd_t hci_pin_code_request_reply = {
    OPCODE(OGF_LINK_CONTROL, 0x0d), "B1P"
    // BD_ADDR, pin length, PIN: c-string
};

hci_cmd_t hci_reset = {
    OPCODE(OGF_CONTROLLER_BASEBAND, 0x03), ""
};

hci_cmd_t hci_create_connection = {
    OPCODE(OGF_LINK_CONTROL, 0x05), "B21121"
    // BD_ADDR, Packet_Type, Page_Scan_Repetition_Mode, Reserved, Clock_Offset, Allow_Role_Switch
};

hci_cmd_t hci_write_page_timeout = {
    OPCODE(OGF_CONTROLLER_BASEBAND, 0x18), "2"
    // Page_Timeout * 0.625 ms
};

hci_cmd_t hci_write_authentication_enable = {
    OPCODE(OGF_CONTROLLER_BASEBAND, 0x20), "1"
    // Authentication_Enable
};

hci_cmd_t hci_host_buffer_size = {
    OPCODE(OGF_CONTROLLER_BASEBAND, 0x33), "2122"
    // Host_ACL_Data_Packet_Length:, Host_Synchronous_Data_Packet_Length:, Host_Total_Num_ACL_Data_Packets:, Host_Total_Num_Synchronous_Data_Packets:
};

hci_cmd_t hci_read_bd_addr = {
OPCODE(OGF_INFORMATIONAL_PARAMETERS, 0x09), ""
// no params
};


// the stack is here
static hci_stack_t       hci_stack;


void bt_store_16(uint8_t *buffer, uint16_t pos, uint16_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
}

void bt_store_32(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
    buffer[pos++] = value >> 16;
    buffer[pos++] = value >> 24;
}

void bt_flip_addr(bd_addr_t dest, bd_addr_t src){
    dest[0] = src[5];
    dest[1] = src[4];
    dest[2] = src[3];
    dest[3] = src[2];
    dest[4] = src[1];
    dest[5] = src[0];
}

void hexdump(void *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

#if 0
static void *hci_daemon_thread(void *arg){
    printf("HCI Daemon started\n");
    hci_run(transport, &config);
    return NULL;
}
#endif

/**
 * Linked link list 
 */

/**
 * get link for given address
 *
 * @return connection OR NULL, if not found
 */
#if 0
static hci_connection_t *link_for_addr(bd_addr_t addr){
    return NULL;
}
#endif

/** 
 * Handler called by HCI transport
 */
static void dummy_handler(uint8_t *packet, int size){
}

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
void hci_register_event_packet_handler(void (*handler)(uint8_t *packet, int size)){
    hci_stack.event_packet_handler = handler;
}
void hci_register_acl_packet_handler  (void (*handler)(uint8_t *packet, int size)){
    hci_stack.acl_packet_handler = handler;
}

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
                    // ca. 15 sec
                    hci_send_cmd(&hci_write_page_timeout, 0x6000);
                    break;
                case 2:
                    // done.
                    hci_stack.state = HCI_STATE_WORKING;
                    micro_packet = BTSTACK_EVENT_HCI_WORKING;
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


/**
 * pre: numcmds >= 0 - it's allowed to send a command to the controller
 */
int hci_send_cmd(hci_cmd_t *cmd, ...){
    uint8_t * hci_cmd_buffer = hci_stack.hci_cmd_buffer;
    hci_cmd_buffer[0] = cmd->opcode & 0xff;
    hci_cmd_buffer[1] = cmd->opcode >> 8;
    int pos = 3;

    va_list argptr;
    va_start(argptr, cmd);
    const char *format = cmd->format;
    uint16_t word;
    uint32_t longword;
    uint8_t * ptr;
    while (*format) {
        switch(*format) {
            case '1': //  8 bit value
            case '2': // 16 bit value
            case 'H': // hci_handle
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                hci_cmd_buffer[pos++] = word & 0xff;
                if (*format == '2') {
                    hci_cmd_buffer[pos++] = word >> 8;
                } else if (*format == 'H') {
                    // TODO
                } 
                break;
            case '3':
            case '4':
                longword = va_arg(argptr, uint32_t);
                // longword = va_arg(argptr, int);
                hci_cmd_buffer[pos++] = longword;
                hci_cmd_buffer[pos++] = longword >> 8;
                hci_cmd_buffer[pos++] = longword >> 16;
                if (*format == '4'){
                    hci_cmd_buffer[pos++] = longword >> 24;
                }
                break;
            case 'B': // bt-addr
                ptr = va_arg(argptr, uint8_t *);
                hci_cmd_buffer[pos++] = ptr[5];
                hci_cmd_buffer[pos++] = ptr[4];
                hci_cmd_buffer[pos++] = ptr[3];
                hci_cmd_buffer[pos++] = ptr[2];
                hci_cmd_buffer[pos++] = ptr[1];
                hci_cmd_buffer[pos++] = ptr[0];
                break;
            case 'P': // c string passed as pascal string with leading 1-byte len
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 16);
                pos += 16;
                break;
            default:
                break;
        }
        format++;
    };
    va_end(argptr);
    hci_cmd_buffer[2] = pos - 3;
    // send packet
    hci_stack.num_cmd_packets--;
    return hci_stack.hci_transport->send_cmd_packet(hci_cmd_buffer, pos);
}