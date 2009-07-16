/*
 *  hci_dump.h
 *
 *  Dump HCI trace in BlueZ's hcidump format
 * 
 *  Created by Matthias Ringwald on 5/26/09.
 */

#include <stdint.h>

typedef enum {
    HCI_DUMP_BLUEZ = 0,
    HCI_DUMP_PACKETLOGGER,
    HCI_DUMP_STDOUT
} hci_dump_format_t;

void hci_dump_open(char *filename, hci_dump_format_t format);
void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
void hci_dump_close();
