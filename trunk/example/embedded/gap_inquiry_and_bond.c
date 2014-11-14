// *****************************************************************************
//
// minimal setup for HCI code
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack-config.h"

#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "gap.h"

#define MAX_DEVICES 10
enum DEVICE_STATE { BONDING_REQUEST, BONDING_REQUESTED, BONDING_COMPLETED };

struct device {
    bd_addr_t  address;
    uint16_t   clockOffset;
    uint32_t   classOfDevice;
    uint8_t    pageScanRepetitionMode;
    uint8_t    rssi;
    enum DEVICE_STATE  state; 
};

#define INQUIRY_INTERVAL 5
struct device devices[MAX_DEVICES];
int deviceCount = 0;


enum STATE {INIT, W4_INQUIRY_MODE_COMPLETE, ACTIVE} ;
enum STATE state = INIT;


int getDeviceIndexForAddress( bd_addr_t addr){
    int j;
    for (j=0; j< deviceCount; j++){
        if (BD_ADDR_CMP(addr, devices[j].address) == 0){
            return j;
        }
    }
    return -1;
}

void start_scan(void){
    printf("Starting inquiry scan..\n");
    hci_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
}

int has_more_bonding_requests(void){
    int i;
    for (i=0;i<deviceCount;i++) {
        if (devices[i].state == BONDING_REQUEST) return 1;
    }
    return 0;
}

void do_next_bonding_request(void){
    int i;
    for (i=0;i<deviceCount;i++) {
        // remote name request
        if (devices[i].state == BONDING_REQUEST){
            devices[i].state = BONDING_REQUESTED;
            printf("Dedicated bonding with %s...\n", bd_addr_to_str(devices[i].address));

            //
            gap_dedicated_bonding(devices[i].address, 0);   // no MITM protection
            return;
        }
    }
}

static void continue_bonding(){
    if (has_more_bonding_requests()){
        do_next_bonding_request();
        return;
    } 
    start_scan();
}

static void packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
    bd_addr_t addr;
    int i;
    int numResponses;

    // printf("packet_handler: pt: 0x%02x, packet[0]: 0x%02x\n", packet_type, packet[0]);
    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event = packet[0];

    switch(state){

        case INIT: 
            if (packet[2] == HCI_STATE_WORKING) {
                hci_send_cmd(&hci_write_inquiry_mode, 0x01); // with RSSI
                state = W4_INQUIRY_MODE_COMPLETE;
            }
            break;

        case W4_INQUIRY_MODE_COMPLETE:
            if ( COMMAND_COMPLETE_EVENT(packet, hci_write_inquiry_mode) ) {
                start_scan();
                state = ACTIVE;
            }
            break;
            
        case ACTIVE:
            switch(event){
                case HCI_EVENT_INQUIRY_RESULT:
                case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:{
                    numResponses = packet[2];
                    int offset = 3;
                    for (i=0; i<numResponses && deviceCount < MAX_DEVICES;i++){
                        bt_flip_addr(addr, &packet[offset]);
                        offset += 6;
                        int index = getDeviceIndexForAddress(addr);
                        if (index >= 0) continue;   // already in our list
                        memcpy(devices[deviceCount].address, addr, 6);

                        devices[deviceCount].pageScanRepetitionMode = packet[offset];
                        offset += 1;

                        if (event == HCI_EVENT_INQUIRY_RESULT){
                            offset += 2; // Reserved + Reserved
                            devices[deviceCount].classOfDevice = READ_BT_24(packet, offset);
                            offset += 3;
                            devices[deviceCount].clockOffset =   READ_BT_16(packet, offset) & 0x7fff;
                            offset += 2;
                            devices[deviceCount].rssi  = 0;
                        } else {
                            offset += 1; // Reserved
                            devices[deviceCount].classOfDevice = READ_BT_24(packet, offset);
                            offset += 3;
                            devices[deviceCount].clockOffset =   READ_BT_16(packet, offset) & 0x7fff;
                            offset += 2;
                            devices[deviceCount].rssi  = packet[offset];
                            offset += 1;
                        }
                        devices[deviceCount].state = BONDING_REQUEST;
                        printf("Device found: %s with COD: 0x%06x, pageScan %d, clock offset 0x%04x, rssi 0x%02x\n", bd_addr_to_str(addr),
                                devices[deviceCount].classOfDevice, devices[deviceCount].pageScanRepetitionMode,
                                devices[deviceCount].clockOffset, devices[deviceCount].rssi);
                        deviceCount++;
                    }

                    break;
                }   
                case HCI_EVENT_INQUIRY_COMPLETE:
                    continue_bonding();
                    break;

                case GAP_DEDICATED_BONDING_COMPLETED:
                    // data: event(8), len(8), status (8), bd_addr(48)
                    printf("GAP Dedicated Bonding Complete, status %u\n", packet[2]);
                    bt_flip_addr(addr, &packet[3]);
                    int index = getDeviceIndexForAddress(addr);
                    if (index >= 0) {
                        devices[index].state = BONDING_COMPLETED;
                    }
                    continue_bonding();
                    break;

                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}

// main == setup
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]) {
    
    hci_register_packet_handler(packet_handler);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	
    // go!
    run_loop_execute();	
    
    // happy compiler!
    return 0;
}

