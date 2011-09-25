//*****************************************************************************
//
// accel_demo
//
//*****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <msp430x54x.h>

#include "bt_control_cc256x.h"
#include "hal_adc.h"
#include "hal_board.h"
#include "hal_compat.h"
#include "hal_lcd.h"
#include "hal_usb.h"

#include "UserExperienceGraphics.h"

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "hci.h"
#include "l2cap.h"
#include "btstack_memory.h"
#include "remote_device_db.h"
#include "rfcomm.h"
#include "sdp.h"
#include "config.h"

#define HEARTBEAT_PERIOD_MS 1000

#define FONT_HEIGHT		12                    // Each character has 13 lines 
#define FONT_WIDTH       8
static int row = 0;
char lineBuffer[80];

static uint8_t   rfcomm_channel_nr = 1;
static uint16_t  rfcomm_channel_id;
static uint8_t   spp_service_buffer[100];

// LCD setup
void doLCD(void){
    //Initialize LCD
    // 138 x 110, 4-level grayscale pixels.
    halLcdInit();       
    halLcdSetContrast(100);
    halLcdClearScreen(); 
    halLcdImage(TI_TINY_BUG, 4, 32, 104, 12 );

    halLcdPrintLine("BTstack on ", 0, 0);
    halLcdPrintLine("TI MSP430", 1, 0);
    halLcdPrintLine("SPP ACCEL", 2, 0);
    halLcdPrintLine("Init...", 4, 0);
    row = 5;
}

void clearLine(int line){
    halLcdClearImage(130, FONT_HEIGHT, 0, line*FONT_HEIGHT);
}

void printLine(char *text){
    printf("LCD: %s\n\r", text);
    halLcdPrintLine(text, row++, 0);
}


// SPP description
static uint8_t accel_buffer[6];

static void  prepare_accel_packet(){
    int16_t accl_x;
    int16_t accl_y;
    int16_t accl_z;
    
    /* read the digital accelerometer x- direction and y - direction output */
    halAccelerometerRead((int *)&accl_x, (int *)&accl_y, (int *)&accl_z);

    accel_buffer[0] = 0x01; // Start of "header"
    accel_buffer[1] = accl_x;
    accel_buffer[2] = (accl_x >> 8);
    accel_buffer[3] = accl_y;
    accel_buffer[4] = (accl_y >> 8);
    int index;
    uint8_t checksum = 0;
    for (index = 0; index < 5; index++) {
        checksum += accel_buffer[index];
    }
    accel_buffer[5] = checksum;
    
    /* start the ADC to read the next accelerometer output */
    halAdcStartRead();
    
    printf("Accel: X: %04d, Y: %04d, Z: %04d\n\r", accl_x, accl_y, accl_z);
} 

// Bluetooth logic
static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    int err;
    
	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - set local name
					if (packet[2] == HCI_STATE_WORKING) {
                        hci_send_cmd(&hci_write_local_name, "BlueMSP-Demo");
					}
					break;
				
				case HCI_EVENT_COMMAND_COMPLETE:
					if (COMMAND_COMPLETE_EVENT(packet, hci_read_bd_addr)){
                        bt_flip_addr(event_addr, &packet[6]);
                        printf("BD-ADDR: %s\n\r", bd_addr_to_str(event_addr));
                        break;
                    }
					if (COMMAND_COMPLETE_EVENT(packet, hci_write_local_name)){
                        hci_discoverable_control(1);
                        break;
                    }
                    break;

				case HCI_EVENT_LINK_KEY_REQUEST:
					// deny link key request
                    printf("Link key request\n\r");
                    bt_flip_addr(event_addr, &packet[2]);
					hci_send_cmd(&hci_link_key_request_negative_reply, &event_addr);
					break;
					
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
					printLine( "PIN = 0000");
                    printf("Pin code request - using '0000'\n\r");
                    bt_flip_addr(event_addr, &packet[2]);
					hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
					break;
                
                case RFCOMM_EVENT_INCOMING_CONNECTION:
					// data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
					bt_flip_addr(event_addr, &packet[2]); 
					rfcomm_channel_nr = packet[8];
					rfcomm_channel_id = READ_BT_16(packet, 9);
					printf("RFCOMM channel %u requested for %s\n\r", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection_internal(rfcomm_channel_id);
					break;
					
				case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
					// data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
    				if (packet[2]) {
						printf("RFCOMM channel open failed, status %u\n\r", packet[2]);
                        printLine("Connection failed :(");
					} else {
						rfcomm_channel_id = READ_BT_16(packet, 12);
						mtu = READ_BT_16(packet, 14);
						printf("\n\rRFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n\r", rfcomm_channel_id, mtu);
					}
					break;
                    
                case DAEMON_EVENT_HCI_PACKET_SENT:
                case RFCOMM_EVENT_CREDITS:
                    if (!rfcomm_channel_id) break;
                    // try send
                    err = rfcomm_send_internal(rfcomm_channel_id, (uint8_t *)accel_buffer, sizeof(accel_buffer));
                    switch (err){
                        case 0:
                            prepare_accel_packet();
                            break;
                        case BTSTACK_ACL_BUFFERS_FULL:
                            break;
                        default:
                           printf("rfcomm_send_internal() -> err %d\n\r", err);
                        break;
                    }
                    break;
                    
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    rfcomm_channel_id = 0;
                    break;
                
                default:
                    break;
			}
            break;
                        
        default:
            break;
	}
}

// main
int main(void) {

    // stop watchdog timer
    WDTCTL = WDTPW + WDTHOLD;

    //Initialize clock and peripherals 
    halBoardInit();  
    halBoardStartXT1();	
    halBoardSetSystemClock(SYSCLK_16MHZ);
    
    // Debug UART
    halUsbInit();

    // Accel
    halAccelerometerInit(); 
    
    // MindTree demo doesn't calibrate
    // halAccelerometerCalibrate();
    
    // init LEDs
    LED_PORT_OUT |= LED_1 | LED_2;
    LED_PORT_DIR |= LED_1 | LED_2;
    
    // show off
    doLCD();
    
    prepare_accel_packet();

	printf("Init BTstack...\n\r");
	
	/// GET STARTED ///
	btstack_memory_init();
    run_loop_init(RUN_LOOP_EMBEDDED);
	
    // init HCI
	hci_transport_t    * transport = hci_transport_h4_dma_instance();
	bt_control_t       * control   = bt_control_cc256x_instance();
    hci_uart_config_t  * config    = hci_uart_config_cc256x_instance();
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
	hci_init(transport, config, control, remote_db);
	
    // use eHCILL
    bt_control_cc256x_enable_ehcill(1);

    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);
    
    // init RFCOMM
    rfcomm_init();
    rfcomm_register_packet_handler(packet_handler);
    rfcomm_register_service_internal(NULL, rfcomm_channel_nr, 100);  // reserved channel, mtu=100

    // init SDP, create record for SPP and register with SDP
    sdp_init();
	memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    service_record_item_t * service_record_item = (service_record_item_t *) spp_service_buffer;
    sdp_create_spp_service( (uint8_t*) &service_record_item->service_record, 1, "SPP Accel");
    printf("SDP service buffer size: %u\n\r", (uint16_t) (sizeof(service_record_item_t) + de_get_len((uint8_t*) &service_record_item->service_record)));
    sdp_register_service_internal(NULL, service_record_item);
    
    // ready - enable irq used in h4 task
    __enable_interrupt();   
    
 	// turn on!
	hci_power_control(HCI_POWER_ON);
		
    // go!
    run_loop_execute();	
    
    // happy compiler!
    return 0;
}

/*

rfcomm_send_internal gets called before we have credits
rfcomm_send_internal returns undefined error codes???

*/

