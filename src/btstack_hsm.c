/*
 * Copyright (C) 2024 BlueKitchen GmbH
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
#define BTSTACK_FILE__ "btstack_hsm.c"

#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "btstack_config.h"
#include "btstack_debug.h"

#include "btstack_hsm.h"

btstack_hsm_state_t btstack_hsm_transit(btstack_hsm_t * const me, btstack_hsm_state_handler_t const target) {
    me->temp = target;
    return BTSTACK_HSM_TRAN_STATUS;
}

btstack_hsm_state_t btstack_hsm_super(btstack_hsm_t * const me, btstack_hsm_state_handler_t const target) {
    me->temp = target;
    return BTSTACK_HSM_SUPER_STATUS;
}

btstack_hsm_state_t btstack_hsm_top(btstack_hsm_t * const me, btstack_hsm_event_t const * const e) {
    UNUSED(me);
    UNUSED(e);
    return BTSTACK_HSM_IGNORED_STATUS;
}

void btstack_hsm_constructor(btstack_hsm_t * const me, btstack_hsm_state_handler_t initial, btstack_hsm_state_handler_t path[], int8_t depth) {
    me->state = btstack_hsm_top;
    me->temp = initial;
    me->path = path;
    me->depth = depth;
}

static btstack_hsm_state_t btstack_hsm_get_super( btstack_hsm_t * const me, btstack_hsm_state_handler_t const handler) {
    // empty event to trigger default state action, a.k. the super state
    static btstack_hsm_event_t const empty_evt = { BTSTACK_HSM_EMPTY_SIG };
    return handler( me, &empty_evt );
}

static btstack_hsm_event_t const entry_evt = { BTSTACK_HSM_ENTRY_SIG };
static btstack_hsm_event_t const exit_evt = { BTSTACK_HSM_EXIT_SIG };

void btstack_hsm_init(btstack_hsm_t * const me, btstack_hsm_event_t const * const e) {
    btstack_assert(me->state != NULL);
    btstack_assert(me->temp != NULL);
    btstack_hsm_state_handler_t target = me->state;
    btstack_hsm_state_t status = me->temp(me, e);
    btstack_assert( status == BTSTACK_HSM_TRAN_STATUS );
    static btstack_hsm_event_t const init_evt = { BTSTACK_HSM_INIT_SIG };

    int_fast8_t level;
    btstack_hsm_state_handler_t *root_path = me->path;
    memset(root_path, 0, sizeof(btstack_hsm_state_handler_t)*me->depth);

    do {
        level = 0;
        btstack_hsm_state_handler_t current = me->temp;
        for(; current != target; current=me->temp, level++ ) {
            root_path[level] = current;
            btstack_hsm_get_super( me, current );
        }
        for(; level>0;) {
            root_path[--level]( me, &entry_evt );
        }
        target = root_path[0];
        status = target( me, &init_evt );
    } while (status == BTSTACK_HSM_TRAN_STATUS);
    btstack_assert( status != BTSTACK_HSM_TRAN_STATUS );
    me->state = target;
}

static void btstack_hsm_handler_super_cache( btstack_hsm_t * const me, btstack_hsm_state_handler_t cache[], int idx, btstack_hsm_state_handler_t handler ) {
    if( cache[idx] == NULL ) {
        cache[idx] = handler;
        btstack_hsm_get_super(me, handler);
    } else {
        me->temp = cache[idx];
    }
}

btstack_hsm_state_t btstack_hsm_dispatch(btstack_hsm_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    btstack_hsm_state_handler_t current = me->state;
    do {
        status = current(me, e);
        // if the state doesn't handle the event try at the super state too
        if( status == BTSTACK_HSM_UNHANDLED_STATUS ) {
            status = btstack_hsm_get_super( me, current );
        }
        current = me->temp;
    } while( status == BTSTACK_HSM_SUPER_STATUS );

    // if we don't switch states we are done now
    if( status != BTSTACK_HSM_TRAN_STATUS ) {
        return status;
    }
    btstack_hsm_state_handler_t source = me->state;
    btstack_hsm_state_handler_t target = me->temp;
    btstack_hsm_state_handler_t *root_path = me->path;
    memset(root_path, 0, sizeof(btstack_hsm_state_handler_t)*me->depth);

    // why should that be possible/necessary?
    btstack_assert( source != target );

    // handle entry/exit edges
    int_fast8_t level = 0;
    bool lca_found = false;
    // calculates the lowest common ancestor of the state graph
    for(; source != btstack_hsm_top; source=me->temp) {
        level = 0;
        for(current=target; current != btstack_hsm_top; current=me->temp, ++level ) {
            if( current == source ) {
                lca_found = true;
                break;
            }

            btstack_hsm_handler_super_cache( me, root_path, level, current );
        }
        if( lca_found == true ) {
            break;
        }
        source( me, &exit_evt );
        btstack_hsm_get_super( me, source );
    }

    for(level--; level > 0; ) {
        root_path[--level]( me, &entry_evt );
    }
    me->state = target;
    return status;
}

