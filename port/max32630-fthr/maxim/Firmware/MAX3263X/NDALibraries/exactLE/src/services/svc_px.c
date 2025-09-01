/*************************************************************************************************/
/*!
 *  \file   svc_px.c
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

#include "wsf_types.h"
#include "att_api.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "svc_px.h"
#include "svc_cfg.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Characteristic read permissions */
#ifndef PX_SEC_PERMIT_READ
#define PX_SEC_PERMIT_READ SVC_SEC_PERMIT_READ
#endif

/*! Characteristic write permissions */
#ifndef PX_SEC_PERMIT_WRITE
#define PX_SEC_PERMIT_WRITE SVC_SEC_PERMIT_WRITE
#endif

/**************************************************************************************************
 Static Variables
**************************************************************************************************/

/* UUIDs */
static const uint8_t svcAlLvlUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_ALERT_LEVEL)};
static const uint8_t svcTxPwrUuid[ATT_16_UUID_LEN] = {UINT16_TO_BYTES(ATT_UUID_TX_POWER_LEVEL)};

/**************************************************************************************************
 Service variables
**************************************************************************************************/

/* Link loss service declaration */
static const uint8_t llsValSvc[] = {UINT16_TO_BYTES(ATT_UUID_LINK_LOSS_SERVICE)};
static const uint16_t llsLenSvc = sizeof(llsValSvc);

/* Link loss alert level characteristic */
static const uint8_t llsValAlCh[] = {ATT_PROP_READ | ATT_PROP_WRITE, UINT16_TO_BYTES(LLS_AL_HDL), UINT16_TO_BYTES(ATT_UUID_ALERT_LEVEL)};
static const uint16_t llsLenAlCh = sizeof(llsValAlCh);

/* Link loss alert level */
static uint8_t llsValAl[] = {0};
static const uint16_t llsLenAl = sizeof(llsValAl);

/* Immediate alert service declaration */
static const uint8_t iasValSvc[] = {UINT16_TO_BYTES(ATT_UUID_IMMEDIATE_ALERT_SERVICE)};
static const uint16_t iasLenSvc = sizeof(iasValSvc);

/* Immediate alert alert level characteristic */
static const uint8_t iasValAlCh[] = {ATT_PROP_WRITE_NO_RSP, UINT16_TO_BYTES(IAS_AL_HDL), UINT16_TO_BYTES(ATT_UUID_ALERT_LEVEL)};
static const uint16_t iasLenAlCh = sizeof(iasValAlCh);

/* Immediate alert alert level */
static uint8_t iasValAl[] = {0};
static const uint16_t iasLenAl = sizeof(iasValAl);

/* TX power service declaration */
static const uint8_t txsValSvc[] = {UINT16_TO_BYTES(ATT_UUID_TX_POWER_SERVICE)};
static const uint16_t txsLenSvc = sizeof(txsValSvc);

/* TX power level characteristic */
static const uint8_t txsValTxCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(TXS_TX_HDL), UINT16_TO_BYTES(ATT_UUID_TX_POWER_LEVEL)};
static const uint16_t txsLenTxCh = sizeof(txsValTxCh);

/* TX power level */
static uint8_t txsValTx[] = {0};
static const uint16_t txsLenTx = sizeof(txsValTx);

/* Attribute list */
static const attsAttr_t pxList[] =
{
  {
    attPrimSvcUuid, 
    (uint8_t *) llsValSvc,
    (uint16_t *) &llsLenSvc, 
    sizeof(llsValSvc),
    0,
    ATTS_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) llsValAlCh,
    (uint16_t *) &llsLenAlCh,
    sizeof(llsValAlCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    svcAlLvlUuid,
    llsValAl,
    (uint16_t *) &llsLenAl,
    sizeof(llsValAl),
    0,
    PX_SEC_PERMIT_READ | PX_SEC_PERMIT_WRITE
  },
  {
    attPrimSvcUuid, 
    (uint8_t *) iasValSvc,
    (uint16_t *) &iasLenSvc, 
    sizeof(iasValSvc),
    0,
    ATTS_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) iasValAlCh,
    (uint16_t *) &iasLenAlCh,
    sizeof(iasValAlCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    svcAlLvlUuid,
    iasValAl,
    (uint16_t *) &iasLenAl,
    sizeof(iasValAl),
    ATTS_SET_WRITE_CBACK,
    PX_SEC_PERMIT_WRITE
  },
  {
    attPrimSvcUuid, 
    (uint8_t *) txsValSvc,
    (uint16_t *) &txsLenSvc, 
    sizeof(txsValSvc),
    0,
    ATTS_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) txsValTxCh,
    (uint16_t *) &txsLenTxCh,
    sizeof(txsValTxCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    svcTxPwrUuid,
    txsValTx,
    (uint16_t *) &txsLenTx,
    sizeof(txsValTx),
    0,
    PX_SEC_PERMIT_READ
  }
};

/* Group structure */
static attsGroup_t svcPxGroup =
{
  NULL,
  (attsAttr_t *) pxList,
  NULL,
  NULL,
  PX_START_HDL,
  PX_END_HDL
};

/*************************************************************************************************/
/*!
 *  \fn     SvcPxAddGroup
 *        
 *  \brief  Add the services to the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcPxAddGroup(void)
{
  /* add services */
  AttsAddGroup(&svcPxGroup);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcPxRemoveGroup
 *        
 *  \brief  Remove the services from the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcPxRemoveGroup(void)
{
  AttsRemoveGroup(PX_START_HDL);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcPxCbackRegister
 *        
 *  \brief  Register callbacks for the service.
 *
 *  \param  readCback   Read callback function.
 *  \param  writeCback  Write callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcPxCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback)
{
  svcPxGroup.readCback = readCback;
  svcPxGroup.writeCback = writeCback;
}
