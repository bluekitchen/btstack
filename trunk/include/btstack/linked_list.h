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

/*
 *  linked_list.h
 *
 *  Created by Matthias Ringwald on 7/13/09.
 */

#ifndef __LINKED_LIST_H
#define __LINKED_LIST_H

#if defined __cplusplus
extern "C" {
#endif
	
typedef struct linked_item {
    struct linked_item *next; // <-- next element in list, or NULL
    void *user_data;          // <-- pointer to struct base
} linked_item_t;

typedef linked_item_t * linked_list_t;

typedef struct {
	int advance_on_next;
    linked_item_t * prev;	// points to the item before the current one
    linked_item_t * curr;	// points to the current item (to detect item removal)
} linked_list_iterator_t;


void            linked_item_set_user(linked_item_t *item, void *user_data);        // <-- set user data
void *          linked_item_get_user(linked_item_t *item);                         // <-- get user data
int             linked_list_empty(linked_list_t * list);
void            linked_list_add(linked_list_t * list, linked_item_t *item);        // <-- add item to list as first element
void            linked_list_add_tail(linked_list_t * list, linked_item_t *item);   // <-- add item to list as last element
int             linked_list_remove(linked_list_t * list, linked_item_t *item);     // <-- remove item from list
linked_item_t * linked_list_get_last_item(linked_list_t * list);                   // <-- find the last item in the list

//
// iterator for linked lists. alloes to remove current element. also robust against removal of current element by linked_list_remove
//
void            linked_list_iterator_init(linked_list_iterator_t * it, linked_list_t * list);
int             linked_list_iterator_has_next(linked_list_iterator_t * it);
linked_item_t * linked_list_iterator_next(linked_list_iterator_t * it);
void            linked_list_iterator_remove(linked_list_iterator_t * it);

void test_linked_list(void);

#if defined __cplusplus
}
#endif

#endif // __LINKED_LIST_H
