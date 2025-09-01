/*************************************************************************************************/
/*!
 *  \file   wsps_api.h
 *
 *  \brief  Weight Scale profile sensor.
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
#ifndef WSPS_API_H
#define WSPS_API_H

#include "wsf_timer.h"
#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************************/
/*!
 *  \fn     WspsMeasComplete
 *        
 *  \brief  Weight scale measurement complete.
 *
 *  \param  connId      DM connection identifier.
 *  \param  wsmCccIdx   Index of weight scale measurement CCC descriptor in CCC descriptor
 *                      handle table.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WspsMeasComplete(dmConnId_t connId, uint8_t wsmCccIdx);

/*************************************************************************************************/
/*!
 *  \fn     WspsSetWsmFlags
 *        
 *  \brief  Set the weight scale measurement flags.
 *
 *  \param  flags      Weight scale measurement flags.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WspsSetWsmFlags(uint8_t flags);

#ifdef __cplusplus
};
#endif

#endif /* WSPS_API_H */
