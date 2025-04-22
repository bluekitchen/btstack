/*************************************************************************************************/
/*!
 *  \file   svc_dis.c
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

#include "wsf_types.h"
#include "att_api.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "svc_dis.h"
#include "svc_cfg.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Characteristic read permissions */
#ifndef DIS_SEC_PERMIT_READ
#define DIS_SEC_PERMIT_READ SVC_SEC_PERMIT_READ
#endif

/**************************************************************************************************
 Service variables
**************************************************************************************************/

/* Device information service declaration */
static const uint8_t disValSvc[] = {UINT16_TO_BYTES(ATT_UUID_DEVICE_INFO_SERVICE)};
static const uint16_t disLenSvc = sizeof(disValSvc);

/* Manufacturer name string characteristic */
static const uint8_t disValMfrCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(DIS_MFR_HDL), UINT16_TO_BYTES(ATT_UUID_MANUFACTURER_NAME)};
static const uint16_t disLenMfrCh = sizeof(disValMfrCh);

/* Manufacturer name string */
static const uint8_t disUuMfr[] = {UINT16_TO_BYTES(ATT_UUID_MANUFACTURER_NAME)};
static const uint8_t disValMfr[] = "wicentric inc.";
static const uint16_t disLenMfr = sizeof(disValMfr) - 1;

/* System ID characteristic */
static const uint8_t disValSidCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(DIS_SID_HDL), UINT16_TO_BYTES(ATT_UUID_SYSTEM_ID)};
static const uint16_t disLenSidCh = sizeof(disValSidCh);

/* System ID */
static const uint8_t disUuSid[] = {UINT16_TO_BYTES(ATT_UUID_SYSTEM_ID)};
static uint8_t disValSid[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
static const uint16_t disLenSid = sizeof(disValSid);    

/* Model number string characteristic */
static const uint8_t disValMnCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(DIS_MN_HDL), UINT16_TO_BYTES(ATT_UUID_MODEL_NUMBER)};
static const uint16_t disLenMnCh = sizeof(disValMnCh);

/* Model number string */
static const uint8_t disUuMn[] = {UINT16_TO_BYTES(ATT_UUID_MODEL_NUMBER)};
static const uint8_t disValMn[] = "wicentric model num";
static const uint16_t disLenMn = sizeof(disValMn) - 1;

/* Serial number string characteristic */
static const uint8_t disValSnCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(DIS_SN_HDL), UINT16_TO_BYTES(ATT_UUID_SERIAL_NUMBER)};
static const uint16_t disLenSnCh = sizeof(disValSnCh);

/* Serial number string */
static const uint8_t disUuSn[] = {UINT16_TO_BYTES(ATT_UUID_SERIAL_NUMBER)};
static const uint8_t disValSn[] = "wicentric serial num";
static const uint16_t disLenSn = sizeof(disValSn) - 1;

/* Firmware revision string characteristic */
static const uint8_t disValFwrCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(DIS_FWR_HDL), UINT16_TO_BYTES(ATT_UUID_FIRMWARE_REV)};
static const uint16_t disLenFwrCh = sizeof(disValFwrCh);

/* Firmware revision string */
static const uint8_t disUuFwr[] = {UINT16_TO_BYTES(ATT_UUID_FIRMWARE_REV)};
static const uint8_t disValFwr[] = "wicentric fw rev";
static const uint16_t disLenFwr = sizeof(disValFwr) - 1;

/* Hardware revision string characteristic */
static const uint8_t disValHwrCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(DIS_HWR_HDL), UINT16_TO_BYTES(ATT_UUID_HARDWARE_REV)};
static const uint16_t disLenHwrCh = sizeof(disValHwrCh);

/* Hardware revision string */
static const uint8_t disUuHwr[] = {UINT16_TO_BYTES(ATT_UUID_HARDWARE_REV)};
static const uint8_t disValHwr[] = "wicentric hw rev";
static const uint16_t disLenHwr = sizeof(disValHwr) - 1;

/* Software revision string characteristic */
static const uint8_t disValSwrCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(DIS_SWR_HDL), UINT16_TO_BYTES(ATT_UUID_SOFTWARE_REV)};
static const uint16_t disLenSwrCh = sizeof(disValSwrCh);

/* Software revision string */
static const uint8_t disUuSwr[] = {UINT16_TO_BYTES(ATT_UUID_SOFTWARE_REV)};
static const uint8_t disValSwr[] = "wicentric sw rev";
static const uint16_t disLenSwr = sizeof(disValSwr) - 1;

/* Registration certificate data characteristic */
static const uint8_t disValRcdCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(DIS_RCD_HDL), UINT16_TO_BYTES(ATT_UUID_11073_CERT_DATA)};
static const uint16_t disLenRcdCh = sizeof(disValRcdCh);

/* Registration certificate data */
static const uint8_t disUuRcd[] = {UINT16_TO_BYTES(ATT_UUID_11073_CERT_DATA)};
static const uint8_t disValRcd[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint16_t disLenRcd = sizeof(disValRcd);

/* Attribute list for dis group */
static const attsAttr_t disList[] =
{
  {
    attPrimSvcUuid, 
    (uint8_t *) disValSvc,
    (uint16_t *) &disLenSvc, 
    sizeof(disValSvc),
    0,
    ATTS_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) disValMfrCh,
    (uint16_t *) &disLenMfrCh,
    sizeof(disValMfrCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    disUuMfr,
    (uint8_t *) disValMfr,
    (uint16_t *) &disLenMfr,
    sizeof(disValMfr) - 1,
    0,
    DIS_SEC_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) disValSidCh,
    (uint16_t *) &disLenSidCh,
    sizeof(disValSidCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    disUuSid,
    disValSid,
    (uint16_t *) &disLenSid,
    sizeof(disValSid),
    0,
    DIS_SEC_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) disValMnCh,
    (uint16_t *) &disLenMnCh,
    sizeof(disValMnCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    disUuMn,
    (uint8_t *) disValMn,
    (uint16_t *) &disLenMn,
    sizeof(disValMn) - 1,
    0,
    DIS_SEC_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) disValSnCh,
    (uint16_t *) &disLenSnCh,
    sizeof(disValSnCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    disUuSn,
    (uint8_t *) disValSn,
    (uint16_t *) &disLenSn,
    sizeof(disValSn) - 1,
    0,
    DIS_SEC_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) disValFwrCh,
    (uint16_t *) &disLenFwrCh,
    sizeof(disValFwrCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    disUuFwr,
    (uint8_t *) disValFwr,
    (uint16_t *) &disLenFwr,
    sizeof(disValFwr) - 1,
    0,
    DIS_SEC_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) disValHwrCh,
    (uint16_t *) &disLenHwrCh,
    sizeof(disValHwrCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    disUuHwr,
    (uint8_t *) disValHwr,
    (uint16_t *) &disLenHwr,
    sizeof(disValHwr) - 1,
    0,
    DIS_SEC_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) disValSwrCh,
    (uint16_t *) &disLenSwrCh,
    sizeof(disValSwrCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    disUuSwr,
    (uint8_t *) disValSwr,
    (uint16_t *) &disLenSwr,
    sizeof(disValSwr) - 1,
    0,
    DIS_SEC_PERMIT_READ
  },
  {
    attChUuid,
    (uint8_t *) disValRcdCh,
    (uint16_t *) &disLenRcdCh,
    sizeof(disValRcdCh),
    0,
    ATTS_PERMIT_READ
  },
  {
    disUuRcd,
    (uint8_t *) disValRcd,
    (uint16_t *) &disLenRcd,
    sizeof(disValRcd),
    0,
    DIS_SEC_PERMIT_READ
  },
};

/* DIS group structure */
static attsGroup_t svcDisGroup =
{
  NULL,
  (attsAttr_t *) disList,
  NULL,
  NULL,
  DIS_START_HDL,
  DIS_END_HDL
};

/*************************************************************************************************/
/*!
 *  \fn     SvcDisAddGroup
 *        
 *  \brief  Add the services to the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcDisAddGroup(void)
{
  AttsAddGroup(&svcDisGroup);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcDisRemoveGroup
 *        
 *  \brief  Remove the services from the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcDisRemoveGroup(void)
{
  AttsRemoveGroup(DIS_START_HDL);
}
