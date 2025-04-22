/*************************************************************************************************/
/*!
 *  \file   dats_main.c
 *
 *  \brief  Data transmitter sample application.
 *
 *          $Date: 2015-12-22 09:09:58 -0600 (Tue, 22 Dec 2015) $
 *          $Revision: 20740 $
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
#include "svc_ch.h"
#include "svc_core.h"
#include "svc_wp.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Enumeration of client characteristic configuration descriptors */
enum
{
  DATS_GATT_SC_CCC_IDX,           /*! GATT service, service changed characteristic */
  DATS_WP_DAT_CCC_IDX,            /*! Wicentric proprietary service, data transfer characteristic */
  DATS_NUM_CCC_IDX
};

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for slave */
static const appSlaveCfg_t datsSlaveCfg =
{
  {30000,     0,     0},                  /*! Advertising durations in ms */
  {   96,  1600,     0}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for security */
static const appSecCfg_t datsSecCfg =
{
  DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
  0,                                      /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  FALSE                                   /*! TRUE to initiate security upon connection */
};

/*! configurable parameters for connection parameter update */
static const appUpdateCfg_t datsUpdateCfg =
{
  0,                                      /*! Connection idle period in ms before attempting
                                              connection parameter update; set to zero to disable */
  640,                                    /*! Minimum connection interval in 1.25ms units */
  800,                                    /*! Maximum connection interval in 1.25ms units */
  3,                                      /*! Connection latency */
  600,                                    /*! Supervision timeout in 10ms units */
  5                                       /*! Number of update attempts before giving up */
};


/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! advertising data, discoverable mode */
static const uint8_t datsAdvDataDisc[] =
{
  /*! flags */
  2,                                      /*! length */
  DM_ADV_TYPE_FLAGS,                      /*! AD type */
  DM_FLAG_LE_GENERAL_DISC |               /*! flags */
  DM_FLAG_LE_BREDR_NOT_SUP,
  
  /*! manufacturer specific data */
  3,                                      /*! length */
  DM_ADV_TYPE_MANUFACTURER,               /*! AD type */
  UINT16_TO_BYTES(HCI_ID_WICENTRIC)       /*! company ID */
};

/*! scan data, discoverable mode */
static const uint8_t datsScanDataDisc[] =
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
static const attsCccSet_t datsCccSet[DATS_NUM_CCC_IDX] =
{
  /* cccd handle          value range               security level */
  {GATT_SC_CH_CCC_HDL,    ATT_CLIENT_CFG_INDICATE,  DM_SEC_LEVEL_NONE},   /* DATS_GATT_SC_CCC_IDX */
  {WP_DAT_CH_CCC_HDL,     ATT_CLIENT_CFG_NOTIFY,    DM_SEC_LEVEL_NONE}    /* DATS_WP_DAT_CCC_IDX */
};

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! application control block */
static struct
{
  wsfHandlerId_t    handlerId;        /* WSF handler ID */
} datsCb;

/*************************************************************************************************/
/*!
 *  \fn     datsSendData
 *        
 *  \brief  Send notification containing data.
 *
 *  \param  connId      DM connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datsSendData(dmConnId_t connId)
{
  uint8_t str[] = "hello back";
  
  if (AttsCccEnabled(connId, DATS_WP_DAT_CCC_IDX))
  {
    /* send notification */
    AttsHandleValueNtf(connId, WP_DAT_HDL, sizeof(str), str);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datsDmCback
 *        
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datsDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t *pMsg;
  
  if ((pMsg = WsfMsgAlloc(sizeof(dmEvt_t))) != NULL)
  {
    memcpy(pMsg, pDmEvt, sizeof(dmEvt_t));
    WsfMsgSend(datsCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datsAttCback
 *        
 *  \brief  Application ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datsAttCback(attEvt_t *pEvt)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \fn     datsCccCback
 *        
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datsCccCback(attsCccEvt_t *pEvt)
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
 *  \fn     datsWpWriteCback
 *        
 *  \brief  ATTS write callback for proprietary data service.
 *
 *  \return ATT status.
 */
/*************************************************************************************************/
uint8_t datsWpWriteCback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                          uint16_t offset, uint16_t len, uint8_t *pValue, attsAttr_t *pAttr)
{
  /* print received data */
  APP_TRACE_INFO0((char*)pValue);
   
  /* send back some data */
  datsSendData(connId);
  
  return ATT_SUCCESS;
}

/*************************************************************************************************/
/*!
 *  \fn     datsSetup
 *        
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datsSetup(dmEvt_t *pMsg)
{
  /* set advertising and scan response data for discoverable mode */
  AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(datsAdvDataDisc), (uint8_t *) datsAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(datsScanDataDisc), (uint8_t *) datsScanDataDisc);

  /* set advertising and scan response data for connectable mode */
  AppAdvSetData(APP_ADV_DATA_CONNECTABLE, sizeof(datsAdvDataDisc), (uint8_t *) datsAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, sizeof(datsScanDataDisc), (uint8_t *) datsScanDataDisc);
  
  /* start advertising; automatically set connectable/discoverable mode and bondable mode */
  AppAdvStart(APP_MODE_AUTO_INIT);
}

/*************************************************************************************************/
/*!
 *  \fn     datsProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datsProcMsg(dmEvt_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;
  
  switch(pMsg->hdr.event)
  {
    case DM_RESET_CMPL_IND:
      datsSetup(pMsg);
      uiEvent = APP_UI_RESET_CMPL;
      break;

    case DM_ADV_START_IND:
      uiEvent = APP_UI_ADV_START;
      break;
         
    case DM_ADV_STOP_IND:
      uiEvent = APP_UI_ADV_STOP;
      break;
          
    case DM_CONN_OPEN_IND:
      uiEvent = APP_UI_CONN_OPEN;
      break;

    case DM_CONN_CLOSE_IND:
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
 *  \fn     DatsHandlerInit
 *        
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DatsHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("DatsHandlerInit");
  
  /* store handler ID */
  datsCb.handlerId = handlerId;
  
  /* Set configuration pointers */
  pAppSlaveCfg = (appSlaveCfg_t *) &datsSlaveCfg;
  pAppSecCfg = (appSecCfg_t *) &datsSecCfg;
  pAppUpdateCfg = (appUpdateCfg_t *) &datsUpdateCfg;
  
  /* Initialize application framework */
  AppSlaveInit();
}

/*************************************************************************************************/
/*!
 *  \fn     DatsHandler
 *        
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DatsHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{ 
  if (pMsg != NULL)
  {
    APP_TRACE_INFO1("Dats got evt %d", pMsg->event);
  
    if (pMsg->event >= DM_CBACK_START && pMsg->event <= DM_CBACK_END)
    {
      /* process advertising and connection-related messages */
      AppSlaveProcDmMsg((dmEvt_t *) pMsg);
      
      /* process security-related messages */
      AppSlaveSecProcDmMsg((dmEvt_t *) pMsg);
    }
        
    /* perform profile and user interface-related operations */
    datsProcMsg((dmEvt_t *) pMsg);    
  }
}

/*************************************************************************************************/
/*!
 *  \fn     DatsStart
 *        
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DatsStart(void)
{  
  /* Register for stack callbacks */
  DmRegister(datsDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, datsDmCback);
  AttRegister(datsAttCback);
  AttConnRegister(AppServerConnCback);
  AttsCccRegister(DATS_NUM_CCC_IDX, (attsCccSet_t *) datsCccSet, datsCccCback);
  
  /* Initialize attribute server database */
  SvcCoreAddGroup();
  SvcWpCbackRegister(NULL, datsWpWriteCback);
  SvcWpAddGroup();
  
  /* Reset the device */
  DmDevReset();  
}
