/*************************************************************************************************/
/*!
 *  \file   hci_core.c
 *
 *  \brief  HCI core module for dual-chip.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "bda.h"
#include "bstream.h"
#include "hci_core.h"
#include "hci_tr.h"
#include "hci_cmd.h"
#include "hci_api.h"
#include "hci_main.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* LE event mask */
const uint8_t hciLeEventMask[HCI_LE_EVT_MASK_LEN] =
{
  HCI_EVT_MASK_LE_CONN_CMPL_EVT |                 /* Byte 0 */
  HCI_EVT_MASK_LE_ADV_REPORT_EVT |                /* Byte 0 */
  HCI_EVT_MASK_LE_CONN_UPDATE_CMPL_EVT |          /* Byte 0 */
  HCI_EVT_MASK_LE_READ_REMOTE_FEAT_CMPL_EVT |     /* Byte 0 */
  HCI_EVT_MASK_LE_LTK_REQ_EVT,                    /* Byte 0 */
  0,                                              /* Byte 1 */
  0,                                              /* Byte 2 */
  0,                                              /* Byte 3 */
  0,                                              /* Byte 4 */
  0,                                              /* Byte 5 */
  0,                                              /* Byte 6 */
  0                                               /* Byte 7 */
};

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Control block */
hciCoreCb_t hciCoreCb;

/*************************************************************************************************/
/*!
 *  \fn     hciCoreInit
 *
 *  \brief  HCI core initialization.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreInit(void)
{
  uint8_t   i;

  WSF_QUEUE_INIT(&hciCoreCb.aclQueue);

  for (i = 0; i < DM_CONN_MAX; i++)
  {
    hciCoreCb.conn[i].handle = HCI_HANDLE_NONE;
  }

  hciCmdInit();
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreConnAlloc
 *
 *  \brief  Allocate a connection structure.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void hciCoreConnAlloc(uint16_t handle)
{
  uint8_t         i;
  hciCoreConn_t   *pConn = hciCoreCb.conn;

  /* find available connection struct */
  for (i = DM_CONN_MAX; i > 0; i--, pConn++)
  {
    if (pConn->handle == HCI_HANDLE_NONE)
    {
      /* allocate and initialize */
      pConn->handle = handle;
      pConn->flowDisabled = FALSE;
      pConn->outBufs = 0;
      pConn->queuedBufs = 0;

      return;
    }
  }

  HCI_TRACE_WARN0("HCI conn struct alloc failure");
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreConnFree
 *
 *  \brief  Free a connection structure.
 *
 *  \param  handle  Connection handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void hciCoreConnFree(uint16_t handle)
{
  uint8_t         i;
  hciCoreConn_t   *pConn = hciCoreCb.conn;

  /* find connection struct */
  for (i = DM_CONN_MAX; i > 0; i--, pConn++)
  {
    if (pConn->handle == handle)
    {
      /* free structure */
      pConn->handle = HCI_HANDLE_NONE;

      /* optional: iterate through tx ACL queue and free any buffers with this handle */

      /* outstanding buffers are now available; service TX data path */
      hciCoreTxReady(pConn->outBufs);

      return;
    }
  }

  HCI_TRACE_WARN1("hciCoreConnFree handle not found:%u", handle);
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreConnByHandle
 *
 *  \brief  Get a connection structure by handle
 *
 *  \param  handle  Connection handle.
 *
 *  \return Pointer to connection structure or NULL if not found.
 */
/*************************************************************************************************/
static hciCoreConn_t *hciCoreConnByHandle(uint16_t handle)
{
  uint8_t         i;
  hciCoreConn_t   *pConn = hciCoreCb.conn;

  /* find available connection struct */
  for (i = DM_CONN_MAX; i > 0; i--, pConn++)
  {
    if (pConn->handle == handle)
    {
      return pConn;
    }
  }

  return NULL;
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreConnOpen
 *
 *  \brief  Perform internal processing on HCI connection open.
 *
 *  \param  pMsg    Message containing HCI LE Connection Complete event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreConnOpen(uint8_t *pMsg)
{
  uint16_t  handle;

  /* skip over status byte to parse handle */
  pMsg += 1;
  BSTREAM_TO_UINT16(handle, pMsg);

  /* allocate connection structure */
  hciCoreConnAlloc(handle);
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreConnClose
 *
 *  \brief  Perform internal processing on HCI connection close.
 *
 *  \param  pMsg    Message containing HCI Disconnect Complete event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreConnClose(uint8_t *pMsg)
{
  uint16_t  handle;

  /* skip over status byte to parse handle */
  pMsg += 1;
  BSTREAM_TO_UINT16(handle, pMsg);

  /* free connection structure */
  hciCoreConnFree(handle);
}


/*************************************************************************************************/
/*!
 *  \fn     hciCoreSendAclData
 *
 *  \brief  Send ACL data to transport.
 *
 *  \param  pConn    Pointer to connection structure.
 *  \param  pData    WSF buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreSendAclData(hciCoreConn_t *pConn, uint8_t *pData)
{
  /* increment outstanding buf count for handle */
  pConn->outBufs++;

  /* send to transport */
  hciTrSendAclData(pData);

  /* decrement available buffer count */
  if (hciCoreCb.availBufs > 0)
  {
    hciCoreCb.availBufs--;
  }
  else
  {
    HCI_TRACE_WARN0("hciCoreSendAclData availBufs=0");
  }
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreTxReady
 *
 *  \brief  Service the TX data path.
 *
 *  \param  bufs    Number of new buffers now available.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreTxReady(uint8_t bufs)
{
  uint8_t         *pData;
  wsfHandlerId_t  handlerId;
  uint16_t        handle;
  hciCoreConn_t   *pConn;

  /* increment available buffers, with ceiling */
  hciCoreCb.availBufs += bufs;
  if (hciCoreCb.availBufs > hciCoreCb.numBufs)
  {
    hciCoreCb.availBufs = hciCoreCb.numBufs;
  }

  /* service ACL data queue and send as many buffers as we can */
  while (hciCoreCb.availBufs > 0)
  {
    if ((pData = WsfMsgDeq(&hciCoreCb.aclQueue, &handlerId)) != NULL)
    {
      /* parse handle */
      BYTES_TO_UINT16(handle, pData);

      /* look up conn structure and send data */
      if ((pConn = hciCoreConnByHandle(handle)) != NULL)
      {
        hciCoreSendAclData(pConn, pData);
      }
      /* handle not found, connection must be closed */
      else
      {
        /* discard buffer */
        WsfMsgFree(pData);

        HCI_TRACE_WARN1("HciSendAclData discarding buffer, handle=%u", handle);
      }
    }
    else
    {
      break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreNumCmplPkts
 *
 *  \brief  Handle an HCI Number of Completed Packets event.
 *
 *  \param  pMsg    Message containing the HCI Number of Completed Packets event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreNumCmplPkts(uint8_t *pMsg)
{
  uint8_t         numHandles;
  uint16_t        bufs;
  uint16_t        handle;
  uint8_t         availBufs = 0;
  hciCoreConn_t   *pConn;

  /* parse number of handles */
  BSTREAM_TO_UINT8(numHandles, pMsg);

  /* for each handle in event */
  while (numHandles-- > 0)
  {
    /* parse handle and number of buffers */
    BSTREAM_TO_UINT16(handle, pMsg);
    BSTREAM_TO_UINT16(bufs, pMsg);

    if ((pConn = hciCoreConnByHandle(handle)) != NULL)
    {
      /* decrement outstanding buffer count to controller */
      pConn->outBufs -= (uint8_t) bufs;

      /* decrement queued buffer count for this connection */
      pConn->queuedBufs -= (uint8_t) bufs;

      /* increment available buffer count */
      availBufs += (uint8_t) bufs;

      /* call flow control callback */
      if (pConn->flowDisabled && pConn->queuedBufs <= HCI_ACL_QUEUE_LO)
      {
        pConn->flowDisabled = FALSE;
        (*hciCb.flowCback)(handle, FALSE);
      }
    }
  }

  /* service TX data path */
  hciCoreTxReady(availBufs);
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreRecv
 *
 *  \brief  Send a received HCI event or ACL packet to the HCI event handler.
 *
 *  \param  msgType       Message type:  HCI_ACL_TYPE or HCI_EVT_TYPE.
 *  \param  pCoreRecvMsg  Pointer to received message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreRecv(uint8_t msgType, uint8_t *pCoreRecvMsg)
{
  /* dump event for protocol analysis */
  if (msgType == HCI_EVT_TYPE)
  {
    HCI_PDUMP_EVT(*(pCoreRecvMsg + 1) + HCI_EVT_HDR_LEN, pCoreRecvMsg);
  }
  else if (msgType == HCI_ACL_TYPE)
  {
    HCI_PDUMP_RX_ACL(*(pCoreRecvMsg + 2) + HCI_ACL_HDR_LEN, pCoreRecvMsg);
  }

  /* queue buffer */
  WsfMsgEnq(&hciCb.rxQueue, (wsfHandlerId_t) msgType, pCoreRecvMsg);

  /* set event */
  WsfSetEvent(hciCb.handlerId, HCI_EVT_RX);
}

/*************************************************************************************************/
/*!
 *  \fn     HciResetSequence
 *
 *  \brief  Initiate an HCI reset sequence.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciResetSequence(void)
{
  /* set resetting state */
  hciCb.resetting = TRUE;

  /* start the reset sequence */
  hciCoreResetStart();
}

/*************************************************************************************************/
/*!
 *  \fn     HciGetBdAddr
 *
 *  \brief  Return a pointer to the BD address of this device.
 *
 *  \return Pointer to the BD address.
 */
/*************************************************************************************************/
uint8_t *HciGetBdAddr(void)
{
  return hciCoreCb.bdAddr;
}

/*************************************************************************************************/
/*!
 *  \fn     HciGetWhiteListSize
 *
 *  \brief  Return the white list size.
 *
 *  \return White list size.
 */
/*************************************************************************************************/
uint8_t HciGetWhiteListSize(void)
{
  return hciCoreCb.whiteListSize;
}

/*************************************************************************************************/
/*!
 *  \fn     HciGetAdvTxPwr
 *
 *  \brief  Return the advertising transmit power.
 *
 *  \return Advertising transmit power.
 */
/*************************************************************************************************/
int8_t HciGetAdvTxPwr(void)
{
  return hciCoreCb.advTxPwr;
}

/*************************************************************************************************/
/*!
 *  \fn     HciGetBufSize
 *
 *  \brief  Return the ACL buffer size supported by the controller.
 *
 *  \return ACL buffer size.
 */
/*************************************************************************************************/
uint16_t HciGetBufSize(void)
{
  return hciCoreCb.bufSize;
}

/*************************************************************************************************/
/*!
 *  \fn     HciGetNumBufs
 *
 *  \brief  Return the number of ACL buffers supported by the controller.
 *
 *  \return Number of ACL buffers.
 */
/*************************************************************************************************/
uint8_t HciGetNumBufs(void)
{
  return hciCoreCb.numBufs;
}

/*************************************************************************************************/
/*!
 *  \fn     HciGetSupStates
 *
 *  \brief  Return the states supported by the controller.
 *
 *  \return Pointer to the supported states array.
 */
/*************************************************************************************************/
uint8_t *HciGetSupStates(void)
{
  return hciCoreCb.leStates;
}

/*************************************************************************************************/
/*!
 *  \fn     HciGetLeSupFeat
 *
 *  \brief  Return the LE supported features supported by the controller.
 *
 *  \return Supported features.
 */
/*************************************************************************************************/
uint8_t HciGetLeSupFeat(void)
{
  return hciCoreCb.leSupFeat;
}

/*************************************************************************************************/
/*!
 *  \fn     HciSendAclData
 *
 *  \brief  Send data from the stack to HCI.
 *
 *  \param  pData    WSF buffer containing an ACL packet
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSendAclData(uint8_t *pData)
{
  uint16_t        handle;
  hciCoreConn_t   *pConn;

  /* parse handle */
  BYTES_TO_UINT16(handle, pData);

  /* look up connection structure */
  if ((pConn = hciCoreConnByHandle(handle)) != NULL)
  {
    /* if queue empty and buffers available */
    if (WsfQueueEmpty(&hciCoreCb.aclQueue) && hciCoreCb.availBufs > 0)
    {
      /* send data */
      hciCoreSendAclData(pConn, pData);
    }
    else
    {
      /* queue data */
      WsfMsgEnq(&hciCoreCb.aclQueue, 0, pData);
    }

    /* increment buffer queue count for this connection */
    pConn->queuedBufs++;

    /* manage flow control to stack */
    if (pConn->queuedBufs == HCI_ACL_QUEUE_HI && pConn->flowDisabled == FALSE)
    {
      pConn->flowDisabled = TRUE;
      (*hciCb.flowCback)(handle, TRUE);
    }
  }
  /* connection not found, connection must be closed */
  else
  {
    /* discard buffer */
    WsfMsgFree(pData);

    HCI_TRACE_WARN1("HciSendAclData discarding buffer, handle=%u", handle);
  }
}
