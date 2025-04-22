/*************************************************************************************************/
/*!
 *  \file   svc_dis.h
 *        
 *  \brief  Example Device Information Service implementation.
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

#ifndef SVC_DIS_H
#define SVC_DIS_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 Handle Ranges
**************************************************************************************************/

/* Device Information Service */
#define DIS_START_HDL               0x30
#define DIS_END_HDL                 (DIS_MAX_HDL - 1)

/**************************************************************************************************
 Handles
**************************************************************************************************/

/* Device Information Service Handles */
enum
{
  DIS_SVC_HDL = DIS_START_HDL,      /* Information service declaration */
  DIS_MFR_CH_HDL,                   /* Manufacturer name string characteristic */
  DIS_MFR_HDL,                      /* Manufacturer name string */
  DIS_SID_CH_HDL,                   /* System ID characteristic */
  DIS_SID_HDL,                      /* System ID */
  DIS_MN_CH_HDL,                    /* Model number string characteristic */
  DIS_MN_HDL,                       /* Model number string */
  DIS_SN_CH_HDL,                    /* Serial number string characteristic */
  DIS_SN_HDL,                       /* Serial number string */
  DIS_FWR_CH_HDL,                   /* Firmware revision string characteristic */
  DIS_FWR_HDL,                      /* Firmware revision string */
  DIS_HWR_CH_HDL,                   /* Hardware revision string characteristic */
  DIS_HWR_HDL,                      /* Hardware revision string */
  DIS_SWR_CH_HDL,                   /* Software revision string characteristic */
  DIS_SWR_HDL,                      /* Software revision string */
  DIS_RCD_CH_HDL,                   /* Registration certificate data characteristic */
  DIS_RCD_HDL,                      /* Registration certificate data */
  DIS_MAX_HDL
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void SvcDisAddGroup(void);
void SvcDisRemoveGroup(void);

#ifdef __cplusplus
};
#endif

#endif /* SVC_DIS_H */

