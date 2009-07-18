/*
 *  linked_list.h
 *
 *  Created by Matthias Ringwald on 7/13/09.
 */

#pragma once

typedef struct linked_item {
    struct linked_item *next; // <-- next element in list, or NULL
    void *user_data;          // <-- pointer to struct base
} linked_item_t;

typedef linked_item_t * linked_list_t;

void linked_item_set_user(linked_item_t *item, void *user_data);        // <-- set user data
void * linked_item_get_user(linked_item_t *item);                       // <-- get user data
void linked_list_add(linked_list_t * list, linked_item_t *item);        // <-- add item to list
int  linked_list_remove(linked_list_t * list, linked_item_t *item);     // <-- remove item from list

void test_linked_list();