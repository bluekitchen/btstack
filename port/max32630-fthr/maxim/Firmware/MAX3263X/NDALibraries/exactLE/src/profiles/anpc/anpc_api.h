/*************************************************************************************************/
/*!
 *  \file   anpc_api.h
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
#ifndef ANPC_API_H
#define ANPC_API_H

#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Enumeration of handle indexes of characteristics to be discovered */
enum
{
  ANPC_ANS_SNAC_HDL_IDX,          /*! Supported new alert category */
  ANPC_ANS_NA_HDL_IDX,            /*! New alert */
  ANPC_ANS_NA_CCC_HDL_IDX,        /*! New alert CCC descriptor */
  ANPC_ANS_SUAC_HDL_IDX,          /*! Supported unread alert category */
  ANPC_ANS_UAS_HDL_IDX,           /*! Unread alert status */
  ANPC_ANS_UAS_CCC_HDL_IDX,       /*! Unread alert status CCC descriptor */
  ANPC_ANS_ANCP_HDL_IDX,          /*! Alert notification control point */
  ANPC_ANS_HDL_LIST_LEN           /*! Handle list length */
};  

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     AnpcAnsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Alert Notification service. 
 *          Parameter pHdlList must point to an array of length ANPC_ANS_HDL_LIST_LEN. 
 *          If discovery is successful the handles of discovered characteristics and
 *          descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AnpcAnsDiscover(dmConnId_t connId, uint16_t *pHdlList);

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
void AnpcAnsControl(dmConnId_t connId, uint16_t handle, uint8_t command, uint8_t catId);

/*************************************************************************************************/
/*!
 *  \fn     AnpcAnsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length ANPC_ANS_HDL_LIST_LEN. 
 *          If the ATT handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t AnpcAnsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg);
                     
#ifdef __cplusplus
};
#endif

#endif /* ANPC_API_H */
