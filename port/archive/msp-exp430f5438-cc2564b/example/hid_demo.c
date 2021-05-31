/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

// *****************************************************************************
//
// keyboard_demo
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "btstack_chipset_cc256x.h"
#include "hal_adc.h"
#include "hal_board.h"
#include "hal_compat.h"
#include "hal_lcd.h"
#include "hal_usb.h"
#include "UserExperienceGraphics.h"
#include  <msp430x54x.h>

#include "btstack_run_loop.h"
#include "hci_cmd.h"
#include "btstack_memory.h"
#include "hci.h"
#include "l2cap.h"
#include "btstack_link_key_db_memory,h"
 
#define INQUIRY_INTERVAL 15

#define FONT_HEIGHT		12                    // Each character has 13 lines 
#define FONT_WIDTH       8

static int row = 0;

extern int dumpCmds;

const char *hexMap = "0123456789ABCDEF";

static char lineBuffer[20];
static uint8_t num_chars = 0;
static bd_addr_t keyboard;
static int haveKeyboard = 0;

typedef enum { 
	boot = 1,
	inquiry,
	w4_inquiry_cmd_complete,
	w4_l2cap_hii_connect,
	connected
} state_t;

static state_t state = 0;

static btstack_packet_callback_registration_t hci_event_callback_registration;

#define KEYCODE_RETURN     '\n'
#define KEYCODE_ESCAPE      27
#define KEYCODE_TAB			'\t'
#define KEYCODE_BACKSPACE   0x7f
#define KEYCODE_ILLEGAL     0xffff
#define KEYCODE_CAPSLOCK    KEYCODE_ILLEGAL

#define MOD_SHIFT 0x22

/**
 * English (US)
 */
static uint16_t keytable_us_none [] = {
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 0-3 */
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', /*  4 - 13 */
	'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', /* 14 - 23 */
	'u', 'v', 'w', 'x', 'y', 'z',                     /* 24 - 29 */
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', /* 30 - 39 */
	KEYCODE_RETURN, KEYCODE_ESCAPE, KEYCODE_BACKSPACE, KEYCODE_TAB, ' ', /* 40 - 44 */
	'-', '=', '[', ']', '\\', KEYCODE_ILLEGAL, ';', '\'', 0x60, ',',      /* 45 - 54 */
	'.', '/', KEYCODE_CAPSLOCK, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL,  /* 55 - 60 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 61-64 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 65-68 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 69-72 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 73-76 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 77-80 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 81-84 */
	'*', '-', '+', '\n', '1', '2', '3', '4', '5',   /* 85-97 */
	'6', '7', '8', '9', '0', '.', 0xa7,             /* 97-100 */
}; 

static uint16_t keytable_us_shift[] = {
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 0-3 */
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', /*  4 - 13 */
	'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', /* 14 - 23 */
	'U', 'V', 'W', 'X', 'Y', 'Z',                     /* 24 - 29 */
	'!', '@', '#', '$', '%', '^', '&', '*', '(', ')',  /* 30 - 39 */
    KEYCODE_RETURN, KEYCODE_ESCAPE, KEYCODE_BACKSPACE, KEYCODE_TAB, ' ',  /* 40 - 44 */
    '_', '+', '{', '}', '|', KEYCODE_ILLEGAL, ':', '"', 0x7E, '<',         /* 45 - 54 */
    '>', '?', KEYCODE_CAPSLOCK, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL,  /* 55 - 60 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 61-64 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 65-68 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 69-72 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 73-76 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 77-80 */
	KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, KEYCODE_ILLEGAL, /* 81-84 */
	'*', '-', '+', '\n', '1', '2', '3', '4', '5',   /* 85-97 */
	'6', '7', '8', '9', '0', '.', 0xb1,             /* 97-100 */
}; 

// decode hid packet into buffer - return number of valid keys events
#define NUM_KEYS 6
uint8_t last_keyboard_state[NUM_KEYS];
uint16_t input[NUM_KEYS];
static char last_state_init = 0;
uint16_t last_key;
uint16_t last_mod;
uint16_t last_char;
uint8_t  last_key_new;

void doLCD(void){
    //Initialize LCD and backlight    
    // 138 x 110, 4-level grayscale pixels.
    halLcdInit();       
    // halLcdBackLightInit();  
    // halLcdSetBackLight(0);  // 8 for normal
    halLcdSetContrast(100);
    halLcdClearScreen(); 
    halLcdImage(TI_TINY_BUG, 4, 32, 104, 12 );

    halLcdPrintLine("BTstack on ", 0, 0);
    halLcdPrintLine("TI MSP430", 1, 0);
    halLcdPrintLine("Keyboard", 2, 0);
    halLcdPrintLine("Init...", 3, 0);
    row = 4;
}

void clearLine(int line){
    halLcdClearImage(130, FONT_HEIGHT, 0, line*FONT_HEIGHT);
}

void printLine(char *text){
    printf("LCD: %s\n\r", text);
    halLcdPrintLine(text, row++, 0);
}

// put 'lineBuffer' on screen
void showLine(void){
    clearLine(row);
    halLcdPrintLine(lineBuffer, row, 0);
    printf("LCD: %s\n\r", lineBuffer);
}

unsigned char hid_process_packet(unsigned char *hid_report, uint16_t *buffer, uint8_t max_keys){
	
	// check for key report
	if (hid_report[0] != 0xa1 || hid_report[1] != 0x01) {
		return 0;
	}
	
	u_char modifier = hid_report[2];
	u_char result = 0;
	u_char i;
	u_char j;
	
	if (!last_state_init)
	{
		for (i=0;i<NUM_KEYS;i++){
			last_keyboard_state[i] = 0;
		}
		last_state_init = 1;
	}
	
	for (i=0; i< NUM_KEYS && result < max_keys; i++){
		// find key in last state
		uint8_t new_event = hid_report[4+i];
		if (new_event){
			for (j=0; j<NUM_KEYS; j++){
				if (new_event == last_keyboard_state[j]){
					new_event = 0;
					break;
				}
			}
			if (!new_event) continue;
			buffer[result++] = new_event;
			last_key  = new_event;
			last_mod  = modifier;
		}
	}
	
	// store keyboard state
	for (i=0;i<NUM_KEYS;i++){
		last_keyboard_state[i] = hid_report[4+i];
	}
	return result;
}



static void l2cap_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	
	if (packet_type == HCI_EVENT_PACKET && hci_event_packet_get_type(packet) == L2CAP_EVENT_CHANNEL_OPENED){
        if (packet[2]) {
            printf("Connection failed\n\r");
            return;
	    }
		printf("Connected\n\r");
		num_chars = 0;
		lineBuffer[num_chars++] = 'T';
		lineBuffer[num_chars++] = 'y';
		lineBuffer[num_chars++] = 'p';
		lineBuffer[num_chars++] = 'e';
		lineBuffer[num_chars++] = ' ';
		lineBuffer[num_chars] = 0;
		showLine();
	}
	if (packet_type == L2CAP_DATA_PACKET){
		// handle input
		// printf("HID report, size %u\n\r", size);
		uint8_t count = hid_process_packet(packet, (uint16_t *) &input[0], NUM_KEYS);
		if (!count) return;
		
        uint8_t new_char;
        // handle shift
        if (last_mod & MOD_SHIFT) {
            new_char = keytable_us_shift[input[0]];
        } else {
            new_char = keytable_us_none[input[0]];
        }
        // add to buffer
        if (new_char == KEYCODE_BACKSPACE){
            if (num_chars <= 5) return;
            --num_chars;
            lineBuffer[num_chars] = 0;
            showLine();
            return;
        }
        // 17 chars fit into one line
        lineBuffer[num_chars] = new_char;
        lineBuffer[num_chars+1] = 0;
        if(num_chars <  16) num_chars++;
        showLine();
	}
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	int i,j;
	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {
					
				case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - set local name
					if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
						printLine("Inquiry");
						state = inquiry;
						gap_inquiry_start(INQUIRY_INTERVAL);
						break;
					}
					break;
					
				case HCI_EVENT_INQUIRY_RESULT:
					
					// ignore further results
					if (haveKeyboard) break;
					
					// ignore none keyboards
					if ((packet[12] & 0x40) != 0x40 || packet[13] != 0x25) break;

					// flip addr
					reverse_bd_addr(&packet[3], keyboard);
					
					// show
					printf("Keyboard:\n\r");

					// addr
					j=0;
					for (i=0;i<6;i++){
						lineBuffer[j++] = hexMap[ keyboard[i] >>    4 ];
						lineBuffer[j++] = hexMap[ keyboard[i] &  0x0f ];
						if (i<5) lineBuffer[j++] = ':';
					}
					lineBuffer[j++] = 0;
					printLine(lineBuffer);
					
					haveKeyboard = 1;
					hci_send_cmd(&hci_inquiry_cancel);
					state = w4_inquiry_cmd_complete;
					break;
					
				case HCI_EVENT_INQUIRY_COMPLETE:
					printLine("No keyboard found :(");
					break;
				
				case HCI_EVENT_LINK_KEY_REQUEST:
					// deny link key request
					hci_send_cmd(&hci_link_key_request_negative_reply, &keyboard);
					break;
					
				case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
					printLine( "Enter 0000");
					hci_send_cmd(&hci_pin_code_request_reply, &keyboard, 4, "0000");
					break;
					
				case HCI_EVENT_COMMAND_COMPLETE:
					if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_inquiry_cancel)) {
						// inq successfully cancelled
						// printLine("Connecting");
						l2cap_create_channel(l2cap_packet_handler, keyboard, PSM_HID_INTERRUPT, 150);
						break;
					}
			}
	}
}

static hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    1000000,  // main baudrate
    1,		  // flow control
    NULL,
};

int main(void){

    // stop watchdog timer
    WDTCTL = WDTPW + WDTHOLD;

    //Initialize clock and peripherals 
    halBoardInit();  
    halBoardStartXT1();	
    halBoardSetSystemClock(SYSCLK_16MHZ);
    
    // Debug UART
    halUsbInit();

    // show off
    doLCD();
    
   // init LEDs
    LED_PORT_OUT |= LED_1 | LED_2;
    LED_PORT_DIR |= LED_1 | LED_2;
    
	/// GET STARTED ///
	btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // init HCI
	const hci_transport_t * transport = hci_transport_h4_instance(btstack_uart_block_embedded_instance());
    btstack_link_key_db_t * link_key_db = btstack_link_key_db_memory_instance();
	hci_init(transport, &config);
	hci_set_link_key_db(link_key_db);
	hci_set_chipset(btstack_chipset_cc256x_instance());

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // init L2CAP
    l2cap_init();
		
    // ready - enable irq used in h4 task
    __enable_interrupt();   
 	
    // turn on!
	hci_power_control(HCI_POWER_ON);
	
    return 0;
}

