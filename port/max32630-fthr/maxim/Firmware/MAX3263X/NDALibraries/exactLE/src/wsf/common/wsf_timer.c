/*************************************************************************************************/
/*!
 *  \file   wsf_timer.c
 *
 *  \brief  Timer service.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
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

#include "wsf_types.h"
#include "wsf_queue.h"
#include "wsf_timer.h"
#include "wsf_assert.h"
#include "wsf_trace.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* convert seconds to timer ticks */
#define WSF_TIMER_SEC_TO_TICKS(sec)         ((1000 / wsfTimerMsPerTick) * (sec))

/* convert milliseconds to timer ticks */
#define WSF_TIMER_MS_TO_TICKS(ms)           ((ms) / wsfTimerMsPerTick)

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

uint8_t     wsfTimerMsPerTick;      /*!< Number of milliseconds per timer tick */
wsfQueue_t  wsfTimerTimerQueue;     /*!< Timer queue */

/*************************************************************************************************/
/*!
 *  \fn     wsfTimerRemove
 *
 *  \brief  Remove a timer from queue.  Note this function does not lock task scheduling.
 *
 *  \param  pTimer  Pointer to timer.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void wsfTimerRemove(wsfTimer_t *pTimer)
{
  wsfTimer_t  *pElem;
  wsfTimer_t  *pPrev = NULL;

  pElem = (wsfTimer_t *) wsfTimerTimerQueue.pHead;

  /* find timer in queue */
  while (pElem != NULL)
  {
    if (pElem == pTimer)
    {
      break;
    }
    pPrev = pElem;
    pElem = pElem->pNext;
  }

  /* if timer found remove from queue */
  if (pElem != NULL)
  {
    WsfQueueRemove(&wsfTimerTimerQueue, pTimer, pPrev);

    pTimer->isStarted = FALSE;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     wsfTimerInsert
 *
 *  \brief  Insert a timer into the queue sorted by the timer expiration.
 *
 *  \param  pTimer  Pointer to timer.
 *  \param  ticks   Timer ticks until expiration.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void wsfTimerInsert(wsfTimer_t *pTimer, wsfTimerTicks_t ticks)
{
  wsfTimer_t  *pElem;
  wsfTimer_t  *pPrev = NULL;

  /* task schedule lock */
  WsfTaskLock();

  /* if timer is already running stop it first */
  if (pTimer->isStarted)
  {
    wsfTimerRemove(pTimer);
  }

  pTimer->isStarted = TRUE;
  pTimer->ticks = ticks;

  pElem = (wsfTimer_t *) wsfTimerTimerQueue.pHead;

  /* find insertion point in queue */
  while (pElem != NULL)
  {
    if (pTimer->ticks < pElem->ticks)
    {
      break;
    }
    pPrev = pElem;
    pElem = pElem->pNext;
  }

  /* insert timer into queue */
  WsfQueueInsert(&wsfTimerTimerQueue, pTimer, pPrev);

  /* task schedule unlock */
  WsfTaskUnlock();
}


/*************************************************************************************************/
/*!
 *  \fn     WsfTimerInit
 *
 *  \brief  Initialize the timer service.  This function should only be called once
 *          upon system initialization.
 *
 *  \param  msPerTick   Sets the number of milliseconds per timer tick.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTimerInit(uint8_t msPerTick)
{
  wsfTimerMsPerTick = msPerTick;

  WSF_QUEUE_INIT(&wsfTimerTimerQueue);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfTimerStartSec
 *
 *  \brief  Start a timer in units of seconds.
 *
 *  \param  pTimer  Pointer to timer.
 *  \param  sec     Seconds until expiration.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTimerStartSec(wsfTimer_t *pTimer, wsfTimerTicks_t sec)
{
  WSF_TRACE_INFO2("WsfTimerStartSec pTimer:0x%x ticks:%u", pTimer, WSF_TIMER_SEC_TO_TICKS(sec));

  /* insert timer into queue */
  wsfTimerInsert(pTimer, WSF_TIMER_SEC_TO_TICKS(sec));
}

/*************************************************************************************************/
/*!
 *  \fn     WsfTimerStartMs
 *
 *  \brief  Start a timer in units of milliseconds.
 *
 *  \param  pTimer  Pointer to timer.
 *  \param  ms     Milliseconds until expiration.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTimerStartMs(wsfTimer_t *pTimer, wsfTimerTicks_t ms)
{
  WSF_TRACE_INFO2("WsfTimerStartMs pTimer:0x%x ticks:%u", pTimer, WSF_TIMER_MS_TO_TICKS(ms));

  /* insert timer into queue */
  wsfTimerInsert(pTimer, WSF_TIMER_MS_TO_TICKS(ms));
}

/*************************************************************************************************/
/*!
 *  \fn     WsfTimerStop
 *
 *  \brief  Stop a timer.
 *
 *  \param  pTimer  Pointer to timer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTimerStop(wsfTimer_t *pTimer)
{
  WSF_TRACE_INFO1("WsfTimerStop pTimer:0x%x", pTimer);

  /* task schedule lock */
  WsfTaskLock();

  wsfTimerRemove(pTimer);

  /* task schedule unlock */
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \fn     WsfTimerUpdate
 *
 *  \brief  Update the timer service with the number of elapsed ticks.
 *
 *  \param  ticks  Number of ticks since last update.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTimerUpdate(wsfTimerTicks_t ticks)
{
  wsfTimer_t  *pElem;

  /* task schedule lock */
  WsfTaskLock();

  pElem = (wsfTimer_t *) wsfTimerTimerQueue.pHead;

  /* iterate over timer queue */
  while (pElem != NULL)
  {
    /* decrement ticks while preventing underflow */
    if (pElem->ticks > ticks)
    {
      pElem->ticks -= ticks;
    }
    else
    {
      pElem->ticks = 0;

      /* timer expired; set task for this timer as ready */
      WsfTaskSetReady(pElem->handlerId, WSF_TIMER_EVENT);
    }

    pElem = pElem->pNext;
  }

  /* task schedule unlock */
  WsfTaskUnlock();
}

/*************************************************************************************************/
/*!
 *  \fn     WsfTimerNextExpiration
 *
 *  \brief  Return the number of ticks until the next timer expiration.  Note that this
 *          function can return zero even if a timer is running, indicating a timer
 *          has expired but has not yet been serviced.
 *
 *  \param  pTimerRunning   Returns TRUE if a timer is running, FALSE if no timers running.
 *
 *  \return The number of ticks until the next timer expiration.
 */
/*************************************************************************************************/
wsfTimerTicks_t WsfTimerNextExpiration(bool_t *pTimerRunning)
{
  wsfTimerTicks_t ticks;

  /* task schedule lock */
  WsfTaskLock();

  if (wsfTimerTimerQueue.pHead == NULL)
  {
    *pTimerRunning = FALSE;
    ticks = 0;
  }
  else
  {
    *pTimerRunning = TRUE;
    ticks = ((wsfTimer_t *) wsfTimerTimerQueue.pHead)->ticks;
  }

  /* task schedule unlock */
  WsfTaskUnlock();

  return ticks;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfTimerServiceExpired
 *
 *  \brief  Service expired timers for the given task.
 *
 *  \param  taskId      Task ID.
 *
 *  \return Pointer to timer or NULL.
 */
/*************************************************************************************************/
wsfTimer_t *WsfTimerServiceExpired(wsfTaskId_t taskId)
{
  wsfTimer_t  *pElem;
  wsfTimer_t  *pPrev = NULL;

  /* task schedule lock */
  WsfTaskLock();

  pElem = (wsfTimer_t *) wsfTimerTimerQueue.pHead;

  /* find expired timers in queue */
  while (pElem != NULL && pElem->ticks == 0)
  {
    /* remove timer from queue */
    WsfQueueRemove(&wsfTimerTimerQueue, pElem, pPrev);

    pElem->isStarted = FALSE;

    /* task schedule unlock */
    WsfTaskUnlock();

    WSF_TRACE_INFO1("Timer expired pTimer:0x%x", pElem);

    /* return timer */
    return pElem;
  }

  /* task schedule unlock */
  WsfTaskUnlock();

  return NULL;
}
