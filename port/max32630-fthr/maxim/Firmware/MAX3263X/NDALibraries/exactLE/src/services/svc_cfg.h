/*************************************************************************************************/
/*!
 *  \file   svc_cfg.h
 *
 *  \brief  Service configuration.
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
#ifndef SVC_CFG_H
#define SVC_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Default read security permissions for service characteristics */
#ifndef SVC_SEC_PERMIT_READ
#define SVC_SEC_PERMIT_READ ATTS_PERMIT_READ
#endif

/*! Default write security permissions for service characteristics */
#ifndef SVC_SEC_PERMIT_WRITE
#define SVC_SEC_PERMIT_WRITE ATTS_PERMIT_WRITE
#endif

#ifdef __cplusplus
};
#endif

#endif /* SVC_CFG_H */
