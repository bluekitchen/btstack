/*************************************************************************************************/
/*!
 *  \file   hrpc_api.h
 *
 *  \brief  Heart Rate profile client.
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
#ifndef HRPC_API_H
#define HRPC_API_H

#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Heart Rate service enumeration of handle indexes of characteristics to be discovered */
enum
{
  HRPC_HRS_HRM_HDL_IDX,           /*! Heart rate measurement */
  HRPC_HRS_HRM_CCC_HDL_IDX,       /*! Heart rate measurement CCC descriptor */
  HRPC_HRS_BSL_HDL_IDX,           /*! Body sensor location */
  HRPC_HRS_HRCP_HDL_IDX,          /*! Heart rate control point */
  HRPC_HRS_HDL_LIST_LEN           /*! Handle list length */
};  

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     HrpcHrsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Heart Rate service. 
 *          Parameter pHdlList must point to an array of length HRPC_HRS_HDL_LIST_LEN. 
 *          If discovery is successful the handles of discovered characteristics and
 *          descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HrpcHrsDiscover(dmConnId_t connId, uint16_t *pHdlList);

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
void HrpcHrsControl(dmConnId_t connId, uint16_t handle, uint8_t command);

/*************************************************************************************************/
/*!
 *  \fn     HrpcHrsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length HRPC_HRS_HDL_LIST_LEN. 
 *          If the ATT handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  connId    Connection identifier.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t HrpcHrsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg);
                     
#ifdef __cplusplus
};
#endif

#endif /* HRPC_API_H */
