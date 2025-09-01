/*************************************************************************************************/
/*!
 *  \file   paspc_main.c
 *
 *  \brief  Phone Alert Status profile client.
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
#include "paspc_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*!
 *  Phone Alert Status service 
 */

/* Characteristics for discovery */


/*! Alert status */
static const attcDiscChar_t paspcPassAs = 
{
  attAsChUuid,
  ATTC_SET_REQUIRED
};

/*! Alert status CCC descriptor */
static const attcDiscChar_t paspcPassAsCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! Ringer setting */
static const attcDiscChar_t paspcPassRs = 
{
  attRsChUuid,
  ATTC_SET_REQUIRED
};

/*! Ringer setting CCC descriptor */
static const attcDiscChar_t paspcPassRsCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! Ringer control point */
static const attcDiscChar_t paspcPassRcp = 
{
  attRcpChUuid,
  0
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *paspcPassDiscCharList[] =
{
  &paspcPassAs,                     /*! Alert status */
  &paspcPassAsCcc,                  /*! Alert status CCC descriptor */
  &paspcPassRs,                     /*! Ringer setting */
  &paspcPassRsCcc,                  /*! Ringer setting CCC descriptor */
  &paspcPassRcp                     /*! Ringer control point */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(PASPC_PASS_HDL_LIST_LEN == ((sizeof(paspcPassDiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     PaspcPassDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Phone Alert Status service.  Parameter 
 *          pHdlList must point to an array of length PASPC_ANS_HDL_LIST_LEN.  If discovery is 
 *          successful the handles of discovered characteristics and descriptors will be set
 *          in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void PaspcPassDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attPassSvcUuid,
                     PASPC_PASS_HDL_LIST_LEN, (attcDiscChar_t **) paspcPassDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     PaspcPassControl
 *        
 *  \brief  Send a command to the ringer control point.
 *
 *  \param  connId    Connection identifier.
 *  \param  handle    Attribute handle.
 *  \param  command   Control point command.
 *
 *  \return None.
 */
/*************************************************************************************************/
void PaspcPassControl(dmConnId_t connId, uint16_t handle, uint8_t command)
{
  uint8_t buf[1];
  
  if (handle != ATT_HANDLE_NONE)
  {
    buf[0] = command;
    AttcWriteCmd(connId, handle, sizeof(buf), buf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     PaspcPassValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length PASPC_PASS_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t PaspcPassValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg)
{
  uint8_t status = ATT_SUCCESS;
  
  /* alert status */
  if (pMsg->handle == pHdlList[PASPC_PASS_AS_HDL_IDX])
  {
    APP_TRACE_INFO1("Phone alert status: 0x%02x", *pMsg->pValue);
  }
  /* ringer setting */
  else if (pMsg->handle == pHdlList[PASPC_PASS_RS_HDL_IDX])
  {
    APP_TRACE_INFO1("Ringer setting: 0x%02x", *pMsg->pValue);
  }
  /* handle not found in list */
  else
  {
    status = ATT_ERR_NOT_FOUND;
  }
  
  return status;
}