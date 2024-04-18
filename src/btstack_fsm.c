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

/**
 * @title Finite State Machine (FSM)
 *
 */
#define BTSTACK_FILE__ "btstack_fsm.c"

#include <stddef.h>

#include "btstack_fsm.h"

#include "btstack_config.h"
#include "btstack_debug.h"

static btstack_fsm_event_t const entry_evt = { BTSTACK_FSM_ENTRY_SIG };
static btstack_fsm_event_t const exit_evt = { BTSTACK_FSM_EXIT_SIG };

btstack_fsm_state_t btstack_fsm_transit(btstack_fsm_t *me, btstack_fsm_state_handler_t target) {
    me->state = target;
    return BTSTACK_FSM_TRAN_STATUS;
}

void btstack_fsm_constructor(btstack_fsm_t * const me, btstack_fsm_state_handler_t initial) {
    me->state = initial;
}

void btstack_fsm_init(btstack_fsm_t * const me, btstack_fsm_event_t const * const e) {
    btstack_assert(me->state != NULL);
    me->state(me, e);
    me->state(me, &entry_evt);
}

btstack_fsm_state_t btstack_fsm_dispatch(btstack_fsm_t * const me, btstack_fsm_event_t const * const e) {
    btstack_fsm_state_t status;
    btstack_fsm_state_handler_t prev_state = me->state;
    btstack_assert( me->state != NULL );

    status = me->state(me, e);

    if( status == BTSTACK_FSM_TRAN_STATUS ) {
        prev_state(me, &exit_evt);
        me->state(me, &entry_evt);
    }
    return status;
}

void btstack_fsm_dispatch_until(btstack_fsm_t * const me, btstack_fsm_event_t const * const e) {
    btstack_fsm_state_t status;
    do {
        status = btstack_fsm_dispatch( me, e );
    } while( status == BTSTACK_FSM_TRAN_STATUS );
}

