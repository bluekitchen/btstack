/*************************************************************************************************/
/*!
 *  \file   datc_main.c
 *
 *  \brief  Proprietary data transfer client sample application.
 *
 *          $Date: 2015-12-22 10:16:56 -0600 (Tue, 22 Dec 2015) $
 *          $Revision: 20742 $
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
#include "wsf_assert.h"
#include "hci_api.h"
#include "dm_api.h"
#include "att_api.h"
#include "smp_api.h"
#include "app_cfg.h"
#include "app_api.h"
#include "app_db.h"
#include "app_ui.h"
#include "svc_ch.h"
#include "gatt_api.h"
#include "wpc_api.h"
#include "datc_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! application control block */
struct
{
  uint16_t          hdlList[APP_DB_HDL_LIST_LEN];   /*! Cached handle list */
  wsfHandlerId_t    handlerId;                      /*! WSF hander ID */
  bool_t            scanning;                       /*! TRUE if scanning */
  bool_t            autoConnect;                    /*! TRUE if auto-connecting */
  uint8_t           discState;                      /*! Service discovery state */  
  uint8_t           hdlListLen;                     /*! Cached handle list length */
} datcCb;

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for master */
static const appMasterCfg_t datcMasterCfg =
{
  96,                                      /*! The scan interval, in 0.625 ms units */
  48,                                      /*! The scan window, in 0.625 ms units  */
  4000,                                    /*! The scan duration in ms */
  DM_DISC_MODE_NONE,                       /*! The GAP discovery mode */
  DM_SCAN_TYPE_ACTIVE                      /*! The scan type (active or passive) */                                               
};

/*! configurable parameters for security */
static const appSecCfg_t datcSecCfg =
{
  DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
  0,                                      /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  FALSE                                   /*! TRUE to initiate security upon connection */
};

/*! Connection parameters */
static const hciConnSpec_t datcConnCfg =
{
  40,                                     /*! Minimum connection interval in 1.25ms units */
  40,                                     /*! Maximum connection interval in 1.25ms units */
  0,                                      /*! Connection latency */
  600,                                    /*! Supervision timeout in 10ms units */
  0,                                      /*! Unused */
  0                                       /*! Unused */
};

/*! Configurable parameters for service and characteristic discovery */
static const appDiscCfg_t datcDiscCfg =
{
  FALSE                                   /*! TRUE to wait for a secure connection before initiating discovery */
};

/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/*! Discovery states:  enumeration of services to be discovered */
enum
{
  DATC_DISC_GATT_SVC,      /*! GATT service */
  DATC_DISC_WP_SVC,        /*! Wicentric proprietary service */
  DATC_DISC_SVC_MAX        /*! Discovery complete */
};

/*! the Client handle list, datcCb.hdlList[], is set as follows:
 *
 *  ------------------------------- <- DATC_DISC_GATT_START
 *  | GATT handles                |
 *  |...                          |
 *  ------------------------------- <- DATC_DISC_WP_START
 *  | WP handles                  |
 *  | ...                         |
 *  -------------------------------
 */
 
/*! Start of each service's handles in the the handle list */
#define DATC_DISC_GATT_START       0
#define DATC_DISC_WP_START         (DATC_DISC_GATT_START + GATT_HDL_LIST_LEN)
#define DATC_DISC_HDL_LIST_LEN     (DATC_DISC_WP_START + WPC_P1_HDL_LIST_LEN)

/*! Pointers into handle list for each service's handles */
static uint16_t *pDatcGattHdlList = &datcCb.hdlList[DATC_DISC_GATT_START];
static uint16_t *pDatcWpHdlList = &datcCb.hdlList[DATC_DISC_WP_START];

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/* 
 * Data for configuration after service discovery
 */
 
/* Default value for CCC indications */
const uint8_t datcCccIndVal[2] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_INDICATE)};

/* Default value for CCC notifications */
const uint8_t datcCccNtfVal[2] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_NOTIFY)};

/* List of characteristics to configure after service discovery */
static const attcDiscCfg_t datcDiscCfgList[] = 
{  
  /* Write:  GATT service changed ccc descriptor */
  {datcCccIndVal, sizeof(datcCccIndVal), (GATT_SC_CCC_HDL_IDX + DATC_DISC_GATT_START)},

  /* Write:  Proprietary data service changed ccc descriptor */
  {datcCccNtfVal, sizeof(datcCccNtfVal), (WPC_P1_NA_CCC_HDL_IDX + DATC_DISC_WP_START)}  
};

/* Characteristic configuration list length */
#define DATC_DISC_CFG_LIST_LEN   (sizeof(datcDiscCfgList) / sizeof(attcDiscCfg_t))

/* sanity check:  make sure configuration list length is <= handle list length */
WSF_CT_ASSERT(DATC_DISC_CFG_LIST_LEN <= DATC_DISC_HDL_LIST_LEN);

/*************************************************************************************************/
/*!
 *  \fn     datcDmCback
 *        
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t   *pMsg;
  uint16_t  len;
  
  if (pDmEvt->hdr.event == DM_SCAN_REPORT_IND)
  {
    len = sizeof(dmEvt_t) + pDmEvt->scanReport.len;
  }
  else
  {
    len = sizeof(dmEvt_t);
  }
  
  if ((pMsg = WsfMsgAlloc(len)) != NULL)
  {
    memcpy(pMsg, pDmEvt, sizeof(dmEvt_t));
    if (pDmEvt->hdr.event == DM_SCAN_REPORT_IND)
    {
      pMsg->scanReport.pData = (uint8_t *) (pMsg + 1);
      memcpy(pMsg->scanReport.pData, pDmEvt->scanReport.pData, pDmEvt->scanReport.len);
    }
    WsfMsgSend(datcCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcAttCback
 *        
 *  \brief  Application  ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcAttCback(attEvt_t *pEvt)
{
  attEvt_t *pMsg;
  
  if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attEvt_t));
    pMsg->pValue = (uint8_t *) (pMsg + 1);
    memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
    WsfMsgSend(datcCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcScanStart
 *        
 *  \brief  Perform actions on scan start.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcScanStart(dmEvt_t *pMsg)
{
  datcCb.scanning = TRUE;
}

/*************************************************************************************************/
/*!
 *  \fn     datcScanStop
 *        
 *  \brief  Perform actions on scan stop.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcScanStop(dmEvt_t *pMsg)
{
  datcCb.scanning = FALSE;
  datcCb.autoConnect = FALSE;
}

/*************************************************************************************************/
/*!
 *  \fn     datcScanReport
 *        
 *  \brief  Handle a scan report.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcScanReport(dmEvt_t *pMsg)
{
  uint8_t *pData;
  bool_t  connect = FALSE;
  
  /* disregard if not scanning or autoconnecting */
  if (!datcCb.scanning || !datcCb.autoConnect)
  {
    return;
  }
  
  /* if we already have a bond with this device then connect to it */
  if (AppDbFindByAddr(pMsg->scanReport.addrType, pMsg->scanReport.addr) != APP_DB_HDL_NONE)
  {
    connect = TRUE;
  }  
  /* find vendor-specific advertising data */
  else if ((pData = DmFindAdType(DM_ADV_TYPE_MANUFACTURER, pMsg->scanReport.len,
                                 pMsg->scanReport.pData)) != NULL)
  {
    /* check length and vendor ID */
    if (pData[DM_AD_LEN_IDX] >= 3 && BYTES_UINT16_CMP(&pData[DM_AD_DATA_IDX], HCI_ID_WICENTRIC))
    {
      connect = TRUE;
    }
  }

  if (connect)
  {
    /* stop scanning and connect */
    datcCb.autoConnect = FALSE;
    AppScanStop();
    AppConnOpen(pMsg->scanReport.addrType, pMsg->scanReport.addr);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcOpen
 *        
 *  \brief  Perform UI actions on connection open.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcOpen(dmEvt_t *pMsg)
{

}

/*************************************************************************************************/
/*!
 *  \fn     datcValueNtf
 *        
 *  \brief  Process a received ATT notification.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcValueNtf(attEvt_t *pMsg)
{
  /* print the received data */
  APP_TRACE_INFO0((char*)pMsg->pValue);
}

/*************************************************************************************************/
/*!
 *  \fn     datcSetup
 *        
 *  \brief  Set up procedures that need to be performed after device reset.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcSetup(dmEvt_t *pMsg)
{
  datcCb.scanning = FALSE;
  DmConnSetConnSpec((hciConnSpec_t *) &datcConnCfg);
}

/*************************************************************************************************/
/*!
 *  \fn     datcSendData
 *        
 *  \brief  Send example data.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcSendData(dmConnId_t connId)
{
  uint8_t str[] = "hello world";
  
  if (pDatcWpHdlList[WPC_P1_DAT_HDL_IDX] != ATT_HANDLE_NONE)
  {
    AttcWriteCmd(connId, pDatcWpHdlList[WPC_P1_DAT_HDL_IDX], sizeof(str), str);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcBtnCback
 *        
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcBtnCback(uint8_t btn)
{
  dmConnId_t      connId;
  
  /* button actions when connected */
  if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
  {
    switch (btn)
    {
      case APP_UI_BTN_1_SHORT:
        /* send data */
        datcSendData(connId);
        break;
            
      case APP_UI_BTN_1_LONG:
        /* disconnect */
        AppConnClose(connId);
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
        /* if scanning cancel scanning */
        if (datcCb.scanning)
        {
          AppScanStop();
        }
        /* else auto connect */
        else if (!datcCb.autoConnect)
        {
          datcCb.autoConnect = TRUE;
          AppScanStart(datcMasterCfg.discMode, datcMasterCfg.scanType,
                       datcMasterCfg.scanDuration);
        }
        break;
        
      case APP_UI_BTN_1_LONG:
        /* clear bonded device info */
        AppDbDeleteAllRecords();
        break;
        
      default:
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcDiscCback
 *        
 *  \brief  Discovery callback.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    Service or configuration status.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcDiscCback(dmConnId_t connId, uint8_t status)
{
  switch(status)
  {
    case APP_DISC_INIT:
      /* set handle list when initialization requested */
      AppDiscSetHdlList(connId, datcCb.hdlListLen, datcCb.hdlList);
      break;

    case APP_DISC_SEC_REQUIRED:
      /* initiate security */
      AppMasterSecurityReq(connId);
      break;
      
    case APP_DISC_START:
      /* initialize discovery state */
      datcCb.discState = DATC_DISC_GATT_SVC;
      
      /* discover GATT service */
      GattDiscover(connId, pDatcGattHdlList);
      break;
      
    case APP_DISC_FAILED:
      /* if discovery failed for proprietary data service then disconnect */
      if (datcCb.discState == DATC_DISC_WP_SVC)
      {
        AppConnClose(connId);
        break;
      }
      /* else fall through and continue discovery */
      
    case APP_DISC_CMPL:
      /* next discovery state */
      datcCb.discState++;
      
      if (datcCb.discState == DATC_DISC_WP_SVC)
      {
        /* discover proprietary data service */
        WpcP1Discover(connId, pDatcWpHdlList);
      }
      else
      {    
        /* discovery complete */
        AppDiscComplete(connId, APP_DISC_CMPL);

        /* start configuration */
        AppDiscConfigure(connId, APP_DISC_CFG_START, DATC_DISC_CFG_LIST_LEN,
                         (attcDiscCfg_t *) datcDiscCfgList, DATC_DISC_HDL_LIST_LEN, datcCb.hdlList);
      }
      break;
      
    case APP_DISC_CFG_START:
        /* start configuration */
        AppDiscConfigure(connId, APP_DISC_CFG_START, DATC_DISC_CFG_LIST_LEN,
                         (attcDiscCfg_t *) datcDiscCfgList, DATC_DISC_HDL_LIST_LEN, datcCb.hdlList);
      break;
      
    case APP_DISC_CFG_CMPL:
      AppDiscComplete(connId, status);
      break;

    case APP_DISC_CFG_CONN_START:
      /* no connection setup configuration */
      break;
      
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     datcProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void datcProcMsg(dmEvt_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;
  
  switch(pMsg->hdr.event)
  {
    case ATTC_HANDLE_VALUE_NTF:
      datcValueNtf((attEvt_t *) pMsg);
      break;
      
    case DM_RESET_CMPL_IND:
      datcSetup(pMsg);
      uiEvent = APP_UI_RESET_CMPL;
      break;

    case DM_SCAN_START_IND:
      datcScanStart(pMsg);
      uiEvent = APP_UI_SCAN_START;
      break;
         
    case DM_SCAN_STOP_IND:
      datcScanStop(pMsg);
      uiEvent = APP_UI_SCAN_STOP;
      break;

    case DM_SCAN_REPORT_IND:
      datcScanReport(pMsg);
      break;
      
    case DM_CONN_OPEN_IND:
      datcOpen(pMsg);
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
 *  \fn     DatcHandlerInit
 *        
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DatcHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("DatcHandlerInit");
  
  /* store handler ID */
  datcCb.handlerId = handlerId;
  
  /* Set configuration pointers */
  pAppMasterCfg = (appMasterCfg_t *) &datcMasterCfg;
  pAppSecCfg = (appSecCfg_t *) &datcSecCfg;
  pAppDiscCfg = (appDiscCfg_t *) &datcDiscCfg;

  
  /* Initialize application framework */
  AppMasterInit();
  AppDiscInit();
}

/*************************************************************************************************/
/*!
 *  \fn     DatcHandler
 *        
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DatcHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{ 
  if (pMsg != NULL)
  {
    APP_TRACE_INFO1("Datc got evt %d", pMsg->event);

    /* process ATT messages */
    if (pMsg->event <= ATT_CBACK_END)
    {
      /* process discovery-related ATT messages */
      AppDiscProcAttMsg((attEvt_t *) pMsg);
    }
    /* process DM messages */
    else if (pMsg->event <= DM_CBACK_END)
    {
      /* process advertising and connection-related messages */
      AppMasterProcDmMsg((dmEvt_t *) pMsg);
      
      /* process security-related messages */
      AppMasterSecProcDmMsg((dmEvt_t *) pMsg);
      
      /* process discovery-related messages */
      AppDiscProcDmMsg((dmEvt_t *) pMsg);
    }
          
    /* perform profile and user interface-related operations */
    datcProcMsg((dmEvt_t *) pMsg);    
  }
}

/*************************************************************************************************/
/*!
 *  \fn     DatcStart
 *        
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void DatcStart(void)
{  
  /* Register for stack callbacks */
  DmRegister(datcDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, datcDmCback);
  AttRegister(datcAttCback);
  
  /* Register for app framework button callbacks */
  AppUiBtnRegister(datcBtnCback);

  /* Register for app framework discovery callbacks */
  AppDiscRegister(datcDiscCback);
  
  /* Reset the device */
  DmDevReset();  
}
