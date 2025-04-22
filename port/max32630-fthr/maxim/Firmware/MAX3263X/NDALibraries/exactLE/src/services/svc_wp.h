/*************************************************************************************************/
/*!
 *  \file   svc_wp.h
 *        
 *  \brief  Wicentric proprietary service implementation.
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

#ifndef SVC_WP_H
#define SVC_WP_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/**************************************************************************************************
 Handle Ranges
**************************************************************************************************/

/* Proprietary Service */
#define WP_START_HDL               0x200
#define WP_END_HDL                 (WP_MAX_HDL - 1)

/**************************************************************************************************
 Handles
**************************************************************************************************/

/* Proprietary Service Handles */
enum
{
  WP_SVC_HDL = WP_START_HDL,       /* Proprietary service declaration */
  WP_DAT_CH_HDL,                   /* Proprietary data characteristic */ 
  WP_DAT_HDL,                      /* Proprietary data */
  WP_DAT_CH_CCC_HDL,               /* Proprietary data client characteristic configuration */
  WP_MAX_HDL
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void SvcWpAddGroup(void);
void SvcWpRemoveGroup(void);
void SvcWpCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback);

#ifdef __cplusplus
};
#endif

#endif /* SVC_WP_H */
