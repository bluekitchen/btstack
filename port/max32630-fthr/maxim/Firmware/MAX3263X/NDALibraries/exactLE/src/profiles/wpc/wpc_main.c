/*************************************************************************************************/
/*!
 *  \file   wpc_main.c
 *
 *  \brief  Wicentric proprietary profile client.
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
#include "bstream.h"
#include "app_api.h"
#include "wpc_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*!
 *  Wicentric proprietary service P1
 */

/* UUIDs */
static const uint8_t wpcP1SvcUuid[] = {ATT_UUID_P1_SERVICE};    /*! Proprietary service P1 */
static const uint8_t wpcD1ChUuid[] = {ATT_UUID_D1_DATA};        /*! Proprietary data D1 */

/* Characteristics for discovery */

/*! Proprietary data */
static const attcDiscChar_t wpcP1Dat = 
{
  wpcD1ChUuid,
  ATTC_SET_REQUIRED | ATTC_SET_UUID_128
};

/*! Proprietary data descriptor */
static const attcDiscChar_t wpcP1datCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *wpcP1DiscCharList[] =
{
  &wpcP1Dat,                  /*! Proprietary data */
  &wpcP1datCcc                /*! Proprietary data descriptor */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(WPC_P1_HDL_LIST_LEN == ((sizeof(wpcP1DiscCharList) / sizeof(attcDiscChar_t *))));

/*************************************************************************************************/
/*!
 *  \fn     WpcP1Discover
 *        
 *  \brief  Perform service and characteristic discovery for Wicentric proprietary service P1. 
 *          Parameter pHdlList must point to an array of length WPC_P1_HDL_LIST_LEN. 
 *          If discovery is successful the handles of discovered characteristics and
 *          descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WpcP1Discover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_128_UUID_LEN, (uint8_t *) wpcP1SvcUuid,
                     WPC_P1_HDL_LIST_LEN, (attcDiscChar_t **) wpcP1DiscCharList, pHdlList);
}

