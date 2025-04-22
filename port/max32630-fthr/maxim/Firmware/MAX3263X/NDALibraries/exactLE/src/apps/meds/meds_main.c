/*************************************************************************************************/
/*!
 *  \file   meds_main.c
 *
 *  \brief  Medical sensor sample application for the following profiles:
 *            Blood Pressure profile sensor
 *            Weight Scale profile sensor
 *            Health Thermometer profile sensor
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
#include "svc_dis.h"
#include "meds_api.h"
#include "meds_main.h"

/**************************************************************************************************
  Profile Configuration
**************************************************************************************************/

/* Blood pressure profile included */
#ifndef MEDS_BLP_INCLUDED
#define MEDS_BLP_INCLUDED TRUE
#endif

/* Weight scale profile included */
#ifndef MEDS_WSP_INCLUDED
#define MEDS_WSP_INCLUDED TRUE
#endif

/* Health thermometer profile included */
#ifndef MEDS_HTP_INCLUDED
#define MEDS_HTP_INCLUDED TRUE
#endif

/* Default profile to use */
#ifndef MEDS_PROFILE
#define MEDS_PROFILE MEDS_ID_BLP
#endif

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for slave */
static const appSlaveCfg_t medsSlaveCfg =
{
  {30000, 30000,     0},                  /*! Advertising durations in ms */
  {   48,  1600,     0}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for security */
static const appSecCfg_t medsSecCfg =
{
  DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
  0,                                      /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  TRUE                                    /*! TRUE to initiate security upon connection */
};

/*! configurable parameters for connection parameter update */
static const appUpdateCfg_t medsUpdateCfg =
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

/*! advertising data flags */
static const uint8_t medsAdvDataFlags[] =
{
  DM_FLAG_LE_LIMITED_DISC |
  DM_FLAG_LE_BREDR_NOT_SUP
};

/*! advertising data buffer (value is set in medsSetup) */
static uint8_t medsAdvDataDisc[HCI_ADV_DATA_LEN];

/*! scan data, discoverable mode */
static const uint8_t medsScanDataDisc[] =
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
  Global Variables
**************************************************************************************************/

/*! application control block */
medsCb_t medsCb;

/*************************************************************************************************/
/*!
 *  \fn     medsDmCback
 *        
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t *pMsg;
  
  if ((pMsg = WsfMsgAlloc(sizeof(dmEvt_t))) != NULL)
  {
    memcpy(pMsg, pDmEvt, sizeof(dmEvt_t));
    WsfMsgSend(medsCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medsAttCback
 *        
 *  \brief  Application ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsAttCback(attEvt_t *pEvt)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \fn     medsCccCback
 *        
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
void medsCccCback(attsCccEvt_t *pEvt)
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
 *  \fn     medsSetup
 *        
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsSetup(wsfMsgHdr_t *pMsg)
{
  /* start advertising; automatically set connectable/discoverable mode and bondable mode */
  AppAdvStart(APP_MODE_AUTO_INIT);
}

/*************************************************************************************************/
/*!
 *  \fn     medsBtnCback
 *        
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsBtnCback(uint8_t btn)
{
  dmConnId_t      connId;
  
  /* button actions when connected */
  if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
  {
    switch (btn)
    {
      case APP_UI_BTN_1_LONG:
        AppConnClose(connId);
        break;

      default:
        /* all other button presses-- send to profile */
        medsCb.pIf->btn(connId, btn);      
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
        /* all other button presses-- send to profile */
        medsCb.pIf->btn(connId, btn);      
        break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     medsProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void medsProcMsg(wsfMsgHdr_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;
  
  switch(pMsg->event)
  {
    case MEDS_TIMER_IND:
      medsCb.pIf->procMsg(pMsg);
      break;

    case DM_RESET_CMPL_IND:
      medsSetup(pMsg);
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
      medsCb.pIf->procMsg(pMsg);
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
      AppHandlePasskey(&((dmEvt_t *)pMsg)->authReq);
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
 *  \fn     MedsHandlerInit
 *        
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedsHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("MedsHandlerInit");
  
  /* store handler ID */
  medsCb.handlerId = handlerId;
  
  /* Set configuration pointers */
  pAppSlaveCfg = (appSlaveCfg_t *) &medsSlaveCfg;
  pAppSecCfg = (appSecCfg_t *) &medsSecCfg;
  pAppUpdateCfg = (appUpdateCfg_t *) &medsUpdateCfg;
  
  /* Initialize application framework */
  AppSlaveInit();
  
  /* Set default profile to use */
  MedsSetProfile(MEDS_PROFILE);
}

/*************************************************************************************************/
/*!
 *  \fn     MedsHandler
 *        
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedsHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{ 
  if (pMsg != NULL)
  {
    APP_TRACE_INFO1("Meds got evt %d", pMsg->event);
  
    if (pMsg->event >= DM_CBACK_START && pMsg->event <= DM_CBACK_END)
    {
      /* process advertising and connection-related messages */
      AppSlaveProcDmMsg((dmEvt_t *) pMsg);
      
      /* process security-related messages */
      AppSlaveSecProcDmMsg((dmEvt_t *) pMsg);
    }
        
    /* perform profile and user interface-related operations */
    medsProcMsg(pMsg);    
  }
}

/*************************************************************************************************/
/*!
 *  \fn     MedsStart
 *        
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedsStart(void)
{  
  /* Register for stack callbacks */
  DmRegister(medsDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, medsDmCback);
  AttRegister(medsAttCback);
  AttConnRegister(AppServerConnCback);
  
  /* Register for app framework callbacks */
  AppUiBtnRegister(medsBtnCback);
  
  /* Initialize attribute server database */
  SvcCoreAddGroup();
  SvcDisAddGroup();

  /* set advertising and scan response data for discoverable mode */
  AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, 0, (uint8_t *) medsAdvDataDisc);
  AppAdvSetAdValue(APP_ADV_DATA_DISCOVERABLE, DM_ADV_TYPE_FLAGS, sizeof(medsAdvDataFlags),
                   (uint8_t *) medsAdvDataFlags);
  AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(medsScanDataDisc), (uint8_t *) medsScanDataDisc);

  /* set advertising and scan response data for connectable mode */
  AppAdvSetData(APP_ADV_DATA_CONNECTABLE, 0, NULL);
  AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, 0, NULL);
  
  /* call profile start function */
  medsCb.pIf->start();
  
  /* Reset the device */
  DmDevReset();  
}

/*************************************************************************************************/
/*!
 *  \fn     MedsSetProfile
 *        
 *  \brief  Set the profile to be used by the application.  This function is called internally
 *          by MedsHandlerInit() with a default value.  It may also be called by the system
 *          to configure the profile after executing MedsHandlerInit() and before executing
 *          MedsStart().
 *
 *  \param  profile  Profile identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void MedsSetProfile(uint8_t profile)
{
  switch (profile)
  {
#if MEDS_BLP_INCLUDED == TRUE
    case MEDS_ID_BLP:
      medsCb.pIf = &medsBlpIf;
      break;
#endif
#if MEDS_WSP_INCLUDED == TRUE
    case MEDS_ID_WSP:
      medsCb.pIf = &medsWspIf;
      break;
#endif
#if MEDS_HTP_INCLUDED == TRUE
    case MEDS_ID_HTP:
      medsCb.pIf = &medsHtpIf;
      break;
#endif
    default:
      APP_TRACE_WARN1("MedsSetProfile invalid profile:%d", profile);
      break;
  }
}