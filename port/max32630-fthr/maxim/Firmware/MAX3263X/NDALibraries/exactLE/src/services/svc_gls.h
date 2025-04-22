/*************************************************************************************************/
/*!
 *  \file   svc_gls.h
 *        
 *  \brief  Example Glucose service implementation.
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

#ifndef SVC_GLS_H
#define SVC_GLS_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Error Codes */
#define GLS_ERR_IN_PROGRESS       0x80    /* Procedure already in progress */
#define GLS_ERR_CCCD              0x81    /* CCCD improperly configured */

/**************************************************************************************************
 Handle Ranges
**************************************************************************************************/

/* Glucose Service */
#define GLS_START_HDL               0xF0
#define GLS_END_HDL                 (GLS_MAX_HDL - 1)

/**************************************************************************************************
 Handles
**************************************************************************************************/

/* Glucose Service Handles */
enum
{
  GLS_SVC_HDL = GLS_START_HDL,      /* Glucose service declaration */
  GLS_GLM_CH_HDL,                   /* Glucose measurement characteristic */ 
  GLS_GLM_HDL,                      /* Glucose measurement */
  GLS_GLM_CH_CCC_HDL,               /* Glucose measurement client characteristic configuration */
  GLS_GLMC_CH_HDL,                  /* Glucose measurement context characteristic */ 
  GLS_GLMC_HDL,                     /* Glucose measurement context */
  GLS_GLMC_CH_CCC_HDL,              /* Glucose measurement context client characteristic configuration */
  GLS_GLF_CH_HDL,                   /* Glucose feature characteristic */ 
  GLS_GLF_HDL,                      /* Glucose feature */
  GLS_RACP_CH_HDL,                  /* Record access control point characteristic */
  GLS_RACP_HDL,                     /* Record access control point */
  GLS_RACP_CH_CCC_HDL,              /* Record access control point client characteristic configuration */
  GLS_MAX_HDL
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void SvcGlsAddGroup(void);
void SvcGlsRemoveGroup(void);
void SvcGlsCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback);

#ifdef __cplusplus
};
#endif

#endif /* SVC_GLS_H */
