/*
 * Copyright (C) 2021 BlueKitchen GmbH
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
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/**
 * @title Memory TLV Instance
 *
 * Empty implementation for BTstack's Tag Value Length Persistent Storage implementations
 * No keys are stored. Can be used as placeholder during porting to new platform.
 */

#ifndef MOCK_BTSTACK_TLV_H
#define MOCK_BTSTACK_TLV_H

#include <stdint.h>
#include "btstack_tlv.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */
typedef struct {
    btstack_linked_list_t entry_list;
} mock_btstack_tlv_t;

/**
 * Init Tag Length Value Store
 * @param self mock_btstack_tlv_t
 */
const btstack_tlv_t * mock_btstack_tlv_init_instance(mock_btstack_tlv_t * self);

/**
 * Free TLV entries
 * @param self mock_btstack_tlv_t
 */
void mock_btstack_tlv_deinit(mock_btstack_tlv_t * self);

/* API_END */

#if defined __cplusplus
}
#endif
#endif // MOCK_BTSTACK_TLV_H
