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
 *  btstack_run_loop_embedded.h
 *  Functionality special to the embedded run loop
 */

#ifndef BTSTACK_RUN_LOOP_EMBEDDED_H
#define BTSTACK_RUN_LOOP_EMBEDDED_H

#include "btstack_config.h"
#include "btstack_linked_list.h"
#include "btstack_run_loop.h"

#ifdef HAVE_POSIX_TIME
#include <sys/time.h>
#endif
#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif
	
/**
 * Provide btstack_run_loop_embedded instance 
 */
const btstack_run_loop_t * btstack_run_loop_embedded_get_instance(void);

// hack to fix HCI timer handling
#ifdef HAVE_EMBEDDED_TICK
/**
 * @brief Sets how many milliseconds has one tick.
 */
uint32_t btstack_run_loop_embedded_ticks_for_ms(uint32_t time_in_ms);
/**
 * @brief Queries the current time in ticks.
 */
uint32_t btstack_run_loop_embedded_get_ticks(void);
#endif

/**
 * @brief Sets an internal flag that is checked in the critical section just before entering sleep mode. Has to be called by the interrupt handler of a data source to signal the run loop that a new data is available.
 */
void btstack_run_loop_embedded_trigger(void);    
/**
 * @brief Execute run_loop once. It can be used to integrate BTstack's timer and data source processing into a foreign run loop (it is not recommended).
 */
void btstack_run_loop_embedded_execute_once(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // BTSTACK_RUN_LOOP_EMBEDDED_H
