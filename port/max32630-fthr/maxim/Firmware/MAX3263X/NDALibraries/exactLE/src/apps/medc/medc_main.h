/*************************************************************************************************/
/*!
 *  \file   medc_main.h
 *        
 *  \brief  Health/medical collector sample application interface file.
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

#ifndef MEDC_MAIN_H
#define MEDC_MAIN_H

#include "gatt_api.h"
#include "dis_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! the Client handle list, medcCb.hdlList[], is set as follows:
 *
 *  ------------------------------- <- MEDC_DISC_GATT_START
 *  | GATT handles                |
 *  |...                          |
 *  ------------------------------- <- MEDC_DISC_DIS_START
 *  | DIS handles                 |
 *  | ...                         |
 *  ------------------------------- <- Configured med service start
 *  | Med service handles         |
 *  | ...                         |
 *  -------------------------------
 */

/*! Start of each service's handles in the the handle list */
#define MEDC_DISC_GATT_START        0
#define MEDC_DISC_DIS_START         (MEDC_DISC_GATT_START + GATT_HDL_LIST_LEN)

/*! WSF message event starting value */
#define MEDC_MSG_START             0xA0

/*! WSF message event enumeration */
enum
{
  MEDC_TIMER_IND = MEDC_MSG_START,    /*! Timer expired */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! profile interface callback functions */
typedef void (*medcInitCback_t)(void);
typedef void (*medcDiscoverCback_t)(dmConnId_t connId);
typedef void (*medcConfigureCback_t)(dmConnId_t connId, uint8_t status);
typedef void (*medcProcMsgCback_t)(wsfMsgHdr_t *pMsg);
typedef void (*medcBtnCback_t)(dmConnId_t connId, uint8_t btn);

/*! profile interface structure */
typedef struct
{
  medcInitCback_t       init;
  medcDiscoverCback_t   discover;
  medcConfigureCback_t  configure;
  medcProcMsgCback_t    procMsg;
  medcBtnCback_t        btn;
} medcIf_t;

/*! application control block */
typedef struct
{
  uint16_t          hdlList[APP_DB_HDL_LIST_LEN];   /*! Cached handle list */
  medcIf_t          *pIf;                           /*! Profile interface */
  wsfHandlerId_t    handlerId;                      /*! WSF hander ID */
  uint16_t          autoUuid;                       /*! Service UUID for autoconnect */
  bool_t            scanning;                       /*! TRUE if scanning */
  bool_t            autoConnect;                    /*! TRUE if auto-connecting */
  uint8_t           hdlListLen;                     /*! Cached handle list length */
  uint8_t           discState;                      /*! Service discovery state */
  uint8_t           cfgState;                       /*! Service configuration state */
} medcCb_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! Default value for CCC indications and notifications */
extern const uint8_t medcCccIndVal[2];

/*! Default value for CCC notifications */
extern const uint8_t medcCccNtfVal[2];

/*! Pointers into handle list for GATT and DIS service handles */
extern uint16_t *pMedcGattHdlList;
extern uint16_t *pMedcDisHdlList;

/*! application control block */
extern medcCb_t medcCb;

/*! profile interface pointers */
extern medcIf_t medcHrpIf;          /* heart rate profile */
extern medcIf_t medcBlpIf;          /* blood pressure profile */
extern medcIf_t medcGlpIf;          /* glucose profile */
extern medcIf_t medcWspIf;          /* weight scale profile */
extern medcIf_t medcHtpIf;          /* health thermometer profile */

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

#ifdef __cplusplus
};
#endif

#endif /* MEDC_MAIN_H */

