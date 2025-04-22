/*************************************************************************************************/
/*!
 *  \file   glpc_api.h
 *
 *  \brief  Glucose profile collector.
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
#ifndef GLPC_API_H
#define GLPC_API_H

#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Glucose service enumeration of handle indexes of characteristics to be discovered */
enum
{
  GLPC_GLS_GLM_HDL_IDX,           /*! Glucose measurement */
  GLPC_GLS_GLM_CCC_HDL_IDX,       /*! Glucose measurement CCC descriptor */
  GLPC_GLS_GLMC_HDL_IDX,          /*! Glucose measurement context */
  GLPC_GLS_GLMC_CCC_HDL_IDX,      /*! Glucose measurement context CCC descriptor */
  GLPC_GLS_GLF_HDL_IDX,           /*! Glucose feature */
  GLPC_GLS_RACP_HDL_IDX,          /*! Record access control point */
  GLPC_GLS_RACP_CCC_HDL_IDX,      /*! Record access control point CCC descriptor */
  GLPC_GLS_HDL_LIST_LEN           /*! Handle list length */
};  

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Glucose service RACP filter type */
typedef struct
{
  union
  {
    uint16_t      seqNum;         /*! Sequence number filter */
  } param;
  uint8_t         type;           /*! Filter type */
} glpcFilter_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     GlpcGlsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Glucose service. 
 *          Parameter pHdlList must point to an array of length GLPC_GLS_HDL_LIST_LEN. 
 *          If discovery is successful the handles of discovered characteristics and
 *          descriptors will be set in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlpcGlsDiscover(dmConnId_t connId, uint16_t *pHdlList);

/*************************************************************************************************/
/*!
 *  \fn     GlpcGlsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length GLPC_GLS_HDL_LIST_LEN. 
 *          If the ATT handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t GlpcGlsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     GlpcGlsRacpSend
 *        
 *  \brief  Send a command to the glucose service record access control point.
 *
 *  \param  connId  Connection identifier.
 *  \param  handle  Attribute handle. 
 *  \param  opcode  Command opcode.
 *  \param  oper    Command operator or 0 if no operator required.
 *  \param  pFilter Command filter parameters or NULL of no parameters required.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlpcGlsRacpSend(dmConnId_t connId, uint16_t handle, uint8_t opcode, uint8_t oper,
                     glpcFilter_t *pFilter);

/*************************************************************************************************/
/*!
 *  \fn     GlpcGlsSetLastSeqNum
 *        
 *  \brief  Set the last received glucose measurement sequence number.
 *
 *  \param  seqNum   Glucose measurement sequence number.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlpcGlsSetLastSeqNum(uint16_t seqNum);

/*************************************************************************************************/
/*!
 *  \fn     GlpcGlsGetLastSeqNum
 *        
 *  \brief  Get the last received glucose measurement sequence number.
 *
 *  \return Last received glucose measurement sequence number.
 */
/*************************************************************************************************/
uint16_t GlpcGlsGetLastSeqNum(void);

#ifdef __cplusplus
};
#endif

#endif /* GLPC_API_H */
