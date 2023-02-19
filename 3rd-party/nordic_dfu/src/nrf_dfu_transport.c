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
#include "nrf_dfu_transport.h"
#include "nrf_log.h"

static nrf_dfu_transport_t *m_dfu_tran;

uint32_t nrf_dfu_transports_init(nrf_dfu_observer_t observer, nrf_dfu_transport_t *p_dfu_tran)
{
    uint32_t ret_val = NRF_SUCCESS;

    if (NULL == p_dfu_tran || NULL == observer) {
        NRF_LOG_ERROR("Initializing transports fail!");
        return NRF_ERROR_INVALID_PARAM;
    }

    m_dfu_tran = p_dfu_tran;
    NRF_LOG_DEBUG("Initializing transports (found: %x)", m_dfu_tran);

    if (m_dfu_tran->init_func) {
        ret_val = m_dfu_tran->init_func(observer);
    }

    return ret_val;
}

uint32_t nrf_dfu_transports_close(nrf_dfu_transport_t const * p_exception)
{
    uint32_t ret_val = NRF_SUCCESS;

    NRF_LOG_DEBUG("Shutting down transports (found: %x)", m_dfu_tran);

    if (m_dfu_tran->close_func) {
        ret_val = m_dfu_tran->close_func(p_exception);
    }

    return ret_val;
}
