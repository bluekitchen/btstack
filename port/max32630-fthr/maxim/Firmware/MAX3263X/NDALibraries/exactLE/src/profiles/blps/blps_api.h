/*************************************************************************************************/
/*!
 *  \file   blps_api.h
 *
 *  \brief  Blood Pressure profile sensor.
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
#ifndef BLPS_API_H
#define BLPS_API_H

#include "wsf_timer.h"
#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Configurable parameters */
typedef struct
{
  wsfTimerTicks_t     period;     /*! Measurement timer expiration period in ms */
} blpsCfg_t;

/*************************************************************************************************/
/*!
 *  \fn     BlpsInit
 *        
 *  \brief  Initialize the Blood Pressure profile sensor.
 *
 *  \param  handerId    WSF handler ID of the application using this service.
 *  \param  pCfg        Configurable parameters.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BlpsInit(wsfHandlerId_t handlerId, blpsCfg_t *pCfg);

/*************************************************************************************************/
/*!
 *  \fn     BlpsMeasStart
 *        
 *  \brief  Start periodic blood pressure measurement.  This function starts a timer to perform
 *          periodic measurements.
 *
 *  \param  connId      DM connection identifier.
 *  \param  timerEvt    WSF event designated by the application for the timer.
 *  \param  icpCccIdx   Index of intermediate cuff pressure CCC descriptor in CCC descriptor
 *                      handle table.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BlpsMeasStart(dmConnId_t connId, uint8_t timerEvt, uint8_t icpCccIdx);

/*************************************************************************************************/
/*!
 *  \fn     BlpsMeasStop
 *        
 *  \brief  Stop periodic blood pressure measurement.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BlpsMeasStop(void);

/*************************************************************************************************/
/*!
 *  \fn     BlpsMeasComplete
 *        
 *  \brief  Blood pressure measurement complete.
 *
 *  \param  connId      DM connection identifier.
 *  \param  bpmCccIdx   Index of blood pressure measurement CCC descriptor in CCC descriptor
 *                      handle table.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BlpsMeasComplete(dmConnId_t connId, uint8_t bpmCccIdx);

/*************************************************************************************************/
/*!
 *  \fn     BlpsProcMsg
 *        
 *  \brief  This function is called by the application when the periodic measurement
 *          timer expires.
 *
 *  \param  pMsg     Event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BlpsProcMsg(wsfMsgHdr_t *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     BlpsSetBpmFlags
 *        
 *  \brief  Set the blood pressure measurement flags.
 *
 *  \param  flags      Blood pressure measurement flags.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BlpsSetBpmFlags(uint8_t flags);

/*************************************************************************************************/
/*!
 *  \fn     BlpsSetIcpFlags
 *        
 *  \brief  Set the intermediate cuff pressure flags.
 *
 *  \param  flags      Intermediate cuff pressure flags.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BlpsSetIcpFlags(uint8_t flags);

#ifdef __cplusplus
};
#endif

#endif /* BLPS_API_H */
