/*************************************************************************************************/
/*!
 *  \file   hrps_main.c
 *
 *  \brief  Heart Rate profile sensor.
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
#include "svc_hrs.h"
#include "app_api.h"
#include "app_hw.h"
#include "hrps_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Control block */
static struct
{
  wsfTimer_t    measTimer;            /* periodic measurement timer */
  appHrm_t      hrm;                  /* heart rate measurement */
  hrpsCfg_t     cfg;                  /* configurable parameters */
  uint16_t      energyExp;            /* energy expended value */
  uint8_t       flags;                /* heart rate measurement flags */ 
} hrpsCb;

/*************************************************************************************************/
/*!
 *  \fn     hrpsBuildHrm
 *        
 *  \brief  Build a heart rate measurement characteristic.
 *
 *  \param  pBuf     Pointer to buffer to hold the built heart rate measurement characteristic.
 *  \param  pHrm     Heart rate measurement values.
 *
 *  \return Length of pBuf in bytes.
 */
/*************************************************************************************************/
static uint8_t hrpsBuildHrm(uint8_t *pBuf, appHrm_t *pHrm)
{
  uint8_t   *p = pBuf;
  uint8_t   flags = pHrm->flags;
  uint8_t   i;
  uint16_t  *pInterval;
  
  /* flags */
  UINT8_TO_BSTREAM(p, flags);
  
  /* heart rate measurement */
  if (flags & CH_HRM_FLAGS_VALUE_16BIT)
  {
    UINT16_TO_BSTREAM(p, (uint16_t) pHrm->heartRate);
  }
  else
  {
    UINT8_TO_BSTREAM(p, pHrm->heartRate);
  }
  
  /* energy expended */
  if (flags & CH_HRM_FLAGS_ENERGY_EXP)
  {
    UINT16_TO_BSTREAM(p, pHrm->energyExp);
  }
  
  /* rr interval */
  if (flags & CH_HRM_FLAGS_RR_INTERVAL)
  {
    pInterval = pHrm->pRrInterval;
    for (i = pHrm->numIntervals; i > 0; i--, pInterval++)
    {
      UINT16_TO_BSTREAM(p, *pInterval);
    }
  }

  /* return length */
  return (uint8_t) (p - pBuf);
}

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
void HrpsInit(wsfHandlerId_t handlerId, hrpsCfg_t *pCfg)
{
  hrpsCb.measTimer.handlerId = handlerId;
  hrpsCb.cfg = *pCfg;
}

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
void HrpsMeasStart(dmConnId_t connId, uint8_t timerEvt, uint8_t hrmCccIdx)
{
  /* initialize control block */
  hrpsCb.measTimer.msg.param = connId;
  hrpsCb.measTimer.msg.event = timerEvt;
  hrpsCb.measTimer.msg.status = hrmCccIdx;  
  
  /* start timer */
  WsfTimerStartMs(&hrpsCb.measTimer, hrpsCb.cfg.period);
}

/*************************************************************************************************/
/*!
 *  \fn     HrpsMeasStop
 *        
 *  \brief  Stop periodic heart rate measurement.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HrpsMeasStop(void)
{
  /* stop timer */
  WsfTimerStop(&hrpsCb.measTimer);
}

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
void HrpsProcMsg(wsfMsgHdr_t *pMsg)
{
  uint8_t buf[ATT_DEFAULT_PAYLOAD_LEN];
  uint8_t len;
  
  /* if notifications enabled (note ccc idx is stored in hdr.status) */
  if (AttsCccEnabled((dmConnId_t) pMsg->param, pMsg->status))
  {
    /* read heart rate measurement sensor data */
    AppHwHrmRead(&hrpsCb.hrm);
    
    /* build heart rate measurement characteristic */
    len = hrpsBuildHrm(buf, &hrpsCb.hrm);

    /* send notification */
    AttsHandleValueNtf((dmConnId_t) pMsg->param, HRS_HRM_HDL, len, buf);
    
    /* restart timer */
    WsfTimerStartMs(&hrpsCb.measTimer, hrpsCb.cfg.period);
    
    /* increment energy expended for test/demonstration purposes */
    hrpsCb.hrm.energyExp++;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     HrpsWriteCback
 *        
 *  \brief  ATTS write callback for heart rate service.  Use this function as a parameter
 *          to SvcHrsCbackRegister().
 *
 *  \return ATT status.
 */
/*************************************************************************************************/
uint8_t HrpsWriteCback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                       uint16_t offset, uint16_t len, uint8_t *pValue, attsAttr_t *pAttr)
{ 
  if (*pValue == CH_HRCP_RESET_ENERGY_EXP)
  {
    /* reset energy expended */
    hrpsCb.hrm.energyExp = 0;
    return ATT_SUCCESS;
  }
  else
  {
    /* else unknown control point command */
    return HRS_ERR_CP_NOT_SUP;
  }
}

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
void HrpsSetFlags(uint8_t flags)
{
  hrpsCb.hrm.flags = flags;
}
