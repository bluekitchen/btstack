/*************************************************************************************************/
/*!
 *  \file   svc_core.c
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

#include "wsf_types.h"
#include "att_api.h"
#include "att_uuid.h"
#include "bstream.h"
#include "svc_core.h"
#include "svc_ch.h"
#include "svc_cfg.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Characteristic read permissions */
#ifndef CORE_SEC_PERMIT_READ
#define CORE_SEC_PERMIT_READ SVC_SEC_PERMIT_READ
#endif

/*! Characteristic write permissions */
#ifndef CORE_SEC_PERMIT_WRITE
#define CORE_SEC_PERMIT_WRITE SVC_SEC_PERMIT_WRITE
#endif

/**************************************************************************************************
 GAP group
**************************************************************************************************/

/* service */
static const uint8_t gapValSvc[] = {UINT16_TO_BYTES(ATT_UUID_GAP_SERVICE)};
static const uint16_t gapLenSvc = sizeof(gapValSvc);

/* device name characteristic */
static const uint8_t gapValDnCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(GAP_DN_HDL), UINT16_TO_BYTES(ATT_UUID_DEVICE_NAME)};
static const uint16_t gapLenDnCh = sizeof(gapValDnCh);

/* device name */
static uint8_t gapValDn[ATT_DEFAULT_PAYLOAD_LEN] = "wicentric app";
static uint16_t gapLenDn = 13;

/* appearance characteristic */
static const uint8_t gapValApCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(GAP_AP_HDL), UINT16_TO_BYTES(ATT_UUID_APPEARANCE)};
static const uint16_t gapLenApCh = sizeof(gapValApCh);

/* appearance */
static uint8_t gapValAp[] = {UINT16_TO_BYTES(CH_APPEAR_UNKNOWN)};
static const uint16_t gapLenAp = sizeof(gapValAp);

/* Attribute list for GAP group */
static const attsAttr_t gapList[] =
{
  {
    attPrimSvcUuid, 
    (uint8_t *) gapValSvc,
    (uint16_t *) &gapLenSvc, 
    sizeof(gapValSvc),
    0,
    ATTS_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) gapValDnCh,
    (uint16_t *) &gapLenDnCh,
    sizeof(gapValDnCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    attDnChUuid,
    gapValDn,
    &gapLenDn,
    sizeof(gapValDn),
    (ATTS_SET_VARIABLE_LEN | ATTS_SET_WRITE_CBACK),
    (ATTS_PERMIT_READ | CORE_SEC_PERMIT_WRITE)
  },
  {
    attChUuid,
    (uint8_t *) gapValApCh,
    (uint16_t *) &gapLenApCh,
    sizeof(gapValApCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    attApChUuid,
    gapValAp,
    (uint16_t *) &gapLenAp,
    sizeof(gapValAp),
    0,
    ATTS_PERMIT_READ
  }
};

/* GAP group structure */
static attsGroup_t svcGapGroup =
{
  NULL,
  (attsAttr_t *) gapList,
  NULL,
  NULL,
  GAP_START_HDL,
  GAP_END_HDL
};

/**************************************************************************************************
 GATT group
**************************************************************************************************/

/* service */
static const uint8_t gattValSvc[] = {UINT16_TO_BYTES(ATT_UUID_GATT_SERVICE)};
static const uint16_t gattLenSvc = sizeof(gattValSvc);

/* service changed characteristic */
static const uint8_t gattValScCh[] = {ATT_PROP_INDICATE, UINT16_TO_BYTES(GATT_SC_HDL), UINT16_TO_BYTES(ATT_UUID_SERVICE_CHANGED)};
static const uint16_t gattLenScCh = sizeof(gattValScCh);

/* service changed */
static const uint8_t gattValSc[] = {UINT16_TO_BYTES(0x0001), UINT16_TO_BYTES(0xFFFF)};
static const uint16_t gattLenSc = sizeof(gattValSc);

/* service changed client characteristic configuration */
static uint8_t gattValScChCcc[] = {UINT16_TO_BYTES(0x0000)};
static const uint16_t gattLenScChCcc = sizeof(gattValScChCcc);

/* Attribute list for GATT group */
static const attsAttr_t gattList[] =
{
  {
    attPrimSvcUuid, 
    (uint8_t *) gattValSvc,
    (uint16_t *) &gattLenSvc, 
    sizeof(gattValSvc),
    0,
    ATTS_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) gattValScCh,
    (uint16_t *) &gattLenScCh,
    sizeof(gattValScCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    attScChUuid,
    (uint8_t *) gattValSc,
    (uint16_t *) &gattLenSc,
    sizeof(gattValSc),
    0,
    0
  },
  {
    attCliChCfgUuid,
    gattValScChCcc,
    (uint16_t *) &gattLenScChCcc,
    sizeof(gattValScChCcc),
    ATTS_SET_CCC,
    (ATTS_PERMIT_READ | CORE_SEC_PERMIT_WRITE)
  },
};

/* GATT group structure */
static attsGroup_t svcGattGroup =
{
  NULL,
  (attsAttr_t *) gattList,
  NULL,
  NULL,
  GATT_START_HDL,
  GATT_END_HDL 
};

/*************************************************************************************************/
/*!
 *  \fn     SvcCoreAddGroup
 *        
 *  \brief  Add the services to the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcCoreAddGroup(void)
{
  AttsAddGroup(&svcGapGroup);
  AttsAddGroup(&svcGattGroup);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcCoreRemoveGroup
 *        
 *  \brief  Remove the services from the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcCoreRemoveGroup(void)
{
  AttsRemoveGroup(GAP_START_HDL);
  AttsRemoveGroup(GATT_START_HDL);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcCoreGapCbackRegister
 *        
 *  \brief  Register callbacks for the service.
 *
 *  \param  readCback   Read callback function.
 *  \param  writeCback  Write callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcCoreGapCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback)
{
  svcGapGroup.readCback = readCback;
  svcGapGroup.writeCback = writeCback;
}

/*************************************************************************************************/
/*!
 *  \fn     SvcCoreGattCbackRegister
 *        
 *  \brief  Register callbacks for the service.
 *
 *  \param  readCback   Read callback function.
 *  \param  writeCback  Write callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcCoreGattCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback)
{
  svcGattGroup.readCback = readCback;
  svcGattGroup.writeCback = writeCback;
}
