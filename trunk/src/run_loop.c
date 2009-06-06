/*
 *  run_loop.c
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#include "run_loop.h"
#include <sys/select.h>
#include <stdlib.h>

// the run loop
static data_source_t * the_run_loop = NULL;

/**
 * Add data_source to run_loop
 */
void run_loop_add(data_source_t *ds){
    // check if already in list
    data_source_t *it;
    for (it = the_run_loop; it ; it = it->next){
        if (it == ds) {
            return;
        }
    }
    // add first
    ds->next = the_run_loop;
    the_run_loop = ds;
}

/**
 * Remove data_source from run loop
 *
 * @note: assumes that data_source_t.next is first element in data_source
 */
int run_loop_remove(data_source_t *ds){
    data_source_t *it;
    for (it = (data_source_t *) &the_run_loop; it ; it = it->next){
        if (it->next == ds){
            it->next =  ds->next;
            return 0;
        }
    }
    return -1;
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
        for (ds = the_run_loop; ds ; ds = ds->next){
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
        for (ds = the_run_loop; ds != NULL ; ds = next){
            next = ds->next; // cache pointer to next data_source to allow data source to remove itself
            if (FD_ISSET(ds->fd, &descriptors)) {
                ds->process(ds, FD_ISSET(ds->fd, &descriptors));
            }
        }
    }
}



