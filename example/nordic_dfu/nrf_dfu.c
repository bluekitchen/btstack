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
#include "nrf_dfu.h"

#include "nrf_dfu_utils.h"
#include "nrf_dfu_transport.h"
#include "nrf_dfu_req_handler.h"
#include "nrf_dfu_ble.h"
#include "nrf_log.h"

static nrf_dfu_observer_t m_user_observer; //<! Observer callback set by the user.
static nrf_dfu_transport_t m_dfu_tran = {
    .init_func = nrf_dfu_ble_init,
    .close_func = NULL,
};

uint32_t nrf_dfu_init(nrf_dfu_observer_t observer)
{
    uint32_t ret_val;

    m_user_observer = observer;

    NRF_LOG_INFO("Entering DFU mode in application.");

    m_user_observer(NRF_DFU_EVT_DFU_INITIALIZED, NULL, 0);

    ret_val = nrf_dfu_settings_init(false);
    if (ret_val != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("nrf dfu setting init fail, ret: 0x%08x", ret_val);
        return NRF_ERROR_INTERNAL;
    }
    // Initializing transports
    ret_val = nrf_dfu_transports_init(observer, &m_dfu_tran);
    if (ret_val != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Could not initalize DFU transport: 0x%08x", ret_val);
        return ret_val;
    }

    return nrf_dfu_req_handler_init(observer);
}
