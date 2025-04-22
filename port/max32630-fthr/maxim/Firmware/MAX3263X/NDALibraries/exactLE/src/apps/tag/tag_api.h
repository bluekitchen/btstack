/*************************************************************************************************/
/*!
 *  \file   tag_api.h
 *
 *  \brief  Tag sample application interface.
 *
 *          $Date: 2015-12-21 15:07:14 -0600 (Mon, 21 Dec 2015) $
 *          $Revision: 20721 $
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
#ifndef TAG_API_H
#define TAG_API_H

#include "wsf_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/
/*************************************************************************************************/
/*!
 *  \fn     TagStart
 *        
 *  \brief  Start the proximity profile reporter application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void TagStart(void);

/*************************************************************************************************/
/*!
 *  \fn     TagHandlerInit
 *        
 *  \brief  Proximity reporter handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID for App.
 *
 *  \return None.
 */
/*************************************************************************************************/
void TagHandlerInit(wsfHandlerId_t handlerId);


/*************************************************************************************************/
/*!
 *  \fn     TagHandler
 *        
 *  \brief  WSF event handler for proximity reporter.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void TagHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg);

#ifdef __cplusplus
};
#endif

#endif /* TAG_API_H */