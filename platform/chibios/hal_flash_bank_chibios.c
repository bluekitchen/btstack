/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

/*
 *  hal_flash_bank_chbios.c
 * 
 *  HAL abstraction for Flash memory using hal_flash and hal_efl.h on ELFD1
 */

#define BTSTACK_FILE__ "hal_flash_bank_chibios.c"

#include "hal_flash_bank.h"
#include "btstack_debug.h"
#include "btstack_util.h"

#include "hal_flash_bank_chibios.h"
#include "hal.h"

#include <inttypes.h>
#include <string.h> // memcpy

uint32_t hal_flash_bank_chibios_get_size(void * context){
    hal_flash_bank_chbios_t * self = (hal_flash_bank_chbios_t *) context;
    return self->sector_size;
}

uint32_t hal_flash_bank_chibios_get_alignment(void * context){
    UNUSED(context);
    return 4;
}

void hal_flash_bank_chibios_erase(void * context, int bank){
    hal_flash_bank_chbios_t * self = (hal_flash_bank_chbios_t *) context;
    if (bank > 1) return;
    flashStartEraseSector(&EFLD1, self->sectors[bank]);
    // poll until done
    bool done = false;
    while (!done){
        uint32_t retry_delay_ms;
        flash_error_t result = flashQueryErase(&EFLD1, &retry_delay_ms);
        switch (result){
            case FLASH_BUSY_ERASING:
                chThdSleepMilliseconds( retry_delay_ms );
                break;
            case FLASH_NO_ERROR:
                done = true;
                break;
            default:
                btstack_assert(false);
                break;
        }
    }
}

void hal_flash_bank_chibios_read(void * context, int bank, uint32_t offset, uint8_t * buffer, uint32_t size){
    hal_flash_bank_chbios_t * self = (hal_flash_bank_chbios_t *) context;

    if (bank > 1) return;
    if (offset > self->sector_size) return;
    if ((offset + size) > self->sector_size) return;

    flashRead(&EFLD1, self->banks[bank] + offset, size, buffer);
}

void hal_flash_bank_chibios_write(void * context, int bank, uint32_t offset, const uint8_t * data, uint32_t size){
    hal_flash_bank_chbios_t * self = (hal_flash_bank_chbios_t *) context;

    if (bank > 1) return;
    if (offset > self->sector_size) return;
    if ((offset + size) > self->sector_size) return;

    flashProgram(&EFLD1, self->banks[bank] + offset, size, data);
}

static const hal_flash_bank_t hal_flash_bank_chibios = {
    &hal_flash_bank_chibios_get_size,
    &hal_flash_bank_chibios_get_alignment,
    &hal_flash_bank_chibios_erase,
    &hal_flash_bank_chibios_read, 
    &hal_flash_bank_chibios_write
};

const hal_flash_bank_t * hal_flash_bank_chibios_init_instance(hal_flash_bank_chbios_t * context, 
    uint16_t bank_0_sector, uint16_t bank_1_sector){

    // start EFL driver
    eflStart(&EFLD1, NULL);

    const flash_descriptor_t * descriptor = flashGetDescriptor(&EFLD1);
    btstack_assert(bank_0_sector < descriptor->sectors_count);
    btstack_assert(bank_1_sector < descriptor->sectors_count);

    // get bank size and offsets from chibios flash descriptor
    uint32_t bank_0_offset = descriptor->sectors[bank_0_sector].offset;
    uint32_t bank_1_offset = descriptor->sectors[bank_1_sector].offset;
    uint32_t sector_size = btstack_min(descriptor->sectors[bank_0_sector].size, descriptor->sectors[bank_1_sector].size);
    
    context->sector_size = sector_size;
    context->sectors[0] = bank_0_sector;
    context->sectors[1] = bank_1_sector;
    context->banks[0]   = bank_0_offset;
    context->banks[1]   = bank_1_offset;

    log_info("Bank size %" PRIu32 " bytes", sector_size);
    log_info("Bank 0 uses sector %u, offset %" PRIx32, bank_0_sector, bank_0_offset);
    log_info("Bank 1 uses sector %u, offset %" PRIx32, bank_1_sector, bank_1_offset);

    return &hal_flash_bank_chibios;
}
