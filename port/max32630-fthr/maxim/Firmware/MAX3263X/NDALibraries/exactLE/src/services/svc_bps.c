/*************************************************************************************************/
/*!
 *  \file   svc_bps.c
 *        
 *  \brief  Example Blood Pressure service implementation.
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
#include "svc_bps.h"
#include "svc_cfg.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Characteristic read permissions */
#ifndef BPS_SEC_PERMIT_READ
#define BPS_SEC_PERMIT_READ (ATTS_PERMIT_READ | ATTS_PERMIT_READ_ENC)
#endif

/*! Characteristic write permissions */
#ifndef BPS_SEC_PERMIT_WRITE
#define BPS_SEC_PERMIT_WRITE  (ATTS_PERMIT_WRITE | ATTS_PERMIT_WRITE_ENC)
#endif

/**************************************************************************************************
 Service variables
**************************************************************************************************/

/* Blood pressure service declaration */
static const uint8_t bpsValSvc[] = {UINT16_TO_BYTES(ATT_UUID_BLOOD_PRESSURE_SERVICE)};
static const uint16_t bpsLenSvc = sizeof(bpsValSvc);

/* Blood pressure measurement characteristic */ 
static const uint8_t bpsValBpmCh[] = {ATT_PROP_INDICATE, UINT16_TO_BYTES(BPS_BPM_HDL), UINT16_TO_BYTES(ATT_UUID_BP_MEAS)};
static const uint16_t bpsLenBpmCh = sizeof(bpsValBpmCh);

/* Blood pressure measurement */
/* Note these are dummy values */
static const uint8_t bpsValBpm[] = {0};
static const uint16_t bpsLenBpm = sizeof(bpsValBpm);

/* Blood pressure measurement client characteristic configuration */
static uint8_t bpsValBpmChCcc[] = {UINT16_TO_BYTES(0x0000)};
static const uint16_t bpsLenBpmChCcc = sizeof(bpsValBpmChCcc);

/* Intermediate cuff pressure characteristic */ 
static const uint8_t bpsValIcpCh[] = {ATT_PROP_NOTIFY, UINT16_TO_BYTES(BPS_ICP_HDL), UINT16_TO_BYTES(ATT_UUID_INTERMEDIATE_BP)};
static const uint16_t bpsLenIcpCh = sizeof(bpsValIcpCh);

/* Intermediate cuff pressure */
/* Note these are dummy values */
static const uint8_t bpsValIcp[] = {0};
static const uint16_t bpsLenIcp = sizeof(bpsValIcp);

/* Intermediate cuff pressure client characteristic configuration */
static uint8_t bpsValIcpChCcc[] = {UINT16_TO_BYTES(0x0000)};
static const uint16_t bpsLenIcpChCcc = sizeof(bpsValIcpChCcc);

/* Blood pressure feature characteristic */ 
static const uint8_t bpsValBpfCh[] = {ATT_PROP_READ, UINT16_TO_BYTES(BPS_BPF_HDL), UINT16_TO_BYTES(ATT_UUID_BP_FEATURE)};
static const uint16_t bpsLenBpfCh = sizeof(bpsValBpfCh);

/* Blood pressure feature */
static uint8_t bpsValBpf[] = {UINT16_TO_BYTES(CH_BPF_FLAG_MOVEMENT | CH_BPF_FLAG_CUFF_FIT | CH_BPF_FLAG_IRR_PULSE |
                                              CH_BPF_FLAG_PULSE_RANGE | CH_BPF_FLAG_MEAS_POS)};
static const uint16_t bpsLenBpf = sizeof(bpsValBpf);


/* Attribute list for BPS group */
static const attsAttr_t bpsList[] =
{
  /* Blood pressure service declaration */
  {
    attPrimSvcUuid, 
    (uint8_t *) bpsValSvc,
    (uint16_t *) &bpsLenSvc, 
    sizeof(bpsValSvc),
    0,
    ATTS_PERMIT_READ
  },
  /* Blood pressure measurement characteristic */ 
  {
    attChUuid,
    (uint8_t *) bpsValBpmCh,
    (uint16_t *) &bpsLenBpmCh,
    sizeof(bpsValBpmCh),
    0,
    ATTS_PERMIT_READ
  },
  /* Blood pressure measurement */
  {
    attBpmChUuid,
    (uint8_t *) bpsValBpm,
    (uint16_t *) &bpsLenBpm,
    sizeof(bpsValBpm),
    0,
    0
  },
  /* Blood pressure measurement client characteristic configuration */
  {
    attCliChCfgUuid,
    (uint8_t *) bpsValBpmChCcc,
    (uint16_t *) &bpsLenBpmChCcc,
    sizeof(bpsValBpmChCcc),
    ATTS_SET_CCC,
    (ATTS_PERMIT_READ | BPS_SEC_PERMIT_WRITE)
  },
  /* Intermediate cuff pressure characteristic */ 
  {
    attChUuid,
    (uint8_t *) bpsValIcpCh,
    (uint16_t *) &bpsLenIcpCh,
    sizeof(bpsValIcpCh),
    0,
    ATTS_PERMIT_READ
  },
  /* Intermediate cuff pressure */
  {
    attIcpChUuid,
    (uint8_t *) bpsValIcp,
    (uint16_t *) &bpsLenIcp,
    sizeof(bpsValIcp),
    0,
    0
  },
  /* Intermediate cuff pressure client characteristic configuration */
  {
    attCliChCfgUuid,
    (uint8_t *) bpsValIcpChCcc,
    (uint16_t *) &bpsLenIcpChCcc,
    sizeof(bpsValIcpChCcc),
    ATTS_SET_CCC,
    (ATTS_PERMIT_READ | BPS_SEC_PERMIT_WRITE)
  },
  /* Blood pressure feature characteristic */ 
  {
    attChUuid,
    (uint8_t *) bpsValBpfCh,
    (uint16_t *) &bpsLenBpfCh,
    sizeof(bpsValBpfCh),
    0,
    ATTS_PERMIT_READ
  },
  /* Blood pressure feature */
  {
    attBpfChUuid,
    bpsValBpf,
    (uint16_t *) &bpsLenBpf,
    sizeof(bpsValBpf),
    0,
    BPS_SEC_PERMIT_READ
  }
};

/* BPS group structure */
static attsGroup_t svcBpsGroup =
{
  NULL,
  (attsAttr_t *) bpsList,
  NULL,
  NULL,
  BPS_START_HDL,
  BPS_END_HDL
};

/*************************************************************************************************/
/*!
 *  \fn     SvcBpsAddGroup
 *        
 *  \brief  Add the services to the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcBpsAddGroup(void)
{
  AttsAddGroup(&svcBpsGroup);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcBpsRemoveGroup
 *        
 *  \brief  Remove the services from the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcBpsRemoveGroup(void)
{
  AttsRemoveGroup(BPS_START_HDL);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcBpsCbackRegister
 *        
 *  \brief  Register callbacks for the service.
 *
 *  \param  readCback   Read callback function.
 *  \param  writeCback  Write callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcBpsCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback)
{
  svcBpsGroup.readCback = readCback;
  svcBpsGroup.writeCback = writeCback;
}
