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

/**
 * @title Linked List
 *
 */

#ifndef BTSTACK_LINKED_LIST_H
#define BTSTACK_LINKED_LIST_H

#include "btstack_bool.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */
	
typedef struct btstack_linked_item {
    struct btstack_linked_item *next; // <-- next element in list, or NULL
} btstack_linked_item_t;

typedef btstack_linked_item_t * btstack_linked_list_t;

typedef struct {
	int advance_on_next;
    btstack_linked_item_t * prev;	// points to the item before the current one
    btstack_linked_item_t * curr;	// points to the current item (to detect item removal)
} btstack_linked_list_iterator_t;


/**
 * @brief Test if list is empty.
 * @param list
 * @returns true if list is empty
 */
bool btstack_linked_list_empty(btstack_linked_list_t * list);

/**
 * @brief Add item to list as first element.
 * @param list
 * @param item
 * @returns true if item was added, false if item already in list
 */
bool btstack_linked_list_add(btstack_linked_list_t * list, btstack_linked_item_t *item);

/**
 * @brief Add item to list as last element.
 * @param list
 * @param item
 * @returns true if item was added, false if item already in list
 */
bool btstack_linked_list_add_tail(btstack_linked_list_t * list, btstack_linked_item_t *item);

/**
 * @brief Pop (get + remove) first element.
 * @param list
 * @returns first element or NULL if list is empty
 */
btstack_linked_item_t * btstack_linked_list_pop(btstack_linked_list_t * list);

/**
 * @brief Remove item from list
 * @param list
 * @param item
 * @returns true if item was removed, false if it is no't in list
 */
bool btstack_linked_list_remove(btstack_linked_list_t * list, btstack_linked_item_t *item);

/**
 * @brief Get first element.
 * @param list
 * @returns first element or NULL if list is empty
 */
btstack_linked_item_t * btstack_linked_list_get_first_item(btstack_linked_list_t * list);

/**
 * @brief Get last element.
 * @param list
 * @returns first element or NULL if list is empty
 */
btstack_linked_item_t * btstack_linked_list_get_last_item(btstack_linked_list_t * list);   

/**
 * @brief Counts number of items in list
 * @returns number of items in list
 */
int btstack_linked_list_count(btstack_linked_list_t * list);



/**
 * @brief Initialize Linked List Iterator
 * @note robust against removal of current element by btstack_linked_list_remove
 * @param it iterator context
 * @param list
 */
void btstack_linked_list_iterator_init(btstack_linked_list_iterator_t * it, btstack_linked_list_t * list);

/**
 * @brief Has next element
 * @param it iterator context
 * @returns true if next element is available
 */
bool btstack_linked_list_iterator_has_next(btstack_linked_list_iterator_t * it);

/**
 * @brief Get next list eleemnt
 * @param it iterator context
 * @returns list element
 */
btstack_linked_item_t * btstack_linked_list_iterator_next(btstack_linked_list_iterator_t * it);

/**
 * @brief Remove current list element from list
 * @param it iterator context
 */
void btstack_linked_list_iterator_remove(btstack_linked_list_iterator_t * it);

/* API_END */

void test_linked_list(void);


#if defined __cplusplus
}
#endif

#endif // BTSTACK_LINKED_LIST_H
