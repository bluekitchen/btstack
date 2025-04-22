/*************************************************************************************************/
/*!
 *  \file   svc_core.h
 *        
 *  \brief  Example GATT and GAP service implementations.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
 *  
 *  Copyright (c) 2009-2011 Wicentric, Inc., all rights reserved.
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

#ifndef SVC_CORE_H
#define SVC_CORE_H


#ifdef __cplusplus
extern "C" {
#endif


/**************************************************************************************************
 Handle Ranges
**************************************************************************************************/

/* GAP */
#define GAP_START_HDL               0x01
#define GAP_END_HDL                 (GAP_MAX_HDL - 1)

/* GATT */
#define GATT_START_HDL              0x10
#define GATT_END_HDL                (GATT_MAX_HDL - 1)

/**************************************************************************************************
 Handles
**************************************************************************************************/

/* GAP Handles */
enum
{
  GAP_SVC_HDL = GAP_START_HDL,      /* GAP service declaration */
  GAP_DN_CH_HDL,                    /* Device name characteristic */ 
  GAP_DN_HDL,                       /* Device name */
  GAP_AP_CH_HDL,                    /* Appearance characteristic */
  GAP_AP_HDL,                       /* Appearance */
  GAP_MAX_HDL
};

/* GATT Handles */
enum
{
  GATT_SVC_HDL = GATT_START_HDL,    /* GATT service declaration */
  GATT_SC_CH_HDL,                   /* Service changed characteristic */ 
  GATT_SC_HDL,                      /* Service changed */  
  GATT_SC_CH_CCC_HDL,               /* Service changed client characteristic configuration descriptor*/
  GATT_MAX_HDL
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void SvcCoreAddGroup(void);
void SvcCoreRemoveGroup(void);
void SvcCoreGattCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback);
void SvcCoreGapCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback);

#ifdef __cplusplus
};
#endif

#endif /* SVC_CORE_H */

