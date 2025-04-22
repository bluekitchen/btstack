/*************************************************************************************************/
/*!
 *  \file   hrpc_main.c
 *
 *  \brief  Heart Rate profile collector.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "svc_ch.h"
#include "app_api.h"
#include "hrpc_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*!
 *  Heart Rate service 
 */

/* Characteristics for discovery */

/*! Heart rate measurement */
static const attcDiscChar_t hrpcHrsHrm = 
{
  attHrmChUuid,
  ATTC_SET_REQUIRED
};

/*! Heart rate measurement CCC descriptor */
static const attcDiscChar_t hrpcHrsHrmCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! Body sensor location */
static const attcDiscChar_t hrpcHrsBsl = 
{
  attBslChUuid,
  0
};

/*! Heart rate control point */
static const attcDiscChar_t hrpcHrsHrcp = 
{
  attHrcpChUuid,
  0
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *hrpcHrsDiscCharList[] =
{
  &hrpcHrsHrm,                    /*! Heart rate measurement */
  &hrpcHrsHrmCcc,                 /*! Heart rate measurement CCC descriptor */
  &hrpcHrsBsl,                    /*! Body sensor location */
  &hrpcHrsHrcp,                   /*! Heart rate control point */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(HRPC_HRS_HDL_LIST_LEN == ((sizeof(hrpcHrsDiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     hrcpHrsParseHrm
 *        
 *  \brief  Parse a heart rate measurement.
 *
 *  \param  pValue    Pointer to buffer containing value.
 *  \param  len       length of buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hrcpHrsParseHrm(uint8_t *pValue, uint16_t len)
{
  uint8_t   flags;
  uint16_t  minLen = 1 + CH_HRM_LEN_VALUE_8BIT;
  uint16_t  heartRate;
  uint16_t  energyExp;
  uint16_t  rrInterval;
  
  if (len > 0)
  {
    /* get flags */
    BSTREAM_TO_UINT8(flags, pValue);
    
    /* determine expected minimum length based on flags */
    if (flags & CH_HRM_FLAGS_VALUE_16BIT)
    {
      minLen++;
    }
    if (flags & CH_HRM_FLAGS_ENERGY_EXP)
    {
      minLen += CH_HRM_LEN_ENERGY_EXP;
    }
    if (flags & CH_HRM_FLAGS_RR_INTERVAL)
    {
      minLen += CH_HRM_LEN_RR_INTERVAL;
    }
  }
    
  /* verify length */
  if (len < minLen)
  {
    APP_TRACE_INFO2("Heart Rate meas len:%d minLen:%d", len, minLen);
    return;
  }
  
  /* heart rate */
  if (flags & CH_HRM_FLAGS_VALUE_16BIT)
  {
    BSTREAM_TO_UINT16(heartRate, pValue);
  }  
  else
  {
    BSTREAM_TO_UINT8(heartRate, pValue);
  }
  APP_TRACE_INFO1("  Heart rate:   %d", heartRate);
  
  /* energy expended */
  if (flags & CH_HRM_FLAGS_ENERGY_EXP)
  {
    BSTREAM_TO_UINT16(energyExp, pValue);
    APP_TRACE_INFO1("  Energy Exp:   %d", energyExp);
  }
  
  /* r-r interval */
  if (flags & CH_HRM_FLAGS_RR_INTERVAL)
  {
    /* get length of r-r interval bytes */
    len = len + CH_HRM_LEN_RR_INTERVAL - minLen;
    
    /* if len is somehow missing a byte (len is odd) reduce by 1 */
    if (len & 1)
    {
      len--;
    }
    
    /* parse r-r intervals */
    do
    {
      BSTREAM_TO_UINT16(rrInterval, pValue);
      APP_TRACE_INFO1("  r-r Interval: %d", rrInterval);
      len -= 2;
    } while (len > 0);
  }
  
  APP_TRACE_INFO1("  Flags:0x%02x", flags);    
}

/*************************************************************************************************/
/*!
 *  \fn     HrpcHrsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Heart Rate service.  Parameter 
 *          pHdlList must point to an array of length HRPC_HRS_HDL_LIST_LEN.  If discovery is 
 *          successful the handles of discovered characteristics and descriptors will be set
 *          in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HrpcHrsDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attHrsSvcUuid,
                     HRPC_HRS_HDL_LIST_LEN, (attcDiscChar_t **) hrpcHrsDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     HrpcHrsControl
 *        
 *  \brief  Send a command to the heart rate control point.
 *
 *  \param  connId    Connection identifier.
 *  \param  handle    Attribute handle.
 *  \param  command   Control point command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HrpcHrsControl(dmConnId_t connId, uint16_t handle, uint8_t command)
{
  uint8_t buf[1];
  
  if (handle != ATT_HANDLE_NONE)
  {
    buf[0] = command;
    AttcWriteReq(connId, handle, sizeof(buf), buf);
  } 
}

/*************************************************************************************************/
/*!
 *  \fn     HrpcHrsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length HRPC_HRS_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t HrpcHrsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg)
{
  uint8_t   *p;
  uint8_t   sensorLoc;
  uint8_t   status = ATT_SUCCESS;
  
  /* heart rate measurement */
  if (pMsg->handle == pHdlList[HRPC_HRS_HRM_HDL_IDX])
  {
    /* parse value */
    hrcpHrsParseHrm(pMsg->pValue, pMsg->valueLen);
  }
  /* body sensor location */
  else if (pMsg->handle == pHdlList[HRPC_HRS_BSL_HDL_IDX])
  {
    /* parse value */
    p = pMsg->pValue;
    BSTREAM_TO_UINT8(sensorLoc, p);

    /* ignore if out of range */
    if (sensorLoc <= CH_BSENSOR_LOC_FOOT)
    {
      APP_TRACE_INFO1("Body sensor location:%d", sensorLoc);
    }
  }
  else
  {
    status = ATT_ERR_NOT_FOUND;
  }
  
  return status;
}
