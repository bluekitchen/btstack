/*************************************************************************************************/
/*!
 *  \file   medc_glp.c
 *
 *  \brief  Health/medical collector, glucose profile
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
#include "app_ui.h"
#include "svc_ch.h"
#include "glpc_api.h"
#include "medc_main.h"

/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/* Start of cached glucose service handles; begins after DIS */
#define MEDC_DISC_GLS_START         (MEDC_DISC_DIS_START + DIS_HDL_LIST_LEN)

/* Total cached handle list length */
#define MEDC_DISC_HDL_LIST_LEN      (MEDC_DISC_GLS_START + GLPC_GLS_HDL_LIST_LEN)

/*! Pointers into handle list for glucose service handles */
static uint16_t *pMedcGlsHdlList = &medcCb.hdlList[MEDC_DISC_GLS_START];

/* sanity check:  make sure handle list length is <= app db handle list length */
WSF_CT_ASSERT(MEDC_DISC_HDL_LIST_LEN <= APP_DB_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/* List of characteristics to configure after service discovery */
static const attcDiscCfg_t medcCfgGlsList[] = 
{  
  /* Read:  glucose feature */
  {NULL, 0, GLPC_GLS_GLF_HDL_IDX},

  /* Write:  glucose measurement CCC descriptor  */
  {medcCccNtfVal, sizeof(medcCccIndVal), GLPC_GLS_GLM_CCC_HDL_IDX},

  /* Write:  glucose measurement context CCC descriptor  */
  {medcCccNtfVal, sizeof(medcCccIndVal), GLPC_GLS_GLMC_CCC_HDL_IDX},

  /* Write:  record access control point CCC descriptor  */
  {medcCccIndVal, sizeof(medcCccIndVal), GLPC_GLS_RACP_CCC_HDL_IDX}
};

/* Characteristic configuration list length */
#define MEDC_CFG_GLS_LIST_LEN   (sizeof(medcCfgGlsList) / sizeof(attcDiscCfg_t))

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Control block */
static struct
{
  wsfTimer_t  racpTimer;          /*! RACP procedure timer */
  bool_t      inProgress;         /*! RACP procedure in progress */
} medcGlpCb;

/**************************************************************************************************
  Local Functions
**************************************************************************************************/

static void medcGlpInit(void);
static void medcGlpDiscover(dmConnId_t connId);
static void medcGlpConfigure(dmConnId_t connId, uint8_t status);
static void medcGlpProcMsg(wsfMsgHdr_t *pMsg);
static void medcGlpBtn(dmConnId_t connId, uint8_t btn);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! profile interface pointer */
medcIf_t medcGlpIf =
{
  medcGlpInit,
  medcGlpDiscover,
  medcGlpConfigure,
  medcGlpProcMsg,
  medcGlpBtn
};

/*************************************************************************************************/
/*!
 *  \fn     medcGlsValueUpdate
 *        
 *  \brief  Process a received ATT read response, notification, or indication.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcGlsValueUpdate(attEvt_t *pMsg)
{
  if (pMsg->hdr.status == ATT_SUCCESS)
  {
    /* determine which profile the handle belongs to; start with most likely */
    
    /* glucose */
    if (GlpcGlsValueUpdate(pMedcGlsHdlList, pMsg) == ATT_SUCCESS)    
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
 *  \fn     medcGlpInit
 *        
 *  \brief  Profile initialization function.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcGlpInit(void)
{
  /* set handle list length */
  medcCb.hdlListLen = MEDC_DISC_HDL_LIST_LEN;
    
  /* set autoconnect UUID */
  medcCb.autoUuid = ATT_UUID_GLUCOSE_SERVICE;

  /* initialize timer */
  medcGlpCb.racpTimer.handlerId = medcCb.handlerId;
  medcGlpCb.racpTimer.msg.event = MEDC_TIMER_IND;
  
  /* initialize sequence number */
  GlpcGlsSetLastSeqNum(1);
}

/*************************************************************************************************/
/*!
 *  \fn     medcGlpDiscover
 *        
 *  \brief  Discover service for profile.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcGlpDiscover(dmConnId_t connId)
{
  /* discover glucose service */
  GlpcGlsDiscover(connId, pMedcGlsHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     medcGlpConfigure
 *        
 *  \brief  Configure service for profile.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    APP_DISC_CFG_START or APP_DISC_CFG_CONN_START.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcGlpConfigure(dmConnId_t connId, uint8_t status)
{
  /* configure glucose service */
  AppDiscConfigure(connId, status, MEDC_CFG_GLS_LIST_LEN,
                   (attcDiscCfg_t *) medcCfgGlsList,
                   GLPC_GLS_HDL_LIST_LEN, pMedcGlsHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     medcGlpProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcGlpProcMsg(wsfMsgHdr_t *pMsg)
{
  switch(pMsg->event)
  {
    case ATTC_READ_RSP:
    case ATTC_HANDLE_VALUE_NTF:
      /* process value update */
      medcGlsValueUpdate((attEvt_t *) pMsg);
      break;

    case ATTC_WRITE_RSP:
      /* if write to RACP was successful, start procedure timer */
      if ((((attEvt_t *) pMsg)->hdr.status == ATT_SUCCESS) &&
          (((attEvt_t *) pMsg)->handle == pMedcGlsHdlList[GLPC_GLS_RACP_HDL_IDX]))
      {
        medcGlpCb.inProgress = TRUE;
        medcGlpCb.racpTimer.msg.param = pMsg->param;    /* conn ID */
        WsfTimerStartSec(&medcGlpCb.racpTimer, ATT_MAX_TRANS_TIMEOUT);
      }
      break;

    case ATTC_HANDLE_VALUE_IND:
      /* if procedure in progress stop procedure timer */
      if (medcGlpCb.inProgress)
      {
        medcGlpCb.inProgress = FALSE;
        WsfTimerStop(&medcGlpCb.racpTimer);
      }
      
      /* process value */
      medcGlsValueUpdate((attEvt_t *) pMsg);
      break;
      
    case DM_CONN_CLOSE_IND:
      /* if procedure in progress stop procedure timer */
      if (medcGlpCb.inProgress)
      {
        medcGlpCb.inProgress = FALSE;
        WsfTimerStop(&medcGlpCb.racpTimer);
      }      
      break;
      
    case MEDC_TIMER_IND:
      /* if procedure in progress then close connection */
      if (medcGlpCb.inProgress && pMsg->param != DM_CONN_ID_NONE)
      {
        medcGlpCb.inProgress = FALSE;
        AppConnClose((dmConnId_t) pMsg->param);
      }
      break;
      
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medcGlpBtn
 *        
 *  \brief  Handle a button press.
 *
 *  \param  connId    Connection identifier.
 *  \param  btn       Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcGlpBtn(dmConnId_t connId, uint8_t btn)
{
  glpcFilter_t filter;
  
  /* button actions when connected */
  if (connId != DM_CONN_ID_NONE)
  {
    /* handle must be set to send RACP command */
    if (pMedcGlsHdlList[GLPC_GLS_RACP_HDL_IDX] == ATT_HANDLE_NONE)
    {
      return;
    }
    
    switch (btn)
    {
      case APP_UI_BTN_1_SHORT:
        /* report all records */
        GlpcGlsRacpSend(connId, pMedcGlsHdlList[GLPC_GLS_RACP_HDL_IDX],
                        CH_RACP_OPCODE_REPORT, CH_RACP_OPERATOR_ALL, NULL);
        break;

      case APP_UI_BTN_1_MED:
        /* report records greater than sequence number */
        filter.type = CH_RACP_GLS_FILTER_SEQ;
        filter.param.seqNum = GlpcGlsGetLastSeqNum();
        GlpcGlsRacpSend(connId, pMedcGlsHdlList[GLPC_GLS_RACP_HDL_IDX],
                        CH_RACP_OPCODE_REPORT, CH_RACP_OPERATOR_GTEQ, &filter);
        break;

      case APP_UI_BTN_2_SHORT:
        /* report number of records */
        GlpcGlsRacpSend(connId, pMedcGlsHdlList[GLPC_GLS_RACP_HDL_IDX],
                        CH_RACP_OPCODE_REPORT_NUM, CH_RACP_OPERATOR_ALL, NULL);        
        break;
        
      case APP_UI_BTN_2_MED:
        /* report number of records greater than sequence number */
        filter.type = CH_RACP_GLS_FILTER_SEQ;
        filter.param.seqNum = GlpcGlsGetLastSeqNum();
        GlpcGlsRacpSend(connId, pMedcGlsHdlList[GLPC_GLS_RACP_HDL_IDX],
                        CH_RACP_OPCODE_REPORT_NUM, CH_RACP_OPERATOR_GTEQ, &filter);        
        break;                  

      case APP_UI_BTN_2_LONG:
        /* abort */
        GlpcGlsRacpSend(connId, pMedcGlsHdlList[GLPC_GLS_RACP_HDL_IDX],
                        CH_RACP_OPCODE_ABORT, CH_RACP_OPERATOR_NULL, NULL);
        break;
        
      case APP_UI_BTN_2_EX_LONG:
        /* delete all records */
        GlpcGlsRacpSend(connId, pMedcGlsHdlList[GLPC_GLS_RACP_HDL_IDX],
                        CH_RACP_OPCODE_DELETE, CH_RACP_OPERATOR_ALL, NULL);        
        break;                  
        
      default:
        break;
    }    
  }
}