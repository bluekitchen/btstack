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

/*
 *  hci_dump.h
 *
 *  Dump HCI trace as BlueZ's hcidump format, Apple's PacketLogger, or stdout
 * 
 *  Created by Matthias Ringwald on 5/26/09.
 */

#ifndef __HCI_DUMP_H
#define __HCI_DUMP_H

#include <stdint.h>

#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    HCI_DUMP_BLUEZ = 0,
    HCI_DUMP_PACKETLOGGER,
    HCI_DUMP_STDOUT
} hci_dump_format_t;

void hci_dump_open(const char *filename, hci_dump_format_t format);
void hci_dump_set_max_packets(int packets); // -1 for unlimited
void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
void hci_dump_log(const char * format, ...);
void hci_dump_close(void);

#ifdef __AVR__
void hci_dump_log_P(PGM_P format, ...);
#endif

#if defined __cplusplus
}
#endif
#endif // __HCI_DUMP_H
