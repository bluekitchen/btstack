/*************************************************************************************************/
/*!
 *  \file   svc_wss.c
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

#include "wsf_types.h"
#include "att_api.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "svc_ch.h"
#include "svc_wss.h"
#include "svc_cfg.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Characteristic read permissions */
#ifndef WSS_SEC_PERMIT_READ
#define WSS_SEC_PERMIT_READ (ATTS_PERMIT_READ | ATTS_PERMIT_READ_ENC)
#endif

/*! Characteristic write permissions */
#ifndef WSS_SEC_PERMIT_WRITE
#define WSS_SEC_PERMIT_WRITE  (ATTS_PERMIT_WRITE | ATTS_PERMIT_WRITE_ENC)
#endif

/**************************************************************************************************
 Service variables
**************************************************************************************************/

/* Weight scale service declaration */
static const uint8_t wssValSvc[] = {UINT16_TO_BYTES(ATT_UUID_WEIGHT_SCALE_SERVICE)};
static const uint16_t wssLenSvc = sizeof(wssValSvc);

/* Weight measurement characteristic */ 
static const uint8_t wssValWmCh[] = {ATT_PROP_INDICATE, UINT16_TO_BYTES(WSS_WM_HDL), UINT16_TO_BYTES(ATT_UUID_WEIGHT_MEAS)};
static const uint16_t wssLenWmCh = sizeof(wssValWmCh);

/* Weight measurement */
/* Note these are dummy values */
static const uint8_t wssValWm[] = {0};
static const uint16_t wssLenWm = sizeof(wssValWm);

/* Weight measurement client characteristic configuration */
static uint8_t wssValWmChCcc[] = {UINT16_TO_BYTES(0x0000)};
static const uint16_t wssLenWmChCcc = sizeof(wssValWmChCcc);

/* Weight scale feature characteristic */ 
static const uint8_t wssValWsfCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(WSS_WSF_HDL), UINT16_TO_BYTES(ATT_UUID_WEIGHT_SCALE_FEATURE)};
static const uint16_t wssLenWsfCh = sizeof(wssValWsfCh);

/* Weight scale feature */
static uint8_t wssValWsf[] = {UINT16_TO_BYTES(CH_WSF_FLAG_TIMESTAMP)};
static const uint16_t wssLenWsf = sizeof(wssValWsf);

/* Attribute list for WSS group */
static const attsAttr_t wssList[] =
{
  /* Weight scale service declaration */
  {
    attPrimSvcUuid, 
    (uint8_t *) wssValSvc,
    (uint16_t *) &wssLenSvc, 
    sizeof(wssValSvc),
    0,
    ATTS_PERMIT_READ
  },
  /* Weight measurement characteristic */ 
  {
    attChUuid,
    (uint8_t *) wssValWmCh,
    (uint16_t *) &wssLenWmCh,
    sizeof(wssValWmCh),
    0,
    ATTS_PERMIT_READ
  },
  /* Weight measurement */
  {
    attWmChUuid,
    (uint8_t *) wssValWm,
    (uint16_t *) &wssLenWm,
    sizeof(wssValWm),
    0,
    0
  },
  /* Weight measurement client characteristic configuration */
  {
    attCliChCfgUuid,
    (uint8_t *) wssValWmChCcc,
    (uint16_t *) &wssLenWmChCcc,
    sizeof(wssValWmChCcc),
    ATTS_SET_CCC,
    (ATTS_PERMIT_READ | WSS_SEC_PERMIT_WRITE)
  },
  /* Weight scale feature characteristic */ 
  {
    attChUuid,
    (uint8_t *) wssValWsfCh,
    (uint16_t *) &wssLenWsfCh,
    sizeof(wssValWsfCh),
    0,
    ATTS_PERMIT_READ
  },
  /* Weight scale feature */
  {
    attWsfChUuid,
    wssValWsf,
    (uint16_t *) &wssLenWsf,
    sizeof(wssValWsf),
    0,
    WSS_SEC_PERMIT_READ
  }  
};

/* WSS group structure */
static attsGroup_t svcWssGroup =
{
  NULL,
  (attsAttr_t *) wssList,
  NULL,
  NULL,
  WSS_START_HDL,
  WSS_END_HDL
};

/*************************************************************************************************/
/*!
 *  \fn     SvcWssAddGroup
 *        
 *  \brief  Add the services to the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcWssAddGroup(void)
{
  AttsAddGroup(&svcWssGroup);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcWssRemoveGroup
 *        
 *  \brief  Remove the services from the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcWssRemoveGroup(void)
{
  AttsRemoveGroup(WSS_START_HDL);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcWssCbackRegister
 *        
 *  \brief  Register callbacks for the service.
 *
 *  \param  readCback   Read callback function.
 *  \param  writeCback  Write callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcWssCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback)
{
  svcWssGroup.readCback = readCback;
  svcWssGroup.writeCback = writeCback;
}
