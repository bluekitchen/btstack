/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "btstack_linked_list.c"

/*
 *  linked_list.c
 *
 *  Created by Matthias Ringwald on 7/13/09.
 */

#include "btstack_linked_list.h"
#include "btstack_debug.h"

/**
 * tests if list is empty
 */
bool  btstack_linked_list_empty(btstack_linked_list_t * list){
    return *list == (void *) 0;
}

/**
 * btstack_linked_list_get_last_item
 */
btstack_linked_item_t * btstack_linked_list_get_last_item(btstack_linked_list_t * list){        // <-- find the last item in the list
    btstack_linked_item_t *lastItem = NULL;
    btstack_linked_item_t *it;
    for (it = *list; it != NULL; it = it->next){
        if (it != NULL) {
            lastItem = it;
        }
    }
    return lastItem;
}


/**
 * btstack_linked_list_add
 */
bool btstack_linked_list_add(btstack_linked_list_t * list, btstack_linked_item_t *item){        // <-- add item to list
    // check if already in list
    btstack_linked_item_t *it;
    for (it = *list; it != NULL; it = it->next){
        if (it == item) {
            return false;
        }
    }
    // add first
    item->next = *list;
    *list = item;
    return true;
}

bool btstack_linked_list_add_tail(btstack_linked_list_t * list, btstack_linked_item_t *item){   // <-- add item to list as last element
    // check if already in list
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) list; it->next != NULL ; it = it->next){
        if (it->next == item) {
            return false;
        }
    }
    item->next = (btstack_linked_item_t*) 0;
    it->next = item;
    return true;
}

bool  btstack_linked_list_remove(btstack_linked_list_t * list, btstack_linked_item_t *item){    // <-- remove item from list
    if (!item) return false;
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) list; it != NULL; it = it->next){
        if (it->next == item){
            it->next =  item->next;
            return true;
        }
    }
    return false;
}

/**
 * @returns number of items in list
 */
 int btstack_linked_list_count(btstack_linked_list_t * list){
    btstack_linked_item_t *it;
    int counter = 0;
    for (it = (btstack_linked_item_t *) list; it->next != NULL; it = it->next) {
        counter++;
    }
    return counter; 
}

// get first element
btstack_linked_item_t * btstack_linked_list_get_first_item(btstack_linked_list_t * list){
    return * list;
}

// pop (get + remove) first element
btstack_linked_item_t * btstack_linked_list_pop(btstack_linked_list_t * list){
    btstack_linked_item_t * item = *list;
    if (!item) return NULL;
    *list = item->next;
    return item;
}


//
// Linked List Iterator implementation
//

void btstack_linked_list_iterator_init(btstack_linked_list_iterator_t * it, btstack_linked_list_t * head){
    it->advance_on_next = 0;
    it->prev = (btstack_linked_item_t*) head;
    it->curr = * head;
}

bool btstack_linked_list_iterator_has_next(btstack_linked_list_iterator_t * it){
    // log_info("btstack_linked_list_iterator_has_next: advance on next %u, it->prev %p, it->curr %p", it->advance_on_next, it->prev, it->curr);
    if (!it->advance_on_next){
        return it->curr != NULL;
    }
    if (it->prev->next != it->curr){
        // current item has been removed
        return it->prev->next != NULL;
    }
    // current items has not been removed
    return it->curr->next != NULL;
}

btstack_linked_item_t * btstack_linked_list_iterator_next(btstack_linked_list_iterator_t * it){
    if (it->advance_on_next){
        if (it->prev->next == it->curr){
            it->prev = it->curr;
            it->curr = it->curr->next;
        } else {
            // curr was removed from the list, set it again but don't advance prev
            it->curr = it->prev->next;
        }
    } else {
        it->advance_on_next = 1;
    }
    return it->curr;
}

void btstack_linked_list_iterator_remove(btstack_linked_list_iterator_t * it){
    btstack_assert(it->prev->next == it->curr);
    it->curr = it->curr->next;
    it->prev->next = it->curr;
    it->advance_on_next = 0;
}
