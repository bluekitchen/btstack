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

#define BTSTACK_FILE__ "rfcomm_service_db_memory.c"

#include <string.h>
#include <stdlib.h>

#include "rfcomm_service_db.h"
#include "btstack_memory.h"
#include "btstack_debug.h"

#include "btstack_util.h"
#include "btstack_linked_list.h"

// This lists should be only accessed by tests.
static btstack_linked_list_t db_mem_services = NULL;
#define MAX_NAME_LEN 30
typedef struct {
    char service_name[MAX_NAME_LEN+1];
    int  channel;
} db_mem_service_t;

// MARK: PERSISTENT RFCOMM CHANNEL ALLOCATION
uint8_t rfcomm_service_db_channel_for_service(const char *serviceName){
    
    btstack_linked_item_t *it;
    db_mem_service_t * item;
    uint8_t max_channel = 1;

    for (it = (btstack_linked_item_t *) db_mem_services; it ; it = it->next){
        item = (db_mem_service_t *) it;
        if (strncmp(item->service_name, serviceName, MAX_NAME_LEN) == 0) {
            // Match found
            return item->channel;
        }

        // TODO prevent overflow
        if (item->channel >= max_channel) max_channel = item->channel + 1;
    }

    // Allocate new persistant channel
    db_mem_service_t * newItem = malloc(sizeof(db_mem_service_t));

    if (!newItem) return 0;
    
    memset(newItem, 0, sizeof(db_mem_service_t));
    strncpy(newItem->service_name, serviceName, MAX_NAME_LEN);
    newItem->service_name[MAX_NAME_LEN] = 0;
    newItem->channel = max_channel;
    btstack_linked_list_add(&db_mem_services, (btstack_linked_item_t *) newItem);
    return max_channel;
}
