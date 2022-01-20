/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

/*
 *  btstack_run_loop_freertos.c
 *
 *  Run loop on dedicated thread on FreeRTOS
 *  The run loop is triggered from other task/ISRs either via Event Groups
 *  or Task Notifications if HAVE_FREERTOS_TASK_NOTIFICATIONS is defined
 */

#define BTSTACK_FILE__ "btstack_run_loop_freertos.c"

#include <stddef.h> // NULL

#include "btstack_run_loop_freertos.h"

#include "btstack_linked_list.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "hal_time_ms.h"

// some SDKs, e.g. esp-idf, place FreeRTOS headers into an 'freertos' folder to avoid name collisions (e.g. list.h, queue.h, ..)
// wih this flag, the headers are properly found

#ifdef HAVE_FREERTOS_INCLUDE_PREFIX
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#else
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "semphr.h"
#endif

typedef struct function_call {
    void (*fn)(void * arg);
    void * arg;
} function_call_t;

// pick allocation style, prefer static
#if( configSUPPORT_STATIC_ALLOCATION == 1 )
#define USE_STATIC_ALLOC
#elif( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
// ok, nothing to do
#else
#error "Either configSUPPORT_STATIC_ALLOCATION or configSUPPORT_DYNAMIC_ALLOCATION in FreeRTOSConfig.h must be 1"
#endif

// queue to receive events: up to 2 calls from transport, rest for app
#define RUN_LOOP_QUEUE_LENGTH 20
#define RUN_LOOP_QUEUE_ITEM_SIZE sizeof(function_call_t)

#ifdef USE_STATIC_ALLOC
static StaticQueue_t btstack_run_loop_queue_object;
static uint8_t btstack_run_loop_queue_storage[ RUN_LOOP_QUEUE_LENGTH * RUN_LOOP_QUEUE_ITEM_SIZE ];
static StaticSemaphore_t btstack_run_loop_callback_mutex_object;
#endif

static QueueHandle_t        btstack_run_loop_queue;
static TaskHandle_t         btstack_run_loop_task;
static SemaphoreHandle_t    btstack_run_loop_callbacks_mutex;

#ifndef HAVE_FREERTOS_TASK_NOTIFICATIONS
static EventGroupHandle_t   btstack_run_loop_event_group;
#endif

// bit 0 event group reserved to wakeup run loop
#define EVENT_GROUP_FLAG_RUN_LOOP 1

// the run loop
static bool run_loop_exit_requested;

static uint32_t btstack_run_loop_freertos_get_time_ms(void){
    return hal_time_ms();
}

// set timer
static void btstack_run_loop_freertos_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
    ts->timeout = btstack_run_loop_freertos_get_time_ms() + timeout_in_ms + 1;
}

static void btstack_run_loop_freertos_trigger_from_thread(void){
#ifdef HAVE_FREERTOS_TASK_NOTIFICATIONS
    xTaskNotify(btstack_run_loop_task, EVENT_GROUP_FLAG_RUN_LOOP, eSetBits);
#else
    xEventGroupSetBits(btstack_run_loop_event_group, EVENT_GROUP_FLAG_RUN_LOOP);
#endif
}

#if defined(HAVE_FREERTOS_TASK_NOTIFICATIONS) || (INCLUDE_xEventGroupSetBitFromISR == 1)
static void btstack_run_loop_freertos_poll_data_sources_from_irq(void){
    BaseType_t xHigherPriorityTaskWoken;
#ifdef HAVE_FREERTOS_TASK_NOTIFICATIONS
    xTaskNotifyFromISR(btstack_run_loop_task, EVENT_GROUP_FLAG_RUN_LOOP, eSetBits, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
#ifdef ESP_PLATFORM
        portYIELD_FROM_ISR();
#else
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
#endif
    }
#else
    xEventGroupSetBitsFromISR(btstack_run_loop_event_group, EVENT_GROUP_FLAG_RUN_LOOP, &xHigherPriorityTaskWoken);
#endif
}
#endif

static void btstack_run_loop_freertos_trigger_exit_internal(void){
    run_loop_exit_requested = true;
}

/**
 * Execute run_loop
 */
static void btstack_run_loop_freertos_execute(void) {
    log_debug("RL: execute");
    
    run_loop_exit_requested = false;

    while (true) {

        // process data sources
        btstack_run_loop_base_poll_data_sources();

        // execute callbacks - protect list with mutex
        while (1){
            xSemaphoreTake(btstack_run_loop_callbacks_mutex, portMAX_DELAY);
            btstack_context_callback_registration_t * callback_registration = (btstack_context_callback_registration_t *) btstack_linked_list_pop(&btstack_run_loop_base_callbacks);
            xSemaphoreGive(btstack_run_loop_callbacks_mutex);
            if (callback_registration == NULL){
                break;
            }
            (*callback_registration->callback)(callback_registration->context);
        }

        // process registered function calls on run loop thread (deprecated)
        while (true){
            function_call_t message = { NULL, NULL };
            BaseType_t res = xQueueReceive( btstack_run_loop_queue, &message, 0);
            if (res == pdFALSE) break;
            if (message.fn){
                message.fn(message.arg);
            }
        }

        // process timers
        uint32_t now = btstack_run_loop_freertos_get_time_ms();
        btstack_run_loop_base_process_timers(now);

        // exit triggered by btstack_run_loop_trigger_exit (main thread or other thread)
        if (run_loop_exit_requested) break;

        // wait for timeout or event group/task notification
        int32_t timeout_next_timer_ms = btstack_run_loop_base_get_time_until_timeout(now);

        uint32_t timeout_ms = portMAX_DELAY;
        if (timeout_next_timer_ms >= 0){
            timeout_ms = (uint32_t) timeout_next_timer_ms;
        }

        log_debug("RL: wait with timeout %u", (int) timeout_ms);
#ifdef HAVE_FREERTOS_TASK_NOTIFICATIONS
        xTaskNotifyWait(pdFALSE, 0xffffffff, NULL, pdMS_TO_TICKS(timeout_ms));
#else
        xEventGroupWaitBits(btstack_run_loop_event_group, EVENT_GROUP_FLAG_RUN_LOOP, 1, 0, pdMS_TO_TICKS(timeout_ms));
#endif
    }
}

static void btstack_run_loop_freertos_execute_on_main_thread(btstack_context_callback_registration_t * callback_registration){
    // protect list with mutex
    xSemaphoreTake(btstack_run_loop_callbacks_mutex, portMAX_DELAY);
    btstack_run_loop_base_add_callback(callback_registration);
    xSemaphoreGive(btstack_run_loop_callbacks_mutex);
    btstack_run_loop_freertos_trigger_from_thread();
}

static void btstack_run_loop_freertos_init(void){
    btstack_run_loop_base_init();

#ifdef USE_STATIC_ALLOC
    btstack_run_loop_queue = xQueueCreateStatic(RUN_LOOP_QUEUE_LENGTH, RUN_LOOP_QUEUE_ITEM_SIZE, btstack_run_loop_queue_storage, &btstack_run_loop_queue_object);
    btstack_run_loop_callbacks_mutex = xSemaphoreCreateMutexStatic(&btstack_run_loop_callback_mutex_object);
#else
    btstack_run_loop_queue = xQueueCreate(RUN_LOOP_QUEUE_LENGTH, RUN_LOOP_QUEUE_ITEM_SIZE);
    btstack_run_loop_callbacks_mutex = xSemaphoreCreateMutex();
#endif

#ifndef HAVE_FREERTOS_TASK_NOTIFICATIONS
    // event group to wake run loop
    btstack_run_loop_event_group = xEventGroupCreate();
#endif

    // task to handle to optimize 'run on main thread'
    btstack_run_loop_task = xTaskGetCurrentTaskHandle();

    log_info("run loop init, task %p, queue item size %u", btstack_run_loop_task, (int) sizeof(function_call_t));
}

/**
 * @brief Provide btstack_run_loop_posix instance for use with btstack_run_loop_init
 */

static const btstack_run_loop_t btstack_run_loop_freertos = {
    &btstack_run_loop_freertos_init,
    &btstack_run_loop_base_add_data_source,
    &btstack_run_loop_base_remove_data_source,
    &btstack_run_loop_base_enable_data_source_callbacks,
    &btstack_run_loop_base_disable_data_source_callbacks,
    &btstack_run_loop_freertos_set_timer,
    &btstack_run_loop_base_add_timer,
    &btstack_run_loop_base_remove_timer,
    &btstack_run_loop_freertos_execute,
    &btstack_run_loop_base_dump_timer,
    &btstack_run_loop_freertos_get_time_ms,
#if defined(HAVE_FREERTOS_TASK_NOTIFICATIONS) || (INCLUDE_xEventGroupSetBitFromISR == 1)
    &btstack_run_loop_freertos_poll_data_sources_from_irq,
#else
    NULL,
#endif
    btstack_run_loop_freertos_execute_on_main_thread,
    &btstack_run_loop_freertos_trigger_exit_internal,
};

const btstack_run_loop_t * btstack_run_loop_freertos_get_instance(void){
    return &btstack_run_loop_freertos;
}


// @deprecated functions

// schedules execution from regular thread
void btstack_run_loop_freertos_trigger(void){
    btstack_run_loop_freertos_trigger_from_thread();
}

void btstack_run_loop_freertos_execute_code_on_main_thread(void (*fn)(void *arg), void * arg){
    // directly call function if already on btstack task
    if (xTaskGetCurrentTaskHandle() == btstack_run_loop_task){
        (*fn)(arg);
        return;
    }

    function_call_t message;
    message.fn  = fn;
    message.arg = arg;
    BaseType_t res = xQueueSendToBack(btstack_run_loop_queue, &message, 0); // portMAX_DELAY);
    if (res != pdTRUE){
        log_error("Failed to post fn %p", fn);
    }
    btstack_run_loop_freertos_trigger();
}

void btstack_run_loop_freertos_trigger_exit(void){
    btstack_run_loop_freertos_trigger_exit_internal();
}

#if defined(HAVE_FREERTOS_TASK_NOTIFICATIONS) || (INCLUDE_xEventGroupSetBitFromISR == 1)
void btstack_run_loop_freertos_trigger_from_isr(void){
    btstack_run_loop_freertos_poll_data_sources_from_irq();
}
#endif
