/*************************************************************************************************/
/*!
 *  \file   svc_bps.h
 *        
 *  \brief  Example Blood Pressure service implementation.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
 *  
 *  Copyright (c) 2012 Wicentric, Inc., all rights reserved.
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

#ifndef SVC_BPS_H
#define SVC_BPS_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 Handle Ranges
**************************************************************************************************/

/* Blood Pressure Service */
#define BPS_START_HDL               0xE0
#define BPS_END_HDL                 (BPS_MAX_HDL - 1)

/**************************************************************************************************
 Handles
**************************************************************************************************/

/* Blood Pressure Service Handles */
enum
{
  BPS_SVC_HDL = BPS_START_HDL,      /* Blood pressure service declaration */
  BPS_BPM_CH_HDL,                   /* Blood pressure measurement characteristic */ 
  BPS_BPM_HDL,                      /* Blood pressure measurement */
  BPS_BPM_CH_CCC_HDL,               /* Blood pressure measurement client characteristic configuration */
  BPS_ICP_CH_HDL,                   /* Intermediate cuff pressure characteristic */ 
  BPS_ICP_HDL,                      /* Intermediate cuff pressure */
  BPS_ICP_CH_CCC_HDL,               /* Intermediate cuff pressure client characteristic configuration */
  BPS_BPF_CH_HDL,                   /* Blood pressure feature characteristic */ 
  BPS_BPF_HDL,                      /* Blood pressure feature */
  BPS_MAX_HDL
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void SvcBpsAddGroup(void);
void SvcBpsRemoveGroup(void);
void SvcBpsCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback);

#ifdef __cplusplus
};
#endif

#endif /* SVC_BPS_H */
