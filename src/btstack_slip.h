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

/**
 * @title SLIP encoder/decoder
 *
 */

#ifndef BTSTACK_SLIP_H
#define BTSTACK_SLIP_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

#define BTSTACK_SLIP_SOF 0xc0

/* API_START */

// ENCODER

/**
 * @brief Initialise SLIP encoder with data
 * @param data
 * @param len
 */
void btstack_slip_encoder_start(const uint8_t * data, uint16_t len);

/**
 * @brief Check if encoder has data ready
 * @return True if data ready
 */
int  btstack_slip_encoder_has_data(void);

/** 
 * @brief Get next byte from encoder 
 * @return Next bytes from encoder
 */
uint8_t btstack_slip_encoder_get_byte(void);

// DECODER

/**
 * @brief Initialise SLIP decoder with buffer
 * @param buffer to store received data
 * @param max_size of buffer
 */
void btstack_slip_decoder_init(uint8_t * buffer, uint16_t max_size);

/**
 * @brief Process received byte
 * @param input
 */

void btstack_slip_decoder_process(uint8_t input);

/**
 * @brief Get size of decoded frame
 * @return size of frame. Size = 0 => frame not complete
 */

uint16_t btstack_slip_decoder_frame_size(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
