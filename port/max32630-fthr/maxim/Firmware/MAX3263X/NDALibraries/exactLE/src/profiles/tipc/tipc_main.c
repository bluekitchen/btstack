/*************************************************************************************************/
/*!
 *  \file   tipc_main.c
 *
 *  \brief  Time profile client.
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
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "app_api.h"
#include "tipc_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*!
 *  Current Time service 
 */

/* Characteristics for discovery */

/*! Current time */
static const attcDiscChar_t tipcCtsCt = 
{
  attCtChUuid,
  ATTC_SET_REQUIRED
};

/*! Current time client characteristic configuration descriptor */
static const attcDiscChar_t tipcCtsCtCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! Local time information */
static const attcDiscChar_t tipcCtsLti = 
{
  attLtiChUuid,
  0
};

/*! Reference time information */
static const attcDiscChar_t tipcCtsRti = 
{
  attRtiChUuid,
  0
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *tipcCtsDiscCharList[] =
{
  &tipcCtsCt,                   /* Current time */
  &tipcCtsCtCcc,                /* Current time client characteristic configuration descriptor */
  &tipcCtsLti,                  /* Local time information */
  &tipcCtsRti                   /* Reference time information */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(TIPC_CTS_HDL_LIST_LEN == ((sizeof(tipcCtsDiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     TipcCtsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Current Time service.  Parameter 
 *          pHdlList must point to an array of length TIPC_CTS_HDL_LIST_LEN.  If discovery is 
 *          successful the handles of discovered characteristics and descriptors will be set
 *          in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void TipcCtsDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attCtsSvcUuid,
                     TIPC_CTS_HDL_LIST_LEN, (attcDiscChar_t **) tipcCtsDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     TipcCtsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length TIPC_CTS_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t TipcCtsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg)
{
  uint8_t   status = ATT_SUCCESS;
  uint8_t   *p;
  uint16_t  year;
  uint8_t   month, day, hour, min, sec, sec256, dayOfWeek, adjustReason;
  int8_t    timeZone;
  uint8_t   dstOffset, source, accuracy;
  
  /* current time */
  if (pMsg->handle == pHdlList[TIPC_CTS_CT_HDL_IDX])
  {
    /* parse value */
    p = pMsg->pValue;
    BSTREAM_TO_UINT16(year, p);
    BSTREAM_TO_UINT8(month, p);
    BSTREAM_TO_UINT8(day, p);
    BSTREAM_TO_UINT8(hour, p);
    BSTREAM_TO_UINT8(min, p);
    BSTREAM_TO_UINT8(sec, p);
    BSTREAM_TO_UINT8(dayOfWeek, p);
    BSTREAM_TO_UINT8(sec256, p);
    BSTREAM_TO_UINT8(adjustReason, p);

    APP_TRACE_INFO3("Date: %d/%d/%d", month, day, year);
    APP_TRACE_INFO3("Time: %02d:%02d:%02d", hour, min, sec);
    APP_TRACE_INFO3("dayOfWeek:%d sec256:%d adjustReason:%d",  dayOfWeek, sec256, adjustReason);    
  }
  /* local time information */
  else if (pMsg->handle == pHdlList[TIPC_CTS_LTI_HDL_IDX])
  {
    /* parse value */
    p = pMsg->pValue;
    BSTREAM_TO_UINT8(timeZone, p);
    BSTREAM_TO_UINT8(dstOffset, p);
      
    APP_TRACE_INFO2("timeZone:%d dstOffset:%d", timeZone, dstOffset);
  }
  /* reference time information */
  else if (pMsg->handle == pHdlList[TIPC_CTS_RTI_HDL_IDX])
  {
    /* parse value */
    p = pMsg->pValue;
    BSTREAM_TO_UINT8(source, p);
    BSTREAM_TO_UINT8(accuracy, p);
    BSTREAM_TO_UINT8(day, p);
    BSTREAM_TO_UINT8(hour, p);
      
    APP_TRACE_INFO2("Ref. time source:%d accuracy:%d", source, accuracy);
    APP_TRACE_INFO2("Last update days:%d hours:%d", day, hour);
  }
  /* handle not found in list */
  else
  {
    status = ATT_ERR_NOT_FOUND;
  }
  
  return status;
}