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

/*
 *  Made for BlueKitchen by OneWave with <3
 *      Author: ftrefou@onewave.io
 */

#include <stdint.h>
#include <string.h> // memcpy

#include "hal_flash_bank_stm32wb.h"
#include "stm32wbxx_hal.h"
#include "shci.h"
#include "app_conf.h"
#include "btstack_debug.h"

#define STM32WB_FLASH_ALIGNMENT     8
#define LITTLE_TO_64(buff, offset)  ((uint64_t)  (  (((uint64_t) ((uint8_t*)(buff))[(offset)+7]) << 56)  |  (((uint64_t) ((uint8_t*)(buff))[(offset)+6]) << 48)  |  (((uint64_t) ((uint8_t*)(buff))[(offset)+5]) << 40)  |  (((uint64_t) ((uint8_t*)(buff))[(offset)+4]) << 32)  |  (((uint64_t) ((uint8_t*)(buff))[(offset)+3]) << 24)  |  (((uint64_t) ((uint8_t*)(buff))[(offset)+2]) << 16)  |  (((uint64_t) ((uint8_t*)(buff))[(offset)+1]) <<  8)  |  (((uint64_t) ((uint8_t*)(buff))[(offset)+0]) <<  0)  ))

static uint32_t hal_flash_bank_stm32wb_get_size(void * context){
    hal_flash_bank_stm32wb_t * self = (hal_flash_bank_stm32wb_t *) context;
    return self->page_size;
}

static uint32_t hal_flash_bank_stm32wb_get_alignment(void * context){
    UNUSED(context);
    return STM32WB_FLASH_ALIGNMENT;
}

static void hal_flash_bank_stm32wb_erase(void * context, int bank){
    hal_flash_bank_stm32wb_t * self = (hal_flash_bank_stm32wb_t *) context;
    if (bank > 1) return;
    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.Page = self->page_index[bank];
    eraseInit.NbPages = 1;
    uint32_t sectorError;

    // Get HW semaphore to get flash access
    while( LL_HSEM_1StepLock( HSEM, CFG_HW_FLASH_SEMID )){};

    // Inform CPU2 from erase activity
    SHCI_C2_FLASH_EraseActivity(ERASE_ACTIVITY_ON);

    // Unlock the flash
    HAL_FLASH_Unlock();

    // Erase the flash
    HAL_FLASHEx_Erase(&eraseInit, &sectorError);

    // RE-Lock the flash
    HAL_FLASH_Lock();

    // Release HW semaphore
    LL_HSEM_ReleaseLock( HSEM, CFG_HW_FLASH_SEMID, 0 );

    // Inform CPU2 from end of flash erase activity
    SHCI_C2_FLASH_EraseActivity(ERASE_ACTIVITY_OFF);
}

static void hal_flash_bank_stm32wb_read(void * context, int bank, uint32_t offset, uint8_t * buffer, uint32_t size){
    hal_flash_bank_stm32wb_t * self = (hal_flash_bank_stm32wb_t *) context;

    if (bank > 1) return;
    if (offset > self->page_size) return;
    if ((offset + size) > self->page_size) return;

    memcpy(buffer, (uint8_t *) (FLASH_BASE + self->page_start[bank] + offset), size);
}

static void hal_flash_bank_stm32wb_write(void * context, int bank, uint32_t offset, const uint8_t * data, uint32_t size){
    hal_flash_bank_stm32wb_t * self = (hal_flash_bank_stm32wb_t *) context;

    if (bank > 1) return;
    if (offset > self->page_size) return;
    if ((offset + size) > self->page_size) return;

    HAL_StatusTypeDef status = 0;

    // Get HW semaphore to get flash access
    while( LL_HSEM_1StepLock( HSEM, CFG_HW_FLASH_SEMID )){};

    unsigned int i;
    HAL_FLASH_Unlock();
    for (i=0;i<size;i+=STM32WB_FLASH_ALIGNMENT){
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_BASE + self->page_start[bank] + offset + i, LITTLE_TO_64(data, i));
        if (status != 0) break;
    }
    HAL_FLASH_Lock();

    // Release HW semaphore
    LL_HSEM_ReleaseLock( HSEM, CFG_HW_FLASH_SEMID, 0 );

    if (status != 0){
        log_error("Flash error: %x", status);
    } 
}

static const hal_flash_bank_t hal_flash_bank_stm32wb_impl = {
    /* uint32_t (*get_size)() */         &hal_flash_bank_stm32wb_get_size,
    /* uint32_t (*get_alignment)(..); */ &hal_flash_bank_stm32wb_get_alignment,
    /* void (*erase)(..);             */ &hal_flash_bank_stm32wb_erase,
    /* void (*read)(..);              */ &hal_flash_bank_stm32wb_read,
    /* void (*write)(..);             */ &hal_flash_bank_stm32wb_write,
};

const hal_flash_bank_t * hal_flash_bank_stm32wb_init_instance(hal_flash_bank_stm32wb_t * context, uint32_t page_size,
        uint32_t bank_0_page_id, uint32_t bank_1_page_id){
    context->page_size = page_size;
    context->page_index[0] = bank_0_page_id;
    context->page_index[1] = bank_1_page_id;
    context->page_start[0] = bank_0_page_id * page_size;
    context->page_start[1] = bank_1_page_id * page_size;
    return &hal_flash_bank_stm32wb_impl;
}
