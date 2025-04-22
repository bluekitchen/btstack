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
#include "hci_drv.h"


/**************************************************************************************************
  Macros
**************************************************************************************************/

#define HCI_HDR_LEN_MAX           HCI_ACL_HDR_LEN

/**************************************************************************************************
  Data Types
**************************************************************************************************/

typedef enum
{
  HCI_RX_STATE_IDLE,
  HCI_RX_STATE_HEADER,
  HCI_RX_STATE_DATA,
  HCI_RX_STATE_COMPLETE
} hciRxState_t;

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
  uint16_t   len;

  /* get 16-bit length */
  BYTES_TO_UINT16(len, &pData[2]);
  len += HCI_ACL_HDR_LEN;

  /* dump event for protocol analysis */
  HCI_PDUMP_TX_ACL(len, pData);

  /* transmit ACL header and data */
  hciDrvWrite(HCI_ACL_TYPE, len, pData);

  /* free buffer */
  WsfMsgFree(pData);
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
  uint8_t   len;

  /* get length */
  len = pData[2] + HCI_CMD_HDR_LEN;
  
  /* dump event for protocol analysis */
  HCI_PDUMP_CMD(len, pData);

  /* transmit ACL header and data */
  hciDrvWrite(HCI_CMD_TYPE, len, pData);

  /* free command buffer */
  WsfMsgFree(pData);
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
  static uint8_t    stateRx = HCI_RX_STATE_IDLE;
  static uint8_t    pktIndRx;
  static uint16_t   iRx;
  static uint8_t    hdrRx[HCI_HDR_LEN_MAX];
  static uint8_t    *pPktRx;
  static uint8_t    *pDataRx;

  uint8_t dataByte;
  
  /* loop until all bytes of incoming buffer are handled */
  while (len--)
  {
    /* read single byte from incoming buffer and advance to next byte */
    dataByte = *pBuf++;

    /* --- Idle State --- */
    if (stateRx == HCI_RX_STATE_IDLE)
    {
      /* save the packet type */
      pktIndRx = dataByte;
      iRx      = 0;
      stateRx  = HCI_RX_STATE_HEADER;
    }
   
    /* --- Header State --- */
    else if (stateRx == HCI_RX_STATE_HEADER)
    {
      uint8_t  hdrLen = 0;
      uint16_t dataLen = 0;
      
      /* copy current byte into the temp header buffer */
      hdrRx[iRx++] = dataByte;
      
      /* determine header length based on packet type */
      if (pktIndRx == HCI_EVT_TYPE)
      {
        hdrLen = HCI_EVT_HDR_LEN;
      }
      else if (pktIndRx == HCI_ACL_TYPE)
      {
        hdrLen = HCI_ACL_HDR_LEN;
      }
      else
      {      
        /* invalid packet type */
        WSF_ASSERT(0);
      }
      
      /* see if entire header has been read */
      if (iRx == hdrLen)
      {
        /* extract data length from header */
        if (pktIndRx == HCI_EVT_TYPE)
        {
          dataLen = hdrRx[1];
        }
        else if (pktIndRx == HCI_ACL_TYPE)
        {
          BYTES_TO_UINT16(dataLen, &hdrRx[2]);
        }
        
        /* allocate data buffer to hold entire packet */
        if ((pPktRx = (uint8_t*)WsfMsgAlloc(hdrLen + dataLen)) != NULL)
        {
          pDataRx = pPktRx;
          
          /* copy header into data packet (note: memcpy is not so portable) */
          { 
            uint8_t  i;
            for (i = 0; i < hdrLen; i++)
            {
              *pDataRx++ = hdrRx[i];
            }
          }

          /* save number of bytes left to read */
          iRx = dataLen;
          if (iRx == 0)
          {
            stateRx = HCI_RX_STATE_COMPLETE;
          }
          else
          {
            stateRx = HCI_RX_STATE_DATA;
          }
        }
        else
        {
          WSF_ASSERT(0); /* allocate falied */
        }
        
      }
    }

    /* --- Data State --- */
    else if (stateRx == HCI_RX_STATE_DATA)
    {
      /* write incoming byte to allocated buffer */
      *pDataRx++ = dataByte;

      /* determine if entire packet has been read */
      iRx--;
      if (iRx == 0)
      {
        stateRx = HCI_RX_STATE_COMPLETE;
      }
    }
    
    /* --- Complete State --- */
    /* ( Note Well!  There is no else-if construct by design. ) */
    if (stateRx == HCI_RX_STATE_COMPLETE)
    {
      /* deliver data */
      if (pPktRx != NULL)
      {
        hciCoreRecv(pktIndRx, pPktRx);
      }
      
      /* reset state machine */
      stateRx = HCI_RX_STATE_IDLE;
    }
  }
}
