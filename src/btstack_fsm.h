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

#ifndef BTSTACK_FSM_H
#define BTSTACK_FSM_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef uint16_t btstack_fsm_signal_t;
enum btstack_fsm_reserved_signals_e {
    BTSTACK_FSM_EMPTY_SIG,
    BTSTACK_FSM_INIT_SIG,
    BTSTACK_FSM_ENTRY_SIG,
    BTSTACK_FSM_EXIT_SIG,
    BTSTACK_FSM_USER_SIG
};

typedef struct {
    btstack_fsm_signal_t sig;
} btstack_fsm_event_t;

typedef struct btstack_fsm_s btstack_fsm_t;

typedef enum {
    BTSTACK_FSM_TRAN_STATUS,
    BTSTACK_FSM_SUPER_STATUS,
    BTSTACK_FSM_HANDLED_STATUS,
    BTSTACK_FSM_UNHANDLED_STATUS,
    BTSTACK_FSM_IGNORED_STATUS
} btstack_fsm_state_t;

typedef btstack_fsm_state_t (*btstack_fsm_state_handler_t)(btstack_fsm_t * const me, btstack_fsm_event_t const * const e);

struct btstack_fsm_s {
    btstack_fsm_state_handler_t state;
};

/* API_START */

/*
 * @brief Request the transition from the current state to the given new state
 * @param me the current state machine
 * @param target the new state to transit to
 * @result transition status
 */
btstack_fsm_state_t btstack_fsm_transit(btstack_fsm_t * const me, btstack_fsm_state_handler_t target);

/*
 * @brief Constructs a new state hierarchical machine machine, with storage for maximum hierarchy depth.
 * @param me the current state machine
 * @param initial the initial state
 */
void btstack_fsm_constructor(btstack_fsm_t * const me, btstack_fsm_state_handler_t initial);

/*
 * @brief Takes the initial transition of the state machine and sending it a BTSTACK_HSM_INIT_SIG
 * @param me the current state machine
 * @param e event
 */
void btstack_fsm_init(btstack_fsm_t * const me, btstack_fsm_event_t const * const e);

/*
 * @brief Dispatches the given event to the state machine, if a transition is requested, leave the old states and enter the new on.
 *        Handling entering/exiting all states on the way.
 * @param me the current state machine
 * @param e event
 */
btstack_fsm_state_t btstack_fsm_dispatch(btstack_fsm_t * const me, btstack_fsm_event_t const * const e);

/*
 * @brief Dispatches the given event to the state machine until it was handled.
 * @param me the current state machine
 * @param e event
 */
void btstack_fsm_dispatch_until(btstack_fsm_t * const me, btstack_fsm_event_t const * const e);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

