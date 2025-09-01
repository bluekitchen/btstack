/*************************************************************************************************/
/*!
 *  \file   medc_wsp.c
 *
 *  \brief  Health/medical collector, Weight Scale profile
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "wsf_assert.h"
#include "dm_api.h"
#include "att_api.h"
#include "app_cfg.h"
#include "app_api.h"
#include "app_db.h"
#include "svc_ch.h"
#include "wspc_api.h"
#include "medc_main.h"

/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/* Start of cached weight scale service handles; begins after DIS */
#define MEDC_DISC_WSS_START         (MEDC_DISC_DIS_START + DIS_HDL_LIST_LEN)

/* Total cached handle list length */
#define MEDC_DISC_HDL_LIST_LEN      (MEDC_DISC_WSS_START + WSPC_WSS_HDL_LIST_LEN)

/*! Pointers into handle list for weight scale service handles */
static uint16_t *pMedcWssHdlList = &medcCb.hdlList[MEDC_DISC_WSS_START];

/* sanity check:  make sure handle list length is <= app db handle list length */
WSF_CT_ASSERT(MEDC_DISC_HDL_LIST_LEN <= APP_DB_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/* List of characteristics to configure after service discovery */
static const attcDiscCfg_t medcCfgWssList[] = 
{  
  /* Write:  Weight scale measurement CCC descriptor  */
  {medcCccIndVal, sizeof(medcCccIndVal), WSPC_WSS_WSM_CCC_HDL_IDX}
};

/* Characteristic configuration list length */
#define MEDC_CFG_WSS_LIST_LEN   (sizeof(medcCfgWssList) / sizeof(attcDiscCfg_t))

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void medcWspInit(void);
static void medcWspDiscover(dmConnId_t connId);
static void medcWspConfigure(dmConnId_t connId, uint8_t status);
static void medcWspProcMsg(wsfMsgHdr_t *pMsg);
static void medcWspBtn(dmConnId_t connId, uint8_t btn);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! profile interface pointer */
medcIf_t medcWspIf =
{
  medcWspInit,
  medcWspDiscover,
  medcWspConfigure,
  medcWspProcMsg,
  medcWspBtn
};

/*************************************************************************************************/
/*!
 *  \fn     medcWssValueUpdate
 *        
 *  \brief  Process a received ATT read response, notification, or indication.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcWssValueUpdate(attEvt_t *pMsg)
{
  if (pMsg->hdr.status == ATT_SUCCESS)
  {
    /* determine which profile the handle belongs to; start with most likely */
    
    /* weight scale */
    if (WspcWssValueUpdate(pMedcWssHdlList, pMsg) == ATT_SUCCESS)    
    {
      return;
    }
    /* device information */
    if (DisValueUpdate(pMedcDisHdlList, pMsg) == ATT_SUCCESS)
    {
      return;
    }
    /* GATT */
    if (GattValueUpdate(pMedcGattHdlList, pMsg) == ATT_SUCCESS)    
    {
      return;
    }
  }
} 

/*************************************************************************************************/
/*!
 *  \fn     medcWspProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcWspProcMsg(wsfMsgHdr_t *pMsg)
{
  switch(pMsg->event)
  {
    case ATTC_HANDLE_VALUE_IND:
      medcWssValueUpdate((attEvt_t *) pMsg);
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medcWspInit
 *        
 *  \brief  Profile initialization function.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcWspInit(void)
{
  /* set handle list length */
  medcCb.hdlListLen = MEDC_DISC_HDL_LIST_LEN;
}

/*************************************************************************************************/
/*!
 *  \fn     medcWspDiscover
 *        
 *  \brief  Discover service for profile.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcWspDiscover(dmConnId_t connId)
{
  /* discover weight scale service */
  WspcWssDiscover(connId, pMedcWssHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     medcWspConfigure
 *        
 *  \brief  Configure service for profile.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    APP_DISC_CFG_START or APP_DISC_CFG_CONN_START.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcWspConfigure(dmConnId_t connId, uint8_t status)
{
  /* configure weight scale service */
  AppDiscConfigure(connId, status, MEDC_CFG_WSS_LIST_LEN,
                   (attcDiscCfg_t *) medcCfgWssList,
                   WSPC_WSS_HDL_LIST_LEN, pMedcWssHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     medcWspBtn
 *        
 *  \brief  Handle a button press.
 *
 *  \param  connId    Connection identifier.
 *  \param  btn       Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcWspBtn(dmConnId_t connId, uint8_t btn)
{
  return;
}
