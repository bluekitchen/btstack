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
 *  hal_flash_bank_stm32.c
 * 
 *  HAL abstraction for Flash memory that can be written anywhere
 *  after being erased
 */

#include <stdint.h>
#include <string.h> // memcpy
#include "btstack_debug.h"  // assert
#include "btstack_defines.h" // UNUSED
#include "hal_flash_bank_zephyr.h"

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>

static const struct device *const zephyr_flash_controller =
        DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_flash_controller));

static uint32_t hal_flash_bank_zephyr_get_size(void * context){
	hal_flash_bank_zephyr_t * self = (hal_flash_bank_zephyr_t *) context;
	return self->sector_size;
}

static uint32_t hal_flash_bank_zephyr_get_alignment(void * context){
    hal_flash_bank_zephyr_t * self = (hal_flash_bank_zephyr_t *) context;
    return self->alignment;
}

static void hal_flash_bank_zephyr_erase(void * context, int bank){
    btstack_assert(bank >= 0);
    btstack_assert(bank < 2);

	hal_flash_bank_zephyr_t * self = (hal_flash_bank_zephyr_t *) context;

    int result = flash_erase(zephyr_flash_controller, self->banks[bank], self->sector_size);
    if (result != 0){
        log_error("Erase Failed, code %d.", result);
    }
}

static void hal_flash_bank_zephyr_read(void * context, int bank, uint32_t offset, uint8_t * buffer, uint32_t size){
    btstack_assert(bank >= 0);
    btstack_assert(bank < 2);

	hal_flash_bank_zephyr_t * self = (hal_flash_bank_zephyr_t *) context;

    btstack_assert(offset          <= self->sector_size);
    btstack_assert((offset + size) <= self->sector_size);

    int result = flash_read(zephyr_flash_controller, self->banks[bank] + offset, buffer, size);
    if (result != 0){
        log_error("Read Failed, bank %u, offset %u: code %d.", bank, offset, result);
    }
}

static void hal_flash_bank_zephyr_write(void * context, int bank, uint32_t offset, const uint8_t * data, uint32_t size){
    btstack_assert(bank >= 0);
    btstack_assert(bank < 2);

    hal_flash_bank_zephyr_t * self = (hal_flash_bank_zephyr_t *) context;

    btstack_assert(offset          <= self->sector_size);
    btstack_assert((offset + size) <= self->sector_size);

    int result = flash_write(zephyr_flash_controller, self->banks[bank] + offset, data, size);
    if (result != 0) {
        log_error("Write internal ERROR!, bank %u, offset %u: code %u", bank, offset, result);
    }
}

static const hal_flash_bank_t hal_flash_bank_zephyr_impl = {
    .get_size       = &hal_flash_bank_zephyr_get_size,
	.get_alignment  = &hal_flash_bank_zephyr_get_alignment,
	.erase          = &hal_flash_bank_zephyr_erase,
	.read           = &hal_flash_bank_zephyr_read,
    .write          = &hal_flash_bank_zephyr_write,
};

const hal_flash_bank_t * hal_flash_bank_zephyr_init_instance(hal_flash_bank_zephyr_t * context, uint32_t sector_size,
		uint16_t write_alignment, uintptr_t bank_0_addr, uintptr_t bank_1_addr){

    btstack_assert(write_alignment <= 16);

    /* use default: zephyr,flash-controller */
    if (!device_is_ready(zephyr_flash_controller)) {
        log_error("efault flash driver not ready");
        btstack_assert(false);
        return NULL;
    }

    context->sector_size = sector_size;
    context->alignment   = write_alignment;
	context->banks[0]    = bank_0_addr;
	context->banks[1]    = bank_1_addr;

	return &hal_flash_bank_zephyr_impl;
}
