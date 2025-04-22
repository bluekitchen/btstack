/*************************************************************************************************/
/*!
 *  \file   paspc_api.c
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
#ifndef PASPC_API_H
#define PASPC_API_H

#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Phone Alert Status service enumeration of handle indexes of characteristics to be discovered */
enum
{
  PASPC_PASS_AS_HDL_IDX,              /*! Alert status */
  PASPC_PASS_AS_CCC_HDL_IDX,          /*! Alert status CCC descriptor */
  PASPC_PASS_RS_HDL_IDX,              /*! Ringer setting */
  PASPC_PASS_RS_CCC_HDL_IDX,          /*! Ringer setting CCC descriptor */
  PASPC_PASS_RCP_HDL_IDX,             /*! Ringer control point */
  PASPC_PASS_HDL_LIST_LEN             /*! Handle list length */
};  

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     PaspcPassDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Phone Alert Status  service. 
 *          Parameter pHdlList must point to an array of length PASPC_PASS_HDL_LIST_LEN. 
 *          If discovery is successful the handles of discovered characteristics and
 *          descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void PaspcPassDiscover(dmConnId_t connId, uint16_t *pHdlList);

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
void PaspcPassControl(dmConnId_t connId, uint16_t handle, uint8_t command);

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
uint8_t PaspcPassValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg);
   
#ifdef __cplusplus
};
#endif

#endif /* PASPC_API_H */
