/*************************************************************************************************/
/*!
 *  \file   svc_px.h
 *        
 *  \brief  Example Proximity services implementation.
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

#ifndef SVC_PX_H
#define SVC_PX_H

#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 Handle Ranges
**************************************************************************************************/

/* Start and end handle */
#define PX_START_HDL                      0x50
#define PX_END_HDL                        (PX_MAX_HDL - 1)

/**************************************************************************************************
 Handles
**************************************************************************************************/

/* Service Handles */
enum
{
  LLS_SVC_HDL = PX_START_HDL,       /* Link loss service declaration */
  LLS_AL_CH_HDL,                    /* Alert level characteristic */ 
  LLS_AL_HDL,                       /* Alert level */
  IAS_SVC_HDL,                      /* Immediate alert service declaration */
  IAS_AL_CH_HDL,                    /* Alert level characteristic */ 
  IAS_AL_HDL,                       /* Alert level */
  TXS_SVC_HDL,                      /* TX power service declaration */
  TXS_TX_CH_HDL,                    /* TX power level characteristic */ 
  TXS_TX_HDL,                       /* TX power level */
  PX_MAX_HDL
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void SvcPxAddGroup(void);
void SvcPxRemoveGroup(void);
void SvcPxCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback);

#ifdef __cplusplus
};
#endif

#endif /* SVC_PX_H */

