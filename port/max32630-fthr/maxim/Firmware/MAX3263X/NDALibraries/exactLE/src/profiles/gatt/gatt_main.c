/*************************************************************************************************/
/*!
 *  \file   gatt_main.c
 *
 *  \brief  GATT profile.
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
#include "gatt_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! GATT service characteristics for discovery */

/*! Service changed */  
static const attcDiscChar_t gattSc = 
{
  attScChUuid,
  0
};

/*! Service changed client characteristic configuration descriptor */
static const attcDiscChar_t gattScCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_DESCRIPTOR
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *gattDiscCharList[] =
{
  &gattSc,                    /* Service changed */  
  &gattScCcc                  /* Service changed client characteristic configuration descriptor */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(GATT_HDL_LIST_LEN == ((sizeof(gattDiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     GattDiscover
 *        
 *  \brief  Perform service and characteristic discovery for GATT service.  Parameter pHdlList
 *          must point to an array of length GATT_HDL_LIST_LEN.  If discovery is successful
 *          the handles of discovered characteristics and descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GattDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attGattSvcUuid,
                     GATT_HDL_LIST_LEN, (attcDiscChar_t **) gattDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     GattValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length GATT_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t GattValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg)
{
  uint8_t status = ATT_SUCCESS;
  
  /* service changed */
  if (pMsg->handle == pHdlList[GATT_SC_HDL_IDX])
  {
    /* perform service changed */
    AppDiscServiceChanged(pMsg);
  }
  /* handle not found in list */
  else
  {
    status = ATT_ERR_NOT_FOUND;
  }
  
  return status;
}