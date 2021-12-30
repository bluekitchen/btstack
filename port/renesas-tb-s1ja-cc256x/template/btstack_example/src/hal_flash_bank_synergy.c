/*
 * Copyright (C) 2020 BlueKitchen GmbH
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
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */


#include <stdint.h>
#include <string.h> // memcpy

#include "hal_flash_bank_synergy.h"
#include "btstack_debug.h"
#include "hal_data.h"

static uint32_t hal_flash_bank_synergy_get_size(void * context){
    hal_flash_bank_synergy_t * self = (hal_flash_bank_synergy_t *) context;
    return self->page_size;
}

static uint32_t hal_flash_bank_synergy_get_alignment(void * context){
    UNUSED(context);
    return 1;
}

static void hal_flash_bank_synergy_erase(void * context, int bank){
    hal_flash_bank_synergy_t * self = (hal_flash_bank_synergy_t *) context;
    if (bank > 1) return;
    g_flash0.p_api->erase(g_flash0.p_ctrl, self->page_start[bank], 1);
}

static void hal_flash_bank_synergy_read(void * context, int bank, uint32_t offset, uint8_t * buffer, uint32_t size){
    hal_flash_bank_synergy_t * self = (hal_flash_bank_synergy_t *) context;

    if (bank > 1) return;
    if (offset > self->page_size) return;
    if ((offset + size) > self->page_size) return;

    memcpy(buffer, (uint8_t *) (self->page_start[bank] + offset), size);
}

static void hal_flash_bank_synergy_write(void * context, int bank, uint32_t offset, const uint8_t * data, uint32_t size){
    hal_flash_bank_synergy_t * self = (hal_flash_bank_synergy_t *) context;

    if (bank > 1) return;
    if (offset > self->page_size) return;
    if ((offset + size) > self->page_size) return;

    g_flash0.p_api->write(g_flash0.p_ctrl, data, self->page_start[bank] + offset, size);
}

static const hal_flash_bank_t hal_flash_bank_synergy_impl = {
    /* uint32_t (*get_size)() */         &hal_flash_bank_synergy_get_size,
    /* uint32_t (*get_alignment)(..); */ &hal_flash_bank_synergy_get_alignment,
    /* void (*erase)(..);             */ &hal_flash_bank_synergy_erase,
    /* void (*read)(..);              */ &hal_flash_bank_synergy_read,
    /* void (*write)(..);             */ &hal_flash_bank_synergy_write,
};

const hal_flash_bank_t * hal_flash_bank_synergy_init_instance(hal_flash_bank_synergy_t * context, uint32_t page_size,
        uint32_t bank_0_addr, uint32_t bank_1_addr){
    context->page_size = page_size;
    context->page_start[0] = bank_0_addr;
    context->page_start[1] = bank_1_addr;
    return &hal_flash_bank_synergy_impl;
}
