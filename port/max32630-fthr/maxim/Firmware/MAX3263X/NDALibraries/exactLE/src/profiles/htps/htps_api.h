/*************************************************************************************************/
/*!
 *  \file   htps_api.h
 *
 *  \brief  Health Thermometer profile sensor.
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
#ifndef HTPS_API_H
#define HTPS_API_H

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
} htpsCfg_t;

/*************************************************************************************************/
/*!
 *  \fn     HtpsInit
 *        
 *  \brief  Initialize the Health Thermometer profile sensor.
 *
 *  \param  handerId    WSF handler ID of the application using this service.
 *  \param  pCfg        Configurable parameters.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpsInit(wsfHandlerId_t handlerId, htpsCfg_t *pCfg);

/*************************************************************************************************/
/*!
 *  \fn     HtpsMeasStart
 *        
 *  \brief  Start periodic temperature measurement.  This function starts a timer to perform
 *          periodic measurements.
 *
 *  \param  connId      DM connection identifier.
 *  \param  timerEvt    WSF event designated by the application for the timer.
 *  \param  icpCccIdx   Index of intermediate temperature CCC descriptor in CCC descriptor
 *                      handle table.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpsMeasStart(dmConnId_t connId, uint8_t timerEvt, uint8_t icpCccIdx);

/*************************************************************************************************/
/*!
 *  \fn     HtpsMeasStop
 *        
 *  \brief  Stop periodic temperature measurement.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpsMeasStop(void);

/*************************************************************************************************/
/*!
 *  \fn     HtpsMeasComplete
 *        
 *  \brief  Temperature measurement complete.
 *
 *  \param  connId      DM connection identifier.
 *  \param  bpmCccIdx   Index of temperature measurement CCC descriptor in CCC descriptor
 *                      handle table.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpsMeasComplete(dmConnId_t connId, uint8_t bpmCccIdx);

/*************************************************************************************************/
/*!
 *  \fn     HtpsProcMsg
 *        
 *  \brief  This function is called by the application when the periodic measurement
 *          timer expires.
 *
 *  \param  pMsg     Event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpsProcMsg(wsfMsgHdr_t *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     HtpsSetTmFlags
 *        
 *  \brief  Set the temperature measurement flags.
 *
 *  \param  flags      Temperature measurement flags.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpsSetTmFlags(uint8_t flags);

/*************************************************************************************************/
/*!
 *  \fn     HtpsSetItFlags
 *        
 *  \brief  Set the intermediate temperature flags.
 *
 *  \param  flags      Intermediate temperature flags.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpsSetItFlags(uint8_t flags);

#ifdef __cplusplus
};
#endif

#endif /* HTPS_API_H */
