/*************************************************************************************************/
/*!
 *  \file   dis_main.c
 *
 *  \brief  Device information service client.
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
#include "svc_ch.h"
#include "app_api.h"
#include "dis_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! DIS characteristics for discovery */

/*! Manufacturer name string */
static const attcDiscChar_t disMfns = 
{
  attMfnsChUuid,
  0
};

/*! Model number string */
static const attcDiscChar_t disMns = 
{
  attMnsChUuid,
  0
};

/*! Serial number string */
static const attcDiscChar_t disSns = 
{
  attSnsChUuid,
  0
};

/*! Hardware revision string */
static const attcDiscChar_t disHrs = 
{
  attHrsChUuid,
  0
};

/*! Firmware revision string */
static const attcDiscChar_t disFrs = 
{
  attFrsChUuid,
  0
};

/*! Software revision string */
static const attcDiscChar_t disSrs = 
{
  attSrsChUuid,
  0
};

/*! System ID */
static const attcDiscChar_t disSid = 
{
  attSidChUuid,
  0
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *disDiscCharList[] =
{
  &disMfns,             /*! Manufacturer name string */     
  &disMns,              /*! Model number string */
  &disSns,              /*! Serial number string */
  &disHrs,              /*! Hardware revision string */
  &disFrs,              /*! Firmware revision string */
  &disSrs,              /*! Software revision string */
  &disSid               /*! System ID */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(DIS_HDL_LIST_LEN == ((sizeof(disDiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     disFmtString
 *        
 *  \brief  Format a string for printing.
 *
 *  \param  pValue    Pointer to buffer containing value.
 *  \param  len       length of buffer.
 *
 *  \return Buffer containing string.
 */
/*************************************************************************************************/
char *disFmtString(uint8_t *pValue, uint16_t len)
{
  static char buf[ATT_DEFAULT_PAYLOAD_LEN + 1];
   
  len = (len < (sizeof(buf) - 1)) ? len : (sizeof(buf) - 1);
  
  memcpy(buf, pValue, len);
  buf[len] = '\0';
  
  return buf;
}

/*************************************************************************************************/
/*!
 *  \fn     DisDiscover
 *        
 *  \brief  Perform service and characteristic discovery for DIS service.  Parameter pHdlList
 *          must point to an array of length DIS_HDL_LIST_LEN.  If discovery is successful
 *          the handles of discovered characteristics and descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DisDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attDisSvcUuid,
                     DIS_HDL_LIST_LEN, (attcDiscChar_t **) disDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     DisValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length DIS_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t DisValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg)
{
  uint8_t status = ATT_SUCCESS;
    
  /* manufacturer name string */
  if (pMsg->handle == pHdlList[DIS_MFNS_HDL_IDX])
  {
    APP_TRACE_INFO1("Mfgr: %s", disFmtString(pMsg->pValue, pMsg->valueLen));
  }
  /* model number string */
  else if (pMsg->handle == pHdlList[DIS_MNS_HDL_IDX])
  {
    APP_TRACE_INFO1("Model num: %s", disFmtString(pMsg->pValue, pMsg->valueLen));
  }
  /* serial number string */
  else if (pMsg->handle == pHdlList[DIS_SNS_HDL_IDX])
  {
    APP_TRACE_INFO1("Serial num: %s", disFmtString(pMsg->pValue, pMsg->valueLen));
  }
  /* hardware revision string */
  else if (pMsg->handle == pHdlList[DIS_HRS_HDL_IDX])
  {
    APP_TRACE_INFO1("Hardware rev: %s", disFmtString(pMsg->pValue, pMsg->valueLen));
  }
  /* firmware revision string */
  else if (pMsg->handle == pHdlList[DIS_FRS_HDL_IDX])
  {
    APP_TRACE_INFO1("Firmware rev: %s", disFmtString(pMsg->pValue, pMsg->valueLen));
  }
  /* software revision string */
  else if (pMsg->handle == pHdlList[DIS_SRS_HDL_IDX])
  {
    APP_TRACE_INFO1("Software rev: %s", disFmtString(pMsg->pValue, pMsg->valueLen));
  }
  /* system id */
  else if (pMsg->handle == pHdlList[DIS_SID_HDL_IDX])
  {
    if (pMsg->valueLen == CH_SYSTEM_ID_LEN)
    {
      APP_TRACE_INFO0("System ID read ok");
    }
  }
  /* handle not found in list */
  else
  {
    status = ATT_ERR_NOT_FOUND;
  }
  
  return status;
}