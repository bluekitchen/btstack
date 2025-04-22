/*************************************************************************************************/
/*!
 *  \file   medc_blp.c
 *
 *  \brief  Health/medical collector, Blood Pressure profile
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
#include "blpc_api.h"
#include "medc_main.h"

/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/* Start of cached blood pressure service handles; begins after DIS */
#define MEDC_DISC_BPS_START         (MEDC_DISC_DIS_START + DIS_HDL_LIST_LEN)

/* Total cached handle list length */
#define MEDC_DISC_HDL_LIST_LEN      (MEDC_DISC_BPS_START + BLPC_BPS_HDL_LIST_LEN)

/*! Pointers into handle list for blood pressure service handles */
static uint16_t *pMedcBpsHdlList = &medcCb.hdlList[MEDC_DISC_BPS_START];

/* sanity check:  make sure handle list length is <= app db handle list length */
WSF_CT_ASSERT(MEDC_DISC_HDL_LIST_LEN <= APP_DB_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/* List of characteristics to configure after service discovery */
static const attcDiscCfg_t medcCfgBpsList[] = 
{  
  /* Read:  Blood pressure feature */
  {NULL, 0, BLPC_BPS_BPF_HDL_IDX},

  /* Write:  Blood pressure measurement CCC descriptor  */
  {medcCccIndVal, sizeof(medcCccIndVal), BLPC_BPS_BPM_CCC_HDL_IDX},

  /* Write:  Intermediate cuff pressure CCC descriptor  */
  {medcCccNtfVal, sizeof(medcCccNtfVal), BLPC_BPS_ICP_CCC_HDL_IDX},
};

/* Characteristic configuration list length */
#define MEDC_CFG_BPS_LIST_LEN   (sizeof(medcCfgBpsList) / sizeof(attcDiscCfg_t))

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void medcBlpInit(void);
static void medcBlpDiscover(dmConnId_t connId);
static void medcBlpConfigure(dmConnId_t connId, uint8_t status);
static void medcBlpProcMsg(wsfMsgHdr_t *pMsg);
static void medcBlpBtn(dmConnId_t connId, uint8_t btn);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! profile interface pointer */
medcIf_t medcBlpIf =
{
  medcBlpInit,
  medcBlpDiscover,
  medcBlpConfigure,
  medcBlpProcMsg,
  medcBlpBtn
};

/*************************************************************************************************/
/*!
 *  \fn     medcBpsValueUpdate
 *        
 *  \brief  Process a received ATT read response, notification, or indication.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcBpsValueUpdate(attEvt_t *pMsg)
{
  if (pMsg->hdr.status == ATT_SUCCESS)
  {
    /* determine which profile the handle belongs to; start with most likely */
    
    /* blood pressure */
    if (BlpcBpsValueUpdate(pMedcBpsHdlList, pMsg) == ATT_SUCCESS)    
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
 *  \fn     medcBlpProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcBlpProcMsg(wsfMsgHdr_t *pMsg)
{
  switch(pMsg->event)
  {
    case ATTC_READ_RSP:
    case ATTC_HANDLE_VALUE_NTF:
    case ATTC_HANDLE_VALUE_IND:
      medcBpsValueUpdate((attEvt_t *) pMsg);
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medcBlpInit
 *        
 *  \brief  Profile initialization function.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcBlpInit(void)
{
  /* set handle list length */
  medcCb.hdlListLen = MEDC_DISC_HDL_LIST_LEN;
  
  /* set autoconnect UUID */
  medcCb.autoUuid = ATT_UUID_BLOOD_PRESSURE_SERVICE;
}

/*************************************************************************************************/
/*!
 *  \fn     medcBlpDiscover
 *        
 *  \brief  Discover service for profile.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcBlpDiscover(dmConnId_t connId)
{
  /* discover blood pressure service */
  BlpcBpsDiscover(connId, pMedcBpsHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     medcBlpConfigure
 *        
 *  \brief  Configure service for profile.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    APP_DISC_CFG_START or APP_DISC_CFG_CONN_START.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcBlpConfigure(dmConnId_t connId, uint8_t status)
{
  /* configure blood pressure service */
  AppDiscConfigure(connId, status, MEDC_CFG_BPS_LIST_LEN,
                   (attcDiscCfg_t *) medcCfgBpsList,
                   BLPC_BPS_HDL_LIST_LEN, pMedcBpsHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     medcBlpBtn
 *        
 *  \brief  Handle a button press.
 *
 *  \param  connId    Connection identifier.
 *  \param  btn       Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcBlpBtn(dmConnId_t connId, uint8_t btn)
{
  return;
}