/*
 *  BTWiiMote.h
 *
 *  Created by Matthias Ringwald on 8/2/09.
 */

#include <stdint.h>

void start_bt();
void hci_register_event_packet_handler(void (*handler)(uint8_t *packet, uint16_t size));

void set_bt_state_cb(void (*cb)(char *text));
void set_data_cb(void (*handler)(uint8_t x, uint8_t y, uint8_t z));
