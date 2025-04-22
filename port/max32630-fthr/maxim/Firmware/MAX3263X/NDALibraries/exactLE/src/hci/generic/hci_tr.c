/*************************************************************************************************/
/*!
 *  \file   hci_tr.c
 *
 *  \brief  HCI transport module.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
 *
 *  Copyright (c) 2011 Wicentric, Inc., all rights reserved.
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
#include "wsf_trace.h"
#include "wsf_assert.h"
#include "bstream.h"
#include "hci_api.h"
#include "hci_core.h"
#include "hci_tr.h"

/*************************************************************************************************/
/*!
 *  \fn     hciTrSendAclData
 *        
 *  \brief  Send a complete HCI ACL packet to the transport. 
 *
 *  \param  pData    WSF msg buffer containing an ACL packet.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrSendAclData(uint8_t *pData)
{
  /* This function sends the ACL packet to the transport driver, then
   * calls WsfMsgFree(pData) to free the buffer containing the packet.
   */
}


/*************************************************************************************************/
/*!
 *  \fn     hciTrSendCmd
 *        
 *  \brief  Send a complete HCI command to the transport.
 *
 *  \param  pData    WSF msg buffer containing an HCI command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrSendCmd(uint8_t *pData)
{
  /* This function sends the HCI command to the transport driver, then
   * calls WsfMsgFree(pData) to free the buffer containing the command.
   */
}


/*************************************************************************************************/
/*!
 *  \fn     hciSerialRxIncoming
 *        
 *  \brief  Receive function.  Gets called by external code when bytes are received.
 *
 *  \param  pBuf   Pointer to buffer of incoming bytes.
 *  \param  len    Number of bytes in incoming buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrSerialRxIncoming(uint8_t *pBuf, uint8_t len)
{
  /* This function allocates a WSF message buffer, assembles a complete HCI event or ACL
   * packet into the buffer, then calls hciCoreRecv() to send the buffer to the stack.
   */
}
