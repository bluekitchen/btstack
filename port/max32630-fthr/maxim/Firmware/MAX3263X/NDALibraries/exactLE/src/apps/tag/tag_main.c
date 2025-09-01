/*************************************************************************************************/
/*!
 *  \file   tag_main.c
 *
 *  \brief  Tag sample application for the following profiles:
 *            Proximity profile reporter
 *            Find Me profile target
 *            Find Me profile locator
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
#include "app_cfg.h"
#include "app_api.h"
#include "app_db.h"
#include "app_ui.h"
#include "svc_core.h"
#include "svc_px.h"
#include "svc_ch.h"
#include "gatt_api.h"
#include "fmpl_api.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! proximity reporter control block */
static struct
{
  uint16_t          hdlList[APP_DB_HDL_LIST_LEN];
  wsfHandlerId_t    handlerId;
  uint8_t           discState;
} tagCb;

/**************************************************************************************************
  Configurable Parameters
**************************************************************************************************/

/*! configurable parameters for slave */
static const appSlaveCfg_t tagSlaveCfg =
{
  {15000, 45000,     0},                  /*! Advertising durations in ms */
  {   56,   640,  1824}                   /*! Advertising intervals in 0.625 ms units */
};

/*! configurable parameters for security */
static const appSecCfg_t tagSecCfg =
{
  DM_AUTH_BOND_FLAG,                      /*! Authentication and bonding flags */
  0,                                      /*! Initiator key distribution flags */
  DM_KEY_DIST_LTK,                        /*! Responder key distribution flags */
  FALSE,                                  /*! TRUE if Out-of-band pairing data is present */
  FALSE                                   /*! TRUE to initiate security upon connection */
};

/*! configurable parameters for connection parameter update */
static const appUpdateCfg_t tagUpdateCfg =
{
  6000,                                   /*! Connection idle period in ms before attempting
                                              connection parameter update; set to zero to disable */
  640,                                    /*! Minimum connection interval in 1.25ms units */
  800,                                    /*! Maximum connection interval in 1.25ms units */
  0,                                      /*! Connection latency */
  600,                                    /*! Supervision timeout in 10ms units */
  5                                       /*! Number of update attempts before giving up */
};

/*! Configurable parameters for service and characteristic discovery */
static const appDiscCfg_t tagDiscCfg =
{
  FALSE                                   /*! TRUE to wait for a secure connection before initiating discovery */
};

/**************************************************************************************************
  Advertising Data
**************************************************************************************************/

/*! advertising data, discoverable mode */
static const uint8_t tagAdvDataDisc[] =
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

/*! scan data */
static const uint8_t tagScanData[] =
{
  /*! service UUID list */
  7,                                      /*! length */
  DM_ADV_TYPE_16_UUID,                    /*! AD type */
  UINT16_TO_BYTES(ATT_UUID_LINK_LOSS_SERVICE),
  UINT16_TO_BYTES(ATT_UUID_IMMEDIATE_ALERT_SERVICE),
  UINT16_TO_BYTES(ATT_UUID_TX_POWER_SERVICE)
};

/**************************************************************************************************
  ATT Client Discovery Data
**************************************************************************************************/

/*! Discovery states:  enumeration of services to be discovered */
enum
{
  TAG_DISC_IAS_SVC,       /* Immediate Alert service */
  TAG_DISC_GATT_SVC,      /* GATT service */
  TAG_DISC_SVC_MAX        /* Discovery complete */
};

/*! the Client handle list, tagCb.hdlList[], is set as follows:
 *
 *  ------------------------------- <- TAG_DISC_IAS_START
 *  | IAS alert level handle      |
 *  ------------------------------- <- TAG_DISC_GATT_START
 *  | GATT svc changed handle     |
 *  -------------------------------
 *  | GATT svc changed ccc handle |
 *  -------------------------------
 */

/*! Start of each service's handles in the the handle list */
#define TAG_DISC_IAS_START        0
#define TAG_DISC_GATT_START       (TAG_DISC_IAS_START + FMPL_IAS_HDL_LIST_LEN)
#define TAG_DISC_HDL_LIST_LEN     (TAG_DISC_GATT_START + GATT_HDL_LIST_LEN)

/*! Pointers into handle list for each service's handles */
static uint16_t *pTagIasHdlList = &tagCb.hdlList[TAG_DISC_IAS_START];
static uint16_t *pTagGattHdlList = &tagCb.hdlList[TAG_DISC_GATT_START];

/* sanity check:  make sure handle list length is <= app db handle list length */
WSF_CT_ASSERT(TAG_DISC_HDL_LIST_LEN <= APP_DB_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Client Data
**************************************************************************************************/

/* Default value for GATT service changed ccc descriptor */
static const uint8_t tagGattScCccVal[] = {UINT16_TO_BYTES(ATT_CLIENT_CFG_INDICATE)};

/* List of characteristics to configure */
static const attcDiscCfg_t tagDiscCfgList[] = 
{
  /* Write:  GATT service changed ccc descriptor */
  {tagGattScCccVal, sizeof(tagGattScCccVal), (GATT_SC_CCC_HDL_IDX + TAG_DISC_GATT_START)}
};

/* Characteristic configuration list length */
#define TAG_DISC_CFG_LIST_LEN   (sizeof(tagDiscCfgList) / sizeof(attcDiscCfg_t))

/* sanity check:  make sure configuration list length is <= handle list length */
WSF_CT_ASSERT(TAG_DISC_CFG_LIST_LEN <= TAG_DISC_HDL_LIST_LEN);

/**************************************************************************************************
  ATT Server Data
**************************************************************************************************/

/*! enumeration of client characteristic configuration descriptors used in local ATT server */
enum
{
  TAG_GATT_SC_CCC_IDX,        /*! GATT service, service changed characteristic */
  TAG_NUM_CCC_IDX             /*! Number of ccc's */
};

/*! client characteristic configuration descriptors settings, indexed by ccc enumeration */
static const attsCccSet_t tagCccSet[TAG_NUM_CCC_IDX] =
{
  /* cccd handle         value range                 security level */
  {GATT_SC_CH_CCC_HDL,   ATT_CLIENT_CFG_INDICATE,    DM_SEC_LEVEL_ENC}    /* TAG_GATT_SC_CCC_IDX */
};

/*************************************************************************************************/
/*!
 *  \fn     tagAlert
 *        
 *  \brief  Perform an alert based on the alert characteristic value
 *
 *  \param  alert  alert characteristic value
 *
 *  \return None.
 */
/*************************************************************************************************/
static void tagAlert(uint8_t alert)
{
  /* perform alert according to setting of alert alert */
  if (alert == CH_ALERT_LVL_NONE)
  {
    AppUiAction(APP_UI_ALERT_CANCEL);
  }
  else if (alert == CH_ALERT_LVL_MILD)
  {
    AppUiAction(APP_UI_ALERT_LOW);
  }
  else if (alert == CH_ALERT_LVL_HIGH)
  {
    AppUiAction(APP_UI_ALERT_HIGH);
  }  
}

/*************************************************************************************************/
/*!
 *  \fn     tagDmCback
 *        
 *  \brief  Application DM callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void tagDmCback(dmEvt_t *pDmEvt)
{
  dmEvt_t *pMsg;
  
  if ((pMsg = WsfMsgAlloc(sizeof(dmEvt_t))) != NULL)
  {
    memcpy(pMsg, pDmEvt, sizeof(dmEvt_t));
    WsfMsgSend(tagCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     tagAttCback
 *        
 *  \brief  Application ATT callback.
 *
 *  \param  pEvt    ATT callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void tagAttCback(attEvt_t *pEvt)
{
  attEvt_t *pMsg;
  
  if ((pMsg = WsfMsgAlloc(sizeof(attEvt_t) + pEvt->valueLen)) != NULL)
  {
    memcpy(pMsg, pEvt, sizeof(attEvt_t));
    pMsg->pValue = (uint8_t *) (pMsg + 1);
    memcpy(pMsg->pValue, pEvt->pValue, pEvt->valueLen);
    WsfMsgSend(tagCb.handlerId, pMsg);
  }
  
  return;
}

/*************************************************************************************************/
/*!
 *  \fn     tagCccCback
 *        
 *  \brief  Application ATTS client characteristic configuration callback.
 *
 *  \param  pDmEvt  DM callback event
 *
 *  \return None.
 */
/*************************************************************************************************/
static void tagCccCback(attsCccEvt_t *pEvt)
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
    WsfMsgSend(tagCb.handlerId, pMsg);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     tagIasWriteCback
 *        
 *  \brief  ATTS write callback for Immediate Alert service.
 *
 *  \return ATT status.
 */
/*************************************************************************************************/
static uint8_t tagIasWriteCback(dmConnId_t connId, uint16_t handle, uint8_t operation,
                                uint16_t offset, uint16_t len, uint8_t *pValue,
                                attsAttr_t *pAttr)
{
  ATT_TRACE_INFO3("tagIasWriteCback connId:%d handle:0x%04x op:0x%02x",
                  connId, handle, operation);
  ATT_TRACE_INFO2("                 offset:0x%04x len:0x%04x", offset, len);

  tagAlert(*pValue);
  
  return ATT_SUCCESS;
}


/*************************************************************************************************/
/*!
 *  \fn     tagClose
 *        
 *  \brief  Perform UI actions on connection close.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void tagClose(dmEvt_t *pMsg)
{
  uint8_t   *pVal;
  uint16_t  len;
  
  /* perform alert according to setting of link loss alert */
  if (AttsGetAttr(LLS_AL_HDL, &len, &pVal) == ATT_SUCCESS)
  {
    if (*pVal == CH_ALERT_LVL_MILD || *pVal == CH_ALERT_LVL_HIGH)
    {
      tagAlert(*pVal);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     tagSetup
 *        
 *  \brief  Set up advertising and other procedures that need to be performed after
 *          device reset.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void tagSetup(dmEvt_t *pMsg)
{
  /* set advertising and scan response data for discoverable mode */
  AppAdvSetData(APP_ADV_DATA_DISCOVERABLE, sizeof(tagAdvDataDisc), (uint8_t *) tagAdvDataDisc);
  AppAdvSetData(APP_SCAN_DATA_DISCOVERABLE, sizeof(tagScanData), (uint8_t *) tagScanData);

  /* set advertising and scan response data for connectable mode */
  AppAdvSetData(APP_ADV_DATA_CONNECTABLE, 0, NULL);
  AppAdvSetData(APP_SCAN_DATA_CONNECTABLE, sizeof(tagScanData), (uint8_t *) tagScanData);
  
  /* start advertising; automatically set connectable/discoverable mode and bondable mode */
  AppAdvStart(APP_MODE_AUTO_INIT);
}

/*************************************************************************************************/
/*!
 *  \fn     tagValueUpdate
 *        
 *  \brief  Process a received ATT indication.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void tagValueUpdate(attEvt_t *pMsg)
{
  if (pMsg->hdr.status == ATT_SUCCESS)
  {
    /* determine which profile the handle belongs to */

    /* GATT */
    if (GattValueUpdate(pTagGattHdlList, pMsg) == ATT_SUCCESS)    
    {
      return;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     tagBtnCback
 *        
 *  \brief  Button press callback.
 *
 *  \param  btn    Button press.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void tagBtnCback(uint8_t btn)
{
  dmConnId_t  connId;
  
  /* button actions when connected */
  if ((connId = AppConnIsOpen()) != DM_CONN_ID_NONE)
  {
    switch (btn)
    {
      case APP_UI_BTN_1_SHORT:
        /* send immediate alert, high */
        FmplSendAlert(connId, pTagIasHdlList[FMPL_IAS_AL_HDL_IDX], CH_ALERT_LVL_HIGH);
        break;
        
      case APP_UI_BTN_1_MED:
        /* send immediate alert, none */
        FmplSendAlert(connId, pTagIasHdlList[FMPL_IAS_AL_HDL_IDX], CH_ALERT_LVL_NONE);
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
 *  \fn     tagDiscCback
 *        
 *  \brief  Discovery callback.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    Service or configuration status.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void tagDiscCback(dmConnId_t connId, uint8_t status)
{
  switch(status)
  {
    case APP_DISC_INIT:
      /* set handle list when initialization requested */
      AppDiscSetHdlList(connId, TAG_DISC_HDL_LIST_LEN, tagCb.hdlList);
      break;

    case APP_DISC_SEC_REQUIRED:
      /* request security */
      AppSlaveSecurityReq(connId);
      break;
            
    case APP_DISC_START:
      /* initialize discovery state */
      tagCb.discState = TAG_DISC_IAS_SVC;
      
      /* discover immediate alert service */
      FmplIasDiscover(connId, pTagIasHdlList);
      break;
      
    case APP_DISC_FAILED:
      /* if immediate alert service not found */
      if (tagCb.discState == TAG_DISC_IAS_SVC)
      {
        /* discovery failed */
        AppDiscComplete(connId, APP_DISC_FAILED);
        break;
      }
      /* else fall through to continue discovery */
      
    case APP_DISC_CMPL:
      /* next discovery state */
      tagCb.discState++;
      
      if (tagCb.discState == TAG_DISC_GATT_SVC)
      {
        /* discover GATT service */
        GattDiscover(connId, pTagGattHdlList);
      }
      else
      {    
        /* discovery complete */
        AppDiscComplete(connId, APP_DISC_CMPL);

        /* start configuration */
        AppDiscConfigure(connId, APP_DISC_CFG_START, TAG_DISC_CFG_LIST_LEN,
                         (attcDiscCfg_t *) tagDiscCfgList, TAG_DISC_HDL_LIST_LEN, tagCb.hdlList);
      }
      break;
      
    case APP_DISC_CFG_START:
      /* start configuration */
      AppDiscConfigure(connId, APP_DISC_CFG_START, TAG_DISC_CFG_LIST_LEN,
                       (attcDiscCfg_t *) tagDiscCfgList, TAG_DISC_HDL_LIST_LEN, tagCb.hdlList);
      break;
      
    case APP_DISC_CFG_CMPL: 
      AppDiscComplete(connId, APP_DISC_CFG_CMPL);
      break;

    case APP_DISC_CFG_CONN_START:
      /* no connection setup configuration for this application */
      break;
      
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     tagProcMsg
 *        
 *  \brief  Process messages from the event handler.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void tagProcMsg(dmEvt_t *pMsg)
{
  uint8_t uiEvent = APP_UI_NONE;
  
  switch(pMsg->hdr.event)
  {
    case ATTC_HANDLE_VALUE_IND:
      tagValueUpdate((attEvt_t *) pMsg);
      break;
  
    case DM_RESET_CMPL_IND:
      tagSetup(pMsg);
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
      tagClose(pMsg);
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
 *  \fn     TagHandlerInit
 *        
 *  \brief  Proximity reporter handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
void TagHandlerInit(wsfHandlerId_t handlerId)
{
  APP_TRACE_INFO0("TagHandlerInit");
  
  /* store handler ID */
  tagCb.handlerId = handlerId;
  
  /* Set configuration pointers */
  pAppSlaveCfg = (appSlaveCfg_t *) &tagSlaveCfg;
  pAppSecCfg = (appSecCfg_t *) &tagSecCfg;
  pAppUpdateCfg = (appUpdateCfg_t *) &tagUpdateCfg;
  pAppDiscCfg = (appDiscCfg_t *) &tagDiscCfg;

  /* Initialize application framework */
  AppSlaveInit();
  AppDiscInit();
}

/*************************************************************************************************/
/*!
 *  \fn     TagHandler
 *        
 *  \brief  WSF event handler for proximity reporter.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void TagHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg)
{ 
  if (pMsg != NULL)
  {
    APP_TRACE_INFO1("Tag got evt %d", pMsg->event);

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
    tagProcMsg((dmEvt_t *) pMsg);    
  }
}

/*************************************************************************************************/
/*!
 *  \fn     TagStart
 *        
 *  \brief  Start the proximity profile reporter application.
 *
 *  \return None.
 */
/*************************************************************************************************/
void TagStart(void)
{  
  /* Register for stack callbacks */
  DmRegister(tagDmCback);
  DmConnRegister(DM_CLIENT_ID_APP, tagDmCback);
  AttRegister(tagAttCback);
  AttConnRegister(AppServerConnCback);  
  AttsCccRegister(TAG_NUM_CCC_IDX, (attsCccSet_t *) tagCccSet, tagCccCback);
  
  /* Register for app framework button callbacks */
  AppUiBtnRegister(tagBtnCback);

  /* Register for app framework discovery callbacks */
  AppDiscRegister(tagDiscCback);
  
  /* Initialize attribute server database */
  SvcCoreAddGroup();
  SvcPxCbackRegister(NULL, tagIasWriteCback);
  SvcPxAddGroup();

  /* Reset the device */
  DmDevReset();  
}
