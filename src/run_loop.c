/*
 *  run_loop.c
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#include "run_loop.h"
#include "linked_list.h"

#include <sys/select.h>
#include <stdlib.h>

// the run loop
static linked_list_t the_run_loop = NULL;

/**
 * Add data_source to run_loop
 */
void run_loop_add(data_source_t *ds){
    linked_list_add(&the_run_loop, (linked_item_t *) ds);
}

/**
 * Remove data_source from run loop
 */
int run_loop_remove(data_source_t *ds){
    return linked_list_remove(&the_run_loop, (linked_item_t *) ds);
}

/**
 * Execute run_loop
 */
void run_loop_execute() {
    fd_set descriptors;
    data_source_t *ds;
    while (1) {
        // collect FDs
        FD_ZERO(&descriptors);
        int highest_fd = 0;
        for (ds = (data_source_t *) the_run_loop; ds ; ds = (data_source_t *) ds->item.next){
            if (ds->fd) {
                FD_SET(ds->fd, &descriptors);
                if (ds->fd > highest_fd) {
                    highest_fd = ds->fd;
                }
            }
        }
        // wait for ready FDs
        select( highest_fd+1 , &descriptors, NULL, NULL, NULL);

        // process input
        data_source_t *next;
        for (ds = (data_source_t *) the_run_loop; ds != NULL ; ds = next){
            next = (data_source_t *) ds->item.next; // cache pointer to next data_source to allow data source to remove itself
            if (FD_ISSET(ds->fd, &descriptors)) {
                ds->process(ds, FD_ISSET(ds->fd, &descriptors));
            }
        }
    }
}



