/*************************************************************************************************/
/*!
 *  \file   glps_api.h
 *
 *  \brief  Glucose profile sensor.
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
#ifndef GLPS_API_H
#define GLPS_API_H

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************************/
/*!
 *  \fn     GlpsInit
 *        
 *  \brief  Initialize the Glucose profile sensor.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlpsInit(void);

/*************************************************************************************************/
/*!
 *  \fn     GlpsProcMsg
 *        
 *  \brief  This function is called by the application when a message that requires
 *          processing by the glucose profile sensor is received.
 *
 *  \param  pMsg     Event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlpsProcMsg(wsfMsgHdr_t *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     GlpsRacpWriteCback
 *        
 *  \brief  ATTS write callback for glucose service record access control point.  Use this
 *          function as a parameter to SvcGlsCbackRegister().
 *
 *  \return ATT status.
 */
/*************************************************************************************************/
uint8_t GlpsRacpWriteCback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                           uint16_t offset, uint16_t len, uint8_t *pValue, attsAttr_t *pAttr);

/*************************************************************************************************/
/*!
 *  \fn     GlpsSetFeature
 *        
 *  \brief  Set the supported features of the glucose sensor.
 *
 *  \param  feature   Feature bitmask.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlpsSetFeature(uint16_t feature);

/*************************************************************************************************/
/*!
 *  \fn     GlpsSetCccIdx
 *        
 *  \brief  Set the CCCD index used by the application for glucose service characteristics.
 *
 *  \param  glmCccIdx   Glucose measurement CCCD index.
 *  \param  glmcCccIdx  Glucose measurement context CCCD index.
 *  \param  racpCccIdx  Record access control point CCCD index.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlpsSetCccIdx(uint8_t glmCccIdx, uint8_t glmcCccIdx, uint8_t racpCccIdx);

#ifdef __cplusplus
};
#endif

#endif /* GLPS_API_H */
