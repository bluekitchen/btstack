/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#ifndef __MESH_IV_INDEX_SEQ_NUMBER_H
#define __MESH_IV_INDEX_SEQ_NUMBER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * IV Index
 */

void     mesh_set_iv_index(uint32_t iv_index);
uint32_t mesh_get_iv_index(void);

uint32_t mesh_get_iv_index_for_tx(void);

/**
 * @brief Get IV Update state
 */
int mesh_iv_update_active(void);

/**
 * @brief Trigger IV Update
 */
void mesh_trigger_iv_update(void);

/**
 * @breif IV update was completed
 */
void mesh_iv_update_completed(void);

/** 
 * @brief IV Index was recovered
 * @param iv_update_active
 * @param iv_index
 */
void mesh_iv_index_recovered(uint8_t iv_update_active, uint32_t iv_index);

/**
 * @brief Set callback for sequence number update
 */
void mesh_sequence_number_set_update_callback(void (*callback)(void));

/**
 * Sequence Number
 */
void     mesh_sequence_number_set(uint32_t seq);
uint32_t mesh_sequence_number_peek(void);
uint32_t mesh_sequence_number_next(void);


#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
