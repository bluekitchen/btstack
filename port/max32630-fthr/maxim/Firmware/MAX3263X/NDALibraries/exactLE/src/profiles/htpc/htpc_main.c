/*************************************************************************************************/
/*!
 *  \file   htpc_main.c
 *
 *  \brief  Health Thermometer profile collector.
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
#include "htpc_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*!
 *  Health Thermometer service 
 */

/* Characteristics for discovery */

/*! Temperature measurement */
static const attcDiscChar_t htpcHtsTm = 
{
  attTmChUuid,
  ATTC_SET_REQUIRED
};

/*! Temperature measurement CCC descriptor */
static const attcDiscChar_t htpcHtsTmCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! Intermediate temperature */
static const attcDiscChar_t htpcHtsIt = 
{
  attItChUuid,
  0
};

/*! Intermediate temperature CCC descriptor */
static const attcDiscChar_t htpcHtsItCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_DESCRIPTOR
};

/*! Temperature type */
static const attcDiscChar_t htpcHtsTt = 
{
  attTtChUuid,
  0
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *htpcHtsDiscCharList[] =
{
  &htpcHtsTm,                    /*! Temperature measurement */
  &htpcHtsTmCcc,                 /*! Temperature measurement CCC descriptor */
  &htpcHtsIt,                    /*! Intermediate temperature */
  &htpcHtsItCcc,                 /*! Intermediate temperature CCC descriptor */
  &htpcHtsTt                     /*! Temperature type */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(HTPC_HTS_HDL_LIST_LEN == ((sizeof(htpcHtsDiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     htpcHtsParseTm
 *        
 *  \brief  Parse a temperature measurement.
 *
 *  \param  pValue    Pointer to buffer containing value.
 *  \param  len       length of buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void htpcHtsParseTm(uint8_t *pValue, uint16_t len)
{
  uint8_t   flags;
  uint32_t  tempUint;
  int32_t  tempM;
  int8_t    tempE;
  uint16_t  year;
  uint8_t   month, day, hour, min, sec;
  uint8_t   tempType;
  uint16_t  minLen = CH_TM_FLAGS_LEN + CH_TM_MEAS_LEN;

  
  if (len > 0)
  {
    /* get flags */
    BSTREAM_TO_UINT8(flags, pValue);

    /* determine expected minimum length based on flags */
    if (flags & CH_TM_FLAG_TIMESTAMP)
    {
      minLen += CH_TM_TIMESTAMP_LEN;
    }
    if (flags & CH_TM_FLAG_TEMP_TYPE)
    {
      minLen += CH_TM_TEMP_TYPE_LEN;
    }
  }
    
  /* verify length */
  if (len < minLen)
  {
    APP_TRACE_INFO2("Temperature meas len:%d minLen:%d", len, minLen);
    return;
  }
  
  /* Temperature */
  BSTREAM_TO_UINT32(tempUint, pValue);
  UINT32_TO_FLT(tempM, tempE, tempUint);
  APP_TRACE_INFO2("  Temperature:%de%d", tempM, tempE);
  
  /* timestamp */
  if (flags & CH_TM_FLAG_TIMESTAMP)
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

  /* temperature type */
  if (flags & CH_TM_FLAG_TEMP_TYPE)
  {
    BSTREAM_TO_UINT8(tempType, pValue);
    APP_TRACE_INFO1("  Temp. Type:%d", tempType);
  }

  APP_TRACE_INFO1("  Flags:0x%02x", flags);  
}

/*************************************************************************************************/
/*!
 *  \fn     HtpcHtsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Health Thermometer service.  Parameter 
 *          pHdlList must point to an array of length HTPC_HTS_HDL_LIST_LEN.  If discovery is 
 *          successful the handles of discovered characteristics and descriptors will be set
 *          in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpcHtsDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attHtsSvcUuid,
                     HTPC_HTS_HDL_LIST_LEN, (attcDiscChar_t **) htpcHtsDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     HtpcHtsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length HTPC_HTS_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t HtpcHtsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg)
{
  uint8_t   status = ATT_SUCCESS;

  /* temperature measurement */
  if (pMsg->handle == pHdlList[HTPC_HTS_TM_HDL_IDX])
  {
    APP_TRACE_INFO0("Temperature measurement");

    /* parse value */
    htpcHtsParseTm(pMsg->pValue, pMsg->valueLen);
  }
  /* intermediate cuff pressure  */
  else if (pMsg->handle == pHdlList[HTPC_HTS_IT_HDL_IDX])
  {
    APP_TRACE_INFO0("Intermed. temperature");

    /* parse value */
    htpcHtsParseTm(pMsg->pValue, pMsg->valueLen);
  }
  /* temperature type */
  else if (pMsg->handle == pHdlList[HTPC_HTS_TT_HDL_IDX])
  {
    APP_TRACE_INFO1("Temperature type:%d", pMsg->pValue[0]);
  }
  else
  {
    status = ATT_ERR_NOT_FOUND;
  }
  
  return status;
}
