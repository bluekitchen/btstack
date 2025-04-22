/*************************************************************************************************/
/*!
 *  \file   wsf_msg.c
 *
 *  \brief  Message passing service.
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
#include "wsf_msg.h"
#include "wsf_assert.h"
#include "wsf_buf.h"
#include "wsf_queue.h"
#include "wsf_trace.h"
#include "wsf_os.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Internal message buf structure */
typedef struct wsfMsg_tag
{
  struct wsfMsg_tag   *pNext;
  wsfHandlerId_t      handlerId;
} wsfMsg_t;

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgAlloc
 *
 *  \brief  Allocate a message buffer to be sent with WsfMsgSend().
 *
 *  \param  len   Message length in bytes.
 *
 *  \return Pointer to message buffer or NULL if allocation failed.
 */
/*************************************************************************************************/
void *WsfMsgAlloc(uint16_t len)
{
  wsfMsg_t  *pMsg;

  pMsg = WsfBufAlloc(len + sizeof(wsfMsg_t));

  /* hide header */
  if (pMsg != NULL)
  {
    pMsg++;
  }

  return pMsg;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgFree
 *
 *  \brief  Free a message buffer allocated with WsfMsgAlloc().
 *
 *  \param  pMsg  Pointer to message buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfMsgFree(void *pMsg)
{
  WsfBufFree(((wsfMsg_t *) pMsg) - 1);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgSend
 *
 *  \brief  Send a message to an event handler.
 *
 *  \param  handlerId   Event handler ID.
 *  \param  pMsg        Pointer to message buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfMsgSend(wsfHandlerId_t handlerId, void *pMsg)
{
  WSF_TRACE_MSG1("WsfMsgSend handlerId:%u", handlerId);

  /* get queue for this handler and enqueue message */
  WsfMsgEnq(WsfTaskMsgQueue(handlerId), handlerId, pMsg);

  /* set task for this handler as ready to run */
  WsfTaskSetReady(handlerId, WSF_MSG_QUEUE_EVENT);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgEnq
 *
 *  \brief  Enqueue a message.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  handerId  Set message handler ID to this value.
 *  \param  pElem     Pointer to message buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfMsgEnq(wsfQueue_t *pQueue, wsfHandlerId_t handlerId, void *pMsg)
{
  wsfMsg_t    *p;

  WSF_ASSERT(pMsg != NULL);

  /* get message header */
  p = ((wsfMsg_t *) pMsg) - 1;

  /* set handler ID */
  p->handlerId = handlerId;

  WsfQueueEnq(pQueue, p);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgDeq
 *
 *  \brief  Dequeue a message.
 *
 *  \param  pQueue      Pointer to queue.
 *  \param  pHandlerId  Handler ID of returned message; this is a return parameter.
 *
 *  \return Pointer to message that has been dequeued or NULL if queue is empty.
 */
/*************************************************************************************************/
void *WsfMsgDeq(wsfQueue_t *pQueue, wsfHandlerId_t *pHandlerId)
{
  wsfMsg_t *pMsg;

  if ((pMsg = WsfQueueDeq(pQueue)) != NULL)
  {
    *pHandlerId = pMsg->handlerId;

    /* hide header */
    pMsg++;
  }

  return pMsg;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgPeek
 *
 *  \brief  Get the next message without removing it from the queue.
 *
 *  \param  pQueue      Pointer to queue.
 *  \param  pHandlerId  Handler ID of returned message; this is a return parameter.
 *
 *  \return Pointer to the next message on the queue or NULL if queue is empty.
 */
/*************************************************************************************************/
void *WsfMsgPeek(wsfQueue_t *pQueue, wsfHandlerId_t *pHandlerId)
{
  wsfMsg_t *pMsg = pQueue->pHead;

  if (pMsg != NULL)
  {
    *pHandlerId = pMsg->handlerId;

    /* hide header */
    pMsg++;
  }

  return pMsg;
}
