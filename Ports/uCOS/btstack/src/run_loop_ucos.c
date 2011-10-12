/*
 * Copyright (C) 2011 by Matthias Ringwald
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
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 */

/*
 *  run_loop_ucos.c
 *
 *  @brief BTstack run loop for uC/OS
 *
 *  Created by Albis Technologies.
 */

#include <btstack/port_ucos.h>
#include <btstack/run_loop.h>

#include "run_loop_ucos.h"
#include "hci_transport.h"
#include "serial.h"

DEFINE_THIS_FILE

static linked_list_t timers;
static data_source_t *transportDataSource = 0;

/*=============================================================================
*  Run loop message queue.
*============================================================================*/
#define MSG_QUEUE_BUFFER_SIZE 32

#define MSG_ID_INCOMING_TRANSPORT_PACKET   1
#define MSG_ID_OUTGOING_RFCOMM_DATA        2

static struct
{
    OS_EVENT *queue;
    void * queueBuffer[MSG_QUEUE_BUFFER_SIZE];
} messages;

/*=============================================================================
=============================================================================*/
void run_loop_notify_incoming_transport_packet(void)
{
    INT8U err;

    err = OSQPost(messages.queue, (void *)MSG_ID_INCOMING_TRANSPORT_PACKET);
    SYS_ASSERT(err == USE_OS_NO_ERROR);
}

/*=============================================================================
=============================================================================*/
void run_loop_notify_outgoing_rfcomm_data(void)
{
    INT8U err;

    err = OSQPost(messages.queue, (void *)MSG_ID_OUTGOING_RFCOMM_DATA);
    SYS_ASSERT(err == USE_OS_NO_ERROR);
}

/*=============================================================================
=============================================================================*/
void run_loop_add_data_source(data_source_t *ds)
{
    transportDataSource = ds;
}

/*=============================================================================
=============================================================================*/
void run_loop_add_timer(timer_source_t *ts)
{
    linked_item_t *it;

    for(it = (linked_item_t *)&timers; it->next; it = it->next)
    {
        if(ts->timeout < ((timer_source_t *)it->next)->timeout)
        {
            break;
        }
    }

    ts->item.next = it->next;
    it->next = (linked_item_t *)ts;
}

/*=============================================================================
=============================================================================*/
void run_loop_set_timer(timer_source_t *ts, uint32_t timeout_in_ms)
{
    unsigned long ticks = MSEC_TO_TICKS(timeout_in_ms);
    ts->timeout = OSTimeGet() + ticks;
}

/*=============================================================================
=============================================================================*/
int run_loop_remove_timer(timer_source_t *ts)
{
    return linked_list_remove(&timers, (linked_item_t *) ts);
}

/*=============================================================================
=============================================================================*/
void run_loop_execute(void)
{
    INT8U err;
    void *event;
    INT16U timeout;

    for(;;)
    {
        /* Get next timeout. */
        timeout = 0;
        if(timers)
        {
            timeout = ((timer_source_t *)timers)->timeout - OSTimeGet();
        }

        event = OSQPend(messages.queue, timeout, &err);

        if(err == USE_OS_NO_ERROR)
        {
            if((unsigned long)event == MSG_ID_INCOMING_TRANSPORT_PACKET)
            {
                transportDataSource->process(transportDataSource);
            }
        }

        /* Process timers. */
        while(timers)
        {
            timer_source_t *ts = (timer_source_t *)timers;
            if(ts->timeout > OSTimeGet()) break;
            run_loop_remove_timer(ts);
            ts->process(ts);
        }
    }
}

/*=============================================================================
=============================================================================*/
void run_loop_init(RUN_LOOP_TYPE type)
{
    timers = 0;

    messages.queue = OSQCreate(messages.queueBuffer, MSG_QUEUE_BUFFER_SIZE);
    SYS_ASSERT(messages.queue);
}
