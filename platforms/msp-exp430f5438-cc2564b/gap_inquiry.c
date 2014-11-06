// *****************************************************************************
//
// minimal setup for HCI code
//
// *****************************************************************************
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <msp430x54x.h>

#include "bt_control_cc256x.h"
#include "hal_board.h"
#include "hal_compat.h"
#include "hal_usb.h"

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "remote_device_db.h"
#include "btstack-config.h"


#define MAX_DEVICES 10
enum DEVICE_STATE { REMOTE_NAME_REQUEST, REMOTE_NAME_INQUIRED, REMOTE_NAME_FETCHED };
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

int has_more_remote_name_requests(void){
    int i;
    for (i=0;i<deviceCount;i++) {
        if (devices[i].state == REMOTE_NAME_REQUEST) return 1;
    }
    return 0;
}

void do_next_remote_name_request(void){
    int i;
    for (i=0;i<deviceCount;i++) {
        // remote name request
        if (devices[i].state == REMOTE_NAME_REQUEST){
            devices[i].state = REMOTE_NAME_INQUIRED;
            printf("Get remote name of %s...\n", bd_addr_to_str(devices[i].address));
            hci_send_cmd(&hci_remote_name_request, devices[i].address,
                        devices[i].pageScanRepetitionMode, 0, devices[i].clockOffset | 0x8000);
            return;
        }
    }
}

static void continue_remote_names(){
    if (has_more_remote_name_requests()){
        do_next_remote_name_request();
        return;
    } 
    start_scan();
}

static void packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
    bd_addr_t addr;
    int i;
    int index;
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
                        index = getDeviceIndexForAddress(addr);
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
                        devices[deviceCount].state = REMOTE_NAME_REQUEST;
                        printf("Device found: %s with COD: 0x%06x, pageScan %d, clock offset 0x%04x, rssi 0x%02x\n", bd_addr_to_str(addr),
                                devices[deviceCount].classOfDevice, devices[deviceCount].pageScanRepetitionMode,
                                devices[deviceCount].clockOffset, devices[deviceCount].rssi);
                        deviceCount++;
                    }

                    break;
                }
                case HCI_EVENT_INQUIRY_COMPLETE:
                    for (i=0;i<deviceCount;i++) {
                        // retry remote name request
                        if (devices[i].state == REMOTE_NAME_INQUIRED)
                            devices[i].state = REMOTE_NAME_REQUEST;
                    }
                    continue_remote_names();
                    break;

                case BTSTACK_EVENT_REMOTE_NAME_CACHED:
                    bt_flip_addr(addr, &packet[3]);
                    printf("Cached remote name for %s: '%s'\n", bd_addr_to_str(addr), &packet[9]);
                    break;

                case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
                    bt_flip_addr(addr, &packet[3]);
                    index = getDeviceIndexForAddress(addr);
                    if (index >= 0) {
                        if (packet[2] == 0) {
                            printf("Name: '%s'\n", &packet[9]);
                            devices[index].state = REMOTE_NAME_FETCHED;
                        } else {
                            printf("Failed to get name: page timeout\n");
                        }
                    }
                    continue_remote_names();
                    break;

                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}

static void hw_setup(void){
    // stop watchdog timer
    WDTCTL = WDTPW + WDTHOLD;

    //Initialize clock and peripherals 
    halBoardInit();  
    halBoardStartXT1(); 
    halBoardSetSystemClock(SYSCLK_16MHZ);
    
    // init debug UART
    halUsbInit();

    // init LEDs
    LED_PORT_OUT |= LED_1 | LED_2;
    LED_PORT_DIR |= LED_1 | LED_2;
    
    // ready - enable irq used in h4 task
    __enable_interrupt();   

}

static void btstack_setup(void){
    btstack_memory_init();
    run_loop_init(RUN_LOOP_EMBEDDED);
    
    // init HCI
    hci_transport_t    * transport = hci_transport_h4_dma_instance();
    bt_control_t       * control   = bt_control_cc256x_instance();
    hci_uart_config_t  * config    = hci_uart_config_cc256x_instance();
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, config, control, remote_db);
    hci_register_packet_handler(packet_handler);
}


// main == setup
int btstack_main(void)
{
    hw_setup();
    btstack_setup();
    
    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    // go!
    run_loop_execute(); 
    
    // happy compiler!
    return 0;
}

