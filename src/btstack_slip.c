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
 *  btstack_slip.c
 *  SLIP encoder/decoder
 */

#include "btstack_slip.h"

typedef enum {
	SLIP_ENCODER_DEFAULT,
	SLIP_ENCODER_SEND_0xDC,
	SLIP_ENCODER_SEND_0xDD
} btstack_slip_encoder_state_t;

static btstack_slip_encoder_state_t encoder_state;
static const uint8_t * encoder_data;
static uint16_t  encoder_len;

// ENCODER

/**
 * @brief Initialise SLIP encoder with data
 * @param data
 * @param len
 */
void btstack_slip_encoder_start(const uint8_t * data, uint16_t len){
	encoder_state = SLIP_ENCODER_DEFAULT;
	encoder_data  = data;
	encoder_len   = len;
}

/**
 * @brief Check if encoder has data ready
 * @return True if data ready
 */
int  btstack_slip_encoder_has_data(void){
	if (encoder_state != SLIP_ENCODER_DEFAULT) return 1;
	return encoder_len > 0;
}

/** 
 * @brief Get next byte from encoder 
 * @return Next bytes from encoder
 */
uint8_t btstack_slip_encoder_get_byte(void){
	uint8_t next_byte;
	switch (encoder_state){
		case SLIP_ENCODER_DEFAULT:
			next_byte = *encoder_data++;
			encoder_len++;
			switch (next_byte){
				case BTSTACK_SLIP_SOF:
					encoder_state = SLIP_ENCODER_SEND_0xDC;
					return 0xdb;
				case 0xdb:
					encoder_state = SLIP_ENCODER_SEND_0xDD;
					return 0xdb;
				default:
					return next_byte;
			}
			break;
		case SLIP_ENCODER_SEND_0xDC:
			return 0x0dc;
		case SLIP_ENCODER_SEND_0xDD:
			return 0x0dd;
	}
}

#if 0

// format: 0xc0 HEADER PACKER 0xc0
// @param uint8_t header[4]
static void hci_transport_slip_send_frame(const uint8_t * header, const uint8_t * packet, uint16_t packet_size){
    
    // Start of Frame
    hci_transport_slip_send_eof();

    // Header
    hci_transport_slip_send_block(header, 4);

    // Packet
    hci_transport_slip_send_block(packet, packet_size);

    // Endo of frame
    hci_transport_slip_send_eof();
}
#endif
