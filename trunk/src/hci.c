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

hci_cmd_t hci_inquiry = {
    OPCODE(OGF_LINK_CONTROL, 0x01), "311"
    // LAP, Inquiry length, Num_responses
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


// the stack is here
static hci_stack_t       hci_stack;


void bt_store_16(uint8_t *buffer, uint16_t pos, uint16_t value){
    buffer[pos] = value & 0xff;
    buffer[pos+1] = value >> 8;
}

void hexdump(uint8_t *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", data[i]);
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
 * Handler called by HCI transport
 */
static void dummy_handler(uint8_t *packet, int size){
}

static void acl_handler(uint8_t *packet, int size){
    hci_stack.acl_packet_handler(packet, size);
}

static void event_handler(uint8_t *packet, int size){

    if ( COMMAND_COMPLETE_EVENT(packet, hci_reset) ) {
        // reset done, write page timeout
        hci_send_cmd(&hci_write_page_timeout, 0x6000); // ca. 15 sec
        return;
    }
    
    if ( COMMAND_COMPLETE_EVENT(packet, hci_write_page_timeout) ) {
        uint8_t micro_packet = 100;
        hci_stack.event_packet_handler(&micro_packet, 1);
        return;
    }

    hci_stack.event_packet_handler(packet, size);
}

/** Register L2CAP handlers */
void hci_register_event_packet_handler(void (*handler)(uint8_t *packet, int size)){
    hci_stack.event_packet_handler = handler;
}
void hci_register_acl_packet_handler  (void (*handler)(uint8_t *packet, int size)){
    hci_stack.acl_packet_handler = handler;
}

void hci_init(hci_transport_t *transport, void *config){
    
    // reference to use transport layer implementation
    hci_stack.hci_transport = transport;
    
    // empty cmd buffer
    hci_stack.hci_cmd_buffer = malloc(3+255);
    
    // higher level handler
    hci_stack.event_packet_handler = dummy_handler;
    hci_stack.acl_packet_handler = dummy_handler;
    
    // register packet handlers with transport
    transport->register_event_packet_handler( event_handler);
    transport->register_acl_packet_handler( acl_handler);
    
    // open low-level device
    transport->open(config);
    
    // open unix socket
    
    // wait for connections
    
    // enter loop
    
    // handle events
}

int hci_power_control(HCI_POWER_MODE power_mode){
    return 0;
}

void hci_run(){

    // send hci reset
    hci_send_cmd(&hci_reset);

#if 0    
    while (1) {
        //  construct file descriptor set to wait for
        //  select
        
        // for each ready file in FD - call handle_data
        sleep(1);
    }
#endif
}





int hci_send_acl_packet(uint8_t *packet, int size){
    return hci_stack.hci_transport->send_acl_packet(packet, size);
}

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
    return hci_stack.hci_transport->send_cmd_packet(hci_cmd_buffer, pos);
}