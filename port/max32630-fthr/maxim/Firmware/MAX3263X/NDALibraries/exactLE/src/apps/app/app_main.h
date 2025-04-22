/*************************************************************************************************/
/*!
 *  \file   app_main.h
 *
 *  \brief  Application framework main module.
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
#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "wsf_os.h"
#include "dm_api.h"
#include "app_db.h"

#ifdef __cplusplus
extern "C" {
#endif


/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! No security record handle  */
#define APP_DB_HDL_NONE             NULL

/*! App WSF handler event bitmasks */
#define APP_BTN_DOWN_EVT            0x10  /*! Button pressed down event */

/*! App WSF message event starting values */
#define APP_MSG_START               0x00
#define APP_SLAVE_MSG_START         0x10
#define APP_MASTER_MSG_START        0x20

/*! App WSF message event enumeration */
enum
{
  APP_BTN_POLL_IND = APP_MSG_START,       /*! Button poll timer expired */
  APP_UI_TIMER_IND                        /*! UI timer expired */
};

/*! App slave WSF message event enumeration */
enum
{
  APP_CONN_UPDATE_TIMEOUT_IND = APP_SLAVE_MSG_START   /*! Connection parameter update timer expired */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Message handling function type */
typedef void (*appMsgHandler_t)(wsfMsgHdr_t *pMsg);

/*! Connection control block */
typedef struct
{
  appDbHdl_t        dbHdl;                /*! Device database handle */
  dmConnId_t        connId;               /*! Connection ID */
  bool_t            bonded;               /*! TRUE if bonded with peer device */
  bool_t            bondByLtk;            /*! TRUE if bonded state being determined by LTK */
  bool_t            bondByPairing;        /*! TRUE if bonded state being determined by pairing */
  bool_t            initiatingSec;        /*! TRUE if initiating security */
  uint8_t           rcvdKeys;             /*! Bitmask of keys received during pairing */
} appConnCb_t;

/*! Main control block */
typedef struct
{
  appMsgHandler_t   slaveCback;           /*! Slave message handler callback */
  appMsgHandler_t   masterCback;          /*! Slave message handler callback */
} appCb_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! Connection control block array */
extern appConnCb_t appConnCb[DM_CONN_MAX];

/*! WSF handler ID */
extern wsfHandlerId_t appHandlerId;

/*! Main control block */
extern appCb_t appCb;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

uint32_t appGenPasscode(dmConnId_t connId);
void appSetBondable(bool_t bonded);
void appSetDbHdl(dmConnId_t connId, appDbHdl_t dbHdl);
bool_t appCheckBonded(dmConnId_t connId);
bool_t appCheckBondByLtk(dmConnId_t connId);

void appUiBtnPoll(void);
void appUiTimerExpired(wsfMsgHdr_t *pMsg);

#ifdef __cplusplus
};
#endif

#endif /* APP_MAIN_H */
