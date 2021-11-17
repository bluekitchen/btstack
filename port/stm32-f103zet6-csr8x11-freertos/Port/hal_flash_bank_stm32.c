/*
 * Copyright (C) 2017 BlueKitchen GmbH
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 */

/*
 *  hal_flash_bank_stm32.c
 * 
 *  HAL abstraction for Flash memory that can be written anywhere
 *  after being erased
 */

#include <stdint.h>
#include <string.h> // memcpy

#include "hal_flash_bank_stm32.h"
#include "stm32f1xx_hal.h"

#define STM32_FLASH_ALIGNMENT	4

static uint32_t hal_flash_bank_stm32_get_size(void * context){
	hal_flash_bank_stm32_t * self = (hal_flash_bank_stm32_t *) context;
	return self->page_size;
}

static uint32_t hal_flash_bank_memory_get_alignment(void * context){
    UNUSED(context);
    return STM32_FLASH_ALIGNMENT;
}

static void hal_flash_bank_stm32_erase(void * context, int bank){
	hal_flash_bank_stm32_t * self = (hal_flash_bank_stm32_t *) context;
	if (bank > 1) return;
	FLASH_EraseInitTypeDef eraseInit;
	eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
	eraseInit.PageAddress = self->page_start[bank];
	eraseInit.NbPages = 1;
	uint32_t sectorError;
	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(&eraseInit, &sectorError);
	HAL_FLASH_Lock();
}

static void hal_flash_bank_stm32_read(void * context, int bank, uint32_t offset, uint8_t * buffer, uint32_t size){
	hal_flash_bank_stm32_t * self = (hal_flash_bank_stm32_t *) context;

	if (bank > 1) return;
	if (offset > self->page_size) return;
	if ((offset + size) > self->page_size) return;

	memcpy(buffer, ((uint8_t *) self->page_start[bank]) + offset, size);
}

static void hal_flash_bank_stm32_write(void * context, int bank, uint32_t offset, const uint8_t * data, uint32_t size){
	hal_flash_bank_stm32_t * self = (hal_flash_bank_stm32_t *) context;

	if (bank > 1) return;
	if (offset > self->page_size) return;
	if ((offset + size) > self->page_size) return;

	unsigned int i;
	HAL_FLASH_Unlock();
	for (i=0;i<size;i+=STM32_FLASH_ALIGNMENT){
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, self->page_start[bank] + offset +i, *(uint32_t *)(data+i));
	}
	HAL_FLASH_Lock();
}

static const hal_flash_bank_t hal_flash_bank_stm32_impl = {
	/* uint32_t (*get_size)() */         &hal_flash_bank_stm32_get_size,
	/* uint32_t (*get_alignment)(..); */ &hal_flash_bank_memory_get_alignment,
	/* void (*erase)(..);             */ &hal_flash_bank_stm32_erase,
	/* void (*read)(..);              */ &hal_flash_bank_stm32_read,
	/* void (*write)(..);             */ &hal_flash_bank_stm32_write,
};

const hal_flash_bank_t * hal_flash_bank_stm32_init_instance(hal_flash_bank_stm32_t * context, uint32_t page_size,
        uint32_t bank_0_page_id, uint32_t bank_1_page_id){
    context->page_size = page_size;
    context->page_index[0] = bank_0_page_id;
    context->page_index[1] = bank_1_page_id;
    context->page_start[0] = FLASH_BASE + bank_0_page_id * page_size;
    context->page_start[1] = FLASH_BASE + bank_1_page_id * page_size;
	return &hal_flash_bank_stm32_impl;
}
