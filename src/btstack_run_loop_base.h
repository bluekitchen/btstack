/*
 * Copyright (C) 2019 BlueKitchen GmbH
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
 *  btstack_run_loop_base.h
 *
 *  Portable implementation of timer and data source managment as base for platform specific implementations
 */

#ifndef BTSTACK_RUN_LOOP_BASE_H
#define BTSTACK_RUN_LOOP_BASE_H

#include "btstack_run_loop.h"

#include "btstack_linked_list.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

// private data (access only by run loop implementations)
extern btstack_linked_list_t btstack_run_loop_base_timers;
extern btstack_linked_list_t btstack_run_loop_base_data_sources;
	
/**
 * @brief Init
 */
void btstack_run_loop_base_init(void);

/**
 * @brief Add timer source.
 */
void btstack_run_loop_base_add_timer(btstack_timer_source_t * timer); 

/**
 * @brief Remove timer source.
 */
int  btstack_run_loop_base_remove_timer(btstack_timer_source_t * timer);

/**
 * @brief Process timers: remove expired timers from list and call their process function
 * @param now
 */
void  btstack_run_loop_base_process_timers(uint32_t now);

/**
 * @brief Get time until first timer fires
 * @returns -1 if no timers, time until next timeout otherwise
 */
int32_t btstack_run_loop_base_get_time_until_timeout(uint32_t now);

/**
 * @brief Add data source to run loop
 * @param data_source to add
 */
void btstack_run_loop_base_add_data_source(btstack_data_source_t * data_source);

/**
 * @brief Remove data source from run loop
 * @param data_source to remove
 */
int btstack_run_loop_base_remove_data_source(btstack_data_source_t * data_source);

/**
 * @brief Enable callbacks for a data source
 * @param data_source to remove
 * @param callback types to enable
 */
void btstack_run_loop_base_enable_data_source_callbacks(btstack_data_source_t * data_source, uint16_t callbacks);

/**
 * @brief Enable callbacks for a data source
 * @param data_source to remove
 * @param callback types to disable
 */
void btstack_run_loop_base_disable_data_source_callbacks(btstack_data_source_t * data_source, uint16_t callbacks);

#if defined __cplusplus
}
#endif

#endif // BTSTACK_RUN_LOOP_BASE_H
