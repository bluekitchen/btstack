/**
 * Copyright (c) 2016 - 2021, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "nrf_dfu_flash.h"
#include "nrf_dfu_types.h"

#include "nrf_log.h"
#include "stm32f4xx_hal.h"

/* Base address of the Flash sectors */
#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) /* Base address of Sector 0, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Base address of Sector 1, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000) /* Base address of Sector 2, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000) /* Base address of Sector 3, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08010000) /* Base address of Sector 4, 64 Kbytes   */
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08020000) /* Base address of Sector 5, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08040000) /* Base address of Sector 6, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x08060000) /* Base address of Sector 7, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08080000) /* Base address of Sector 8, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x080A0000) /* Base address of Sector 9, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x080C0000) /* Base address of Sector 10, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x080E0000) /* Base address of Sector 11, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_END   (ADDR_FLASH_SECTOR_11 + (128 * 1024))

#define NRF_DFU_FLASH_VALIDATION    1

static uint32_t hal_get_sector_from_address(uint32_t addr)
{
    uint32_t sector = 0xFF;

    if ((addr < ADDR_FLASH_SECTOR_1) && (addr >= ADDR_FLASH_SECTOR_0)) {
        sector = FLASH_SECTOR_0;
    } else if((addr < ADDR_FLASH_SECTOR_2) && (addr >= ADDR_FLASH_SECTOR_1)) {
        sector = FLASH_SECTOR_1;
    } else if ((addr < ADDR_FLASH_SECTOR_3) && (addr >= ADDR_FLASH_SECTOR_2)) {
        sector = FLASH_SECTOR_2;
    } else if ((addr < ADDR_FLASH_SECTOR_4) && (addr >= ADDR_FLASH_SECTOR_3)) {
        sector = FLASH_SECTOR_3;
    } else if ((addr < ADDR_FLASH_SECTOR_5) && (addr >= ADDR_FLASH_SECTOR_4)) {
        sector = FLASH_SECTOR_4;
    } else if ((addr < ADDR_FLASH_SECTOR_6) && (addr >= ADDR_FLASH_SECTOR_5)) {
        sector = FLASH_SECTOR_5;
    } else if ((addr < ADDR_FLASH_SECTOR_7) && (addr >= ADDR_FLASH_SECTOR_6)) {
        sector = FLASH_SECTOR_6;
    } else if ((addr < ADDR_FLASH_SECTOR_8) && (addr >= ADDR_FLASH_SECTOR_7)) { 
        sector = FLASH_SECTOR_7;
    } else if ((addr < ADDR_FLASH_SECTOR_9) && (addr >= ADDR_FLASH_SECTOR_8)) { 
        sector = FLASH_SECTOR_8;
    } else if ((addr < ADDR_FLASH_SECTOR_10) && (addr >= ADDR_FLASH_SECTOR_9)) { 
        sector = FLASH_SECTOR_9;
    } else if ((addr < ADDR_FLASH_SECTOR_11) && (addr >= ADDR_FLASH_SECTOR_10)) { 
        sector = FLASH_SECTOR_10;
    } else if ((addr < ADDR_FLASH_SECTOR_END) && (addr >= ADDR_FLASH_SECTOR_11)) {
        sector = FLASH_SECTOR_11;
    }
    return sector;
}

static uint32_t hal_flash_write(void *src, uint32_t addr, uint32_t len)
{
	unsigned int i;
	HAL_FLASH_Unlock();
	for (i = 0;i < len;i+=1){
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, *(uint8_t *)(src+i));
	}

	HAL_FLASH_Lock();
#if NRF_DFU_FLASH_VALIDATION
    if (memcmp((void *)addr, src, len) != 0) {
        NRF_LOG_ERROR("Program Failed");
        return NRF_ERROR_INTERNAL;
    }
#endif
    return 0;
}

static uint32_t hal_flash_erase(uint32_t addr, uint32_t len)
{
    uint32_t sector;
    FLASH_EraseInitTypeDef eraseInit;

    sector = hal_get_sector_from_address(addr);
    if (sector > FLASH_SECTOR_11) {
        NRF_LOG_ERROR("get sector fail by invalid addr 0x%x ", addr);
        return NRF_ERROR_INTERNAL;
    }
	eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
	eraseInit.Sector = sector;
	eraseInit.NbSectors = 1;
	eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_1;	// safe value
	uint32_t sectorError;
	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(&eraseInit, &sectorError);
	HAL_FLASH_Lock();

#if NRF_DFU_FLASH_VALIDATION
    for (size_t i = 0; i < len; i++) {
        uint8_t *val = (void *)(addr + i);
        if (*val != 0xff) {
            NRF_LOG_ERROR("Erase at 0x%x Failed", (int)val);
            return NRF_ERROR_INTERNAL;
        }
    }
#endif
    return 0;
}

ret_code_t nrf_dfu_flash_init(bool sd_irq_initialized)
{
    NRF_LOG_DEBUG("nrf_dfu_flash_init, nothing to do!");
    return NRF_SUCCESS;
}

ret_code_t nrf_dfu_flash_store(uint32_t                   dest,
                               void               const * p_src,
                               uint32_t                   len,
                               nrf_dfu_flash_callback_t   callback)
{
    ret_code_t rc;

    NRF_LOG_DEBUG("nrf_fstorage_write(addr=%p, src=%p, len=%d bytes)",
                  dest, p_src, len);

    //lint -save -e611 (Suspicious cast)
    rc = hal_flash_write(p_src, dest, len);
    //lint -restore

    return rc;
}


ret_code_t nrf_dfu_flash_erase(uint32_t                 page_addr,
                               uint32_t                 num_pages,
                               nrf_dfu_flash_callback_t callback)
{
    ret_code_t rc;
    uint32_t len = num_pages * CODE_PAGE_SIZE;
    NRF_LOG_DEBUG("nrf_fstorage_erase(addr=0x%p, len=%d pages)", page_addr, num_pages);

    if ((page_addr + len) > (NRF_DFU_BANK1_START_ADDR + NRF_DFU_BANK1_SIZE)) {
        NRF_LOG_ERROR("out of boundary, start_addr:0x%x, len:0x%x", page_addr, len);
        return NRF_ERROR_INVALID_PARAM;
    }

    if (page_addr % NRF_DFU_FLASH_SECTOR_SIZE) {
        NRF_LOG_WARNING("start addr not alignd, start_addr:0x%x", page_addr);
        return NRF_SUCCESS;
    }
    if (len < NRF_DFU_FLASH_SECTOR_SIZE) {
        len = NRF_DFU_FLASH_SECTOR_SIZE;
    }
    //lint -save -e611 (Suspicious cast)
    for (uint32_t i = 0; i < (len / NRF_DFU_FLASH_SECTOR_SIZE); i++) {
        rc = hal_flash_erase(page_addr + i * NRF_DFU_FLASH_SECTOR_SIZE, NRF_DFU_FLASH_SECTOR_SIZE);
        if (rc != 0) {
            NRF_LOG_ERROR("Erase fail at addr: 0x%x ", page_addr + NRF_DFU_FLASH_SECTOR_SIZE * i);
            return NRF_ERROR_INTERNAL;
        }
    }
    //lint -restore

    return rc;
}

