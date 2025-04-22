/*************************************************************************************************/
/*!
 *  \file   medc_main.c
 *
 *  \brief  Health/medical collector sample application for the following profiles:
 *            Heart Rate profile collector
 *            Blood Pressure profile collector
 *            Glucose profile collector
 *            Weight scale profile collector
 *            Health Thermometer profile collector
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
#include "dis_api.h"
#include "medc_api.h"
#include "medc_main.h"

/**************************************************************************************************
  Profile Configuration
**************************************************************************************************/

/* Heart rate profile included */
#ifndef MEDC_HRP_INCLUDED
#define MEDC_HRP_INCLUDED TRUE
#endif

/* Blood pressure profile included */
#ifndef MEDC_BLP_INCLUDED
#define MEDC_BLP_INCLUDED TRUE
#endif

/* Glucose profile included */
#ifndef MEDC_GLP_INCLUDED
#define MEDC_GLP_INCLUDED TRUE
#endif

/* Weight scale profile included */
#ifndef MEDC_WSP_INCLUDED
#define MEDC_WSP_INCLUDED TRUE
#endif

/* Health thermometer profile included */
#ifndef MEDC_HTP_INCLUDED
#define MEDC_HTP_INCLUDED TRUE
#endif

/* Default profile to use */
#ifndef MEDC_PROFILE
#define MEDC_PROFILE MEDC_ID_HRP
#endif

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for master */
static const appMasterCfg_t medcMasterCfg =
{
  96,                                      /*! The scan interval, in 0.625 ms units */
  48,                                      /*! The scan window, in 0.625 ms units  */
  4000,                                    /*! The scan duration in ms */
  DM_DISC_MODE_NONE,                       /*! The GAP discovery mode */
  DM_SCAN_TYPE_ACTIVE                      /*! The scan type (active or passive) */                                               
};

/*! configurable parameters for security */
static const appSecCfg_t medcSecCfg =
{
  DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
  0,                                      /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  FALSE                                   /*! TRUE to initiate security upon connection */
};

/*! SMP security parameter configuration */
static const smpCfg_t medcSmpCfg =
{
  3000,                                   /*! 'Repeated attempts' timeout in msec */
  SMP_IO_DISP_ONLY,                       /*! I/O Capability */
  7,                                      /*! Minimum encryption key length */
  16,                                     /*! Maximum encryption key length */
  3,                                      /*! Attempts to trigger 'repeated attempts' timeout */
  0                                       /*! Device authentication requirements */
};

/*! Connection parameters */
static const hciConnSpec_t medcConnCfg =
{
  40,                                     /*! Minimum connection interval in 1.25ms units */
  40,                                     /*! Maximum connection interval in 1.25ms units */
  0,                                      /*! Connection latency */
  600,                                    /*! Supervision timeout in 10ms units */
  0,                                      /*! Unused */
  0                                       /*! Unused */
};

/*! Configurable parameters for service and characteristic discovery */
static const appDiscCfg_t medcDiscCfg =
{
  FALSE                                   /*! TRUE to wait for a secure connection before initiating discovery */
};

/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/*! Discovery states:  enumeration of services to be discovered */
enum
{
  MEDC_DISC_GATT_SVC,      /*! GATT service */
  MEDC_DISC_DIS_SVC,       /*! Device Information service */
  MEDC_DISC_MED_SVC,       /*! Configured med. service */
  MEDC_DISC_SVC_MAX        /*! Discovery complete */
};

/*! Pointers into handle list for each service's handles */
uint16_t *pMedcGattHdlList = &medcCb.hdlList[MEDC_DISC_GATT_START];
uint16_t *pMedcDisHdlList = &medcCb.hdlList[MEDC_DISC_DIS_START];

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/*! Configuration states:  enumeration of services to be discovered */
enum
{
  MEDC_CFG_DIS_SVC,         /*! DIS services */
  MEDC_CFG_GATT_SVC,        /*! GATT services */
  MEDC_CFG_MED_SVC,         /*! Configured med. service */
  MEDC_CFG_SVC_MAX          /*! Configuration complete */
};

/* 
 * Data for configuration after service discovery
 */
 
/* Default value for CCC indications */
const uint8_t medcCccIndVal[2] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_INDICATE)};

/* Default value for CCC notifications */
const uint8_t medcCccNtfVal[2] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_NOTIFY)};

/* HRS Control point "Reset Energy Expended" */
//static const uint8_t medcHrsRstEnExp[] = {CH_HRCP_RESET_ENERGY_EXP};

/* List of DIS characteristics to configure after service discovery */
static const attcDiscCfg_t medcCfgDisList[] = 
{  
  /* Read:  DIS Manufacturer name string */
  {NULL, 0, DIS_MFNS_HDL_IDX},

  /* Read:  DIS Model number string */
  {NULL, 0, DIS_MNS_HDL_IDX},

  /* Read:  DIS Serial number string */
  {NULL, 0, DIS_SNS_HDL_IDX},

  /* Read:  DIS Hardware revision string */
  {NULL, 0, DIS_HRS_HDL_IDX},

  /* Read:  DIS Firmware revision string */
  {NULL, 0, DIS_FRS_HDL_IDX},

  /* Read:  DIS Software revision string */
  {NULL, 0, DIS_SRS_HDL_IDX},

  /* Read:  DIS System ID */
  {NULL, 0, DIS_SID_HDL_IDX}
};

/* List of GATT characteristics to configure after service discovery */
static const attcDiscCfg_t medcCfgGattList[] = 
{  
  /* Write:  GATT service changed ccc descriptor */
  {medcCccIndVal, sizeof(medcCccIndVal), GATT_SC_CCC_HDL_IDX}
};

/* Characteristic configuration list length */
#define MEDC_CFG_GATT_LIST_LEN   (sizeof(medcCfgGattList) / sizeof(attcDiscCfg_t))
#define MEDC_CFG_DIS_LIST_LEN   (sizeof(medcCfgDisList) / sizeof(attcDiscCfg_t))

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! application control block */
medcCb_t medcCb;

/*************************************************************************************************/
/*!
 *  \fn     medcDmCback
 *        
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcDmCback(dmEvt_t *pDmEvt)
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
    WsfMsgSend(medcCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medcAttCback
 *        
 *  \brief  Application  ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcAttCback(attEvt_t *pEvt)
{
  attEvt_t *pMsg;
  
  if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attEvt_t));
    pMsg->pValue = (uint8_t *) (pMsg + 1);
    memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
    WsfMsgSend(medcCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medcScanStart
 *        
 *  \brief  Perform actions on scan start.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcScanStart(dmEvt_t *pMsg)
{
  medcCb.scanning = TRUE;
}

/*************************************************************************************************/
/*!
 *  \fn     medcScanStop
 *        
 *  \brief  Perform actions on scan stop.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcScanStop(dmEvt_t *pMsg)
{
  medcCb.scanning = FALSE;
  medcCb.autoConnect = FALSE;
}

/*************************************************************************************************/
/*!
 *  \fn     medcScanReport
 *        
 *  \brief  Handle a scan report.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcScanReport(dmEvt_t *pMsg)
{
  uint8_t *pData;
  uint8_t len;
  bool_t  connect = FALSE;
  
  /* disregard if not scanning or autoconnecting */
  if (!medcCb.scanning || !medcCb.autoConnect)
  {
    return;
  }
  
  /* if we already have a bond with this device then connect to it */
  if (AppDbFindByAddr(pMsg->scanReport.addrType, pMsg->scanReport.addr) != APP_DB_HDL_NONE)
  {
    connect = TRUE;
  }
  /* otherwise look for desired service in advertising data */
  else
  {
    /* find Service UUID list; if full list not found search for partial */
    if ((pData = DmFindAdType(DM_ADV_TYPE_16_UUID, pMsg->scanReport.len,
                              pMsg->scanReport.pData)) == NULL)
    {
      pData = DmFindAdType(DM_ADV_TYPE_16_UUID_PART, pMsg->scanReport.len,
                           pMsg->scanReport.pData);
    }
    
    /* if found and length checks out ok */
    if (pData != NULL && pData[DM_AD_LEN_IDX] >= (ATT_16_UUID_LEN + 1))
    {
      len = pData[DM_AD_LEN_IDX] - 1;
      pData += DM_AD_DATA_IDX;
      
      while (len >= ATT_16_UUID_LEN)
      {
        if (BYTES_UINT16_CMP(pData, medcCb.autoUuid))
        {
          connect = TRUE;
          break;
        }
        pData += ATT_16_UUID_LEN;
        len -= ATT_16_UUID_LEN;
      }
    }
  }
    
  if (connect)
  {
    /* stop scanning and connect */
    medcCb.autoConnect = FALSE;
    AppScanStop();
    AppConnOpen(pMsg->scanReport.addrType, pMsg->scanReport.addr);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medcOpen
 *        
 *  \brief  Perform UI actions on connection open.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcOpen(dmEvt_t *pMsg)
{

}

/*************************************************************************************************/
/*!
 *  \fn     medcSetup
 *        
 *  \brief  Set up procedures that need to be performed after device reset.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcSetup(dmEvt_t *pMsg)
{
  medcCb.scanning = FALSE;
  DmConnSetConnSpec((hciConnSpec_t *) &medcConnCfg);
}

/*************************************************************************************************/
/*!
 *  \fn     medcBtnCback
 *        
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcBtnCback(uint8_t btn)
{
  dmConnId_t      connId;
  
  /* button actions when connected */
  if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
  {
    switch (btn)
    {
      case APP_UI_BTN_1_LONG:
        /* disconnect */
        AppConnClose(connId);
        break;
        
      default:
        /* all other button presses-- send to profile */
        medcCb.pIf->btn(connId, btn);
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
        if (medcCb.scanning)
        {
          AppScanStop();
        }
        /* else auto connect */
        else if (!medcCb.autoConnect)
        {
          medcCb.autoConnect = TRUE;
          AppScanStart(medcMasterCfg.discMode, medcMasterCfg.scanType,
                       medcMasterCfg.scanDuration);
        }
        break;
        
      case APP_UI_BTN_1_LONG:
        /* clear bonded device info */
        AppDbDeleteAllRecords();
        break;
        
      default:
        /* all other button presses-- send to profile */
        medcCb.pIf->btn(connId, btn);
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medcDiscCback
 *        
 *  \brief  Discovery callback.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    Service or configuration status.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcDiscCback(dmConnId_t connId, uint8_t status)
{
  switch(status)
  {
    case APP_DISC_INIT:
      /* set handle list when initialization requested */
      AppDiscSetHdlList(connId, medcCb.hdlListLen, medcCb.hdlList);
      break;

    case APP_DISC_SEC_REQUIRED:
      /* initiate security */
      AppMasterSecurityReq(connId);
      break;
      
    case APP_DISC_START:
      /* initialize discovery state */
      medcCb.discState = MEDC_DISC_GATT_SVC;
      
      /* discover GATT service */
      GattDiscover(connId, pMedcGattHdlList);
      break;
      
    case APP_DISC_FAILED:
      /* if discovery failed for desired service then disconnect */
      if (medcCb.discState == MEDC_DISC_MED_SVC)
      {
        AppConnClose(connId);
        break;
      }
      /* else fall through and continue discovery */
      
    case APP_DISC_CMPL:
      /* next discovery state */
      medcCb.discState++;
      
      if (medcCb.discState == MEDC_DISC_DIS_SVC)
      {
        /* discover device information service */
        DisDiscover(connId, pMedcDisHdlList);
      }
      else if (medcCb.discState == MEDC_DISC_MED_SVC)
      {
        /* discover med profile service */
        medcCb.pIf->discover(connId);
      }
      else
      {    
        /* discovery complete */
        AppDiscComplete(connId, APP_DISC_CMPL);

        /* start configuration: configure DIS service */
        medcCb.cfgState = MEDC_CFG_DIS_SVC;
        AppDiscConfigure(connId, APP_DISC_CFG_START, MEDC_CFG_DIS_LIST_LEN,
                         (attcDiscCfg_t *) medcCfgDisList,
                         DIS_HDL_LIST_LEN, pMedcDisHdlList);
      }
      break;
      
    case APP_DISC_CFG_START:
      /* start configuration: configure DIS service */
      medcCb.cfgState = MEDC_CFG_DIS_SVC;
      AppDiscConfigure(connId, APP_DISC_CFG_START, MEDC_CFG_DIS_LIST_LEN,
                       (attcDiscCfg_t *) medcCfgDisList,
                       DIS_HDL_LIST_LEN, pMedcDisHdlList);
      break;
      
    case APP_DISC_CFG_CMPL:
      /* next configuration state */
      medcCb.cfgState++;

      if (medcCb.cfgState == MEDC_CFG_GATT_SVC)
      {
        /* configure GATT */
        AppDiscConfigure(connId, APP_DISC_CFG_START, MEDC_CFG_GATT_LIST_LEN,
                         (attcDiscCfg_t *) medcCfgGattList,
                         GATT_HDL_LIST_LEN, pMedcGattHdlList);      
      }
      else if (medcCb.cfgState == MEDC_CFG_MED_SVC)
      {
        /* configure med profile service */
        medcCb.pIf->configure(connId, APP_DISC_CFG_START);
      }
      else
      {
        AppDiscComplete(connId, status);
      }
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
 *  \fn     medcProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medcProcMsg(dmEvt_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;
  
  switch(pMsg->hdr.event)
  {
    case ATTC_READ_RSP:
    case ATTC_WRITE_RSP:
    case ATTC_HANDLE_VALUE_NTF:
    case ATTC_HANDLE_VALUE_IND:
    case MEDC_TIMER_IND:
      medcCb.pIf->procMsg(&pMsg->hdr);
      break;
      
    case DM_RESET_CMPL_IND:
      medcSetup(pMsg);
      uiEvent = APP_UI_RESET_CMPL;
      break;

    case DM_SCAN_START_IND:
      medcScanStart(pMsg);
      uiEvent = APP_UI_SCAN_START;
      break;
         
    case DM_SCAN_STOP_IND:
      medcScanStop(pMsg);
      uiEvent = APP_UI_SCAN_STOP;
      break;

    case DM_SCAN_REPORT_IND:
      medcScanReport(pMsg);
      break;
      
    case DM_CONN_OPEN_IND:
      medcOpen(pMsg);
      uiEvent = APP_UI_CONN_OPEN;
      break;
         
    case DM_CONN_CLOSE_IND:
      medcCb.pIf->procMsg(&pMsg->hdr);
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
 *  \fn     MedcHandlerInit
 *        
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedcHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("MedcHandlerInit");
  
  /* store handler ID */
  medcCb.handlerId = handlerId;
  
  /* Set configuration pointers */
  pAppMasterCfg = (appMasterCfg_t *) &medcMasterCfg;
  pAppSecCfg = (appSecCfg_t *) &medcSecCfg;
  pAppDiscCfg = (appDiscCfg_t *) &medcDiscCfg;
  
  /* Set stack configuration pointers */
  pSmpCfg = (smpCfg_t *) &medcSmpCfg;
  
  /* Initialize application framework */
  AppMasterInit();
  AppDiscInit();
  
  /* Set default profile to use */
  MedcSetProfile(MEDC_PROFILE);
}

/*************************************************************************************************/
/*!
 *  \fn     MedcHandler
 *        
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedcHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{ 
  if (pMsg != NULL)
  {
    APP_TRACE_INFO1("Medc got evt %d", pMsg->event);

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
    medcProcMsg((dmEvt_t *) pMsg);    
  }
}

/*************************************************************************************************/
/*!
 *  \fn     MedcStart
 *        
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedcStart(void)
{  
  /* Register for stack callbacks */
  DmRegister(medcDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, medcDmCback);
  AttRegister(medcAttCback);
  
  /* Register for app framework button callbacks */
  AppUiBtnRegister(medcBtnCback);

  /* Register for app framework discovery callbacks */
  AppDiscRegister(medcDiscCback);
  
  /* Reset the device */
  DmDevReset();  
}

/*************************************************************************************************/
/*!
 *  \fn     MedcSetProfile
 *        
 *  \brief  Set the profile to be used by the application.  This function is called internally
 *          by MedcHandlerInit() with a default value.  It may also be called by the system
 *          to configure the profile after executing MedcHandlerInit() and before executing
 *          MedcStart().
 *
 *  \param  profile  Profile identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedcSetProfile(uint8_t profile)
{
  switch (profile)
  {
#if MEDC_HRP_INCLUDED == TRUE
    case MEDC_ID_HRP:
      medcCb.pIf = &medcHrpIf;
      medcCb.pIf->init();
      break;
#endif
#if MEDC_BLP_INCLUDED == TRUE
    case MEDC_ID_BLP:
      medcCb.pIf = &medcBlpIf;
      medcCb.pIf->init();
      break;
#endif
#if MEDC_GLP_INCLUDED == TRUE
    case MEDC_ID_GLP:
      medcCb.pIf = &medcGlpIf;
      medcCb.pIf->init();
      break;
#endif
#if MEDC_WSP_INCLUDED == TRUE
    case MEDC_ID_WSP:
      medcCb.pIf = &medcWspIf;
      medcCb.pIf->init();
      break;
#endif
#if MEDC_HTP_INCLUDED == TRUE
    case MEDC_ID_HTP:
      medcCb.pIf = &medcHtpIf;
      medcCb.pIf->init();
      break;
#endif
    default:
      APP_TRACE_WARN1("MedcSetProfile invalid profile:%d", profile);
      break;
  }
}