/*************************************************************************************************/
/*!
 *  \file   fmpl_main.c
 *
 *  \brief  Find Me profile, locator role.
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
#include "app_api.h"
#include "fmpl_api.h"

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! Immediate Alert service characteristics for discovery */

/*! Alert level */  
const attcDiscChar_t fmplIasAl = 
{
  attAlChUuid,
  ATTC_SET_REQUIRED
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *fmplIasDiscCharList[] =
{
  &fmplIasAl                  /* Alert level */  
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(FMPL_IAS_HDL_LIST_LEN == ((sizeof(fmplIasDiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     FmplIasDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Immediate Alert service.  Note
 *          that pHdlList must point to an array of handles of length FMPL_IAS_HDL_LIST_LEN.
 *          If discovery is successful the handles of discovered characteristics and
 *          descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void FmplIasDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attIasSvcUuid, 
                     FMPL_IAS_HDL_LIST_LEN, (attcDiscChar_t **) fmplIasDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     FmplSendAlert
 *        
 *  \brief  Send an immediate alert to the peer device.
 *
 *  \param  connId DM connection ID.
 *  \param  handle Attribute handle.
 *  \param  alert  Alert value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void FmplSendAlert(dmConnId_t connId, uint16_t handle, uint8_t alert)
{
  uint8_t buf[1];
  
  if (handle != ATT_HANDLE_NONE)
  {
    buf[0] = alert;
    AttcWriteCmd(connId, handle, 1, buf);
  }
}

