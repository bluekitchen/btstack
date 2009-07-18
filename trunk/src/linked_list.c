/*
 *  linked_list.c
 *
 *  Created by Matthias Ringwald on 7/13/09.
 */

#include "linked_list.h"

/**
 * linked_list_add
 */
void linked_list_add(linked_list_t * list, linked_item_t *item){        // <-- add item to list
    // check if already in list
    linked_item_t *it;
    for (it = *list; it ; it = it->next){
        if (it == item) {
            return;
        }
    }
    // add first
    item->next = *list;
    *list = item;
}

/**
 * Remove data_source from run loop
 *
 * @note: assumes that data_source_t.next is first element in data_source
 */
int  linked_list_remove(linked_list_t * list, linked_item_t *item){    // <-- remove item from list
    linked_item_t *it;
    for (it = (linked_item_t *) list; it ; it = it->next){
        if (it->next == item){
            it->next =  item->next;
            return 0;
        }
    }
    return -1;
}

void linked_item_set_user(linked_item_t *item, void *user_data){
    item->next = (void *) 0;
    item->user_data = user_data;
};

void * linked_item_get_user(linked_item_t *item) {
    return item->user_data;
};

#include <strings.h>
void test_linked_list(){
    linked_list_t testList = 0;
    linked_item_t itemA;
    linked_item_t itemB;
    linked_item_set_user(&itemA, (void *) 0);
    linked_item_set_user(&itemB, (void *) 0);
    linked_list_add(&testList, &itemA);
    linked_list_add(&testList, &itemB);
    linked_list_remove(&testList, &itemB);
}