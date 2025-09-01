/*************************************************************************************************/
/*!
 *  \file   gluc_main.c
 *
 *  \brief  Glucose sensor sample application.
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
#include "app_db.h"
#include "app_ui.h"
#include "app_hw.h"
#include "svc_ch.h"
#include "svc_core.h"
#include "svc_gls.h"
#include "svc_dis.h"
#include "glps_api.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! enumeration of client characteristic configuration descriptors */
enum
{
  GLUC_GATT_SC_CCC_IDX,                    /*! GATT service, service changed characteristic */
  GLUC_GLS_GLM_CCC_IDX,                    /*! Glucose service, glucose measurement characteristic */
  GLUC_GLS_GLMC_CCC_IDX,                   /*! Glucose service, glucose measurement context characteristic */
  GLUC_GLS_RACP_CCC_IDX,                   /*! Glucose service, record access control point characteristic */
  GLUC_NUM_CCC_IDX
};

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for slave */
static const appSlaveCfg_t glucSlaveCfg =
{
  {30000, 30000,     0},                  /*! Advertising durations in ms */
  {   48,  1600,     0}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for security */
static const appSecCfg_t glucSecCfg =
{
  DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
  0,                                      /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  TRUE                                    /*! TRUE to initiate security upon connection */
};

/*! configurable parameters for connection parameter update */
static const appUpdateCfg_t glucUpdateCfg =
{
  0,                                      /*! Connection idle period in ms before attempting
                                              connection parameter update; set to zero to disable */
  640,                                    /*! Minimum connection interval in 1.25ms units */
  800,                                    /*! Maximum connection interval in 1.25ms units */
  0,                                      /*! Connection latency */
  600,                                    /*! Supervision timeout in 10ms units */
  5                                       /*! Number of update attempts before giving up */
};

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! advertising data, discoverable mode */
static const uint8_t glucAdvDataDisc[] =
{
  /*! flags */
  2,                                      /*! length */
  DM_ADV_TYPE_FLAGS,                      /*! AD type */
  DM_FLAG_LE_LIMITED_DISC |               /*! flags */
  DM_FLAG_LE_BREDR_NOT_SUP,
  
  /*! service UUID list */
  5,                                      /*! length */
  DM_ADV_TYPE_16_UUID,                    /*! AD type */
  UINT16_TO_BYTES(ATT_UUID_GLUCOSE_SERVICE),
  UINT16_TO_BYTES(ATT_UUID_DEVICE_INFO_SERVICE)  
};

/*! scan data, discoverable mode */
static const uint8_t glucScanDataDisc[] =
{
  /*! device name */
  14,                                     /*! length */
  DM_ADV_TYPE_LOCAL_NAME,                 /*! AD type */
  'w',
  'i',
  'c',
  'e',
  'n',
  't',
  'r',
  'i',
  'c',
  ' ',
  'a',
  'p',
  'p'
};

/**************************************************************************************************
  Client Characteristic Configuration Descriptors
**************************************************************************************************/

/*! client characteristic configuration descriptors settings, indexed by above enumeration */
static const attsCccSet_t glucCccSet[GLUC_NUM_CCC_IDX] =
{
  /* cccd handle          value range               security level */
  {GATT_SC_CH_CCC_HDL,    ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_ENC},    /* GLUC_GATT_SC_CCC_IDX */
  {GLS_GLM_CH_CCC_HDL,    ATT_CLIENT_CFG_NOTIFY,    DM_SEC_LEVEL_ENC},    /* GLUC_GLS_GLM_CCC_IDX */
  {GLS_GLMC_CH_CCC_HDL,   ATT_CLIENT_CFG_NOTIFY,    DM_SEC_LEVEL_ENC},    /* GLUC_GLS_GLMC_CCC_IDX */
  {GLS_RACP_CH_CCC_HDL,   ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_ENC}     /* GLUC_GLS_RACP_CCC_IDX */
};

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! application control block */
static struct
{
  wsfHandlerId_t    handlerId;
} glucCb;

/*************************************************************************************************/
/*!
 *  \fn     glucDmCback
 *        
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void glucDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t *pMsg;
  
  if ((pMsg = WsfMsgAlloc(sizeof(dmEvt_t))) != NULL)
  {
    memcpy(pMsg, pDmEvt, sizeof(dmEvt_t));
    WsfMsgSend(glucCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     glucAttCback
 *        
 *  \brief  Application ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void glucAttCback(attEvt_t *pEvt)
{
  attEvt_t *pMsg;
  
  if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attEvt_t));
    pMsg->pValue = (uint8_t *) (pMsg + 1);
    memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
    WsfMsgSend(glucCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     glucCccCback
 *        
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void glucCccCback(attsCccEvt_t *pEvt)
{
  appDbHdl_t    dbHdl;
  
  /* if CCC not set from initialization and there's a device record */
  if ((pEvt->handle != ATT_HANDLE_NONE) &&
      ((dbHdl = AppDbGetHdl((dmConnId_t) pEvt->hdr.param)) != APP_DB_HDL_NONE))
  {
    /* store value in device database */  
    AppDbSetCccTblValue(dbHdl, pEvt->idx, pEvt->value);
  } 
}

/*************************************************************************************************/
/*!
 *  \fn     glucSetup
 *        
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void glucSetup(dmEvt_t *pMsg)
{
  /* set advertising and scan response data for discoverable mode */
  AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(glucAdvDataDisc), (uint8_t *) glucAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(glucScanDataDisc), (uint8_t *) glucScanDataDisc);

  /* set advertising and scan response data for connectable mode */
  AppAdvSetData(APP_ADV_DATA_CONNECTABLE, 0, NULL);
  AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, 0, NULL);
  
  /* start advertising; automatically set connectable/discoverable mode and bondable mode */
  AppAdvStart(APP_MODE_AUTO_INIT);
}

/*************************************************************************************************/
/*!
 *  \fn     glucBtnCback
 *        
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void glucBtnCback(uint8_t btn)
{
  dmConnId_t      connId;
  
  /* button actions when connected */
  if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
  {
    switch (btn)
    {
      case APP_UI_BTN_1_SHORT:
        break;
        
      case APP_UI_BTN_1_MED:
        break;
        
      case APP_UI_BTN_1_LONG:
        AppConnClose(connId);
        break;

      case APP_UI_BTN_2_SHORT:
        break;
        
      default:
        break;
    }    
  }
  /* button actions when not connected */
  else
  {
    switch (btn)
    {
      case APP_UI_BTN_1_SHORT:
        /* start or restart advertising */
        AppAdvStart(APP_MODE_AUTO_INIT);
        break;
        
      case APP_UI_BTN_1_MED:
        /* enter discoverable and bondable mode mode */
        AppSetBondable(TRUE);
        AppAdvStart(APP_MODE_DISCOVERABLE);
        break;
        
      case APP_UI_BTN_1_LONG:
        /* clear bonded device info and restart advertising */
        AppDbDeleteAllRecords();
        AppAdvStart(APP_MODE_AUTO_INIT);
        break;
        
      default:
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     glucProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void glucProcMsg(dmEvt_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;
  
  switch(pMsg->hdr.event)
  {
    case ATTS_HANDLE_VALUE_CNF:
      GlpsProcMsg(&pMsg->hdr);
      break;
      
    case DM_RESET_CMPL_IND:
      glucSetup(pMsg);
      uiEvent = APP_UI_RESET_CMPL;
      break;

    case DM_ADV_START_IND:
      uiEvent = APP_UI_ADV_START;
      break;
         
    case DM_ADV_STOP_IND:
      uiEvent = APP_UI_ADV_STOP;
      break;
          
    case DM_CONN_OPEN_IND:
      GlpsProcMsg(&pMsg->hdr);
      uiEvent = APP_UI_CONN_OPEN;
      break;
         
    case DM_CONN_CLOSE_IND:
      GlpsProcMsg(&pMsg->hdr);
      uiEvent = APP_UI_CONN_CLOSE;
      break;
       
    case DM_SEC_PAIR_CMPL_IND:
      uiEvent = APP_UI_SEC_PAIR_CMPL;
      break;
     
    case DM_SEC_PAIR_FAIL_IND:
      uiEvent = APP_UI_SEC_PAIR_FAIL;
      break;
     
    case DM_SEC_ENCRYPT_IND:
      uiEvent = APP_UI_SEC_ENCRYPT;
      break;
       
    case DM_SEC_ENCRYPT_FAIL_IND:
      uiEvent = APP_UI_SEC_ENCRYPT_FAIL;
      break;

    case DM_SEC_AUTH_REQ_IND:
      AppHandlePasskey(&pMsg->authReq);
      break;

    default:
      break;
  }
  
  if (uiEvent != APP_UI_NONE)
  {
    AppUiAction(uiEvent);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     GlucHandlerInit
 *        
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlucHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("GlucHandlerInit");
  
  /* store handler ID */
  glucCb.handlerId = handlerId;
  
  /* Set configuration pointers */
  pAppSlaveCfg = (appSlaveCfg_t *) &glucSlaveCfg;
  pAppSecCfg = (appSecCfg_t *) &glucSecCfg;
  pAppUpdateCfg = (appUpdateCfg_t *) &glucUpdateCfg;
  
  /* Initialize application framework */
  AppSlaveInit();
  
  /* initialize glucose profile sensor */  
  GlpsInit();
  GlpsSetCccIdx(GLUC_GLS_GLM_CCC_IDX, GLUC_GLS_GLMC_CCC_IDX, GLUC_GLS_RACP_CCC_IDX);
}

/*************************************************************************************************/
/*!
 *  \fn     GlucHandler
 *        
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlucHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{ 
  if (pMsg != NULL)
  {
    APP_TRACE_INFO1("Gluc got evt %d", pMsg->event);
  
    if (pMsg->event >= DM_CBACK_START && pMsg->event <= DM_CBACK_END)
    {
      /* process advertising and connection-related messages */
      AppSlaveProcDmMsg((dmEvt_t *) pMsg);
      
      /* process security-related messages */
      AppSlaveSecProcDmMsg((dmEvt_t *) pMsg);
    }
        
    /* perform profile and user interface-related operations */
    glucProcMsg((dmEvt_t *) pMsg);    
  }
}

/*************************************************************************************************/
/*!
 *  \fn     GlucStart
 *        
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlucStart(void)
{  
  /* Register for stack callbacks */
  DmRegister(glucDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, glucDmCback);
  AttRegister(glucAttCback);
  AttConnRegister(AppServerConnCback);
  AttsCccRegister(GLUC_NUM_CCC_IDX, (attsCccSet_t *) glucCccSet, glucCccCback);
  
  /* Register for app framework callbacks */
  AppUiBtnRegister(glucBtnCback);
  
  /* Initialize attribute server database */
  SvcCoreAddGroup();
  SvcGlsAddGroup();
  SvcGlsCbackRegister(NULL, GlpsRacpWriteCback);
  SvcDisAddGroup();

  /* Set supported features after starting database */
  GlpsSetFeature(CH_GLF_LOW_BATT | CH_GLF_MALFUNC | CH_GLF_SAMPLE_SIZE | CH_GLF_INSERT_ERR |
                 CH_GLF_TYPE_ERR | CH_GLF_RES_HIGH_LOW | CH_GLF_TEMP_HIGH_LOW | CH_GLF_READ_INT |   
                 CH_GLF_GENERAL_FAULT);
  
  /* Reset the device */
  DmDevReset();  
}
