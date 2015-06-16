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
 *  run_loop_private.h
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#ifndef __RUN_LOOP_PRIVATE_H
#define __RUN_LOOP_PRIVATE_H

#include <btstack/run_loop.h>

#ifdef HAVE_TIME
#include <sys/time.h>
#endif

#if defined __cplusplus
extern "C" {
#endif

// 
void run_loop_timer_dump(void);

// internal use only
typedef struct {
	void (*init)(void);
	void (*add_data_source)(data_source_t *dataSource);
	int  (*remove_data_source)(data_source_t *dataSource);
	void (*set_timer)(timer_source_t * timer, uint32_t timeout_in_ms);
	void (*add_timer)(timer_source_t *timer);
	int  (*remove_timer)(timer_source_t *timer); 
	void (*execute)(void);
	void (*dump_timer)(void);
	uint32_t (*get_time_ms)(void);
} run_loop_t;

#if defined __cplusplus
}
#endif

#endif // __RUN_LOOP_PRIVATE_H
