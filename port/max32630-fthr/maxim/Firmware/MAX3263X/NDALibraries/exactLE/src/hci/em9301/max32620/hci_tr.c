/*******************************************************************************
 * Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 * $Date: 2016-06-01 11:06:52 -0500 (Wed, 01 Jun 2016) $ 
 * $Revision: 23137 $
 *
 ******************************************************************************/
 
#include <string.h>
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

typedef enum {
  HCI_RX_STATE_IDLE,
  HCI_RX_STATE_HEADER,
  HCI_RX_STATE_DATA_LEN,
  HCI_RX_STATE_DATA,
  HCI_RX_STATE_COMPLETE
} hciRxState_t;

/**************************************************************************************************
  File Scope Data
**************************************************************************************************/

static volatile uint8_t stateRx = HCI_RX_STATE_IDLE;
static uint8_t hdrRx[HCI_HDR_LEN_MAX + 1];
static uint8_t *pPktRx = NULL;

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
}


/*************************************************************************************************/
/*!
 *  \fn     hciSerialRxIncoming
 *        
 *  \brief  Receive function.  Gets called by external code when bytes are received.
 *
 *  \param  len    Number of bytes in incoming buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciTrRxIncoming(uint8_t len)
{
  /* --- Header State --- */
  if (stateRx == HCI_RX_STATE_HEADER)
  {
    WSF_ASSERT(len == 3);

    /* determine header length based on packet type */
    if (hdrRx[0] == HCI_EVT_TYPE) {
      uint16_t dataLen = hdrRx[2];

      /* allocate data buffer to hold entire packet */
      if ((pPktRx = (uint8_t*)WsfMsgAlloc(HCI_EVT_HDR_LEN + dataLen)) != NULL) {

        /* copy header into data packet */
        memcpy(pPktRx, &hdrRx[1], HCI_EVT_HDR_LEN);

        /* save number of bytes left to read */
        if (dataLen == 0) {
          stateRx = HCI_RX_STATE_COMPLETE;
        } else {
          /* Read the data */
          stateRx = HCI_RX_STATE_DATA;
          if (hciDrvRead(dataLen, &pPktRx[HCI_EVT_HDR_LEN], 1) == 0) {
            stateRx = HCI_RX_STATE_IDLE;
          }
        }
      }
      else {
        WSF_ASSERT(0); /* allocate falied */
      }
    }
    else if (hdrRx[0] == HCI_ACL_TYPE) {
      /* Read the rest of the header */
      stateRx = HCI_RX_STATE_DATA_LEN;
      if (hciDrvRead(2, &hdrRx[3], 0) == 0) {
        stateRx = HCI_RX_STATE_IDLE;
      }
    } else {      
      /* invalid packet type */
      WSF_ASSERT(0);
    }
  }

  /* --- Data Length State --- */
  else if (stateRx == HCI_RX_STATE_DATA_LEN)
  {
    WSF_ASSERT(len == 2);

    uint16_t dataLen;
    BYTES_TO_UINT16(dataLen, &hdrRx[3]);

    /* allocate data buffer to hold entire packet */
    if ((pPktRx = (uint8_t*)WsfMsgAlloc(HCI_ACL_HDR_LEN + dataLen)) != NULL)
    {
      /* copy header into data packet */
      memcpy(pPktRx, &hdrRx[1], HCI_ACL_HDR_LEN);

      /* save number of bytes left to read */
      if (dataLen == 0) {
        stateRx = HCI_RX_STATE_COMPLETE;
      } else {
        /* Read the data */
        stateRx = HCI_RX_STATE_DATA;
        if (hciDrvRead(dataLen, &pPktRx[HCI_ACL_HDR_LEN], 1) == 0) {
          stateRx = HCI_RX_STATE_IDLE;
        }
      }
    }
    else {
      WSF_ASSERT(0); /* allocate falied */
    }
  }

  /* --- Data State --- */
  else if (stateRx == HCI_RX_STATE_DATA)
  {
    uint16_t dataLen = 0;

    if (hdrRx[0] == HCI_EVT_TYPE) {
      dataLen = hdrRx[2];
    } else if (hdrRx[0] == HCI_ACL_TYPE) {
      BYTES_TO_UINT16(dataLen, &hdrRx[3]);
    } else {      
      WSF_ASSERT(0);  /* invalid packet type */
    }

    if (len == dataLen) {
      stateRx = HCI_RX_STATE_COMPLETE;
    } else {
      WSF_ASSERT(0); /* incorrect number of bytes received */
    }
  }

  /* --- Complete State --- */
  /* ( Note Well!  There is no else-if construct by design. ) */
  if (stateRx == HCI_RX_STATE_COMPLETE)
  {
    /* deliver data */
    if (pPktRx != NULL) {
      hciCoreRecv(hdrRx[0], pPktRx);
      pPktRx = NULL;
    }

    /* reset state machine */
    stateRx = HCI_RX_STATE_IDLE;
  }
}


/*************************************************************************************************/
void hciTrRxStart(void)
{
  WSF_ASSERT(stateRx == HCI_RX_STATE_IDLE);

  stateRx = HCI_RX_STATE_HEADER;

  if (hciDrvRead(3, hdrRx, 0) == 0) {
    stateRx = HCI_RX_STATE_IDLE;
  }
}


/*************************************************************************************************/
void hciTrReset(void)
{
  stateRx = HCI_RX_STATE_IDLE;
  if (pPktRx != NULL) {
    WsfMsgFree(pPktRx);
  }
}
