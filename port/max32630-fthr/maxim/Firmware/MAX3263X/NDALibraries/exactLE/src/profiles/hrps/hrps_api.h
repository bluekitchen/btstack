/*************************************************************************************************/
/*!
 *  \file   hrps_api.h
 *
 *  \brief  Heart Rate profile sensor.
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
#ifndef HRPS_API_H
#define HRPS_API_H

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
} hrpsCfg_t;

/*************************************************************************************************/
/*!
 *  \fn     HrpsInit
 *        
 *  \brief  Initialize the Heart Rate profile sensor.
 *
 *  \param  handerId    WSF handler ID of the application using this service.
 *  \param  pCfg        Configurable parameters.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HrpsInit(wsfHandlerId_t handlerId, hrpsCfg_t *pCfg);

/*************************************************************************************************/
/*!
 *  \fn     HrpsMeasStart
 *        
 *  \brief  Start periodic heart rate measurement.  This function starts a timer to perform
 *          periodic measurements.
 *
 *  \param  connId      DM connection identifier.
 *  \param  timerEvt    WSF event designated by the application for the timer.
 *  \param  hrmCccIdx   Index of heart rate CCC descriptor in CCC descriptor handle table.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HrpsMeasStart(dmConnId_t connId, uint8_t timerEvt, uint8_t hrmCccIdx);

/*************************************************************************************************/
/*!
 *  \fn     HrpsMeasStop
 *        
 *  \brief  Stop periodic heart rate measurement.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HrpsMeasStop(void);

/*************************************************************************************************/
/*!
 *  \fn     HrpsProcMsg
 *        
 *  \brief  This function is called by the application when the periodic measurement
 *          timer expires.
 *
 *  \param  pMsg     Event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HrpsProcMsg(wsfMsgHdr_t *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     HrpsWriteCback
 *        
 *  \brief  ATTS write callback for heart rate service Use this function as a parameter
 *          to SvcHrsCbackRegister().
 *
 *  \return ATT status.
 */
/*************************************************************************************************/
uint8_t HrpsWriteCback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                       uint16_t offset, uint16_t len, uint8_t *pValue, attsAttr_t *pAttr);

/*************************************************************************************************/
/*!
 *  \fn     HrpsSetFlags
 *        
 *  \brief  Set the heart rate measurement flags.
 *
 *  \param  flags      Heart rate measurement flags.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HrpsSetFlags(uint8_t flags);

#ifdef __cplusplus
};
#endif

#endif /* HRPS_API_H */
