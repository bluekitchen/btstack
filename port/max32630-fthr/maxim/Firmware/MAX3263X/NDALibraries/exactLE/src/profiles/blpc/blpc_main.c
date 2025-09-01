/*************************************************************************************************/
/*!
 *  \file   blpc_main.c
 *
 *  \brief  Blood Pressure profile collector.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "svc_ch.h"
#include "app_api.h"
#include "blpc_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*!
 *  Blood Pressure service 
 */

/* Characteristics for discovery */

/*! Blood pressure measurement */
static const attcDiscChar_t blpcBpsBpm = 
{
  attBpmChUuid,
  ATTC_SET_REQUIRED
};

/*! Blood pressure measurement CCC descriptor */
static const attcDiscChar_t blpcBpsBpmCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! Intermediate cuff pressure */
static const attcDiscChar_t blpcBpsIcp = 
{
  attIcpChUuid,
  0
};

/*! Intermediate cuff pressure CCC descriptor */
static const attcDiscChar_t blpcBpsIcpCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_DESCRIPTOR
};

/*! Blood pressure feature */
static const attcDiscChar_t blpcBpsBpf = 
{
  attBpfChUuid,
  ATTC_SET_REQUIRED
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *blpcBpsDiscCharList[] =
{
  &blpcBpsBpm,                    /*! Blood pressure measurement */
  &blpcBpsBpmCcc,                 /*! Blood pressure measurement CCC descriptor */
  &blpcBpsIcp,                    /*! Intermediate cuff pressure */
  &blpcBpsIcpCcc,                 /*! Intermediate cuff pressure CCC descriptor */
  &blpcBpsBpf                     /*! Blood pressure feature */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(BLPC_BPS_HDL_LIST_LEN == ((sizeof(blpcBpsDiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     blpcBpsParseBpm
 *        
 *  \brief  Parse a blood pressure measurement.
 *
 *  \param  pValue    Pointer to buffer containing value.
 *  \param  len       length of buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void blpcBpsParseBpm(uint8_t *pValue, uint16_t len)
{
  uint8_t   flags;
  uint16_t  systolic, diastolic, map;
  uint16_t  year;
  uint8_t   month, day, hour, min, sec;
  uint16_t  pulseRate;
  uint8_t   userId;
  uint16_t  measStatus;
  uint16_t  minLen = CH_BPM_FLAGS_LEN + CH_BPM_MEAS_LEN;

  
  if (len > 0)
  {
    /* get flags */
    BSTREAM_TO_UINT8(flags, pValue);

    /* determine expected minimum length based on flags */
    if (flags & CH_BPM_FLAG_TIMESTAMP)
    {
      minLen += CH_BPM_TIMESTAMP_LEN;
    }
    if (flags & CH_BPM_FLAG_PULSE_RATE)
    {
      minLen += CH_BPM_PULSE_RATE_LEN;
    }
    if (flags & CH_BPM_FLAG_USER_ID)
    {
      minLen += CH_BPM_USER_ID_LEN;
    }
    if (flags & CH_BPM_FLAG_MEAS_STATUS)
    {
      minLen += CH_BPM_MEAS_STATUS_LEN;
    }
  }
    
  /* verify length */
  if (len < minLen)
  {
    APP_TRACE_INFO2("Blood Pressure meas len:%d minLen:%d", len, minLen);
    return;
  }
  
  /* blood pressure */
  BSTREAM_TO_UINT16(systolic, pValue);
  BSTREAM_TO_UINT16(diastolic, pValue);
  BSTREAM_TO_UINT16(map, pValue);
  APP_TRACE_INFO3("  Systolic:0x%04x Diastolic:0x%04x MAP:0x%04x",
                  systolic, diastolic, map);
  
  /* timestamp */
  if (flags & CH_BPM_FLAG_TIMESTAMP)
  {
    BSTREAM_TO_UINT16(year, pValue);
    BSTREAM_TO_UINT8(month, pValue);
    BSTREAM_TO_UINT8(day, pValue);
    BSTREAM_TO_UINT8(hour, pValue);
    BSTREAM_TO_UINT8(min, pValue);
    BSTREAM_TO_UINT8(sec, pValue);
    APP_TRACE_INFO3("  Date: %d/%d/%d", month, day, year);
    APP_TRACE_INFO3("  Time: %02d:%02d:%02d", hour, min, sec);
  }
  
  /* pulse rate */
  if (flags & CH_BPM_FLAG_PULSE_RATE)
  {
    BSTREAM_TO_UINT16(pulseRate, pValue);
    APP_TRACE_INFO1("  Pulse rate:0x%04x", pulseRate);
  }

  /* user id */
  if (flags & CH_BPM_FLAG_USER_ID)
  {
    BSTREAM_TO_UINT8(userId, pValue);
    APP_TRACE_INFO1("  User ID:%d", userId);
  }

  /* measurement status */
  if (flags & CH_BPM_FLAG_MEAS_STATUS)
  {
    BSTREAM_TO_UINT16(measStatus, pValue);
    APP_TRACE_INFO1("  Meas. status:0x%04x", measStatus);
  }

  APP_TRACE_INFO1("  Flags:0x%02x", flags);  
}

/*************************************************************************************************/
/*!
 *  \fn     BlpcBpsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Blood Pressure service.  Parameter 
 *          pHdlList must point to an array of length BLPC_BPS_HDL_LIST_LEN.  If discovery is 
 *          successful the handles of discovered characteristics and descriptors will be set
 *          in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BlpcBpsDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attBpsSvcUuid,
                     BLPC_BPS_HDL_LIST_LEN, (attcDiscChar_t **) blpcBpsDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     BlpcBpsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length BLPC_BPS_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t BlpcBpsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg)
{
  uint16_t  feature;
  uint8_t   *p;
  uint8_t   status = ATT_SUCCESS;

  /* blood pressure measurement */
  if (pMsg->handle == pHdlList[BLPC_BPS_BPM_HDL_IDX])
  {
    APP_TRACE_INFO0("BP measurement");

    /* parse value */
    blpcBpsParseBpm(pMsg->pValue, pMsg->valueLen);
  }
  /* intermediate cuff pressure  */
  else if (pMsg->handle == pHdlList[BLPC_BPS_ICP_HDL_IDX])
  {
    APP_TRACE_INFO0("BP intermed. cuff pressure");

    /* parse value */
    blpcBpsParseBpm(pMsg->pValue, pMsg->valueLen);
  }
  /* blood pressure feature  */
  else if (pMsg->handle == pHdlList[BLPC_BPS_BPF_HDL_IDX])
  {
    /* parse value */
    p = pMsg->pValue;
    BSTREAM_TO_UINT16(feature, p);

    APP_TRACE_INFO1("BP feature:0x%04x", feature);
  }
  else
  {
    status = ATT_ERR_NOT_FOUND;
  }
  
  return status;
}
