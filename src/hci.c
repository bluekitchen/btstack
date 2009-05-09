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


hci_cmd_t hci_inquiry = {
    0x01, 0x01, "311" // LAP, Inquiry length, Num_responses
};

hci_cmd_t hci_reset = {
    0x03, 0x03, ""
};


static hci_transport_t *hci_transport;

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

void hci_init(hci_transport_t *transport, void *config){
    
    // reference to use transport layer implementation
    hci_transport = transport;
    
    // open unix socket
    
    // wait for connections
    
    // enter loop
    
    // handle events
}

int hci_power_control(HCI_POWER_MODE power_mode){
    return 0;
}

int hci_send_cmd_packet(uint8_t *buffer, int size){
    return hci_transport->send_cmd_packet(buffer, size);
}

void hci_run(){
    while (1) {
        //  construct file descriptor set to wait for
        //  select
        
        // for each ready file in FD - call handle_data
        sleep(1);
    }
}


void hci_create_cmd_packet(uint8_t *buffer, uint8_t *cmd_len, hci_cmd_t *cmd, ...){
    buffer[0] = cmd->ocf;
    buffer[1] = cmd->ocf >> 8 | cmd->ogf << 2;
    int pos = 3;

    va_list argptr;
    va_start(argptr, cmd);
    const char *format = cmd->format;
    uint16_t word;
    uint32_t longword;
    uint8_t * bt_addr;
    while (*format) {
        switch(*format) {
            case '1': //  8 bit value
            case '2': // 16 bit value
            case 'H': // hci_handle
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                buffer[pos++] = word & 0xff;
                if (*format == '2') {
                    buffer[pos++] = word >> 8;
                } else if (*format == 'H') {
                    // TODO
                } 
                break;
            case '3':
            case '4':
                longword = va_arg(argptr, uint32_t);
                // longword = va_arg(argptr, int);
                buffer[pos++] = longword;
                buffer[pos++] = longword >> 8;
                buffer[pos++] = longword >> 16;
                if (*format == '4'){
                    buffer[pos++] = longword >> 24;
                }
                break;
            case 'B': // bt-addr
                bt_addr = va_arg(argptr, uint8_t *);
                memcpy( &buffer[pos], bt_addr, 6);
                pos += 6;
                break;
            default:
                break;
        }
        format++;
    };
    va_end(argptr);
    buffer[2] = pos - 3;
    *cmd_len = pos;
}