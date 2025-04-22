/*************************************************************************************************/
/*!
 *  \file   hci_core.h
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
#ifndef HCI_CORE_H
#define HCI_CORE_H

#include "wsf_queue.h"
#include "wsf_os.h"
#include "hci_api.h"
#include "cfg_stack.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* ACL buffer flow control watermark levels */
#define HCI_ACL_QUEUE_HI          5             /* Disable flow when this many buffers queued */
#define HCI_ACL_QUEUE_LO          1             /* Enable flow when this many buffers queued */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Per-connection structure for ACL packet accounting */
typedef struct
{
  uint16_t        handle;                       /* Connection handle */
  bool_t          flowDisabled;                 /* TRUE if data flow disabled */
  uint8_t         queuedBufs;                   /* Queued ACL buffers on this connection */
  uint8_t         outBufs;                      /* Outstanding ACL buffers sent to controller */
} hciCoreConn_t;

/* Main control block for dual-chip implementation */
typedef struct
{
  hciCoreConn_t   conn[DM_CONN_MAX];            /* Connection structures */
  uint8_t         leStates[HCI_LE_STATES_LEN];  /* Controller LE supported states */
  bdAddr_t        bdAddr;                       /* Bluetooth device address */
  wsfQueue_t      aclQueue;                     /* HCI ACL TX queue */
  uint16_t        bufSize;                      /* Controller ACL data buffer size */
  uint8_t         availBufs;                    /* Current avail ACL data buffers */
  uint8_t         numBufs;                      /* Controller number of ACL data buffers */
  uint8_t         whiteListSize;                /* Controller white list size */
  uint8_t         numCmdPkts;                   /* Controller command packed count */
  uint8_t         leSupFeat;                    /* Controller LE supported features */
  int8_t          advTxPwr;                     /* Controller advertising TX power */
} hciCoreCb_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Control block */
extern hciCoreCb_t hciCoreCb;

/* LE event mask */
extern const uint8_t hciLeEventMask[HCI_LE_EVT_MASK_LEN];

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void hciCoreInit(void);
void hciCoreResetStart(void);
void hciCoreResetSequence(uint8_t *pMsg);
void hciCoreConnOpen(uint8_t *pMsg);
void hciCoreConnClose(uint8_t *pMsg);
void hciCoreSendAclData(hciCoreConn_t *pConn, uint8_t *pData);
void hciCoreTxReady(uint8_t bufs);
void hciCoreNumCmplPkts(uint8_t *pMsg);
void hciCoreRecv(uint8_t msgType, uint8_t *pCoreRecvMsg);
uint8_t hciCoreVsCmdCmplRcvd(uint16_t opcode, uint8_t *pMsg, uint8_t len);

#ifdef __cplusplus
};
#endif

#endif /* HCI_CORE_H */
