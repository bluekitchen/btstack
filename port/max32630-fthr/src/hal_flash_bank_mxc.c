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
 *  hal_flash_bank_mxc.c
 * 
 *  HAL abstraction for Flash memory that can be written anywhere
 *  after being erased implemented with memory
 */

#include <stdint.h>
#include <string.h> // memcpy

#include "hal_flash_bank_mxc.h"

#include "flc.h" // Maxim Flash Controller

static uint32_t hal_flash_bank_mxc_get_size(void * context){
	hal_flash_bank_mxc_t * self = (hal_flash_bank_mxc_t *) context;
	return self->sector_size;
}

static uint32_t hal_flash_bank_mxc_get_alignment(void * context){
    (void)(context);
    return 4;
}

static void hal_flash_bank_mxc_erase(void * context, int bank){
	hal_flash_bank_mxc_t * self = (hal_flash_bank_mxc_t *) context;

	if (bank > 1) return;

	// Erase page
	FLC_PageErase(self->banks[bank], MXC_V_FLC_ERASE_CODE_PAGE_ERASE, MXC_V_FLC_FLSH_UNLOCK_KEY);
}

static void hal_flash_bank_mxc_read(void * context, int bank, uint32_t offset, uint8_t * buffer, uint32_t size){
	hal_flash_bank_mxc_t * self = (hal_flash_bank_mxc_t *) context;

	if (bank > 1) return;
	if (offset > self->sector_size) return;
	if ((offset + size) > self->sector_size) return;

	memcpy(buffer, ((uint8_t *) self->banks[bank]) + offset, size);
}

static void hal_flash_bank_mxc_write(void * context, int bank, uint32_t offset, const uint8_t * data, uint32_t size){
	hal_flash_bank_mxc_t * self = (hal_flash_bank_mxc_t *) context;

	if (bank > 1) return;
	if (offset > self->sector_size) return;
	if ((offset + size) > self->sector_size) return;

	FLC_Write(self->banks[bank] + offset, data, size, MXC_V_FLC_FLSH_UNLOCK_KEY);
}

static const hal_flash_bank_t hal_flash_bank_mxc_impl = {
	/* uint32_t (*get_size)() */         &hal_flash_bank_mxc_get_size,
	/* uint32_t (*get_alignment)(..); */ &hal_flash_bank_mxc_get_alignment,
	/* void (*erase)(..);             */ &hal_flash_bank_mxc_erase,
	/* void (*read)(..);              */ &hal_flash_bank_mxc_read,
	/* void (*write)(..);             */ &hal_flash_bank_mxc_write,
};

const hal_flash_bank_t * hal_flash_bank_mxc_init_instance(hal_flash_bank_mxc_t * context, uint32_t sector_size,
		uintptr_t bank_0_addr, uintptr_t bank_1_addr){
	context->sector_size = sector_size;
	context->banks[0]   = bank_0_addr;
	context->banks[1]   = bank_1_addr;

	// prepare FLC
	FLC_Init();

	return &hal_flash_bank_mxc_impl;
}

