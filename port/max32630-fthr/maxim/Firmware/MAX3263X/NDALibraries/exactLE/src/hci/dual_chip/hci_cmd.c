/*************************************************************************************************/
/*!
 *  \file   hci_cmd.c
 *
 *  \brief  HCI command module.
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
#include "wsf_queue.h"
#include "wsf_timer.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "hci_cmd.h"
#include "hci_tr.h"
#include "hci_api.h"
#include "hci_main.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* HCI command timeout in seconds */
#define HCI_CMD_TIMEOUT           2

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Local control block type */
typedef struct
{
  wsfTimer_t      cmdTimer;       /* HCI command timeout timer */
  wsfQueue_t      cmdQueue;       /* HCI command queue */
  uint16_t        cmdOpcode;      /* Opcode of last HCI command sent */
  uint8_t         numCmdPkts;     /* Number of outstanding HCI commands that can be sent */
} hciCmdCb_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Local control block */
hciCmdCb_t hciCmdCb;

/*************************************************************************************************/
/*!
 *  \fn     hciCmdAlloc
 *        
 *  \brief  Allocate an HCI command buffer and set the command header fields.
 *
 *  \param  opcode  Command opcode.
 *  \param  len     length of command parameters.
 *
 *  \return Pointer to WSF msg buffer.
 */
/*************************************************************************************************/
uint8_t *hciCmdAlloc(uint16_t opcode, uint8_t len)
{
  uint8_t   *p;
  
  /* allocate buffer */
  if ((p = WsfMsgAlloc(len + HCI_CMD_HDR_LEN)) != NULL)
  {
    /* set HCI command header */
    UINT16_TO_BSTREAM(p, opcode);
    UINT8_TO_BSTREAM(p, len);
    p -= HCI_CMD_HDR_LEN;
  }
  
  return p;
}

/*************************************************************************************************/
/*!
 *  \fn     hciCmdSend
 *        
 *  \brief  Send an HCI command and service the HCI command queue.
 *
 *  \param  pData  Buffer containing HCI command to send or NULL.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCmdSend(uint8_t *pData)
{
  uint8_t         *p;
  wsfHandlerId_t  handlerId;
  
  /* queue command if present */
  if (pData != NULL)
  {
    WsfMsgEnq(&hciCmdCb.cmdQueue, 0, pData);
  }
  
  /* service the HCI command queue; first check if controller can accept any commands */
  if (hciCmdCb.numCmdPkts > 0)
  {
    /* if queue not empty */
    if ((p = WsfMsgDeq(&hciCmdCb.cmdQueue, &handlerId)) != NULL)
    {
      /* decrement controller command packet count */
      hciCmdCb.numCmdPkts--;
      
      /* store opcode of command we're sending */
      BYTES_TO_UINT16(hciCmdCb.cmdOpcode, p);
      
      /* start command timeout */
      WsfTimerStartSec(&hciCmdCb.cmdTimer, HCI_CMD_TIMEOUT);
      
      /* send command to transport */
      hciTrSendCmd(p);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     hciCmdInit
 *        
 *  \brief  Initialize the HCI cmd module.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCmdInit(void)
{
  WSF_QUEUE_INIT(&hciCmdCb.cmdQueue);

  /* initialize numCmdPkts for special case of first command */
  hciCmdCb.numCmdPkts = 1;

  /* initialize timer */
  hciCmdCb.cmdTimer.msg.event = HCI_MSG_CMD_TIMEOUT;
  hciCmdCb.cmdTimer.handlerId = hciCb.handlerId;
}

/*************************************************************************************************/
/*!
 *  \fn     hciCmdTimeout
 *        
 *  \brief  Process an HCI command timeout.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCmdTimeout(wsfMsgHdr_t *pMsg)
{
  HCI_TRACE_INFO0("hciCmdTimeout");
}

/*************************************************************************************************/
/*!
 *  \fn     hciCmdRecvCmpl
 *        
 *  \brief  Process an HCI Command Complete or Command Status event.
 *
 *  \param  numCmdPkts  Number of commands that can be sent to the controller.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCmdRecvCmpl(uint8_t numCmdPkts)
{
  /* stop the command timeout timer */
  WsfTimerStop(&hciCmdCb.cmdTimer);
  
  /*
   * Set the number of commands that can be sent to the controller.  Setting this
   * to 1 rather than incrementing by numCmdPkts allows only one command at a time to
   * be sent to the controller and simplifies the code.
   */
  hciCmdCb.numCmdPkts = 1;
  
  /* send the next queued command */
  hciCmdSend(NULL);
}

/*************************************************************************************************/
/*!
 *  \fn     HciDisconnectCmd
 *        
 *  \brief  HCI disconnect command.
 */
/*************************************************************************************************/
void HciDisconnectCmd(uint16_t handle, uint8_t reason)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_DISCONNECT, HCI_LEN_DISCONNECT)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    UINT8_TO_BSTREAM(p, reason);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeAddDevWhiteListCmd
 *        
 *  \brief  HCI LE add device white list command.
 */
/*************************************************************************************************/
void HciLeAddDevWhiteListCmd(uint8_t addrType, uint8_t *pAddr)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_ADD_DEV_WHITE_LIST, HCI_LEN_LE_ADD_DEV_WHITE_LIST)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, addrType);
    BDA_TO_BSTREAM(p, pAddr);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeClearWhiteListCmd
 *        
 *  \brief  HCI LE clear white list command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciLeClearWhiteListCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_CLEAR_WHITE_LIST, HCI_LEN_LE_CLEAR_WHITE_LIST)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeConnUpdateCmd
 *        
 *  \brief  HCI connection update command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeConnUpdateCmd(uint16_t handle, hciConnSpec_t *pConnSpec)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_CONN_UPDATE, HCI_LEN_LE_CONN_UPDATE)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    UINT16_TO_BSTREAM(p, pConnSpec->connIntervalMin);
    UINT16_TO_BSTREAM(p, pConnSpec->connIntervalMax);
    UINT16_TO_BSTREAM(p, pConnSpec->connLatency);
    UINT16_TO_BSTREAM(p, pConnSpec->supTimeout);
    UINT16_TO_BSTREAM(p, pConnSpec->minCeLen);
    UINT16_TO_BSTREAM(p, pConnSpec->maxCeLen);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeCreateConnCmd
 *        
 *  \brief  HCI LE create connection command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeCreateConnCmd(uint16_t scanInterval, uint16_t scanWindow, uint8_t filterPolicy,
                        uint8_t peerAddrType, uint8_t *pPeerAddr, uint8_t ownAddrType,
                        hciConnSpec_t *pConnSpec)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_CREATE_CONN, HCI_LEN_LE_CREATE_CONN)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, scanInterval);
    UINT16_TO_BSTREAM(p, scanWindow);
    UINT8_TO_BSTREAM(p, filterPolicy);
    UINT8_TO_BSTREAM(p, peerAddrType);
    BDA_TO_BSTREAM(p, pPeerAddr);
    UINT8_TO_BSTREAM(p, ownAddrType);
    UINT16_TO_BSTREAM(p, pConnSpec->connIntervalMin);
    UINT16_TO_BSTREAM(p, pConnSpec->connIntervalMax);
    UINT16_TO_BSTREAM(p, pConnSpec->connLatency);
    UINT16_TO_BSTREAM(p, pConnSpec->supTimeout);
    UINT16_TO_BSTREAM(p, pConnSpec->minCeLen);
    UINT16_TO_BSTREAM(p, pConnSpec->maxCeLen);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeCreateConnCancelCmd
 *        
 *  \brief  HCI LE create connection cancel command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeCreateConnCancelCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_CREATE_CONN_CANCEL, HCI_LEN_LE_CREATE_CONN_CANCEL)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeEncryptCmd
 *        
 *  \brief  HCI LE encrypt command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeEncryptCmd(uint8_t *pKey, uint8_t *pData)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_ENCRYPT, HCI_LEN_LE_ENCRYPT)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    memcpy(p, pKey, HCI_KEY_LEN);
    p += HCI_KEY_LEN;
    memcpy(p, pData, HCI_ENCRYPT_DATA_LEN);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeLtkReqNegReplCmd
 *        
 *  \brief  HCI LE long term key request negative reply command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeLtkReqNegReplCmd(uint16_t handle)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_LTK_REQ_NEG_REPL, HCI_LEN_LE_LTK_REQ_NEG_REPL)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeLtkReqReplCmd
 *        
 *  \brief  HCI LE long term key request reply command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeLtkReqReplCmd(uint16_t handle, uint8_t *pKey)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_LTK_REQ_REPL, HCI_LEN_LE_LTK_REQ_REPL)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    memcpy(p, pKey, HCI_KEY_LEN);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeRandCmd
 *        
 *  \brief  HCI LE random command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeRandCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_RAND, HCI_LEN_LE_RAND)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeReadAdvTXPowerCmd
 *        
 *  \brief  HCI LE read advertising TX power command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadAdvTXPowerCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_READ_ADV_TX_POWER, HCI_LEN_LE_READ_ADV_TX_POWER)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeReadBufSizeCmd
 *        
 *  \brief  HCI LE read buffer size command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadBufSizeCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_READ_BUF_SIZE, HCI_LEN_LE_READ_BUF_SIZE)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeReadChanMapCmd
 *        
 *  \brief  HCI LE read channel map command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadChanMapCmd(uint16_t handle)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_READ_CHAN_MAP, HCI_LEN_LE_READ_CHAN_MAP)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeReadLocalSupFeatCmd
 *        
 *  \brief  HCI LE read local supported feautre command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadLocalSupFeatCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_READ_LOCAL_SUP_FEAT, HCI_LEN_LE_READ_LOCAL_SUP_FEAT)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeReadRemoteFeatCmd
 *        
 *  \brief  HCI LE read remote feature command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadRemoteFeatCmd(uint16_t handle)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_READ_REMOTE_FEAT, HCI_LEN_LE_READ_REMOTE_FEAT)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeReadSupStatesCmd
 *        
 *  \brief  HCI LE read supported states command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadSupStatesCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_READ_SUP_STATES, HCI_LEN_LE_READ_SUP_STATES)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeReadWhiteListSizeCmd
 *        
 *  \brief  HCI LE read white list size command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeReadWhiteListSizeCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_READ_WHITE_LIST_SIZE, HCI_LEN_LE_READ_WHITE_LIST_SIZE)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeRemoveDevWhiteListCmd
 *        
 *  \brief  HCI LE remove device white list command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeRemoveDevWhiteListCmd(uint8_t addrType, uint8_t *pAddr)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_REMOVE_DEV_WHITE_LIST, HCI_LEN_LE_REMOVE_DEV_WHITE_LIST)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, addrType);
    BDA_TO_BSTREAM(p, pAddr);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeSetAdvEnableCmd
 *        
 *  \brief  HCI LE set advanced enable command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetAdvEnableCmd(uint8_t enable)
{
  uint8_t *pBuf;
  uint8_t *p;

  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_ADV_ENABLE, HCI_LEN_LE_SET_ADV_ENABLE)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, enable);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeSetAdvDataCmd
 *        
 *  \brief  HCI LE set advertising data command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetAdvDataCmd(uint8_t len, uint8_t *pData)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_ADV_DATA, HCI_LEN_LE_SET_ADV_DATA)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, len);
    memcpy(p, pData, len);
    p += len;
    memset(p, 0, (HCI_ADV_DATA_LEN - len));
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeSetAdvParamCmd
 *        
 *  \brief  HCI LE set advertising parameters command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetAdvParamCmd(uint16_t advIntervalMin, uint16_t advIntervalMax, uint8_t advType,
                         uint8_t ownAddrType, uint8_t directAddrType, uint8_t *pDirectAddr,
                         uint8_t advChanMap, uint8_t advFiltPolicy)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_ADV_PARAM, HCI_LEN_LE_SET_ADV_PARAM)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, advIntervalMin);
    UINT16_TO_BSTREAM(p, advIntervalMax);
    UINT8_TO_BSTREAM(p, advType);
    UINT8_TO_BSTREAM(p, ownAddrType);
    UINT8_TO_BSTREAM(p, directAddrType);
    if (pDirectAddr != NULL)
    {
      BDA_TO_BSTREAM(p, pDirectAddr);
    }
    else
    {
      p = BdaClr(p);
    }
    UINT8_TO_BSTREAM(p, advChanMap);
    UINT8_TO_BSTREAM(p, advFiltPolicy);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeSetEventMaskCmd
 *        
 *  \brief  HCI LE set event mask command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetEventMaskCmd(uint8_t *pLeEventMask)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_EVENT_MASK, HCI_LEN_LE_SET_EVENT_MASK)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    memcpy(p, pLeEventMask, HCI_LE_EVT_MASK_LEN);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeSetHostChanClassCmd
 *        
 *  \brief  HCI set host channel class command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetHostChanClassCmd(uint8_t *pChanMap)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_HOST_CHAN_CLASS, HCI_LEN_LE_SET_HOST_CHAN_CLASS)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    memcpy(p, pChanMap, HCI_CHAN_MAP_LEN);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeSetRandAddrCmd
 *        
 *  \brief  HCI LE set random address command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetRandAddrCmd(uint8_t *pAddr)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_RAND_ADDR, HCI_LEN_LE_SET_RAND_ADDR)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    BDA_TO_BSTREAM(p, pAddr);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeSetScanEnableCmd
 *        
 *  \brief  HCI LE set scan enable command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetScanEnableCmd(uint8_t enable, uint8_t filterDup)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_SCAN_ENABLE, HCI_LEN_LE_SET_SCAN_ENABLE)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, enable);
    UINT8_TO_BSTREAM(p, filterDup);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeSetScanParamCmd
 *        
 *  \brief  HCI set scan parameters command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetScanParamCmd(uint8_t scanType, uint16_t scanInterval, uint16_t scanWindow,
                          uint8_t ownAddrType, uint8_t scanFiltPolicy)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_SCAN_PARAM, HCI_LEN_LE_SET_SCAN_PARAM)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, scanType);
    UINT16_TO_BSTREAM(p, scanInterval);
    UINT16_TO_BSTREAM(p, scanWindow);
    UINT8_TO_BSTREAM(p, ownAddrType);
    UINT8_TO_BSTREAM(p, scanFiltPolicy);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeSetScanRespDataCmd
 *        
 *  \brief  HCI LE set scan response data.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeSetScanRespDataCmd(uint8_t len, uint8_t *pData)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_SET_SCAN_RESP_DATA, HCI_LEN_LE_SET_SCAN_RESP_DATA)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT8_TO_BSTREAM(p, len);
    memcpy(p, pData, len);
    p += len;
    memset(p, 0, (HCI_SCAN_DATA_LEN - len));
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciLeStartEncryptionCmd
 *        
 *  \brief  HCI LE start encryption command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciLeStartEncryptionCmd(uint16_t handle, uint8_t *pRand, uint16_t diversifier, uint8_t *pKey)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_LE_START_ENCRYPTION, HCI_LEN_LE_START_ENCRYPTION)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    memcpy(p, pRand, HCI_RAND_LEN);
    p += HCI_RAND_LEN;
    UINT16_TO_BSTREAM(p, diversifier);
    memcpy(p, pKey, HCI_KEY_LEN);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciReadBdAddrCmd
 *        
 *  \brief  HCI read BD address command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciReadBdAddrCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_READ_BD_ADDR, HCI_LEN_READ_BD_ADDR)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciReadBufSizeCmd
 *        
 *  \brief  HCI read buffer size command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciReadBufSizeCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_READ_BUF_SIZE, HCI_LEN_READ_BUF_SIZE)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciReadLocalSupFeatCmd
 *        
 *  \brief  HCI read local supported feature command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciReadLocalSupFeatCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_READ_LOCAL_SUP_FEAT, HCI_LEN_READ_LOCAL_SUP_FEAT)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciReadLocalVerInfoCmd
 *        
 *  \brief  HCI read local version info command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciReadLocalVerInfoCmd(void)
{
  uint8_t *pBuf;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_READ_LOCAL_VER_INFO, HCI_LEN_READ_LOCAL_VER_INFO)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciReadRemoteVerInfoCmd
 *        
 *  \brief  HCI read remote version info command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciReadRemoteVerInfoCmd(uint16_t handle)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_READ_REMOTE_VER_INFO, HCI_LEN_READ_REMOTE_VER_INFO)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciReadRssiCmd
 *        
 *  \brief  HCI read RSSI command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciReadRssiCmd(uint16_t handle)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_READ_RSSI, HCI_LEN_READ_RSSI)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciReadTxPwrLvlCmd
 *        
 *  \brief  HCI read Tx power level command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciReadTxPwrLvlCmd(uint16_t handle, uint8_t type)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_READ_TX_PWR_LVL, HCI_LEN_READ_TX_PWR_LVL)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    UINT16_TO_BSTREAM(p, handle);
    UINT8_TO_BSTREAM(p, type);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciResetCmd
 *        
 *  \brief  HCI reset command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciResetCmd(void)
{
  uint8_t *pBuf;
  
  /* initialize numCmdPkts for special case of reset command */
  hciCmdCb.numCmdPkts = 1;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_RESET, HCI_LEN_RESET)) != NULL)
  {
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciSetEventMaskCmd
 *        
 *  \brief  HCI set event mask command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciSetEventMaskCmd(uint8_t *pEventMask)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(HCI_OPCODE_SET_EVENT_MASK, HCI_LEN_SET_EVENT_MASK)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    memcpy(p, pEventMask, HCI_EVT_MASK_LEN);
    hciCmdSend(pBuf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HciVendorSpecificCmd
 *        
 *  \brief  HCI vencor specific command.
 *        
 *  \return None.
 */
/*************************************************************************************************/
void HciVendorSpecificCmd(uint16_t opcode, uint8_t len, uint8_t *pData)
{
  uint8_t *pBuf;
  uint8_t *p;
  
  if ((pBuf = hciCmdAlloc(opcode, len)) != NULL)
  {
    p = pBuf + HCI_CMD_HDR_LEN;
    memcpy(p, pData, len);
    hciCmdSend(pBuf);
  }
}

