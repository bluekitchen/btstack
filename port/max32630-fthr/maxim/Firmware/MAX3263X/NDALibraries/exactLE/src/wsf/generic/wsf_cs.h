/*************************************************************************************************/
/*!
 *  \file   wsf_cs.h
 *        
 *  \brief  Critical section macros.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
 *  
 *  Copyright (c) 2009 Wicentric, Inc., all rights reserved.
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
#ifndef WSF_CS_H
#define WSF_CS_H

#include "wsf_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \def    WSF_CS_INIT
 *        
 *  \brief  Initialize critical section.  This macro may define a variable.
 *
 *  \param  cs    Critical section variable to be defined.
 */
/*************************************************************************************************/
#define WSF_CS_INIT(cs)

/*************************************************************************************************/
/*!
 *  \def    WSF_CS_ENTER
 *        
 *  \brief  Enter a critical section.
 *
 *  \param  cs    Critical section variable.
 */
/*************************************************************************************************/
#define WSF_CS_ENTER(cs)        WsfCsEnter()
  
/*************************************************************************************************/
/*!
 *  \def    WSF_CS_EXIT
 *        
 *  \brief  Exit a critical section.
 *
 *  \param  cs    Critical section variable.
 */
/*************************************************************************************************/
#define WSF_CS_EXIT(cs)        WsfCsExit()

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/
void WsfCsEnter(void);
void WsfCsExit(void);

#ifdef __cplusplus
};
#endif

#endif /* WSF_CS_H */
