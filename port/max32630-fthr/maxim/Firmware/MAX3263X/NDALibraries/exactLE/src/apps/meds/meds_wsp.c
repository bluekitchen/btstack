/*************************************************************************************************/
/*!
 *  \file   meds_wsp.c
 *
 *  \brief  Medical sensor sample, weight scale profile
 *
 *          $Date: 2015-12-21 15:48:53 -0600 (Mon, 21 Dec 2015) $
 *          $Revision: 20728 $
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
#include "bstream.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "hci_api.h"
#include "dm_api.h"
#include "att_api.h"
#include "app_api.h"
#include "app_ui.h"
#include "svc_ch.h"
#include "svc_wss.h"
#include "svc_dis.h"
#include "svc_core.h"
#include "wsps_api.h"
#include "meds_main.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! enumeration of client characteristic configuration descriptors */
enum
{
  MEDS_WSP_GATT_SC_CCC_IDX,                /*! GATT service, service changed characteristic */
  MEDS_WSP_WSS_WM_CCC_IDX,                 /*! Weight scale service, weight measurement characteristic */
  MEDS_WSP_NUM_CCC_IDX
};

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! Service UUID list */
static const uint8_t medsSvcUuidList[] =
{
  UINT16_TO_BYTES(ATT_UUID_WEIGHT_SCALE_SERVICE),
  UINT16_TO_BYTES(ATT_UUID_DEVICE_INFO_SERVICE)  
};

/**************************************************************************************************
  Client Characteristic Configuration Descriptors
**************************************************************************************************/

/*! client characteristic configuration descriptors settings, indexed by above enumeration */
static const attsCccSet_t medsWspCccSet[MEDS_WSP_NUM_CCC_IDX] =
{
  /* cccd handle          value range               security level */
  {GATT_SC_CH_CCC_HDL,    ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_ENC},    /* MEDS_WSP_GATT_SC_CCC_IDX */
  {WSS_WM_CH_CCC_HDL,    ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_ENC}      /* MEDS_WSP_WSS_WM_CCC_IDX */
};

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void medsWspStart(void);
static void medsWspProcMsg(wsfMsgHdr_t *pMsg);
static void medsWspBtn(dmConnId_t connId, uint8_t btn);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! profile interface pointer */
medsIf_t medsWspIf =
{
  medsWspStart,
  medsWspProcMsg,
  medsWspBtn
};

/*************************************************************************************************/
/*!
 *  \fn     medsWspStart
 *        
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsWspStart(void)
{
  /* set up CCCD table and callback */
  AttsCccRegister(MEDS_WSP_NUM_CCC_IDX, (attsCccSet_t *) medsWspCccSet, medsCccCback);

  /* add weight scale service */
  SvcWssAddGroup();

  /* initialize weight scale profile sensor */
  WspsSetWsmFlags(CH_WSM_FLAG_UNITS_KG | CH_WSM_FLAG_TIMESTAMP);
                  
  /* set advertising data */
  AppAdvSetAdValue(APP_ADV_DATA_DISCOVERABLE, DM_ADV_TYPE_16_UUID, sizeof(medsSvcUuidList),
                   (uint8_t *) medsSvcUuidList);
}

/*************************************************************************************************/
/*!
 *  \fn     medsWspProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsWspProcMsg(wsfMsgHdr_t *pMsg)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \fn     medsWspBtn
 *        
 *  \brief  Button press callback.
 *
 *  \param  connId  Connection identifier.
 *  \param  btn     Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsWspBtn(dmConnId_t connId, uint8_t btn)
{
  /* button actions when connected */
  if (connId != DM_CONN_ID_NONE)
  {
    switch (btn)
    {
      case APP_UI_BTN_1_SHORT:
        /* send measurement */
        WspsMeasComplete(connId, MEDS_WSP_WSS_WM_CCC_IDX);
        break;
        
      default:
        break;
    }    
  }
}
