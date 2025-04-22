/*************************************************************************************************/
/*!
 *  \file   medc_hrp.c
 *
 *  \brief  Health/medical collector, Heart Rate profile
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
#include "hrpc_api.h"
#include "medc_main.h"

/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/* Start of cached heart rate service handles; begins after DIS */
#define MEDC_DISC_HRS_START         (MEDC_DISC_DIS_START + DIS_HDL_LIST_LEN)

/* Total cached handle list length */
#define MEDC_DISC_HDL_LIST_LEN      (MEDC_DISC_HRS_START + HRPC_HRS_HDL_LIST_LEN)

/*! Pointers into handle list heart rate service handles */
static uint16_t *pMedcHrsHdlList = &medcCb.hdlList[MEDC_DISC_HRS_START];

/* sanity check:  make sure handle list length is <= app db handle list length */
WSF_CT_ASSERT(MEDC_DISC_HDL_LIST_LEN <= APP_DB_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/* HRS Control point "Reset Energy Expended" */
static const uint8_t medcHrsRstEnExp[] = {CH_HRCP_RESET_ENERGY_EXP};

/* List of characteristics to configure after service discovery */
static const attcDiscCfg_t medcCfgHrsList[] = 
{  
  /* Read:  HRS Body sensor location */
  {NULL, 0, HRPC_HRS_BSL_HDL_IDX},

  /* Write:  HRS Control point "Reset Energy Expended"  */
  {medcHrsRstEnExp, sizeof(medcHrsRstEnExp), HRPC_HRS_HRCP_HDL_IDX},

  /* Write:  HRS Heart rate measurement CCC descriptor  */
  {medcCccNtfVal, sizeof(medcCccNtfVal), HRPC_HRS_HRM_CCC_HDL_IDX},
};

/* Characteristic configuration list length */
#define MEDC_CFG_HRS_LIST_LEN   (sizeof(medcCfgHrsList) / sizeof(attcDiscCfg_t))

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void medcHrpInit(void);
static void medcHrpDiscover(dmConnId_t connId);
static void medcHrpConfigure(dmConnId_t connId, uint8_t status);
static void medcHrpProcMsg(wsfMsgHdr_t *pMsg);
static void medcHrpBtn(dmConnId_t connId, uint8_t btn);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! profile interface pointer */
medcIf_t medcHrpIf =
{
  medcHrpInit,
  medcHrpDiscover,
  medcHrpConfigure,
  medcHrpProcMsg,
  medcHrpBtn
};

/*************************************************************************************************/
/*!
 *  \fn     medcHrsValueUpdate
 *        
 *  \brief  Process a received ATT read response, notification, or indication.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHrsValueUpdate(attEvt_t *pMsg)
{
  if (pMsg->hdr.status == ATT_SUCCESS)
  {
    /* determine which profile the handle belongs to; start with most likely */
    
    /* heart rate */
    if (HrpcHrsValueUpdate(pMedcHrsHdlList, pMsg) == ATT_SUCCESS)    
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
 *  \fn     medcHrpProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHrpProcMsg(wsfMsgHdr_t *pMsg)
{
  switch(pMsg->event)
  {
    case ATTC_READ_RSP:
    case ATTC_HANDLE_VALUE_NTF:
    case ATTC_HANDLE_VALUE_IND:
      medcHrsValueUpdate((attEvt_t *) pMsg);
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medcHrpInit
 *        
 *  \brief  Profile initialization function.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHrpInit(void)
{
  /* set handle list length */
  medcCb.hdlListLen = MEDC_DISC_HDL_LIST_LEN;
  
  /* set autoconnect UUID */
  medcCb.autoUuid = ATT_UUID_HEART_RATE_SERVICE;
}

/*************************************************************************************************/
/*!
 *  \fn     medcHrpDiscover
 *        
 *  \brief  Discover service for profile.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHrpDiscover(dmConnId_t connId)
{
  /* discover heart rate service */
  HrpcHrsDiscover(connId, pMedcHrsHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     medcHrpConfigure
 *        
 *  \brief  Configure service for profile.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    APP_DISC_CFG_START or APP_DISC_CFG_CONN_START.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHrpConfigure(dmConnId_t connId, uint8_t status)
{
  /* configure heart rate service */
  AppDiscConfigure(connId, status, MEDC_CFG_HRS_LIST_LEN,
                   (attcDiscCfg_t *) medcCfgHrsList,
                   HRPC_HRS_HDL_LIST_LEN, pMedcHrsHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     medcHrpBtn
 *        
 *  \brief  Handle a button press.
 *
 *  \param  connId    Connection identifier.
 *  \param  btn       Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcHrpBtn(dmConnId_t connId, uint8_t btn)
{
  return;
}