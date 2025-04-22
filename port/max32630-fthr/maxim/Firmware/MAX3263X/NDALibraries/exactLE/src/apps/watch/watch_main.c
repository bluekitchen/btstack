/*************************************************************************************************/
/*!
 *  \file   watch.c
 *
 *  \brief  Watch sample application for the following profiles:
 *            Time profile client
 *            Alert Notification profile client
 *            Phone Alert Status profile client
 *
 *          $Date: 2015-12-21 15:07:14 -0600 (Mon, 21 Dec 2015) $
 *          $Revision: 20721 $
 *
 *  Copyright (c) 2011 Wicentric, Inc., all rights reserved.
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
#include "svc_core.h"
#include "svc_ch.h"
#include "gatt_api.h"
#include "tipc_api.h"
#include "anpc_api.h"
#include "paspc_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! application control block */
static struct
{
  uint16_t          hdlList[APP_DB_HDL_LIST_LEN];
  wsfHandlerId_t    handlerId;
  uint8_t           discState;
} watchCb;

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for slave */
static const appSlaveCfg_t watchSlaveCfg =
{
  {30000,     0,     0},                  /*! Advertising durations in ms */
  {   48,   800,     0}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for security */
static const appSecCfg_t watchSecCfg =
{
  DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
  0,                                      /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  TRUE                                    /*! TRUE to initiate security upon connection */
};

/*! configurable parameters for connection parameter update */
static const appUpdateCfg_t watchUpdateCfg =
{
  6000,                                   /*! Connection idle period in ms before attempting
                                              connection parameter update; set to zero to disable */
  640,                                    /*! Minimum connection interval in 1.25ms units */
  800,                                    /*! Maximum connection interval in 1.25ms units */
  0,                                      /*! Connection latency */
  600,                                    /*! Supervision timeout in 10ms units */
  5                                       /*! Number of update attempts before giving up */
};

/*! SMP security parameter configuration */
/* Configuration structure */
static const smpCfg_t watchSmpCfg =
{
  3000,                                   /*! 'Repeated attempts' timeout in msec */
  SMP_IO_DISP_ONLY,                       /*! I/O Capability */
  7,                                      /*! Minimum encryption key length */
  16,                                     /*! Maximum encryption key length */
  3,                                      /*! Attempts to trigger 'repeated attempts' timeout */
  0                                       /*! Device authentication requirements */
};

/*! Configurable parameters for service and characteristic discovery */
static const appDiscCfg_t watchDiscCfg =
{
  FALSE                                   /*! TRUE to wait for a secure connection before initiating discovery */
};

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! advertising data, discoverable mode */
static const uint8_t watchAdvDataDisc[] =
{
  /*! flags */
  2,                                      /*! length */
  DM_ADV_TYPE_FLAGS,                      /*! AD type */
  DM_FLAG_LE_LIMITED_DISC |               /*! flags */
  DM_FLAG_LE_BREDR_NOT_SUP,
  
  /*! tx power */
  2,                                      /*! length */
  DM_ADV_TYPE_TX_POWER,                   /*! AD type */
  0,                                      /*! tx power */

  /*! service solicitation UUID list */
  7,                                      /*! length */
  DM_ADV_TYPE_16_SOLICIT,                 /*! AD type */
  UINT16_TO_BYTES(ATT_UUID_CURRENT_TIME_SERVICE),
  UINT16_TO_BYTES(ATT_UUID_ALERT_NOTIF_SERVICE),
  UINT16_TO_BYTES(ATT_UUID_PHONE_ALERT_SERVICE)  
};

/*! scan data, discoverable mode */
static const uint8_t watchScanDataDisc[] =
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
  ATT Client Discovery Data
**************************************************************************************************/

/*! Discovery states:  enumeration of services to be discovered */
enum
{
  WATCH_DISC_GATT_SVC,      /* GATT service */
  WATCH_DISC_CTS_SVC,       /* Current Time service */
  WATCH_DISC_ANS_SVC,       /* Alert Notification service */
  WATCH_DISC_PASS_SVC,      /* Phone Alert Status service */
  WATCH_DISC_SVC_MAX        /* Discovery complete */
};

/*! the Client handle list, watchCb.hdlList[], is set as follows:
 *
 *  ------------------------------- <- WATCH_DISC_GATT_START
 *  | GATT handles                |
 *  |...                          |
 *  ------------------------------- <- WATCH_DISC_CTS_START
 *  | CTS handles                 |
 *  | ...                         |
 *  ------------------------------- <- WATCH_DISC_ANS_START
 *  | ANS handles                 |
 *  | ...                         |
 *  ------------------------------- <- WATCH_DISC_PASS_START
 *  | PASS handles                |
 *  | ...                         |
 *  -------------------------------
 */

/*! Start of each service's handles in the the handle list */
#define WATCH_DISC_GATT_START       0
#define WATCH_DISC_CTS_START        (WATCH_DISC_GATT_START + GATT_HDL_LIST_LEN)
#define WATCH_DISC_ANS_START        (WATCH_DISC_CTS_START + TIPC_CTS_HDL_LIST_LEN)
#define WATCH_DISC_PASS_START       (WATCH_DISC_ANS_START + ANPC_ANS_HDL_LIST_LEN)
#define WATCH_DISC_HDL_LIST_LEN     (WATCH_DISC_PASS_START + PASPC_PASS_HDL_LIST_LEN)

/*! Pointers into handle list for each service's handles */
static uint16_t *pWatchGattHdlList = &watchCb.hdlList[WATCH_DISC_GATT_START];
static uint16_t *pWatchCtsHdlList = &watchCb.hdlList[WATCH_DISC_CTS_START];
static uint16_t *pWatchAnsHdlList = &watchCb.hdlList[WATCH_DISC_ANS_START];
static uint16_t *pWatchPassHdlList = &watchCb.hdlList[WATCH_DISC_PASS_START];

/* sanity check:  make sure handle list length is <= app db handle list length */
WSF_CT_ASSERT(WATCH_DISC_HDL_LIST_LEN <= APP_DB_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Client Configuration Data
**************************************************************************************************/

/* 
 * Data for configuration after service discovery
 */
 
/* Default value for CCC indications */
static const uint8_t watchCccIndVal[] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_INDICATE)};

/* Default value for CCC notifications */
static const uint8_t watchCccNtfVal[] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_NOTIFY)};

/* ANS Control point value for "Enable New Alert Notification" */
static const uint8_t watchAncpEnNewVal[] = {CH_ANCP_ENABLE_NEW, CH_ALERT_CAT_ID_ALL};

/* ANS Control point value for "Notify New Alert Immediately" */
static const uint8_t watchAncpNotNewVal[] = {CH_ANCP_NOTIFY_NEW, CH_ALERT_CAT_ID_ALL};

/* ANS Control point value for "Enable Unread Alert Status Notification" */
static const uint8_t watchAncpEnUnrVal[] = {CH_ANCP_ENABLE_UNREAD, CH_ALERT_CAT_ID_ALL};

/* ANS Control point value for "Notify Unread Alert Status Immediately" */
static const uint8_t watchAncpNotUnrVal[] = {CH_ANCP_NOTIFY_UNREAD, CH_ALERT_CAT_ID_ALL};
  
/* List of characteristics to configure after service discovery */
static const attcDiscCfg_t watchDiscCfgList[] = 
{  
  /* Read:  CTS Current time */
  {NULL, 0, (TIPC_CTS_CT_HDL_IDX + WATCH_DISC_CTS_START)},

  /* Read:  CTS Local time information */
  {NULL, 0, (TIPC_CTS_LTI_HDL_IDX + WATCH_DISC_CTS_START)},

  /* Read:  CTS Reference time information */
  {NULL, 0, (TIPC_CTS_RTI_HDL_IDX + WATCH_DISC_CTS_START)},

  /* Read:  ANS Supported new alert category */
  {NULL, 0, (ANPC_ANS_SNAC_HDL_IDX + WATCH_DISC_ANS_START)},

  /* Read:  ANS Supported unread alert category */
  {NULL, 0, (ANPC_ANS_SUAC_HDL_IDX + WATCH_DISC_ANS_START)},

  /* Read:  PASS Alert status */
  {NULL, 0, (PASPC_PASS_AS_HDL_IDX + WATCH_DISC_PASS_START)},

  /* Read:  PASS Ringer setting */
  {NULL, 0, (PASPC_PASS_RS_HDL_IDX + WATCH_DISC_PASS_START)},

  /* Write:  GATT service changed ccc descriptor */
  {watchCccIndVal, sizeof(watchCccIndVal), (GATT_SC_CCC_HDL_IDX + WATCH_DISC_GATT_START)},

  /* Write:  CTS Current time ccc descriptor */
  {watchCccNtfVal, sizeof(watchCccNtfVal), (TIPC_CTS_CT_CCC_HDL_IDX + WATCH_DISC_CTS_START)},

  /* Write:  ANS New alert ccc descriptor */
  {watchCccNtfVal, sizeof(watchCccNtfVal), (ANPC_ANS_NA_CCC_HDL_IDX + ANPC_ANS_ANCP_HDL_IDX)},

  /* Write:  ANS Unread alert status ccc descriptor */
  {watchCccNtfVal, sizeof(watchCccNtfVal), (ANPC_ANS_UAS_CCC_HDL_IDX + WATCH_DISC_ANS_START)},
  
  /* Write:  PASS Alert status ccc descriptor */
  {watchCccNtfVal, sizeof(watchCccNtfVal), (PASPC_PASS_AS_CCC_HDL_IDX + WATCH_DISC_PASS_START)},
  
  /* Write:  PASS Ringer setting ccc descriptor */
  {watchCccNtfVal, sizeof(watchCccNtfVal), (PASPC_PASS_RS_CCC_HDL_IDX + WATCH_DISC_PASS_START)},
  
  /* Write:  ANS Control point "Enable New Alert Notification" */
  {watchAncpEnNewVal, sizeof(watchAncpEnNewVal), (ANPC_ANS_ANCP_HDL_IDX + ANPC_ANS_ANCP_HDL_IDX)},
  
  /* Write:  ANS Control point "Notify New Alert Immediately" */
  {watchAncpNotNewVal, sizeof(watchAncpNotNewVal), (ANPC_ANS_ANCP_HDL_IDX + ANPC_ANS_ANCP_HDL_IDX)},

  /* Write:  ANS Control point "Enable Unread Alert Status Notification" */
  {watchAncpEnUnrVal, sizeof(watchAncpEnUnrVal), (ANPC_ANS_ANCP_HDL_IDX + ANPC_ANS_ANCP_HDL_IDX)},

  /* Write:  ANS Control point "Notify Unread Alert Status Immediately" */
  {watchAncpNotUnrVal, sizeof(watchAncpNotUnrVal), (ANPC_ANS_ANCP_HDL_IDX + ANPC_ANS_ANCP_HDL_IDX)}
};

/* Characteristic configuration list length */
#define WATCH_DISC_CFG_LIST_LEN   (sizeof(watchDiscCfgList) / sizeof(attcDiscCfg_t))

/* sanity check:  make sure configuration list length is <= handle list length */
WSF_CT_ASSERT(WATCH_DISC_CFG_LIST_LEN <= WATCH_DISC_HDL_LIST_LEN);

/* 
 * Data for configuration on connection setup
 */

/* List of characteristics to configure on connection setup */
static const attcDiscCfg_t watchDiscConnCfgList[] = 
{  
  /* Read:  ANS Supported new alert category */
  {NULL, 0, (ANPC_ANS_SNAC_HDL_IDX + WATCH_DISC_ANS_START)},

  /* Read:  ANS Supported unread alert category */
  {NULL, 0, (ANPC_ANS_SUAC_HDL_IDX + WATCH_DISC_ANS_START)},

  /* Read:  PASS Alert status */
  {NULL, 0, (PASPC_PASS_AS_HDL_IDX + WATCH_DISC_PASS_START)},

  /* Write:  ANS Control point "Enable New Alert Notification" */
  {watchAncpEnNewVal, sizeof(watchAncpEnNewVal), (ANPC_ANS_ANCP_HDL_IDX + ANPC_ANS_ANCP_HDL_IDX)},
  
  /* Write:  ANS Control point "Notify New Alert Immediately" */
  {watchAncpNotNewVal, sizeof(watchAncpNotNewVal), (ANPC_ANS_ANCP_HDL_IDX + ANPC_ANS_ANCP_HDL_IDX)},

  /* Write:  ANS Control point "Enable Unread Alert Status Notification" */
  {watchAncpEnUnrVal, sizeof(watchAncpEnUnrVal), (ANPC_ANS_ANCP_HDL_IDX + ANPC_ANS_ANCP_HDL_IDX)},

  /* Write:  ANS Control point "Notify Unread Alert Status Immediately" */
  {watchAncpNotUnrVal, sizeof(watchAncpNotUnrVal), (ANPC_ANS_ANCP_HDL_IDX + ANPC_ANS_ANCP_HDL_IDX)}
};

/* Characteristic configuration list length */
#define WATCH_DISC_CONN_CFG_LIST_LEN   (sizeof(watchDiscConnCfgList) / sizeof(attcDiscCfg_t))

/* sanity check:  make sure configuration list length is <= handle list length */
WSF_CT_ASSERT(WATCH_DISC_CONN_CFG_LIST_LEN <= WATCH_DISC_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Server Data
**************************************************************************************************/

/*! enumeration of client characteristic configuration descriptors used in local ATT server */
enum
{
  WATCH_GATT_SC_CCC_IDX,        /*! GATT service, service changed characteristic */
  WATCH_NUM_CCC_IDX             /*! Number of ccc's */
};

/*! client characteristic configuration descriptors settings, indexed by ccc enumeration */
static const attsCccSet_t watchCccSet[WATCH_NUM_CCC_IDX] =
{
  /* cccd handle         value range                 security level */
  {GATT_SC_CH_CCC_HDL,   ATT_CLIENT_CFG_INDICATE,    DM_SEC_LEVEL_ENC}    /* WATCH_GATT_SC_CCC_IDX */
};

/*************************************************************************************************/
/*!
 *  \fn     watchDmCback
 *        
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void watchDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t *pMsg;
  
  if ((pMsg = WsfMsgAlloc(sizeof(dmEvt_t))) != NULL)
  {
    memcpy(pMsg, pDmEvt, sizeof(dmEvt_t));
    WsfMsgSend(watchCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     watchAttCback
 *        
 *  \brief  Application  ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void watchAttCback(attEvt_t *pEvt)
{
  attEvt_t *pMsg;
  
  if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attEvt_t));
    pMsg->pValue = (uint8_t *) (pMsg + 1);
    memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
    WsfMsgSend(watchCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     watchCccCback
 *        
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void watchCccCback(attsCccEvt_t *pEvt)
{
  attsCccEvt_t  *pMsg;
  appDbHdl_t    dbHdl;
  
  /* if CCC not set from initialization and there's a device record */
  if ((pEvt->handle != ATT_HANDLE_NONE) &&
      ((dbHdl = AppDbGetHdl((dmConnId_t) pEvt->hdr.param)) != APP_DB_HDL_NONE))
  {
    /* store value in device database */  
    AppDbSetCccTblValue(dbHdl, pEvt->idx, pEvt->value);
  }
  
  if ((pMsg = WsfMsgAlloc(sizeof(attsCccEvt_t))) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attsCccEvt_t));
    WsfMsgSend(watchCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     watchOpen
 *        
 *  \brief  Perform UI actions on connection open.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void watchOpen(dmEvt_t *pMsg)
{
  /* if not bonded send a security request on connection open; devices must pair before
   * service discovery will be initiated
   */
  if (AppDbCheckBonded() == FALSE)
  {
    DmSecSlaveReq((dmConnId_t) pMsg->hdr.param, pAppSecCfg->auth);
  }    
}

/*************************************************************************************************/
/*!
 *  \fn     watchSetup
 *        
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void watchSetup(dmEvt_t *pMsg)
{
  /* set advertising and scan response data for discoverable mode */
  AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(watchAdvDataDisc), (uint8_t *) watchAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(watchScanDataDisc), (uint8_t *) watchScanDataDisc);

  /* set advertising and scan response data for connectable mode */
  AppAdvSetData(APP_ADV_DATA_CONNECTABLE, 0, NULL);
  AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, 0, NULL);
  
  /* start advertising; automatically set connectable/discoverable mode and bondable mode */
  AppAdvStart(APP_MODE_AUTO_INIT);
}

/*************************************************************************************************/
/*!
 *  \fn     watchValueUpdate
 *        
 *  \brief  Process a received ATT read response, notification, or indication.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void watchValueUpdate(attEvt_t *pMsg)
{
  if (pMsg->hdr.status == ATT_SUCCESS)
  {
    /* determine which profile the handle belongs to; start with most likely */
    
    /* alert notification */
    if (AnpcAnsValueUpdate(pWatchAnsHdlList, pMsg) == ATT_SUCCESS)    
    {
      return;
    }
    /* phone alert status */
    if (PaspcPassValueUpdate(pWatchPassHdlList, pMsg) == ATT_SUCCESS)
    {
      return;
    }

    /* current time */
    if (TipcCtsValueUpdate(pWatchCtsHdlList, pMsg) == ATT_SUCCESS)    
    {
      return;
    }

    /* GATT */
    if (GattValueUpdate(pWatchGattHdlList, pMsg) == ATT_SUCCESS)    
    {
      return;
    }
  }
} 

/*************************************************************************************************/
/*!
 *  \fn     watchBtnCback
 *        
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void watchBtnCback(uint8_t btn)
{
  dmConnId_t      connId;
  static uint8_t  ringer = CH_RCP_SILENT;
  
  /* button actions when connected */
  if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
  {
    switch (btn)
    {
      case APP_UI_BTN_1_SHORT:
        /* mute ringer once */
        PaspcPassControl(connId, pWatchPassHdlList[PASPC_PASS_RCP_HDL_IDX], CH_RCP_MUTE_ONCE);
        break;
        
      case APP_UI_BTN_1_MED:
        /* toggle between silencing ringer and enabling ringer */
        PaspcPassControl(connId, pWatchPassHdlList[PASPC_PASS_RCP_HDL_IDX], ringer);
        ringer = (ringer == CH_RCP_SILENT) ? CH_RCP_CANCEL_SILENT : CH_RCP_SILENT;
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
 *  \fn     watchDiscCback
 *        
 *  \brief  Discovery callback.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    Service or configuration status.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void watchDiscCback(dmConnId_t connId, uint8_t status)
{
  switch(status)
  {
    case APP_DISC_INIT:
      /* set handle list when initialization requested */
      AppDiscSetHdlList(connId, WATCH_DISC_HDL_LIST_LEN, watchCb.hdlList);
      break;

    case APP_DISC_SEC_REQUIRED:
      /* request security */
      AppSlaveSecurityReq(connId);
      break;

    case APP_DISC_START:
      /* initialize discovery state */
      watchCb.discState = WATCH_DISC_GATT_SVC;
      
      /* discover GATT service */
      GattDiscover(connId, pWatchGattHdlList);
      break;
      
    case APP_DISC_FAILED:      
    case APP_DISC_CMPL:
      /* next discovery state */
      watchCb.discState++;
      
      if (watchCb.discState == WATCH_DISC_CTS_SVC)
      {
        /* discover current time service */
        TipcCtsDiscover(connId, pWatchCtsHdlList);
      }
      else if (watchCb.discState == WATCH_DISC_ANS_SVC)
      {
        /* discover alert notification service */
        AnpcAnsDiscover(connId, pWatchAnsHdlList);
      }
      else if (watchCb.discState == WATCH_DISC_PASS_SVC)
      {
        /* discover phone alert status service */
        PaspcPassDiscover(connId, pWatchPassHdlList);
      }
      else
      {    
        /* discovery complete */
        AppDiscComplete(connId, APP_DISC_CMPL);

        /* start configuration */
        AppDiscConfigure(connId, APP_DISC_CFG_START, WATCH_DISC_CFG_LIST_LEN,
                         (attcDiscCfg_t *) watchDiscCfgList, WATCH_DISC_HDL_LIST_LEN, watchCb.hdlList);
      }
      break;
      
    case APP_DISC_CFG_START:
      /* start configuration */
      AppDiscConfigure(connId, APP_DISC_CFG_START, WATCH_DISC_CFG_LIST_LEN,
                       (attcDiscCfg_t *) watchDiscCfgList, WATCH_DISC_HDL_LIST_LEN, watchCb.hdlList);
      break;
      
    case APP_DISC_CFG_CMPL: 
      AppDiscComplete(connId, status);
      break;

    case APP_DISC_CFG_CONN_START:
      /* start connection setup configuration */
      AppDiscConfigure(connId, APP_DISC_CFG_CONN_START, WATCH_DISC_CONN_CFG_LIST_LEN,
                       (attcDiscCfg_t *) watchDiscConnCfgList, WATCH_DISC_HDL_LIST_LEN, watchCb.hdlList);
      break;
      
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     watchProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void watchProcMsg(dmEvt_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;
  
  switch(pMsg->hdr.event)
  {
    case ATTC_READ_RSP:
    case ATTC_HANDLE_VALUE_NTF:
    case ATTC_HANDLE_VALUE_IND:
      watchValueUpdate((attEvt_t *) pMsg);
      break;
      
    case DM_RESET_CMPL_IND:
      watchSetup(pMsg);
      uiEvent = APP_UI_RESET_CMPL;
      break;

    case DM_ADV_START_IND:
      uiEvent = APP_UI_ADV_START;
      break;
         
    case DM_ADV_STOP_IND:
      uiEvent = APP_UI_ADV_STOP;
      break;
          
    case DM_CONN_OPEN_IND:
      watchOpen(pMsg);
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
 *  \fn     WatchHandlerInit
 *        
 *  \brief  Application handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WatchHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("WatchHandlerInit");
  
  /* store handler ID */
  watchCb.handlerId = handlerId;
  
  /* Set configuration pointers */
  pAppSlaveCfg = (appSlaveCfg_t *) &watchSlaveCfg;
  pAppSecCfg = (appSecCfg_t *) &watchSecCfg;
  pAppUpdateCfg = (appUpdateCfg_t *) &watchUpdateCfg;
  pAppDiscCfg = (appDiscCfg_t *) &watchDiscCfg;
  
  /* Set stack configuration pointers */
  pSmpCfg = (smpCfg_t *) &watchSmpCfg;
  
  /* Initialize application framework */
  AppSlaveInit();
  AppDiscInit();
}

/*************************************************************************************************/
/*!
 *  \fn     WatchHandler
 *        
 *  \brief  WSF event handler for application.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WatchHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{ 
  if (pMsg != NULL)
  {
    APP_TRACE_INFO1("Watch got evt %d", pMsg->event);

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
      AppSlaveProcDmMsg((dmEvt_t *) pMsg);
      
      /* process security-related messages */
      AppSlaveSecProcDmMsg((dmEvt_t *) pMsg);
      
      /* process discovery-related messages */
      AppDiscProcDmMsg((dmEvt_t *) pMsg);
    }
          
    /* perform profile and user interface-related operations */
    watchProcMsg((dmEvt_t *) pMsg);    
  }
}

/*************************************************************************************************/
/*!
 *  \fn     WatchStart
 *        
 *  \brief  Start the application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WatchStart(void)
{  
  /* Register for stack callbacks */
  DmRegister(watchDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, watchDmCback);
  AttRegister(watchAttCback);
  AttConnRegister(AppServerConnCback);  
  AttsCccRegister(WATCH_NUM_CCC_IDX, (attsCccSet_t *) watchCccSet, watchCccCback);
  
  /* Register for app framework button callbacks */
  AppUiBtnRegister(watchBtnCback);

  /* Register for app framework discovery callbacks */
  AppDiscRegister(watchDiscCback);
  
  /* Initialize attribute server database */
  SvcCoreAddGroup();

  /* Reset the device */
  DmDevReset();  
}
