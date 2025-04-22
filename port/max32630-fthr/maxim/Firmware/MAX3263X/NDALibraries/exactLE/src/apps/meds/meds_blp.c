/*************************************************************************************************/
/*!
 *  \file   meds_blp.c
 *
 *  \brief  Medical sensor sample, blood pressure profile
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
#include "bstream.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "hci_api.h"
#include "dm_api.h"
#include "att_api.h"
#include "app_api.h"
#include "app_ui.h"
#include "svc_ch.h"
#include "svc_bps.h"
#include "svc_dis.h"
#include "svc_core.h"
#include "blps_api.h"
#include "meds_main.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! enumeration of client characteristic configuration descriptors */
enum
{
  MEDS_BLP_GATT_SC_CCC_IDX,                /*! GATT service, service changed characteristic */
  MEDS_BLP_BPS_BPM_CCC_IDX,                /*! Blood pressure service, blood pressure measurement characteristic */
  MEDS_BLP_BPS_ICP_CCC_IDX,                /*! Blood pressure service, intermediate cuff pressure characteristic */
  MEDS_BLP_NUM_CCC_IDX
};

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! blood pressure measurement configuration */
static const blpsCfg_t medsBlpsCfg =
{
  2000      /*! Measurement timer expiration period in ms */
};

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! Service UUID list */
static const uint8_t medsSvcUuidList[] =
{
  UINT16_TO_BYTES(ATT_UUID_BLOOD_PRESSURE_SERVICE),
  UINT16_TO_BYTES(ATT_UUID_DEVICE_INFO_SERVICE)  
};

/**************************************************************************************************
  Client Characteristic Configuration Descriptors
**************************************************************************************************/

/*! client characteristic configuration descriptors settings, indexed by above enumeration */
static const attsCccSet_t medsBlpCccSet[MEDS_BLP_NUM_CCC_IDX] =
{
  /* cccd handle          value range               security level */
  {GATT_SC_CH_CCC_HDL,    ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_ENC},    /* MEDS_BLP_GATT_SC_CCC_IDX */
  {BPS_BPM_CH_CCC_HDL,    ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_ENC},    /* MEDS_BLP_BPS_BPM_CCC_IDX */
  {BPS_ICP_CH_CCC_HDL,    ATT_CLIENT_CFG_NOTIFY,    DM_SEC_LEVEL_ENC}     /* MEDS_BLP_BPS_ICP_CCC_IDX */
};

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void medsBlpStart(void);
static void medsBlpProcMsg(wsfMsgHdr_t *pMsg);
static void medsBlpBtn(dmConnId_t connId, uint8_t btn);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! profile interface pointer */
medsIf_t medsBlpIf =
{
  medsBlpStart,
  medsBlpProcMsg,
  medsBlpBtn
};

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! application control block */
static struct
{
  bool_t            measuring;
} medsBlpCb;

/*************************************************************************************************/
/*!
 *  \fn     medsBlpClose
 *        
 *  \brief  Perform UI actions on connection close.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsBlpClose(wsfMsgHdr_t *pMsg)
{
  /* stop blood pressure measurement */
  BlpsMeasStop();
  medsBlpCb.measuring = FALSE;
}

/*************************************************************************************************/
/*!
 *  \fn     medsBlpStart
 *        
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsBlpStart(void)
{
  /* set up CCCD table and callback */
  AttsCccRegister(MEDS_BLP_NUM_CCC_IDX, (attsCccSet_t *) medsBlpCccSet, medsCccCback);

  /* add blood pressure service */
  SvcBpsAddGroup();

  /* initialize blood pressure profile sensor */
  BlpsInit(medsCb.handlerId, (blpsCfg_t *) &medsBlpsCfg);
  BlpsSetBpmFlags(CH_BPM_FLAG_UNITS_MMHG | CH_BPM_FLAG_TIMESTAMP | 
                  CH_BPM_FLAG_PULSE_RATE | CH_BPM_FLAG_MEAS_STATUS);
  BlpsSetIcpFlags(CH_BPM_FLAG_UNITS_MMHG | CH_BPM_FLAG_PULSE_RATE |
                  CH_BPM_FLAG_MEAS_STATUS);
                  
  /* set advertising data */
  AppAdvSetAdValue(APP_ADV_DATA_DISCOVERABLE, DM_ADV_TYPE_16_UUID, sizeof(medsSvcUuidList),
                   (uint8_t *) medsSvcUuidList);  
  
}

/*************************************************************************************************/
/*!
 *  \fn     medsBlpProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsBlpProcMsg(wsfMsgHdr_t *pMsg)
{
  switch(pMsg->event)
  {
    case MEDS_TIMER_IND:
      BlpsProcMsg(pMsg);
      break;

    case DM_CONN_CLOSE_IND:
      medsBlpClose(pMsg);
      break;
       
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medsBlpBtn
 *        
 *  \brief  Button press callback.
 *
 *  \param  connId  Connection identifier.
 *  \param  btn     Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsBlpBtn(dmConnId_t connId, uint8_t btn)
{
  /* button actions when connected */
  if (connId != DM_CONN_ID_NONE)
  {
    switch (btn)
    {
      case APP_UI_BTN_1_SHORT:
        /* start or complete measurement */
        if (!medsBlpCb.measuring)
        {
          BlpsMeasStart(connId, MEDS_TIMER_IND, MEDS_BLP_BPS_ICP_CCC_IDX);
          medsBlpCb.measuring = TRUE;
        }
        else
        {
          BlpsMeasComplete(connId, MEDS_BLP_BPS_BPM_CCC_IDX);
          medsBlpCb.measuring = FALSE;
        }
        break;
        
      default:
        break;
    }    
  }
}
