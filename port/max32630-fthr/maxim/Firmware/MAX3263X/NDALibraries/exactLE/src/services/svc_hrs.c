/*************************************************************************************************/
/*!
 *  \file   svc_hrs.c
 *        
 *  \brief  Example Heart Rate service implementation.
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
#include "svc_ch.h"
#include "svc_hrs.h"
#include "svc_cfg.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Characteristic read permissions */
#ifndef HRS_SEC_PERMIT_READ
#define HRS_SEC_PERMIT_READ SVC_SEC_PERMIT_READ
#endif

/*! Characteristic write permissions */
#ifndef HRS_SEC_PERMIT_WRITE
#define HRS_SEC_PERMIT_WRITE SVC_SEC_PERMIT_WRITE
#endif

/**************************************************************************************************
 Static Variables
**************************************************************************************************/

/* UUIDs */
static const uint8_t svcHrmUuid[] = {UINT16_TO_BYTES(ATT_UUID_HR_MEAS)};
static const uint8_t svcSlUuid[] = {UINT16_TO_BYTES(ATT_UUID_HR_SENSOR_LOC)};
static const uint8_t svcCpUuid[] = {UINT16_TO_BYTES(ATT_UUID_HR_CP)};

/**************************************************************************************************
 Service variables
**************************************************************************************************/

/* Heart rate service declaration */
static const uint8_t hrsValSvc[] = {UINT16_TO_BYTES(ATT_UUID_HEART_RATE_SERVICE)};
static const uint16_t hrsLenSvc = sizeof(hrsValSvc);

/* Heart rate measurement characteristic */ 
static const uint8_t hrsValHrmCh[] = {ATT_PROP_NOTIFY, UINT16_TO_BYTES(HRS_HRM_HDL), UINT16_TO_BYTES(ATT_UUID_HR_MEAS)};
static const uint16_t hrsLenHrmCh = sizeof(hrsValHrmCh);

/* Heart rate measurement */
/* Note these are dummy values */
static const uint8_t hrsValHrm[] = {0};
static const uint16_t hrsLenHrm = sizeof(hrsValHrm);

/* Heart rate measurement client characteristic configuration */
static uint8_t hrsValHrmChCcc[] = {UINT16_TO_BYTES(0x0000)};
static const uint16_t hrsLenHrmChCcc = sizeof(hrsValHrmChCcc);

/* Body sensor location characteristic */ 
static const uint8_t hrsValSlCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(HRS_SL_HDL), UINT16_TO_BYTES(ATT_UUID_HR_SENSOR_LOC)};
static const uint16_t hrsLenSlCh = sizeof(hrsValSlCh);

/* Body sensor location */
static uint8_t hrsValSl[] = {CH_BSENSOR_LOC_WRIST};
static const uint16_t hrsLenSl = sizeof(hrsValSl);

/* Control point characteristic */
static const uint8_t hrsValCpCh[] = {ATT_PROP_WRITE, UINT16_TO_BYTES(HRS_CP_HDL), UINT16_TO_BYTES(ATT_UUID_HR_CP)};
static const uint16_t hrsLenCpCh = sizeof(hrsValCpCh);

/* Control point */
/* Note these are dummy values */
static const uint8_t hrsValCp[] = {0};
static const uint16_t hrsLenCp = sizeof(hrsValCp);

/* Attribute list for HRS group */
static const attsAttr_t hrsList[] =
{
  {
    attPrimSvcUuid, 
    (uint8_t *) hrsValSvc,
    (uint16_t *) &hrsLenSvc, 
    sizeof(hrsValSvc),
    0,
    ATTS_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) hrsValHrmCh,
    (uint16_t *) &hrsLenHrmCh,
    sizeof(hrsValHrmCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    svcHrmUuid,
    (uint8_t *) hrsValHrm,
    (uint16_t *) &hrsLenHrm,
    sizeof(hrsValHrm),
    0,
    0
  },
  {
    attCliChCfgUuid,
    (uint8_t *) hrsValHrmChCcc,
    (uint16_t *) &hrsLenHrmChCcc,
    sizeof(hrsValHrmChCcc),
    ATTS_SET_CCC,
    (ATTS_PERMIT_READ | HRS_SEC_PERMIT_WRITE)
  },
  {
    attChUuid,
    (uint8_t *) hrsValSlCh,
    (uint16_t *) &hrsLenSlCh,
    sizeof(hrsValSlCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    svcSlUuid,
    hrsValSl,
    (uint16_t *) &hrsLenSl,
    sizeof(hrsValSl),
    0,
    HRS_SEC_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) hrsValCpCh,
    (uint16_t *) &hrsLenCpCh,
    sizeof(hrsValCpCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    svcCpUuid,
    (uint8_t *) hrsValCp,
    (uint16_t *) &hrsLenCp,
    sizeof(hrsValCp),
    ATTS_SET_WRITE_CBACK,
    HRS_SEC_PERMIT_WRITE
  }
};

/* HRS group structure */
static attsGroup_t svcHrsGroup =
{
  NULL,
  (attsAttr_t *) hrsList,
  NULL,
  NULL,
  HRS_START_HDL,
  HRS_END_HDL
};

/*************************************************************************************************/
/*!
 *  \fn     SvcHrsAddGroup
 *        
 *  \brief  Add the services to the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcHrsAddGroup(void)
{
  AttsAddGroup(&svcHrsGroup);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcHrsRemoveGroup
 *        
 *  \brief  Remove the services from the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcHrsRemoveGroup(void)
{
  AttsRemoveGroup(HRS_START_HDL);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcHrsCbackRegister
 *        
 *  \brief  Register callbacks for the service.
 *
 *  \param  readCback   Read callback function.
 *  \param  writeCback  Write callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcHrsCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback)
{
  svcHrsGroup.readCback = readCback;
  svcHrsGroup.writeCback = writeCback;
}
