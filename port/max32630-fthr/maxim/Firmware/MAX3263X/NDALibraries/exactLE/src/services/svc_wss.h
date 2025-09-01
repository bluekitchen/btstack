/*************************************************************************************************/
/*!
 *  \file   svc_wss.h
 *        
 *  \brief  Example Weight Scale service implementation.
 *
 *          $Date: 2015-08-25 13:59:48 -0500 (Tue, 25 Aug 2015) $
 *          $Revision: 18761 $
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

#ifndef SVC_WSS_H
#define SVC_WSS_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 Handle Ranges
**************************************************************************************************/

/* Weight Scale Service */
#define WSS_START_HDL               0x0100
#define WSS_END_HDL                 (WSS_MAX_HDL - 1)

/**************************************************************************************************
 Handles
**************************************************************************************************/

/* Weight Scale Service Handles */
enum
{
  WSS_SVC_HDL = WSS_START_HDL,      /* Weight scale service declaration */
  WSS_WM_CH_HDL,                    /* Weight measurement characteristic */ 
  WSS_WM_HDL,                       /* Weight measurement */
  WSS_WM_CH_CCC_HDL,                /* Weight measurement client characteristic configuration */
  WSS_WSF_CH_HDL,                   /* Weight scale feature characteristic */ 
  WSS_WSF_HDL,                      /* Weight scale feature */
  WSS_MAX_HDL
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void SvcWssAddGroup(void);
void SvcWssRemoveGroup(void);
void SvcWssCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback);

#ifdef __cplusplus
};
#endif

#endif /* SVC_WSS_H */
