/*************************************************************************************************/
/*!
 *  \file   hci_main.c
 *
 *  \brief  HCI main module.
 *
 *          $Date: 2015-03-04 16:32:32 -0600 (Wed, 04 Mar 2015) $
 *          $Revision: 16801 $
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
#include "hci_api.h"
#include "hci_main.h"
#include "hci_core.h"
#include "hci_cmd.h"
#include "hci_evt.h"

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Control block */
hciCb_t hciCb;

/*************************************************************************************************/
/*!
 *  \fn     HciEvtRegister
 *        
 *  \brief  Register a callback for HCI events.
 *
 *  \param  evtCback  Callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciEvtRegister(hciEvtCback_t evtCback)
{
  hciCb.evtCback = evtCback;
}

/*************************************************************************************************/
/*!
 *  \fn     HciSecRegister
 *        
 *  \brief  Register a callback for certain HCI security events.
 *
 *  \param  secCback  Callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciSecRegister(hciSecCback_t secCback)
{
  hciCb.secCback = secCback;
}

/*************************************************************************************************/
/*!
 *  \fn     HciAclRegister
 *        
 *  \brief  Register callbacks for the HCI data path.
 *
 *  \param  aclCback  ACL data callback function.
 *  \param  flowCback Flow control callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciAclRegister(hciAclCback_t aclCback, hciFlowCback_t flowCback)
{
  hciCb.aclCback = aclCback;
  hciCb.flowCback = flowCback;
}


/*************************************************************************************************/
/*!
 *  \fn     HciHandlerInit
 *        
 *  \brief  HCI handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID for HCI.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciHandlerInit(wsfHandlerId_t handlerId)
{
  /* store handler ID */
  hciCb.handlerId = handlerId;
  
  /* perform other initialization */
  hciCoreInit();
  WSF_QUEUE_INIT(&hciCb.rxQueue);
}

/*************************************************************************************************/
/*!
 *  \fn     HciHandler
 *        
 *  \brief  WSF event handler for HCI.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{
  uint8_t         *pBuf;
  wsfHandlerId_t  handlerId;
  
  /* Handle message */
  if (pMsg != NULL)
  {
    /* Handle HCI command timeout */
    if (pMsg->event == HCI_MSG_CMD_TIMEOUT)
    {
      hciCmdTimeout(pMsg);
    }
  }
  /* Handle events */
  else if (event & HCI_EVT_RX)
  {
    /* Process rx queue */
    while ((pBuf = WsfMsgDeq(&hciCb.rxQueue, &handlerId)) != NULL)
    {
      /* Handle incoming HCI events */
      if (handlerId == HCI_EVT_TYPE)
      {
        /* Parse/process events */
        hciEvtProcessMsg(pBuf);

        /* Handle events during reset sequence */
        if (hciCb.resetting)
        {
          hciCoreResetSequence(pBuf);
        }
        
        /* Free buffer */
        WsfMsgFree(pBuf);
      }
      /* Handle ACL data */
      else
      {
        /* Call ACL callback; client will free buffer */
        hciCb.aclCback(pBuf);
      }
    }
  }
}
