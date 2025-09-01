/*************************************************************************************************/
/*!
 *  \file   anpc_main.c
 *
 *  \brief  Alert Notification profile client.
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
#include "app_api.h"
#include "anpc_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*!
 *  Alert Notification service 
 */

/* Characteristics for discovery */

/*! Supported new alert category */
static const attcDiscChar_t anpcAnsSnac = 
{
  attSnacChUuid,
  ATTC_SET_REQUIRED
};

/*! New alert */
static const attcDiscChar_t anpcAnsNa = 
{
  attNaChUuid,
  ATTC_SET_REQUIRED
};

/*! New alert CCC descriptor */
static const attcDiscChar_t anpcAnsNaCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! Supported unread alert category */
static const attcDiscChar_t anpcAnsSuac = 
{
  attSuacChUuid,
  ATTC_SET_REQUIRED
};

/*! Unread alert status */
static const attcDiscChar_t anpcAnsUas = 
{
  attUasChUuid,
  ATTC_SET_REQUIRED
};
/*! Unread alert status CCC descriptor */
static const attcDiscChar_t anpcAnsUasCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! Alert notification control point */
static const attcDiscChar_t anpcAnsAncp = 
{
  attAncpChUuid,
  ATTC_SET_REQUIRED
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *anpcAnsDiscCharList[] =
{
  &anpcAnsSnac,                   /*! Supported new alert category */
  &anpcAnsNa,                     /*! New alert */
  &anpcAnsNaCcc,                  /*! New alert CCC descriptor */
  &anpcAnsSuac,                   /*! Supported unread alert category */
  &anpcAnsUas,                    /*! Unread alert status */
  &anpcAnsUasCcc,                 /*! Unread alert status CCC descriptor */
  &anpcAnsAncp                    /*! Alert notification control point */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(ANPC_ANS_HDL_LIST_LEN == ((sizeof(anpcAnsDiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     AnpcAnsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Alert Notification service.  Parameter 
 *          pHdlList must point to an array of length ANPC_ANS_HDL_LIST_LEN.  If discovery is 
 *          successful the handles of discovered characteristics and descriptors will be set
 *          in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnpcAnsDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attAnsSvcUuid,
                     ANPC_ANS_HDL_LIST_LEN, (attcDiscChar_t **) anpcAnsDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     AnpcAnsControl
 *        
 *  \brief  Send a command to the alert notification control point.
 *
 *  \param  connId    Connection identifier.
 *  \param  handle    Attribute handle.
 *  \param  command   Control point command.
 *  \param  catId     Alert category ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnpcAnsControl(dmConnId_t connId, uint16_t handle, uint8_t command, uint8_t catId)
{
  uint8_t buf[2];
  
  if (handle != ATT_HANDLE_NONE)
  {
    buf[0] = command;
    buf[1] = catId;
    AttcWriteReq(connId, handle, sizeof(buf), buf);
  } 
}

/*************************************************************************************************/
/*!
 *  \fn     AnpcAnsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length ANPC_ANS_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t AnpcAnsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg)
{
  uint8_t   *p;
  uint16_t  catIdMask;
  uint8_t   catId;
  uint8_t   numAlert;
  uint8_t   status = ATT_SUCCESS;
  uint8_t   buf[19];
  
  /* new alert */
  if (pMsg->handle == pHdlList[ANPC_ANS_NA_HDL_IDX])
  {
    /* parse value */
    p = pMsg->pValue;
    BSTREAM_TO_UINT8(catId, p);
    BSTREAM_TO_UINT8(numAlert, p);
    
    /* null terminate string before printing */
    memcpy(buf, p, pMsg->valueLen - 2);
    buf[pMsg->valueLen - 2] = '\0';
    
    APP_TRACE_INFO2("New alert cat:%d num:%d", catId, numAlert);
    APP_TRACE_INFO1("Msg:%s", buf);
  }
  /* unread alert status */
  else if (pMsg->handle == pHdlList[ANPC_ANS_UAS_HDL_IDX])
  {
    /* parse value */
    p = pMsg->pValue;
    BSTREAM_TO_UINT8(catId, p);
    BSTREAM_TO_UINT8(numAlert, p);
    
    APP_TRACE_INFO2("Unread alert status cat:%d num:%d", catId, numAlert);
  }
  /* supported new alert category */
  else if (pMsg->handle == pHdlList[ANPC_ANS_SNAC_HDL_IDX])
  {
    /* parse value */
    p = pMsg->pValue;
    if (pMsg->valueLen == 1)
    {
      BSTREAM_TO_UINT8(catIdMask, p);
    }
    else
    {
      BSTREAM_TO_UINT16(catIdMask, p);
    }
    
    APP_TRACE_INFO1("Supported new alert category: 0x%04x", catIdMask);
  }
  /* supported unread alert category */
  else if (pMsg->handle == pHdlList[ANPC_ANS_SUAC_HDL_IDX])
  {
    /* parse value */
    p = pMsg->pValue;
    if (pMsg->valueLen == 1)
    {
      BSTREAM_TO_UINT8(catIdMask, p);
    }
    else
    {
      BSTREAM_TO_UINT16(catIdMask, p);
    }
    
    APP_TRACE_INFO1("Supported unread alert category: 0x%04x", catIdMask);
  }
  /* handle not found in list */
  else
  {
    status = ATT_ERR_NOT_FOUND;
  }
  
  return status;
}
