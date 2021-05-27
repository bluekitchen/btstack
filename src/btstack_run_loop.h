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

/**
 * @title Run Loop
 *
 */

#ifndef btstack_run_loop_H
#define btstack_run_loop_H

#include "btstack_config.h"

#include "btstack_bool.h"
#include "btstack_linked_list.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif
	
/**
 * Callback types for run loop data sources
 */
typedef enum {
	DATA_SOURCE_CALLBACK_POLL  = 1 << 0,
	DATA_SOURCE_CALLBACK_READ  = 1 << 1,
	DATA_SOURCE_CALLBACK_WRITE = 1 << 2,
} btstack_data_source_callback_type_t;

typedef struct btstack_data_source {
	// linked item
    btstack_linked_item_t item;

    // item to watch in run loop
    union {
	    // file descriptor for posix systems
	    int  fd;
    	// handle on windows
    	void * handle;	
    } source;

    // callback to call for enabled callback types
    void  (*process)(struct btstack_data_source *ds, btstack_data_source_callback_type_t callback_type);

    // flags storing enabled callback types
    uint16_t flags;

} btstack_data_source_t;

typedef struct btstack_timer_source {
    btstack_linked_item_t item; 
    // timeout in system ticks (HAVE_EMBEDDED_TICK) or milliseconds (HAVE_EMBEDDED_TIME_MS)
    uint32_t timeout;
    // will be called when timer fired
    void  (*process)(struct btstack_timer_source *ts); 
    void * context;
} btstack_timer_source_t;

typedef struct btstack_run_loop {
	void (*init)(void);
	void (*add_data_source)(btstack_data_source_t * data_source);
	bool (*remove_data_source)(btstack_data_source_t * data_source);
	void (*enable_data_source_callbacks)(btstack_data_source_t * data_source, uint16_t callbacks);
	void (*disable_data_source_callbacks)(btstack_data_source_t * data_source, uint16_t callbacks);
	void (*set_timer)(btstack_timer_source_t * timer, uint32_t timeout_in_ms);
	void (*add_timer)(btstack_timer_source_t *timer);
	bool  (*remove_timer)(btstack_timer_source_t *timer);
	void (*execute)(void);
	void (*dump_timer)(void);
	uint32_t (*get_time_ms)(void);
} btstack_run_loop_t;


/*
 *  BTstack Run Loop Base Implementation
 *  Portable implementation of timer and data source management as base for platform specific implementations
 */

// private data (access only by run loop implementations)
extern btstack_linked_list_t btstack_run_loop_base_timers;
extern btstack_linked_list_t btstack_run_loop_base_data_sources;

/**
 * @brief Init
 */
void btstack_run_loop_base_init(void);

/**
 * @brief Add timer source.
 * @param timer to add
 */
void btstack_run_loop_base_add_timer(btstack_timer_source_t * timer);

/**
 * @brief Remove timer source.
 * @param timer to remove
 * @returns true if timer was removed
 */
bool  btstack_run_loop_base_remove_timer(btstack_timer_source_t * timer);

/**
 * @brief Process timers: remove expired timers from list and call their process function
 * @param now
 */
void  btstack_run_loop_base_process_timers(uint32_t now);

/**
 * @brief Dump list of timers via log_info
 */
void btstack_run_loop_base_dump_timer(void);

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
 * @returns true if data srouce was removed
 */
bool btstack_run_loop_base_remove_data_source(btstack_data_source_t * data_source);

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

/**
 * @brief Poll data sources. It calls the procss function for all data sources where DATA_SOURCE_CALLBACK_POLL is set
 */
void btstack_run_loop_base_poll_data_sources(void);


/* API_START */

/**
 * @brief Init main run loop. Must be called before any other run loop call.
 *  
 * Use btstack_run_loop_$(btstack_run_loop_TYPE)_get_instance() from btstack_run_loop_$(btstack_run_loop_TYPE).h to get instance 
 */
void btstack_run_loop_init(const btstack_run_loop_t * run_loop);

/**
 * @brief Set timer based on current time in milliseconds.
 */
void btstack_run_loop_set_timer(btstack_timer_source_t * ts, uint32_t timeout_in_ms);

/**
 * @brief Set callback that will be executed when timer expires.
 */
void btstack_run_loop_set_timer_handler(btstack_timer_source_t * ts, void (*process)(btstack_timer_source_t *_ts));

/**
 * @brief Set context for this timer
 */
void btstack_run_loop_set_timer_context(btstack_timer_source_t * ts, void * context);

/**
 * @brief Get context for this timer
 */
void * btstack_run_loop_get_timer_context(btstack_timer_source_t * ts);

/**
 * @brief Add timer source.
 */
void btstack_run_loop_add_timer(btstack_timer_source_t * timer); 

/**
 * @brief Remove timer source.
 */
int  btstack_run_loop_remove_timer(btstack_timer_source_t * timer);

/**
 * @brief Get current time in ms
 * @note 32-bit ms counter will overflow after approx. 52 days
 */
uint32_t btstack_run_loop_get_time_ms(void);

/**
 * @brief Dump timers using log_info
 */
void btstack_run_loop_timer_dump(void);


/**
 * @brief Set data source callback.
 */
void btstack_run_loop_set_data_source_handler(btstack_data_source_t * data_source, void (*process)(btstack_data_source_t *_ds, btstack_data_source_callback_type_t callback_type));

/**
 * @brief Set data source file descriptor. 
 * @param data_source
 * @param fd file descriptor
 * @note No effect if port doensn't have file descriptors
 */
void btstack_run_loop_set_data_source_fd(btstack_data_source_t * data_source, int fd);

/**
 * @brief Get data source file descriptor. 
 * @param data_source
 */
int btstack_run_loop_get_data_source_fd(btstack_data_source_t * data_source);

/**
 * @brief Set data source file descriptor. 
 * @param data_source
 * @param handle
 * @note No effect if port doensn't have file descriptors
 */
void btstack_run_loop_set_data_source_handle(btstack_data_source_t * data_source, void * handle);

/**
 * @brief Get data source file descriptor. 
 * @param data_source
 */
void * btstack_run_loop_get_data_source_handle(btstack_data_source_t * data_source);

/**
 * @brief Enable callbacks for a data source
 * @param data_source to remove
 * @param callback types to enable
 */
void btstack_run_loop_enable_data_source_callbacks(btstack_data_source_t * data_source, uint16_t callbacks);

/**
 * @brief Enable callbacks for a data source
 * @param data_source to remove
 * @param callback types to disable
 */
void btstack_run_loop_disable_data_source_callbacks(btstack_data_source_t * data_source, uint16_t callbacks);

/**
 * @brief Add data source to run loop
 * @param data_source to add
 */
void btstack_run_loop_add_data_source(btstack_data_source_t * data_source);

/**
 * @brief Remove data source from run loop
 * @param data_source to remove
 */
int btstack_run_loop_remove_data_source(btstack_data_source_t * data_source);

/**
 * @brief Execute configured run loop. This function does not return.
 */
void btstack_run_loop_execute(void);

/**
 * @brief De-Init Run Loop
 */
void btstack_run_loop_deinit(void);

/* API_END */



#if defined __cplusplus
}
#endif

#endif // btstack_run_loop_H
