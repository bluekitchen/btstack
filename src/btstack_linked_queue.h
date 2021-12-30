/*
 * Copyright (C) 2019 BlueKitchen GmbH
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
 * @title Linked Queue
 *
 */

#ifndef BTSTACK_LINKED_QUEUE_H
#define BTSTACK_LINKED_QUEUE_H

#include "btstack_bool.h"
#include "btstack_linked_list.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef struct {
    btstack_linked_item_t * head;
    btstack_linked_item_t * tail;
} btstack_linked_queue_t;

/**
 * @brief Tests if queue is empty
 * @return true if emtpy
 */
bool btstack_linked_queue_empty(btstack_linked_queue_t * queue);

/**
 * @brief Append item to queue
 * @param queue
 * @param item
 */
void btstack_linked_queue_enqueue(btstack_linked_queue_t * queue, btstack_linked_item_t * item);

/**
 * @brief Pop next item from queue
 * @param queue
 * @return item or NULL if queue empty
 */
btstack_linked_item_t * btstack_linked_queue_dequeue(btstack_linked_queue_t * queue);

/**
 * @brief Get first item from queue
 * @param queue
 * @return item or NULL if queue empty
 */
btstack_linked_item_t * btstack_linked_queue_first(btstack_linked_queue_t * queue);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // BTSTACK_LINKED_QUEUE_H
