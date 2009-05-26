/*
 *  hci_dump.h
 *
 *  Dump HCI trace in BlueZ's hcidump format
 * 
 *  Created by Matthias Ringwald on 5/26/09.
 */

#include <stdint.h>

typedef struct {
	uint16_t	len;
	uint8_t		in;
	uint8_t		pad;
	uint32_t	ts_sec;
	uint32_t	ts_usec;
    uint8_t     packet_type;
} __attribute__ ((packed)) hcidump_hdr;

void hci_dump_open(char *filename);
void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
void hci_dump_close();
