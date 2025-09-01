/*************************************************************************************************/
/*!
 *  \file   wsf_os.c
 *
 *  \brief  Software foundation OS main module.
 *
 *          $Date: 2015-03-05 15:14:28 -0600 (Thu, 05 Mar 2015) $
 *          $Revision: 16812 $
 *
 *  Copyright (c) 2009 Wicentric, Inc., all rights reserved.
 *  Wicentric confidential and proprietary.
 *
 *  IMPORTANT.  Your use of this file is governed by a Software License Agreement
 *  ("Agreement") that must be accepted in order to download or otherwise receive a
 *  copy of this file.  You may not use or copy this file for any purpose other than
 *  as described in the Agreement.  If you do not agree to all of the terms of the
 *  Agreement do not use this file and delete all copies in your possession or control;
 *  if you do not have a copy of the Agreement, you must contact Wicentric, Inc. prior
 *  to any use, copying or further distribution of this software.
 */
/*************************************************************************************************/

#ifdef __IAR_SYSTEMS_ICC__
#include <intrinsics.h>
#endif
#ifdef __CROSSWORKS
#include <inmaxq.h>
#endif
#include <string.h>
#include "wsf_types.h"
#include "wsf_os.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "wsf_timer.h"
#include "wsf_queue.h"
#include "wsf_buf.h"
#include "wsf_msg.h"
#include "wsf_cs.h"
#include "wsf_sec.h"

/**************************************************************************************************
  Compile time assert checks
**************************************************************************************************/

WSF_CT_ASSERT(sizeof(uint8_t) == 1);
WSF_CT_ASSERT(sizeof(uint16_t) == 2);
WSF_CT_ASSERT(sizeof(uint32_t) == 4);

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* maximum number of event handlers per task */
#ifndef WSF_MAX_HANDLERS
#define WSF_MAX_HANDLERS      8
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Task structure */
typedef struct
{
  wsfEventHandler_t     handler[WSF_MAX_HANDLERS];
  wsfEventMask_t        handlerEventMask[WSF_MAX_HANDLERS];
  wsfQueue_t            msgQueue;
  wsfTaskEvent_t        taskEventMask;
  uint8_t               numHandler;
} wsfOsTask_t;

/* OS structure */
typedef struct
{
  wsfOsTask_t           task;
} wsfOs_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

uint8_t csNesting = 0;

wsfOs_t wsfOs;

/*************************************************************************************************/
/*!
 *  \fn     WsfCsEnter
 *
 *  \brief  Enter a critical section.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfCsEnter(void)
{
  if (csNesting == 0)
  {
#ifdef __GNUC__
    __asm volatile ("cpsid i");
#else
    __disable_interrupt();
#endif
  }
  csNesting++;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfCsEnter
 *
 *  \brief  Enter a critical section.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfCsExit(void)
{
  WSF_ASSERT(csNesting != 0);

  csNesting--;
  if (csNesting == 0)
  {
#ifdef __GNUC__
    __asm volatile ("cpsie i");
#else
    __enable_interrupt();
#endif
  }
}

/*************************************************************************************************/
/*!
 *  \fn     WsfTaskLock
 *
 *  \brief  Lock task scheduling.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTaskLock(void)
{
  WsfCsEnter();
}

/*************************************************************************************************/
/*!
 *  \fn     WsfTaskUnlock
 *
 *  \brief  Unock task scheduling.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTaskUnlock(void)
{
  WsfCsExit();
}

/*************************************************************************************************/
/*!
 *  \fn     WsfSetEvent
 *
 *  \brief  Set an event for an event handler.
 *
 *  \param  handlerId   Handler ID.
 *  \param  event       Event or events to set.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfSetEvent(wsfHandlerId_t handlerId, wsfEventMask_t event)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(WSF_HANDLER_FROM_ID(handlerId) < WSF_MAX_HANDLERS);

  WSF_TRACE_INFO2("WsfSetEvent handlerId:%u event:%u", handlerId, event);

  WSF_CS_ENTER(cs);
  wsfOs.task.handlerEventMask[WSF_HANDLER_FROM_ID(handlerId)] |= event;
  WSF_CS_EXIT(cs);

  WsfTaskSetReady(handlerId, WSF_HANDLER_EVENT);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfTaskSetReady
 *
 *  \brief  Set the task used by the given handler as ready to run.
 *
 *  \param  handlerId   Event handler ID.
 *  \param  event       Task event mask.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTaskSetReady(wsfHandlerId_t handlerId, wsfTaskEvent_t event)
{
  WSF_CS_INIT(cs);

  WSF_CS_ENTER(cs);
  wsfOs.task.taskEventMask |= event;
  WSF_CS_EXIT(cs);

  /* set event in OS */
}

/*************************************************************************************************/
/*!
 *  \fn     WsfTaskMsgQueue
 *
 *  \brief  Return the message queue used by the given handler.
 *
 *  \param  handlerId   Event handler ID.
 *
 *  \return Task message queue.
 */
/*************************************************************************************************/
wsfQueue_t *WsfTaskMsgQueue(wsfHandlerId_t handlerId)
{
  return &(wsfOs.task.msgQueue);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfOsSetNextHandler
 *
 *  \brief  Set the next WSF handler function in the WSF OS handler array.  This function
 *          should only be called as part of the stack initialization procedure.
 *
 *  \param  handler    WSF handler function.
 *
 *  \return WSF handler ID for this handler.
 */
/*************************************************************************************************/
wsfHandlerId_t WsfOsSetNextHandler(wsfEventHandler_t handler)
{
  wsfHandlerId_t handlerId = wsfOs.task.numHandler++;

  WSF_ASSERT(handlerId < WSF_MAX_HANDLERS);

  wsfOs.task.handler[handlerId] = handler;

  return handlerId;
}

/*************************************************************************************************/
/*!
 *  \fn     wsfOsReadyToSleep
 *
 *  \brief  Check if WSF is ready to sleep.  This function should be called when interrupts
 *          are disabled.
 *
 *  \param  None.
 *
 *  \return Return TRUE if there are no pending WSF task events set, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t wsfOsReadyToSleep(void)
{
  return (wsfOs.task.taskEventMask == 0);
}

/*************************************************************************************************/
/*!
 *  \fn     wsfOsDispatcher
 *
 *  \brief  Event dispatched.  Designed to be called repeatedly from infinite loop.
 *
 *  \param  None.
 *
 *  \return None.
 */
/*************************************************************************************************/
void wsfOsDispatcher(void)
{
  wsfOsTask_t       *pTask;
  void              *pMsg;
  wsfTimer_t        *pTimer;
  wsfEventMask_t    eventMask;
  wsfTaskEvent_t    taskEventMask;
  wsfHandlerId_t    handlerId;
  uint8_t           i;

  WSF_CS_INIT(cs);

  pTask = &wsfOs.task;

  while (pTask->taskEventMask)
  {
    /* get and then clear task event mask */
    WSF_CS_ENTER(cs);
    taskEventMask = pTask->taskEventMask;
    pTask->taskEventMask = 0;
    WSF_CS_EXIT(cs);

    if (taskEventMask & WSF_MSG_QUEUE_EVENT)
    {
      /* handle msg queue */
      while ((pMsg = WsfMsgDeq(&pTask->msgQueue, &handlerId)) != NULL)
      {
        WSF_ASSERT(handlerId < WSF_MAX_HANDLERS);
        (*pTask->handler[handlerId])(0, pMsg);
        WsfMsgFree(pMsg);
      }
    }

    if (taskEventMask & WSF_TIMER_EVENT)
    {
      /* service timers */
      while ((pTimer = WsfTimerServiceExpired(0)) != NULL)
      {
        WSF_ASSERT(pTimer->handlerId < WSF_MAX_HANDLERS);
        (*pTask->handler[pTimer->handlerId])(0, &pTimer->msg);
      }
    }

    if (taskEventMask & WSF_HANDLER_EVENT)
    {
      /* service handlers */
      for (i = 0; i < WSF_MAX_HANDLERS; i++)
      {
        if ((pTask->handlerEventMask[i] != 0) && (pTask->handler[i] != NULL))
        {
          WSF_CS_ENTER(cs);
          eventMask = pTask->handlerEventMask[i];
          pTask->handlerEventMask[i] = 0;
          WSF_CS_EXIT(cs);

          (*pTask->handler[i])(eventMask, NULL);
        }
      }
    }
  }
}
