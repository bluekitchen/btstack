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

#define BTSTACK_FILE__ "btstack_linked_queue.c"

#include "btstack_linked_queue.h"
#include "btstack_debug.h"
#include <stdlib.h>

/**
 * @brief Tests if queue is empty
 * @return true if emtpy
 */
bool btstack_linked_queue_empty(btstack_linked_queue_t * queue){
    return queue->head == NULL;
}

/**
 * @brief Append item to queue
 * @param queue
 * @param item
 */
void btstack_linked_queue_enqueue(btstack_linked_queue_t * queue, btstack_linked_item_t * item){
    if (queue->head == NULL){
        // empty queue
        item->next = NULL;
        queue->head = item;
        queue->tail = item;
    } else {
        // non-empty
        item->next = NULL;
        queue->tail->next = item;
        queue->tail = item;
    }
}

/**
 * @brief Get first item from queue
 * @param queue
 * @return item or NULL if queue empty
 */
btstack_linked_item_t * btstack_linked_queue_first(btstack_linked_queue_t * queue){
    return queue->head;
}

/**
 * @brief Pop next item from queue
 * @param queue
 * @return item or NULL if queue empty
 */
btstack_linked_item_t * btstack_linked_queue_dequeue(btstack_linked_queue_t * queue){
    if (queue->head == NULL){
        return NULL;
    }
    btstack_linked_item_t * item = queue->head;
    queue->head = item->next;
    // reset tail if head becomes NULL
    if (queue->head == NULL){
        queue->tail = NULL;
    }
    return item;
}
