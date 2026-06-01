/*
 * Copyright (C) 2026 BlueKitchen GmbH
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
 * @title Buffered HCI Logging
 *
 * Buffer HCI packets in memory and flush them later via another hci_dump_t implementation.
 *
 * Using this wrapper can be helpful in debugging issues that magically disappear when HCI logging is turned on.
 *
 * By logging first into a buffer, the critical steps should run mostly as before but still
 * provide an HCI log of the issue afterwards.
 *
 * Example Usage with 10kB buffer and flush after 100 ms inactivity
 *
 *   Existing code:
 *
 *     const hci_dump_t * hci_dump_impl = hci_dump_embedded_stdout_get_instance();
 *     hci_dump_init(hci_dump_impl);
 *
 *   Buffered version:
 *
 *     static byte_t hci_dump_buffer[10000] = {0};
 *
 *     const hci_dump_t * hci_dump_impl = hci_dump_embedded_stdout_get_instance();
 *     const hci_dump_t * hci_dump_buffered = hci_dump_buffered_init(hci_dump_impl,
 *                                    hci_dump_buffer,
 *                                    sizeof(hci_dump_buffer),
 *                                    100);
 *     hci_dump_init(hci_dump_buffered);
 *
 */

#ifndef HCI_DUMP_BUFFERED_H
#define HCI_DUMP_BUFFERED_H

#include <stdint.h>

#include "hci_dump.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief Configure buffered HCI dump instance
 * @param hci_dump_impl target hci_dump_t implementation used during flush
 * @param buffer caller-provided buffer used to store packet metadata and payload
 * @param buffer_size size of buffer in bytes
 * @param flush_timeout_ms maximum time in ms to keep buffered packets before flushing, 0 to disable timeout-based flushing
 *
 * @note If a single packet does not fit into the provided buffer, pending packets are flushed first and the packet is
 *       forwarded directly to hci_dump_impl.
 */
void hci_dump_buffered_init(const hci_dump_t * hci_dump_impl, uint8_t * buffer, uint32_t buffer_size, uint32_t flush_timeout_ms);

/**
 * @brief Flush buffered packets immediately
 */
void hci_dump_buffered_flush(void);

/**
 * @brief Get buffered HCI dump instance
 * @return hci_dump_impl
 */
const hci_dump_t * hci_dump_buffered_get_instance(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // HCI_DUMP_BUFFERED_H
