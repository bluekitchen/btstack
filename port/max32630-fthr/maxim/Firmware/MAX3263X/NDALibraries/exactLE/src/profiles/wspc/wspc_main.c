/*************************************************************************************************/
/*!
 *  \file   wspc_main.c
 *
 *  \brief  Weight Scale profile collector.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "svc_ch.h"
#include "app_api.h"
#include "wspc_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*!
 *  Weight Scale service characteristics for discovery
 */

/*! Weight scale measurement */
static const attcDiscChar_t wspcWssWsm = 
{
  attWmChUuid,
  ATTC_SET_REQUIRED
};

/*! Weight scale measurement CCC descriptor */
static const attcDiscChar_t wspcWssWsmCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *wspcWssDiscCharList[] =
{
  &wspcWssWsm,                    /*! Weight scale measurement */
  &wspcWssWsmCcc                  /*! Weight scale measurement CCC descriptor */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(WSPC_WSS_HDL_LIST_LEN == ((sizeof(wspcWssDiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     wspcWssParseWsm
 *        
 *  \brief  Parse a weight scale measurement.
 *
 *  \param  pValue    Pointer to buffer containing value.
 *  \param  len       length of buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void wspcWssParseWsm(uint8_t *pValue, uint16_t len)
{
  uint8_t   flags;
  uint32_t  weight;
  uint16_t  year;
  uint8_t   month, day, hour, min, sec;
  int32_t   mantissa;
  int8_t    exponent;  
  uint16_t  minLen = CH_WSM_FLAGS_LEN + CH_WSM_MEAS_LEN;

  
  if (len > 0)
  {
    /* get flags */
    BSTREAM_TO_UINT8(flags, pValue);

    /* determine expected minimum length based on flags */
    if (flags & CH_WSM_FLAG_TIMESTAMP)
    {
      minLen += CH_WSM_TIMESTAMP_LEN;
    }
  }
    
  /* verify length */
  if (len < minLen)
  {
    APP_TRACE_INFO2("Weight Scale meas len:%d minLen:%d", len, minLen);
    return;
  }
  
  /* weight */
  BSTREAM_TO_UINT32(weight, pValue);
  UINT32_TO_FLT(mantissa, exponent, weight);
  APP_TRACE_INFO2("  Weight: %de%d", mantissa, exponent);
  
  /* timestamp */
  if (flags & CH_WSM_FLAG_TIMESTAMP)
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
  
  APP_TRACE_INFO1("  Flags:0x%02x", flags);  
}

/*************************************************************************************************/
/*!
 *  \fn     WspcWssDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Weight Scale service.  Parameter 
 *          pHdlList must point to an array of length WSPC_WSS_HDL_LIST_LEN.  If discovery is 
 *          successful the handles of discovered characteristics and descriptors will be set
 *          in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WspcWssDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attWssSvcUuid,
                     WSPC_WSS_HDL_LIST_LEN, (attcDiscChar_t **) wspcWssDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     WspcWssValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length WSPC_WSS_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t WspcWssValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg)
{
  uint8_t   status = ATT_SUCCESS;

  /* weight scale measurement */
  if (pMsg->handle == pHdlList[WSPC_WSS_WSM_HDL_IDX])
  {
    APP_TRACE_INFO0("Weight measurement");

    /* parse value */
    wspcWssParseWsm(pMsg->pValue, pMsg->valueLen);
  }
  else
  {
    status = ATT_ERR_NOT_FOUND;
  }
  
  return status;
}
