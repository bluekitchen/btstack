/*
 * Copyright (C) 2021 BlueKitchen GmbH
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

/**
 * @title HCI Transport H5
 *
 */

#ifndef HCI_TRANSPORT_H5_H
#define HCI_TRANSPORT_H5_H

#include "hci_transport.h"
#include "btstack_uart.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/*
 * @brief Setup H5 instance with btstack_uart implementation that supports SLIP frames
 * @param uart_driver to use
 */
const hci_transport_t * hci_transport_h5_instance(const btstack_uart_t * uart_driver);

/*
 * @brief Enable H5 Low Power Mode: enter sleep mode after x ms of inactivity
 * @param inactivity_timeout_ms or 0 for off
 */
void hci_transport_h5_set_auto_sleep(uint16_t inactivity_timeout_ms);

/*
 * @brief Enable BSCP mode H5, by enabling event parity
 * @deprecated Parity can be enabled in UART driver configuration
 */
void hci_transport_h5_enable_bcsp_mode(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // HCI_TRANSPORT_H5_H
