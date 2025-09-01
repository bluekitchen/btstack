/*************************************************************************************************/
/*!
 *  \file   wsps_main.c
 *
 *  \brief  Weight Scale profile sensor.
 *
 *          $Date: 2015-08-25 13:59:48 -0500 (Tue, 25 Aug 2015) $
 *          $Revision: 18761 $
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
#include "svc_wss.h"
#include "app_api.h"
#include "app_hw.h"
#include "wsps_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Control block */
static struct
{
  appWsm_t      wsm;                  /* weight scale measurement */
  uint8_t       wsmFlags;             /* flags */
} wspsCb;

/*************************************************************************************************/
/*!
 *  \fn     wspsBuildWsm
 *        
 *  \brief  Build a weight scale measurement characteristic.
 *
 *  \param  pBuf     Pointer to buffer to hold the built weight scale measurement characteristic.
 *  \param  pWsm     Weight measurement values.
 *
 *  \return Length of pBuf in bytes.
 */
/*************************************************************************************************/
static uint8_t wspsBuildWsm(uint8_t *pBuf, appWsm_t *pWsm)
{
  uint8_t   *p = pBuf;
  uint8_t   flags = pWsm->flags;
  
  /* flags */
  UINT8_TO_BSTREAM(p, flags);

  /* measurement */
  UINT16_TO_BSTREAM(p, pWsm->weight);

  /* time stamp */
  if (flags & CH_WSM_FLAG_TIMESTAMP)
  {
    UINT16_TO_BSTREAM(p, pWsm->timestamp.year);
    UINT8_TO_BSTREAM(p, pWsm->timestamp.month);
    UINT8_TO_BSTREAM(p, pWsm->timestamp.day);
    UINT8_TO_BSTREAM(p, pWsm->timestamp.hour);
    UINT8_TO_BSTREAM(p, pWsm->timestamp.min);
    UINT8_TO_BSTREAM(p, pWsm->timestamp.sec);
  }

  /* return length */
  return (uint8_t) (p - pBuf);
}

/*************************************************************************************************/
/*!
 *  \fn     WspsMeasComplete
 *        
 *  \brief  Weight measurement complete.
 *
 *  \param  connId      DM connection identifier.
 *  \param  wsmCccIdx   Index of weight scale measurement CCC descriptor in CCC descriptor
 *                      handle table.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WspsMeasComplete(dmConnId_t connId, uint8_t wsmCccIdx)
{
  uint8_t buf[ATT_DEFAULT_PAYLOAD_LEN];
  uint8_t len;
  
  /* if indications enabled  */
  if (AttsCccEnabled(connId, wsmCccIdx))
  {
    /* read weight scale measurement sensor data */
    AppHwWsmRead(&wspsCb.wsm);
    
    /* set flags */
    wspsCb.wsm.flags = wspsCb.wsmFlags;
    
    /* build weight scale measurement characteristic */
    len = wspsBuildWsm(buf, &wspsCb.wsm);

    /* send weight scale measurement indication */
    AttsHandleValueInd(connId, WSS_WM_HDL, len, buf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     WspsSetWsmFlags
 *        
 *  \brief  Set the weight scale measurement flags.
 *
 *  \param  flags      Weight measurement flags.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WspsSetWsmFlags(uint8_t flags)
{
  wspsCb.wsmFlags = flags;
}