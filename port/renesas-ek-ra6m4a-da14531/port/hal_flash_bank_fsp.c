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

#define BTSTACK_FILE__ "hal_flash_bank_fsp.c"

#include <stdint.h>
#include <string.h> // memcpy
#include <btstack_util.h>

#include "hal_flash_bank_fsp.h"
#include "btstack_debug.h"
#include "hal_data.h"

static uint32_t hal_flash_bank_fsp_get_size(void * context){
    hal_flash_bank_fsp_t * self = (hal_flash_bank_fsp_t *) context;
    return self->bank_size;
}

static uint32_t hal_flash_bank_fsp_get_alignment(void * context){
    UNUSED(context);
    return 1;
}

// returns offset of first blank data in bank
static uint32_t hal_flash_bank_fsp_detect_blank_start(uint32_t bank_start, uint32_t bank_size){
    // binary search. right points to start of blank or at end of bank
    uint32_t left  = 0;
    uint32_t right = bank_size;
    while (left < right){
        uint32_t middle = (left + right) / 2;
        flash_result_t blank_check_result;
        fsp_err_t err = R_FLASH_HP_BlankCheck(&g_flash0_ctrl, (uint32_t) (bank_start + middle), bank_size - middle, &blank_check_result);
        btstack_assert(FSP_SUCCESS == err);
        if (FLASH_RESULT_BLANK == blank_check_result){
            // blank from middle..right
            right = middle;
        } else {
            // middle is not blank, blank starts at least one byte higher
            left = middle + 1;
        }
    }
    return right;
}

static void hal_flash_bank_fsp_erase(void * context, int bank){
    hal_flash_bank_fsp_t * self = (hal_flash_bank_fsp_t *) context;
    if (bank > 1) return;
    fsp_err_t err = R_FLASH_HP_Erase(&g_flash0_ctrl, self->bank_addr[bank], self->bank_size / self->block_size);
    btstack_assert(err == FSP_SUCCESS);
}

static void hal_flash_bank_fsp_read(void * context, int bank, uint32_t offset, uint8_t * buffer, uint32_t size){
    hal_flash_bank_fsp_t * self = (hal_flash_bank_fsp_t *) context;
    uint32_t end_addr_read = offset + size;

    if (bank > 1) return;
    if (offset > self->bank_size) return;
    if (end_addr_read > self->bank_size) return;

    // first copy valid bytes
    uint32_t bytes_to_copy = 0;
    uint32_t end_addr_copy = btstack_min(end_addr_read, self->blank_start[bank]);
    if (end_addr_copy > offset){
        bytes_to_copy = end_addr_copy - offset;
        memcpy(buffer, (uint8_t *) (self->bank_addr[bank] + offset), bytes_to_copy);
        buffer += bytes_to_copy;
    }
    // fill rest with 0xff
    uint32_t bytes_to_fill = size - bytes_to_copy;
    if (bytes_to_fill > 0){
        memset(buffer, 0xff, bytes_to_fill);
    }
}

static void hal_flash_bank_fsp_write(void * context, int bank, uint32_t offset, const uint8_t * data, uint32_t size){
    hal_flash_bank_fsp_t * self = (hal_flash_bank_fsp_t *) context;

    if (bank > 1) return;
    if (offset > self->bank_size) return;
    if ((offset + size) > self->bank_size) return;

    // update blank start
    self->blank_start[bank] = btstack_max(self->blank_start[bank], offset + size);

    fsp_err_t err = R_FLASH_HP_Write(&g_flash0_ctrl, (uint32_t) data, offset, size);
    btstack_assert(err == FSP_SUCCESS);
}

static const hal_flash_bank_t hal_flash_bank_fsp_impl = {
    /* uint32_t (*get_size)() */         &hal_flash_bank_fsp_get_size,
    /* uint32_t (*get_alignment)(..); */ &hal_flash_bank_fsp_get_alignment,
    /* void (*erase)(..);             */ &hal_flash_bank_fsp_erase,
    /* void (*read)(..);              */ &hal_flash_bank_fsp_read,
    /* void (*write)(..);             */ &hal_flash_bank_fsp_write,
};

const hal_flash_bank_t * hal_flash_bank_fsp_init_instance(hal_flash_bank_fsp_t * context, uint32_t bank_size,
        uint32_t bank_0_addr, uint32_t bank_1_addr){
    // get info
    flash_info_t p_info;
    R_FLASH_HP_InfoGet(&g_flash0_ctrl, &p_info);
    // assume first region of data flash is used
    btstack_assert(bank_0_addr >= p_info.data_flash.p_block_array[0].block_section_st_addr);
    btstack_assert((bank_0_addr + bank_size - 1) <= p_info.data_flash.p_block_array[0].block_section_end_addr);
    btstack_assert(bank_1_addr >= p_info.data_flash.p_block_array[0].block_section_st_addr);
    btstack_assert((bank_1_addr + bank_size - 1) <= p_info.data_flash.p_block_array[0].block_section_end_addr);
    context->block_size = p_info.data_flash.p_block_array[0].block_size;
    context->bank_size = bank_size;
    context->bank_addr[0] = bank_0_addr;
    context->bank_addr[1] = bank_1_addr;
    // get start of blank area
    context->blank_start[0] = hal_flash_bank_fsp_detect_blank_start(context->bank_addr[0], bank_size);
    context->blank_start[1] = hal_flash_bank_fsp_detect_blank_start(context->bank_addr[1], bank_size);
    return &hal_flash_bank_fsp_impl;
}
