/*************************************************************************************************/
/*!
 *  \file   bas_api.h
 *
 *  \brief  Battery service server.
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
#ifndef BAS_API_H
#define BAS_API_H

#include "wsf_timer.h"
#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/


/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*! Battery service configurable parameters */
typedef struct
{
  wsfTimerTicks_t     period;     /*! Battery measurement timer expiration period in seconds */
  uint16_t            count;      /*! Perform battery measurement after this many timer periods */
  uint8_t             threshold;  /*! Send battery level notification to peer when below this level. */
} basCfg_t;

/*************************************************************************************************/
/*!
 *  \fn     BasInit
 *        
 *  \brief  Initialize the battery service server.
 *
 *  \param  handerId    WSF handler ID of the application using this service.
 *  \param  pCfg        Battery service configurable parameters.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BasInit(wsfHandlerId_t handlerId, basCfg_t *pCfg);

/*************************************************************************************************/
/*!
 *  \fn     BasMeasBattStart
 *        
 *  \brief  Start periodic battery level measurement.  This function starts a timer to perform
 *          periodic battery measurements.
 *
 *  \param  connId      DM connection identifier.
 *  \param  timerEvt    WSF event designated by the application for the timer.
 *  \param  battCccIdx  Index of battery level CCC descriptor in CCC descriptor handle table.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BasMeasBattStart(dmConnId_t connId, uint8_t timerEvt, uint8_t battCccIdx);

/*************************************************************************************************/
/*!
 *  \fn     BasMeasBattStop
 *        
 *  \brief  Stop periodic battery level measurement.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BasMeasBattStop(void);

/*************************************************************************************************/
/*!
 *  \fn     BasProcMsg
 *        
 *  \brief  This function is called by the application when the battery periodic measurement
 *          timer expires.
 *
 *  \param  pMsg     Event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BasProcMsg(wsfMsgHdr_t *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     BasSendBattLevel
 *        
 *  \brief  Send the battery level to the peer device.
 *
 *  \param  connId      DM connection identifier.
 *  \param  battCccIdx  Index of battery level CCC descriptor in CCC descriptor handle table.
 *  \param  level       The battery level.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BasSendBattLevel(dmConnId_t connId, uint8_t battCccIdx, uint8_t level);

/*************************************************************************************************/
/*!
 *  \fn     BasReadCback
 *        
 *  \brief  ATTS read callback for battery service used to read the battery level.  Use this
 *          function as a parameter to SvcBattCbackRegister().
 *
 *  \return ATT status.
 */
/*************************************************************************************************/
uint8_t BasReadCback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                     uint16_t offset, attsAttr_t *pAttr);

#ifdef __cplusplus
};
#endif

#endif /* BAS_API_H */
