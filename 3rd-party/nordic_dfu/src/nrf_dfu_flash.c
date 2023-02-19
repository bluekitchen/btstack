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

static nrf_dfu_flash_interface_t *flash_if;

ret_code_t nrf_dfu_flash_init(nrf_dfu_flash_interface_t *flash_interface)
{
    NRF_LOG_DEBUG("nrf_dfu_flash_init!");
    flash_if = flash_interface;
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
    rc = flash_if->write(p_src, dest, len);
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
        rc = flash_if->erase(page_addr + i * NRF_DFU_FLASH_SECTOR_SIZE, NRF_DFU_FLASH_SECTOR_SIZE);
        if (rc != 0) {
            NRF_LOG_ERROR("Erase fail at addr: 0x%x ", page_addr + NRF_DFU_FLASH_SECTOR_SIZE * i);
            return NRF_ERROR_INTERNAL;
        }
    }
    //lint -restore

    return rc;
}

