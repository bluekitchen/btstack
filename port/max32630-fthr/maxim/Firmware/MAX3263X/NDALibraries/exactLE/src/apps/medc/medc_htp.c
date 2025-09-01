/*************************************************************************************************/
/*!
 *  \file   medc_htp.c
 *
 *  \brief  Health/medical collector, Health Thermometer profile
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
#include "htpc_api.h"
#include "medc_main.h"

/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/* Start of cached health thermometer service handles; begins after DIS */
#define MEDC_DISC_HTS_START         (MEDC_DISC_DIS_START + DIS_HDL_LIST_LEN)

/* Total cached handle list length */
#define MEDC_DISC_HDL_LIST_LEN      (MEDC_DISC_HTS_START + HTPC_HTS_HDL_LIST_LEN)

/*! Pointers into handle list for health thermometer service handles */
static uint16_t *pMedcHtsHdlList = &medcCb.hdlList[MEDC_DISC_HTS_START];

/* sanity check:  make sure handle list length is <= app db handle list length */
WSF_CT_ASSERT(MEDC_DISC_HDL_LIST_LEN <= APP_DB_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/* List of characteristics to configure after service discovery */
static const attcDiscCfg_t medcCfgHtsList[] = 
{  
  /* Read:  Temperature type */
  {NULL, 0, HTPC_HTS_TT_HDL_IDX},

  /* Write:  Temperature measurement CCC descriptor  */
  {medcCccIndVal, sizeof(medcCccIndVal), HTPC_HTS_TM_CCC_HDL_IDX},

  /* Write:  Intermediate temperature CCC descriptor  */
  {medcCccNtfVal, sizeof(medcCccNtfVal), HTPC_HTS_IT_CCC_HDL_IDX},
};

/* Characteristic configuration list length */
#define MEDC_CFG_HTS_LIST_LEN   (sizeof(medcCfgHtsList) / sizeof(attcDiscCfg_t))

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void medcHtpInit(void);
static void medcHtpDiscover(dmConnId_t connId);
static void medcHtpConfigure(dmConnId_t connId, uint8_t status);
static void medcHtpProcMsg(wsfMsgHdr_t *pMsg);
static void medcHtpBtn(dmConnId_t connId, uint8_t btn);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! profile interface pointer */
medcIf_t medcHtpIf =
{
  medcHtpInit,
  medcHtpDiscover,
  medcHtpConfigure,
  medcHtpProcMsg,
  medcHtpBtn
};

/*************************************************************************************************/
/*!
 *  \fn     medcHtsValueUpdate
 *        
 *  \brief  Process a received ATT read response, notification, or indication.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHtsValueUpdate(attEvt_t *pMsg)
{
  if (pMsg->hdr.status == ATT_SUCCESS)
  {
    /* determine which profile the handle belongs to; start with most likely */
    
    /* health thermometer */
    if (HtpcHtsValueUpdate(pMedcHtsHdlList, pMsg) == ATT_SUCCESS)    
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
 *  \fn     medcHtpProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHtpProcMsg(wsfMsgHdr_t *pMsg)
{
  switch(pMsg->event)
  {
    case ATTC_READ_RSP:
    case ATTC_HANDLE_VALUE_NTF:
    case ATTC_HANDLE_VALUE_IND:
      medcHtsValueUpdate((attEvt_t *) pMsg);
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medcHtpInit
 *        
 *  \brief  Profile initialization function.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHtpInit(void)
{
  /* set handle list length */
  medcCb.hdlListLen = MEDC_DISC_HDL_LIST_LEN;
  
  /* set autoconnect UUID */
  medcCb.autoUuid = ATT_UUID_HEALTH_THERM_SERVICE;
}

/*************************************************************************************************/
/*!
 *  \fn     medcHtpDiscover
 *        
 *  \brief  Discover service for profile.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHtpDiscover(dmConnId_t connId)
{
  /* discover health thermometer service */
  HtpcHtsDiscover(connId, pMedcHtsHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     medcHtpConfigure
 *        
 *  \brief  Configure service for profile.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    APP_DISC_CFG_START or APP_DISC_CFG_CONN_START.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHtpConfigure(dmConnId_t connId, uint8_t status)
{
  /* configure health thermometer service */
  AppDiscConfigure(connId, status, MEDC_CFG_HTS_LIST_LEN,
                   (attcDiscCfg_t *) medcCfgHtsList,
                   HTPC_HTS_HDL_LIST_LEN, pMedcHtsHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     medcHtpBtn
 *        
 *  \brief  Handle a button press.
 *
 *  \param  connId    Connection identifier.
 *  \param  btn       Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHtpBtn(dmConnId_t connId, uint8_t btn)
{
  return;
}