/*************************************************************************************************/
/*!
 *  \file   wsf_queue.c
 *        
 *  \brief  General purpose queue service.
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
#include "wsf_assert.h"
#include "wsf_cs.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Get next queue element */
#define WSF_QUEUE_NEXT(p)               (((wsfQueueElem_t *)(p))->pNext)

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Queue element */
typedef struct wsfQueueElem_tag
{
  struct wsfQueueElem_tag *pNext;      /* pointer to next element */
} wsfQueueElem_t;

/*************************************************************************************************/
/*!
 *  \fn     WsfQueueEnq
 *        
 *  \brief  Enqueue and element to the tail of a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  pElem     Pointer to element.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfQueueEnq(wsfQueue_t *pQueue, void *pElem)
{
  WSF_CS_INIT(cs);
  
  WSF_ASSERT(pQueue != NULL);
  WSF_ASSERT(pElem != NULL);
  
  /* initialize next pointer */
  WSF_QUEUE_NEXT(pElem) = NULL;
  
  /* enter critical section */
  WSF_CS_ENTER(cs);
  
  /* if queue empty */
  if (pQueue->pHead == NULL)
  {
    pQueue->pHead = pElem;
    pQueue->pTail = pElem;
  }
  /* else enqueue element to the tail of queue */
  else
  {
    WSF_QUEUE_NEXT(pQueue->pTail) = pElem;
    pQueue->pTail = pElem;
  }
  
  /* exit critical section */
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfQueueDeq
 *        
 *  \brief  Dequeue and element from the head of a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *
 *  \return Pointer to element that has been dequeued or NULL if queue is empty.
 */
/*************************************************************************************************/
void *WsfQueueDeq(wsfQueue_t *pQueue)
{
  wsfQueueElem_t  *pElem;
  
  WSF_CS_INIT(cs);
  
  WSF_ASSERT(pQueue != NULL);
  
  /* enter critical section */
  WSF_CS_ENTER(cs);
 
  pElem = pQueue->pHead;
  
  /* if queue is not empty */
  if (pElem != NULL)
  {
    /* set head to next element in queue */
    pQueue->pHead = WSF_QUEUE_NEXT(pElem);
    
    /* check for empty queue */
    if (pQueue->pHead == NULL)
    {
      pQueue->pTail = NULL;
    }
  }
  
  /* exit critical section */
  WSF_CS_EXIT(cs);
  
  return pElem;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfQueuePush
 *        
 *  \brief  Push and element to the head of a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  pElem     Pointer to element.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfQueuePush(wsfQueue_t *pQueue, void *pElem)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);
  WSF_ASSERT(pElem != NULL);
  
  /* enter critical section */
  WSF_CS_ENTER(cs);
  
  /* else push element to head of queue */
  WSF_QUEUE_NEXT(pElem) = pQueue->pHead;
  
  /* if queue was empty set tail */
  if (pQueue->pHead == NULL)
  {
    pQueue->pTail = pElem;
  }

  /* set head */
  pQueue->pHead = pElem;
  
  /* exit critical section */
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfQueueInsert
 *        
 *  \brief  Insert an element into a queue.  This function is typically used when iterating
 *          over a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  pElem     Pointer to element to be inserted.
 *  \param  pPrev     Pointer to previous element in the queue before element to be inserted.
 *                    Note:  set pPrev to NULL if pElem is first element in queue.
 *  \return None.
 */
/*************************************************************************************************/
void WsfQueueInsert(wsfQueue_t *pQueue, void *pElem, void *pPrev)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);
  WSF_ASSERT(pElem != NULL);
  
  /* enter critical section */
  WSF_CS_ENTER(cs);
  
  /* if queue empty or inserting at tail */
  if (pQueue->pHead == NULL || pPrev == pQueue->pTail)
  {
    /* queue as normal */
    WsfQueueEnq(pQueue, pElem);
  }
  /* else if inserting at head */
  else if (pPrev == NULL)
  {
    /* push to head */
    WsfQueuePush(pQueue, pElem);
  }
  else 
  {
    /* insert in middle of queue */
    WSF_QUEUE_NEXT(pElem) = WSF_QUEUE_NEXT(pPrev);
    WSF_QUEUE_NEXT(pPrev) = pElem;
  }

  /* exit critical section */
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfQueueRemove
 *        
 *  \brief  Remove an element from a queue.  This function is typically used when iterating
 *          over a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  pElem     Pointer to element to be removed.
 *  \param  pPrev     Pointer to previous element in the queue before element to be removed.
 *                    Note:  set pPrev to NULL if pElem is first element in queue.
 *  \return None.
 */
/*************************************************************************************************/
void WsfQueueRemove(wsfQueue_t *pQueue, void *pElem, void *pPrev)
{
  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);
  WSF_ASSERT(pQueue->pHead != NULL);
  WSF_ASSERT(pElem != NULL);
  
  /* enter critical section */
  WSF_CS_ENTER(cs);
  
  /* if first element */
  if (pElem == pQueue->pHead)
  {
    /* remove from head of queue */
    pQueue->pHead = WSF_QUEUE_NEXT(pElem);
  }
  else
  {
    /* remove from middle of queue */
    WSF_QUEUE_NEXT(pPrev) = WSF_QUEUE_NEXT(pElem);
  }
  
  /* if last element */
  if (pElem == pQueue->pTail)
  {
    /* update tail */
    pQueue->pTail = pPrev;
  }
  
  /* exit critical section */
  WSF_CS_EXIT(cs);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfQueueCount
 *        
 *  \brief  Count the number of elements in a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *
 *  \return Number of elements in queue.
 */
/*************************************************************************************************/
uint16_t WsfQueueCount(wsfQueue_t *pQueue)
{
  wsfQueueElem_t  *pElem;
  uint16_t        count = 0;
  
  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);

  /* enter critical section */
  WSF_CS_ENTER(cs);

  pElem = pQueue->pHead;

  /* iterate over queue */
  while (pElem != NULL)
  {
    count++;
    pElem = pElem->pNext;
  }
  
  /* exit critical section */
  WSF_CS_EXIT(cs);
  
  return count;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfQueueEmpty
 *        
 *  \brief  Return TRUE if queue is empty.
 *
 *  \param  pQueue    Pointer to queue.
 *
 *  \return TRUE if queue is empty, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t WsfQueueEmpty(wsfQueue_t *pQueue)
{
  bool_t      empty;

  WSF_CS_INIT(cs);

  WSF_ASSERT(pQueue != NULL);

  /* enter critical section */
  WSF_CS_ENTER(cs);

  empty = (pQueue->pHead == NULL);

  /* exit critical section */
  WSF_CS_EXIT(cs);
  
  return empty;
}
