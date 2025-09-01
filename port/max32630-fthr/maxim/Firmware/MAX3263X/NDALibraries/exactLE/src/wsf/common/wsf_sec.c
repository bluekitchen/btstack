/*************************************************************************************************/
/*!
 *  \file   wsf_sec.c
 *        
 *  \brief  AES and random number security service implemented using HCI.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
 *  
 *  Copyright (c) 2010 Wicentric, Inc., all rights reserved.
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
#include "wsf_queue.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "wsf_sec.h"
#include "hci_api.h"
#include "calc128.h"

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Struct for request parameters and callback parameters */
typedef struct
{
  wsfSecAes_t       aes;
  uint8_t           ciphertext[HCI_ENCRYPT_DATA_LEN];
} wsfSecQueueBuf_t;

/* Control block */
typedef struct
{
  uint8_t           rand[HCI_RAND_LEN*2]; /* Random data buffer */
  wsfQueue_t        aesQueue;             /* Queue for AES requests */
  uint8_t           token;                /* Token value */
} wsfSecCb_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Control block */
static wsfSecCb_t wsfSecCb;

/*************************************************************************************************/
/*!
 *  \fn     wsfSecHciCback
 *        
 *  \brief  Callback for HCI encryption and random number events.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void wsfSecHciCback(hciEvt_t *pEvent)
{
  wsfSecQueueBuf_t  *pBuf;
  wsfHandlerId_t    handlerId;
  
  /* handle random number event */
  if (pEvent->hdr.event == HCI_LE_RAND_CMD_CMPL_CBACK_EVT)
  {
    /* move up data by eight bytes */    
    memcpy(&wsfSecCb.rand[HCI_RAND_LEN], wsfSecCb.rand, HCI_RAND_LEN);
    
    /* copy new data to random data buffer */
    memcpy(wsfSecCb.rand, pEvent->leRandCmdCmpl.randNum, HCI_RAND_LEN);
  }
  /* handle encryption event */
  else if (pEvent->hdr.event == HCI_LE_ENCRYPT_CMD_CMPL_CBACK_EVT)
  {
    /* dequeue parameter buffer */
    if ((pBuf = WsfMsgDeq(&wsfSecCb.aesQueue, &handlerId)) != NULL)
    {
      /* set encrypted data pointer and copy */
      pBuf->aes.pCiphertext = pBuf->ciphertext;
      Calc128Cpy(pBuf->aes.pCiphertext, pEvent->leEncryptCmdCmpl.data);
      
      /* send message */
      WsfMsgSend(handlerId, pBuf);
    }
    else
    {
      WSF_TRACE_WARN0("WSF sec queue empty!");
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     WsfSecInit
 *        
 *  \brief  Initialize the security service.  This function should only be called once
 *          upon system initialization.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfSecInit(void)
{
  WSF_QUEUE_INIT(&wsfSecCb.aesQueue);
  wsfSecCb.token = 0;
  
  /* Register callback with HCI */
  HciSecRegister(wsfSecHciCback);
}

/*************************************************************************************************/
/*!
 *  \fn     WsfSecRandInit
 *        
 *  \brief  Initialize the random number service.  This function should only be called once
 *          upon system initialization.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfSecRandInit(void)
{
  /* get new random numbers */
  HciLeRandCmd();
  HciLeRandCmd();  
}

/*************************************************************************************************/
/*!
 *  \fn     WsfSecAes
 *        
 *  \brief  Execute an AES calculation.  When the calculation completes, a WSF message will be
 *          sent to the specified handler.  This function returns a token value that
 *          the client can use to match calls to this function with messages.
 *
 *  \param  pKey        Pointer to 16 byte key.
 *  \param  pPlaintext  Pointer to 16 byte plaintext.
 *  \param  handlerId   WSF handler ID.
 *  \param  param       Client-defined parameter returned in message.
 *  \param  event       Event for client's WSF handler.
 *
 *  \return Token value.
 */
/*************************************************************************************************/
uint8_t WsfSecAes(uint8_t *pKey, uint8_t *pPlaintext, wsfHandlerId_t handlerId,
                  uint16_t param, uint8_t event)
{
  wsfSecQueueBuf_t  *pBuf;
  
  /* allocate a buffer */
  if ((pBuf = WsfMsgAlloc(sizeof(wsfSecQueueBuf_t))) != NULL)
  {
    pBuf->aes.hdr.status = wsfSecCb.token++;
    pBuf->aes.hdr.param = param;
    pBuf->aes.hdr.event = event;
    
    /* queue buffer */
    WsfMsgEnq(&wsfSecCb.aesQueue, handlerId, pBuf);
    
    /* call HCI encrypt function */
    HciLeEncryptCmd(pKey, pPlaintext);
  }
  
  return pBuf->aes.hdr.status;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfSecRand
 *        
 *  \brief  This function returns up to 16 bytes of random data to a buffer provided by the
 *          client.
 *
 *  \param  pRand       Pointer to returned random data.
 *  \param  randLen     Length of random data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfSecRand(uint8_t *pRand, uint8_t randLen)
{
  /* copy data */
  memcpy(pRand, wsfSecCb.rand, randLen);
  
  /* get new random numbers */
  HciLeRandCmd();
  if (randLen > HCI_RAND_LEN)
  {
    HciLeRandCmd();
  }
}

