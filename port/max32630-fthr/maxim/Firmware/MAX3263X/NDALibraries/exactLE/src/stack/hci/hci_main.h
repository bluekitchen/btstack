/*************************************************************************************************/
/*!
 *  \file   hci_main.h
 *
 *  \brief  HCI main module.
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
#ifndef HCI_MAIN_H
#define HCI_MAIN_H

#include "wsf_os.h"
#include "wsf_queue.h"
#include "hci_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Message types for HCI event handler */
#define HCI_MSG_CMD_TIMEOUT       1         /* Command timeout timer expired */

/* Event types for HCI event handler */
#define HCI_EVT_RX                0x01      /* Data received on rx queue */

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Type used in number of completed packets event type */
typedef struct
{
  uint16_t              handle;
  uint16_t              num;
} hciNumPkts_t;

/* Type for HCI number of completed packets event */
typedef struct
{
  uint8_t               numHandles;
  hciNumPkts_t          numPkts[1];
} hciNumCmplPktsEvt_t;

/* Main control block of the HCI subsystem */
typedef struct
{
  wsfQueue_t            rxQueue;
  hciEvtCback_t         evtCback;
  hciSecCback_t         secCback;
  hciAclCback_t         aclCback;
  hciFlowCback_t        flowCback;
  wsfHandlerId_t        handlerId;
  bool_t                resetting;
} hciCb_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Control block */
extern hciCb_t hciCb;

#ifdef __cplusplus
};
#endif

#endif /* HCI_MAIN_H */
