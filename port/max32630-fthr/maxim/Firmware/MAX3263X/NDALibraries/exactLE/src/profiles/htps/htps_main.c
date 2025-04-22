/*************************************************************************************************/
/*!
 *  \file   htps_main.c
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

#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "att_api.h"
#include "svc_ch.h"
#include "svc_hts.h"
#include "app_api.h"
#include "app_hw.h"
#include "htps_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Control block */
static struct
{
  wsfTimer_t    measTimer;            /* periodic measurement timer */
  appTm_t       tm;                   /* temperature measurement */
  htpsCfg_t     cfg;                  /* configurable parameters */
  uint8_t       tmFlags;              /* temperature measurement flags */
  uint8_t       itFlags;              /* intermediate temperature flags */
} htpsCb;

/*************************************************************************************************/
/*!
 *  \fn     htpsBuildTm
 *        
 *  \brief  Build a temperature measurement characteristic.
 *
 *  \param  pBuf    Pointer to buffer to hold the built temperature measurement characteristic.
 *  \param  pTm     Temperature measurement values.
 *
 *  \return Length of pBuf in bytes.
 */
/*************************************************************************************************/
static uint8_t htpsBuildTm(uint8_t *pBuf, appTm_t *pTm)
{
  uint8_t   *p = pBuf;
  uint8_t   flags = pTm->flags;
  
  /* flags */
  UINT8_TO_BSTREAM(p, flags);

  /* measurement */
  UINT32_TO_BSTREAM(p, pTm->temperature);
  
  /* time stamp */
  if (flags & CH_TM_FLAG_TIMESTAMP)
  {
    UINT16_TO_BSTREAM(p, pTm->timestamp.year);
    UINT8_TO_BSTREAM(p, pTm->timestamp.month);
    UINT8_TO_BSTREAM(p, pTm->timestamp.day);
    UINT8_TO_BSTREAM(p, pTm->timestamp.hour);
    UINT8_TO_BSTREAM(p, pTm->timestamp.min);
    UINT8_TO_BSTREAM(p, pTm->timestamp.sec);
  }

  /* temperature type */
  if (flags & CH_TM_FLAG_TEMP_TYPE)
  {
    UINT8_TO_BSTREAM(p, pTm->tempType);
  }

  /* return length */
  return (uint8_t) (p - pBuf);
}

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
void HtpsInit(wsfHandlerId_t handlerId, htpsCfg_t *pCfg)
{
  htpsCb.measTimer.handlerId = handlerId;
  htpsCb.cfg = *pCfg;
}

/*************************************************************************************************/
/*!
 *  \fn     HtpsMeasStart
 *        
 *  \brief  Start periodic temperature measurement.  This function starts a timer to perform
 *          periodic measurements.
 *
 *  \param  connId      DM connection identifier.
 *  \param  timerEvt    WSF event designated by the application for the timer.
 *  \param  itCccIdx    Index of intermediate temperature CCC descriptor in CCC descriptor
 *                      handle table.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpsMeasStart(dmConnId_t connId, uint8_t timerEvt, uint8_t itCccIdx)
{
  /* initialize control block */
  htpsCb.measTimer.msg.param = connId;
  htpsCb.measTimer.msg.event = timerEvt;
  htpsCb.measTimer.msg.status = itCccIdx;  
  
  /* start timer */
  WsfTimerStartMs(&htpsCb.measTimer, htpsCb.cfg.period);
}

/*************************************************************************************************/
/*!
 *  \fn     HtpsMeasStop
 *        
 *  \brief  Stop periodic temperature measurement.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpsMeasStop(void)
{
  /* stop timer */
  WsfTimerStop(&htpsCb.measTimer);
}

/*************************************************************************************************/
/*!
 *  \fn     HtpsMeasComplete
 *        
 *  \brief  Temperature measurement complete.
 *
 *  \param  connId      DM connection identifier.
 *  \param  tmCccIdx    Index of temperature measurement CCC descriptor in CCC descriptor
 *                      handle table.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HtpsMeasComplete(dmConnId_t connId, uint8_t tmCccIdx)
{
  uint8_t buf[ATT_DEFAULT_PAYLOAD_LEN];
  uint8_t len;
  
  /* stop periodic measurement */
  HtpsMeasStop();
  
  /* if indications enabled  */
  if (AttsCccEnabled(connId, tmCccIdx))
  {
    /* read temperature measurement sensor data */
    AppHwTmRead(FALSE, &htpsCb.tm);
    
    /* set flags */
    htpsCb.tm.flags = htpsCb.tmFlags;
    
    /* build temperature measurement characteristic */
    len = htpsBuildTm(buf, &htpsCb.tm);

    /* send temperature measurement indication */
    AttsHandleValueInd(connId, HTS_TM_HDL, len, buf);
  }
}

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
void HtpsProcMsg(wsfMsgHdr_t *pMsg)
{
  uint8_t buf[ATT_DEFAULT_PAYLOAD_LEN];
  uint8_t len;
  
  /* if notifications enabled (note ccc idx is stored in hdr.status) */
  if (AttsCccEnabled((dmConnId_t) pMsg->param, pMsg->status))
  {
    /* read temperature measurement sensor data */
    AppHwTmRead(TRUE, &htpsCb.tm);
    
    /* set flags */
    htpsCb.tm.flags = htpsCb.itFlags;
    
    /* build temperature measurement characteristic */
    len = htpsBuildTm(buf, &htpsCb.tm);

    /* send intermediate temperature notification */
    AttsHandleValueNtf((dmConnId_t) pMsg->param, HTS_IT_HDL, len, buf);
    
    /* restart timer */
    WsfTimerStartMs(&htpsCb.measTimer, htpsCb.cfg.period);
  }
}

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
void HtpsSetTmFlags(uint8_t flags)
{
  htpsCb.tmFlags = flags;
}

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
void HtpsSetItFlags(uint8_t flags)
{
  htpsCb.itFlags = flags;
} 
