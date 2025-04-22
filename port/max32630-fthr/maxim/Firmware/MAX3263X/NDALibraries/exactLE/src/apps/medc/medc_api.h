/*************************************************************************************************/
/*!
 *  \file   medc_api.h
 *
 *  \brief  Health/medical collector sample application.
 *
 *          $Date: 2015-12-21 15:07:14 -0600 (Mon, 21 Dec 2015) $
 *          $Revision: 20721 $
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
#ifndef MEDC_API_H
#define MEDC_API_H

#include "wsf_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Profile identifier used for MedcSetProfile() */
enum
{
  MEDC_ID_HRP,      /*! Heart rate profile */
  MEDC_ID_BLP,      /*! Blood pressure profile */
  MEDC_ID_GLP,      /*! Glucose profile */
  MEDC_ID_WSP,      /*! Weight scale profile */
  MEDC_ID_HTP       /*! Health thermometer profile */
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/
/*************************************************************************************************/
/*!
 *  \fn     MedcStart
 *        
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedcStart(void);

/*************************************************************************************************/
/*!
 *  \fn     MedcHandlerInit
 *        
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID for App.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedcHandlerInit(wsfHandlerId_t handlerId);


/*************************************************************************************************/
/*!
 *  \fn     MedcHandler
 *        
 *  \brief  WSF event handler for the application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedcHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     MedcSetProfile
 *        
 *  \brief  Set the profile to be used by the application.  This function is called internally
 *          by MedcHandlerInit() with a default value.  It may also be called by the system
 *          to configure the profile after executing MedcHandlerInit() and before executing
 *          MedcStart().
 *
 *  \param  profile  Profile identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedcSetProfile(uint8_t profile);

#ifdef __cplusplus
};
#endif

#endif /* MEDC_API_H */
