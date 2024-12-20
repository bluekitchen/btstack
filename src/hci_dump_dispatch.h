/*
 * Copyright (C) 2024 BlueKitchen GmbH
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
 *  @brief HCI Dump Implementation that allows to use a set of HCI Dump Implementations
 */

#ifndef HCI_DUMP_DISPATCH_H
#define HCI_DUMP_DISPATCH_H

#include "hci_dump.h"
#include "btstack_linked_list.h"

#if defined __cplusplus
extern "C" {
#endif

// memory provided by caller to keep hci_dump_t implementation in a list
typedef struct {
    btstack_linked_item_t item;
    const hci_dump_t * hci_dump;
} hci_dump_dispatch_item_t;

/**
 * @brief Init HCI Dump Dispatch
 */
void hci_dump_dispatch_init(void);

/**
 * @brief Register HCI Dump (hci_dump_t *) implementation with dispatcher
 * @param list_item
 * @param dump
 */
void hci_dump_dispatch_register(hci_dump_dispatch_item_t *list_item, const hci_dump_t *dump);

/**
 * @brief Unregister HCI Dump (hci_dump_t *) implementation from dispatcher
 * @param dump
 */
void hci_dump_dispatch_unregister(hci_dump_t *dump);

/**
 * @brief De-Init HCI Dump Dispatch
 */
void hci_dump_dispatch_deinit(void);

/**
 * @brief Get HCI Dump Dispatch instance to be used with hci_dump
 * @return
 */
const hci_dump_t * hci_dump_dispatch_instance(void);

#if defined __cplusplus
}
#endif
#endif // HCI_DUMP_DISPATCH_H
