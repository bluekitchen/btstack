/*
 * Copyright (C) 2020 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hal_timer.c"

/*
 *  hal_timer.c
 *  HAL for 32.768 kHz low power timer with 16 bit resolution
 */

#include "hal_timer.h"

#include "stm32l4xx.h"

// access to timers
extern LPTIM_HandleTypeDef hlptim1;

static void (*hal_timer_callback)(void);

void HAL_LPTIM_CompareMatchCallback(LPTIM_HandleTypeDef *hlptim){
    UNUSED(hlptim);
    (*hal_timer_callback)();
}

void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim){
    UNUSED(hlptim);
    static uint32_t time_seconds = 0;
    time_seconds += 2;
    // printf("Time: %4u s\n", time_seconds);
}

void hal_timer_init(void){
}

void hal_timer_set_callback(void (*callback)(void)){
    hal_timer_callback = callback;
}

uint16_t hal_timer_get_ticks(void){
    return HAL_LPTIM_ReadCounter(&hlptim1);
}

void hal_timer_stop(void){
    __HAL_LPTIM_DISABLE_IT(&hlptim1, LPTIM_IT_CMPM);
    __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_IT_CMPM);
}

void hal_timer_start(uint16_t timeout_ticks){
    __HAL_LPTIM_COMPARE_SET(&hlptim1, timeout_ticks);
    __HAL_LPTIM_ENABLE_IT(&hlptim1, LPTIM_IT_CMPM);
}