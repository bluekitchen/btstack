/*************************************************************************************************/
/*!
 *  \file   hci_vs.c
 *
 *  \brief  HCI vendor specific functions for generic controllers.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "bda.h"
#include "bstream.h"
#include "hci_core.h"
#include "hci_api.h"
#include "hci_main.h"
#include "hci_cmd.h"

#if HCI_VS_TARGET == HCI_VS_GENERIC

/*************************************************************************************************/
/*!
 *  \fn     hciCoreResetStart
 *        
 *  \brief  Start the HCI reset sequence.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreResetStart(void)
{
  /* send an HCI Reset command to start the sequence */
  HciResetCmd();
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreResetSequence
 *        
 *  \brief  Implement the HCI reset sequence.
 *
 *  \param  pMsg    HCI event message from previous command in the sequence.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreResetSequence(uint8_t *pMsg)
{
  uint16_t       opcode;
  wsfMsgHdr_t    hdr;
  static uint8_t randCnt;
  
  /* if event is a command complete event */
  if (*pMsg == HCI_CMD_CMPL_EVT)
  {
    /* parse parameters */
    pMsg += HCI_EVT_HDR_LEN;
    pMsg++;                   /* skip num packets */
    BSTREAM_TO_UINT16(opcode, pMsg);
    pMsg++;                   /* skip status */
    
    /* decode opcode */
    switch (opcode)
    {
      case HCI_OPCODE_RESET:
        /* initialize rand command count */
        randCnt = 0;
        
        /* send next command in sequence */
        HciLeSetEventMaskCmd((uint8_t *) hciLeEventMask);
        break;

      case HCI_OPCODE_LE_SET_EVENT_MASK:
        /* send next command in sequence */
        HciReadBdAddrCmd();
        break;

      case HCI_OPCODE_READ_BD_ADDR:
        /* parse and store event parameters */
        BdaCpy(hciCoreCb.bdAddr, pMsg);

        /* send next command in sequence */
        HciLeReadBufSizeCmd();
        break;

      case HCI_OPCODE_LE_READ_BUF_SIZE:
        /* parse and store event parameters */
        BSTREAM_TO_UINT16(hciCoreCb.bufSize, pMsg);
        BSTREAM_TO_UINT8(hciCoreCb.numBufs, pMsg);

        /* initialize ACL buffer accounting */
        hciCoreCb.availBufs = hciCoreCb.numBufs;
        
        /* send next command in sequence */
        HciLeReadSupStatesCmd();
        break;

      case HCI_OPCODE_LE_READ_SUP_STATES:
        /* parse and store event parameters */
        memcpy(hciCoreCb.leStates, pMsg, HCI_LE_STATES_LEN);

        /* send next command in sequence */
        HciLeReadWhiteListSizeCmd();
        break;

      case HCI_OPCODE_LE_READ_WHITE_LIST_SIZE:
        /* parse and store event parameters */
        BSTREAM_TO_UINT8(hciCoreCb.whiteListSize, pMsg);

        /* send next command in sequence */
        HciLeRandCmd();
        break;

      case HCI_OPCODE_LE_RAND:
        /* check if need to send second rand command */
        if (randCnt == 0)
        {
          randCnt++;
          HciLeRandCmd();
        }
        else
        {
          /* last command in sequence; set resetting state and call callback */
          hciCb.resetting = FALSE;
          hdr.param = 0;
          hdr.event = HCI_RESET_SEQ_CMPL_CBACK_EVT;
          (*hciCb.evtCback)((hciEvt_t *) &hdr);
        }
        break;

      default:
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreVsCmdCmplRcvd
 *        
 *  \brief  Perform internal HCI processing of vendor specific command complete events.
 *
 *  \param  opcode  HCI command opcode.
 *  \param  pMsg    Pointer to input HCI event parameter byte stream.
 *  \param  len     Parameter byte stream length.
 *
 *  \return HCI callback event code or zero.
 */
/*************************************************************************************************/
uint8_t hciCoreVsCmdCmplRcvd(uint16_t opcode, uint8_t *pMsg, uint8_t len)
{
  return HCI_VENDOR_SPEC_CMD_CMPL_CBACK_EVT;
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreVsEvtRcvd
 *        
 *  \brief  Perform internal HCI processing of vendor specific HCI events.
 *
 *  \param  p       Pointer to input HCI event parameter byte stream.
 *  \param  len     Parameter byte stream length.
 *
 *  \return HCI callback event code or zero.
 */
/*************************************************************************************************/
uint8_t hciCoreVsEvtRcvd(uint8_t *p, uint8_t len)
{
  return HCI_VENDOR_SPEC_EVT;
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreHwErrorRcvd
 *        
 *  \brief  Perform internal HCI processing of hardware error event.
 *
 *  \param  p       Pointer to input HCI event parameter byte stream.
 *
 *  \return HCI callback event code or zero.
 */
/*************************************************************************************************/
uint8_t hciCoreHwErrorRcvd(uint8_t *p)
{
  return 0;
}

/*************************************************************************************************/
/*!
 *  \fn     HciVsInit
 *        
 *  \brief  Vendor-specific controller initialization function.
 *
 *  \param  param    Vendor-specific parameter.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciVsInit(uint8_t param)
{

}

#endif
