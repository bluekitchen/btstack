/*************************************************************************************************/
/*!
 *  \file   bas_main.c
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "att_api.h"
#include "svc_ch.h"
#include "svc_batt.h"
#include "app_api.h"
#include "app_hw.h"
#include "bas_api.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* battery level initialization value */
#define BAS_BATT_LEVEL_INIT           0xFF

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Control block */
static struct
{
  wsfTimer_t        measTimer;            /* periodic measurement timer */
  basCfg_t          cfg;                  /* configurable parameters */
  uint16_t          currCount;            /* current measurement period count */
  uint8_t           sentBattLevel;        /* value of last sent battery level */
} basCb;

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
void BasInit(wsfHandlerId_t handlerId, basCfg_t *pCfg)
{
  basCb.measTimer.handlerId = handlerId;
  basCb.cfg = *pCfg;
}

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
void BasMeasBattStart(dmConnId_t connId, uint8_t timerEvt, uint8_t battCccIdx)
{
  /* initialize control block */
  basCb.measTimer.msg.param = connId;
  basCb.measTimer.msg.event = timerEvt;
  basCb.measTimer.msg.status = battCccIdx;  
  basCb.sentBattLevel = BAS_BATT_LEVEL_INIT;
  basCb.currCount = basCb.cfg.count;
  
  /* start timer */
  WsfTimerStartSec(&basCb.measTimer, basCb.cfg.period);
}

/*************************************************************************************************/
/*!
 *  \fn     BasMeasBattStop
 *        
 *  \brief  Stop periodic battery level measurement.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BasMeasBattStop(void)
{
  /* stop timer */
  WsfTimerStop(&basCb.measTimer);
}

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
void BasProcMsg(wsfMsgHdr_t *pMsg)
{
  uint8_t level;
  
  if (--basCb.currCount == 0)
  {
    /* reset count */
    basCb.currCount = basCb.cfg.count;
    
    /* read battery level */
    AppHwBattRead(&level);
    
    /* if battery level should be sent to peer */
    if (level <= basCb.cfg.threshold && level != basCb.sentBattLevel)
    {
      /* note ccc idx is stored in hdr.status */
      BasSendBattLevel((dmConnId_t) pMsg->param, pMsg->status, level);
    }
  }
  
  /* restart timer */
  WsfTimerStartSec(&basCb.measTimer, basCb.cfg.period);
}

/*************************************************************************************************/
/*!
 *  \fn     BasSendBattLevel
 *        
 *  \brief  Send the battery level to the peer device.
 *
 *  \param  connId      DM connection identifier.
 *  \param  idx         Index of battery level CCC descriptor in CCC descriptor handle table. 
 *  \param  level       The battery level.
 *
 *  \return None.
 */
/*************************************************************************************************/
void BasSendBattLevel(dmConnId_t connId, uint8_t idx, uint8_t level)
{
  if (AttsCccEnabled(connId, idx))
  {
    basCb.sentBattLevel = level;
      
    AttsHandleValueNtf(connId, BATT_LVL_HDL, CH_BATT_LEVEL_LEN, &level);
  }
}

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
                     uint16_t offset, attsAttr_t *pAttr)
{
  /* read the battery level and set attribute value */
  AppHwBattRead(pAttr->pValue);
  
  return ATT_SUCCESS;
}
