/*************************************************************************************************/
/*!
 *  \file   blpc_api.h
 *
 *  \brief  Blood Pressure profile client.
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
#ifndef BLPC_API_H
#define BLPC_API_H

#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Blood Pressure service enumeration of handle indexes of characteristics to be discovered */
enum
{
  BLPC_BPS_BPM_HDL_IDX,           /*! Blood pressure measurement */
  BLPC_BPS_BPM_CCC_HDL_IDX,       /*! Blood pressure measurement CCC descriptor */
  BLPC_BPS_ICP_HDL_IDX,           /*! Intermediate cuff pressure */
  BLPC_BPS_ICP_CCC_HDL_IDX,       /*! Intermediate cuff pressure CCC descriptor */
  BLPC_BPS_BPF_HDL_IDX,           /*! Blood pressure feature */
  BLPC_BPS_HDL_LIST_LEN           /*! Handle list length */
};  

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     BlpcBpsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Blood Pressure service. 
 *          Parameter pHdlList must point to an array of length BLPC_BPS_HDL_LIST_LEN. 
 *          If discovery is successful the handles of discovered characteristics and
 *          descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BlpcBpsDiscover(dmConnId_t connId, uint16_t *pHdlList);

/*************************************************************************************************/
/*!
 *  \fn     BlpcBpsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length BLPC_BPS_HDL_LIST_LEN. 
 *          If the ATT handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  connId    Connection identifier.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t BlpcBpsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg);
                     
#ifdef __cplusplus
};
#endif

#endif /* BLPC_API_H */
