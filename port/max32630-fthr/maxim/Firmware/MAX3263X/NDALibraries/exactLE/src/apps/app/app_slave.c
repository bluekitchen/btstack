/*************************************************************************************************/
/*!
 *  \file   app_slave.c
 *
 *  \brief  Application framework module for slave.
 *
 *          $Date: 2015-08-25 13:59:48 -0500 (Tue, 25 Aug 2015) $
 *          $Revision: 18761 $
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
#include "wsf_msg.h"
#include "wsf_timer.h"
#include "dm_api.h"
#include "app_api.h"
#include "app_main.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Convert data storage location to discoverable/connectable mode */
#define APP_LOC_2_MODE(loc)       ((loc) / 2)

/* Convert data storage location to DM advertising data location (adv or scan data) */
#define APP_LOC_2_DM_LOC(loc)     ((loc) & 1)

/* Convert mode advertising data location */
#define APP_MODE_2_ADV_LOC(mode)  (((mode) * 2) + DM_DATA_LOC_ADV)

/* Convert mode scan data location */
#define APP_MODE_2_SCAN_LOC(mode) (((mode) * 2) + DM_DATA_LOC_SCAN)

/**************************************************************************************************
  Data Types
**************************************************************************************************/

typedef struct
{
  wsfTimer_t updateTimer;                           /*! Connection parameter update timer */
  uint8_t    *pAdvData[APP_NUM_DATA_LOCATIONS];     /*! Advertising data pointers */
  uint8_t    advDataLen[APP_NUM_DATA_LOCATIONS];    /*! Advertising data lengths */
  bool_t     bondable;                              /*! TRUE if in bondable mode */
  bool_t     setConnectable;                        /*! TRUE if switching to connectable mode */
  bool_t     connWasIdle;                           /*! TRUE if connection was idle at last check */
  bool_t     advDataSynced;                         /*! TRUE if advertising/scan data is synced */
  uint8_t    advState;                              /*! Advertising state */
  uint8_t    discMode;                              /*! Discoverable/connectable mode */
  uint8_t    attempts;                              /*! Connection parameter update attempts */
} appSlaveCb_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

static appSlaveCb_t appSlaveCb;

/*************************************************************************************************/
/*!
 *  \fn     appSetAdvScanData
 *        
 *  \brief  Set advertising and scan response data.
 *
 *  \param  mode      Discoverable/connectable mode.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSetAdvScanData(uint8_t mode)
{
  uint8_t     advLoc;
  uint8_t     scanLoc;

  /* get advertising/scan data location based on mode */
  advLoc = APP_MODE_2_ADV_LOC(mode);
  scanLoc = APP_MODE_2_SCAN_LOC(mode);

  DmAdvSetData(DM_DATA_LOC_ADV, appSlaveCb.advDataLen[advLoc], appSlaveCb.pAdvData[advLoc]);
  DmAdvSetData(DM_DATA_LOC_SCAN, appSlaveCb.advDataLen[scanLoc], appSlaveCb.pAdvData[scanLoc]);
}

/*************************************************************************************************/
/*!
 *  \fn     appSlaveAdvStart
 *        
 *  \brief  Utility function to start advertising.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSlaveAdvStart(void)
{
  uint16_t interval;

  if (!appSlaveCb.advDataSynced)
  {
    appSlaveCb.advDataSynced = TRUE;
    appSetAdvScanData(appSlaveCb.discMode);
  }
    
  interval = pAppSlaveCfg->advInterval[appSlaveCb.advState];
  
  /* if this advertising state is being used */
  if (interval > 0)
  {
    /* set interval and start advertising */  
    DmAdvSetInterval(interval, interval);
    DmAdvStart(DM_ADV_CONN_UNDIRECT, pAppSlaveCfg->advDuration[appSlaveCb.advState]);     
  }
  /* else done with all advertising states */
  else
  {
    appSlaveCb.advState = APP_ADV_STOPPED;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appSlaveNextAdvState
 *        
 *  \brief  Set the next advertising state.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSlaveNextAdvState(dmEvt_t *pMsg)
{
  /* go to next advertising state */
  appSlaveCb.advState++;
  
  /* if haven't reached stopped state then start advertising */
  if (appSlaveCb.advState < APP_ADV_STOPPED)
  {
    appSlaveAdvStart();
  } 
}

/*************************************************************************************************/
/*!
 *  \fn     appConnUpdateTimerStart
 *        
 *  \brief  Start the connection update timer.
 *
 *  \param  connId    DM connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appConnUpdateTimerStart(dmConnId_t connId)
{
  appSlaveCb.updateTimer.handlerId = appHandlerId;
  appSlaveCb.updateTimer.msg.event = APP_CONN_UPDATE_TIMEOUT_IND;
  appSlaveCb.updateTimer.msg.param = connId;
  WsfTimerStartMs(&appSlaveCb.updateTimer, pAppUpdateCfg->idlePeriod);
}

/*************************************************************************************************/
/*!
 *  \fn     appConnUpdateTimerStop
 *        
 *  \brief  Stop the connection update timer.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appConnUpdateTimerStop(void)
{
  /* stop connection update timer */
  if (pAppUpdateCfg->idlePeriod != 0)
  {
    WsfTimerStop(&appSlaveCb.updateTimer);
  }  
}

/*************************************************************************************************/
/*!
 *  \fn     appSlaveConnOpen
 *        
 *  \brief  Handle a DM_CONN_OPEN_IND event.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSlaveConnOpen(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* advertising is stopped once a connection is opened */
  appSlaveCb.advState = APP_ADV_STOPPED;
  
  /* store connection ID */
  pCb->connId = (dmConnId_t) pMsg->hdr.param;
  
  /* check if we should do connection parameter update */
  if ((pAppUpdateCfg->idlePeriod != 0) &&
      ((pMsg->connOpen.connInterval < pAppUpdateCfg->connIntervalMin) ||
       (pMsg->connOpen.connInterval > pAppUpdateCfg->connIntervalMax) ||
       (pMsg->connOpen.connLatency != pAppUpdateCfg->connLatency) ||
       (pMsg->connOpen.supTimeout != pAppUpdateCfg->supTimeout)))
  {
    appSlaveCb.connWasIdle = FALSE;
    appSlaveCb.attempts = 0;
    appConnUpdateTimerStart(pCb->connId);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appSlaveConnClose
 *        
 *  \brief  Handle a DM_CONN_CLOSE_IND event.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSlaveConnClose(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* stop connection update timer */
  appConnUpdateTimerStop();
  
  /* clear connection ID */
  pCb->connId = DM_CONN_ID_NONE;

  /* if switching to connectable mode then set it up */
  if (appSlaveCb.setConnectable)
  {
    appSlaveCb.setConnectable = FALSE;
    appSlaveCb.discMode = APP_MODE_CONNECTABLE;
    
    /* force update of advertising data */
    appSlaveCb.advDataSynced = FALSE;
  }
  
  /* set advertising state */
  appSlaveCb.advState = APP_ADV_STATE1;
  
  /* start advertising */
  appSlaveAdvStart();
}

/*************************************************************************************************/
/*!
 *  \fn     appSlaveConnUpdate
 *        
 *  \brief  Handle a DM_CONN_UPDATE_IND event.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSlaveConnUpdate(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  if (pAppUpdateCfg->idlePeriod != 0)
  {
    /* if successful */
    if (pMsg->hdr.status == HCI_SUCCESS)
    {
      /* stop connection update timer */
      appConnUpdateTimerStop();
    }
    /* else if update failed and still attempting to do update */
    else if (appSlaveCb.attempts < pAppUpdateCfg->maxAttempts)
    {
      /* start timer and try again */
      appConnUpdateTimerStart(pCb->connId);
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appSlaveSecConnOpen
 *        
 *  \brief  Perform slave security procedures on connection open.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSlaveSecConnOpen(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* initialize state variables */
  pCb->bonded = FALSE;
  pCb->bondByLtk = FALSE;
  pCb->bondByPairing = FALSE;
    
  /* find record for peer device */
  pCb->dbHdl = AppDbFindByAddr(pMsg->connOpen.addrType, pMsg->connOpen.peerAddr);

  /* send slave security request if configured to do so */
  if (pAppSecCfg->initiateSec && AppDbCheckBonded())
  {
    DmSecSlaveReq((dmConnId_t) pMsg->hdr.param, pAppSecCfg->auth);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appSecConnClose
 *        
 *  \brief  Perform security procedures on connection close.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSecConnClose(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* if a device record was created check if it is valid */
  if (pCb->dbHdl != APP_DB_HDL_NONE)
  {
    AppDbCheckValidRecord(pCb->dbHdl);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appSecPairInd
 *        
 *  \brief  Handle pairing indication from peer.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSecPairInd(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  uint8_t iKeyDist;
  uint8_t rKeyDist;
  
  /* if in bondable mode or if peer is not requesting bonding
   * or if already bonded with this device and link is encrypted
   */
  if (appSlaveCb.bondable ||
      ((pMsg->pairInd.auth & DM_AUTH_BOND_FLAG) != DM_AUTH_BOND_FLAG) ||
      (pCb->bonded && (DmConnSecLevel(pCb->connId) > DM_SEC_LEVEL_NONE)))
  {
    /* store bonding state:  if peer is requesting bonding and we want bonding */
    pCb->bondByPairing = (pMsg->pairInd.auth & pAppSecCfg->auth & DM_AUTH_BOND_FLAG) == DM_AUTH_BOND_FLAG;
    
    /* if bonding and no device record */
    if (pCb->bondByPairing && pCb->dbHdl == APP_DB_HDL_NONE)
    {
      /* create a device record if none exists */
      pCb->dbHdl = AppDbNewRecord(DmConnPeerAddrType(pCb->connId), DmConnPeerAddr(pCb->connId));
    }

    /* initialize stored keys */
    pCb->rcvdKeys = 0;

    /* initialize key distribution */
    rKeyDist = pAppSecCfg->rKeyDist;
    iKeyDist = pAppSecCfg->iKeyDist;
    
    /* if peer is using random address request IRK */
    if (DmConnPeerAddrType(pCb->connId) == DM_ADDR_RANDOM)
    {
      iKeyDist = DM_KEY_DIST_IRK;
    }
    
    /* only distribute keys both sides have agreed to */
    rKeyDist &= pMsg->pairInd.rKeyDist;
    iKeyDist &= pMsg->pairInd.iKeyDist;
    
    /* accept pairing request */
    DmSecPairRsp(pCb->connId, pAppSecCfg->oob, pAppSecCfg->auth, iKeyDist, rKeyDist);
  }
  /* otherwise reject pairing request */
  else
  {
    DmSecCancelReq(pCb->connId, SMP_ERR_PAIRING_NOT_SUP);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appSecStoreKey
 *        
 *  \brief  Store security key.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSecStoreKey(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  if (pCb->bondByPairing && pCb->dbHdl != APP_DB_HDL_NONE)
  {
    /* key was received */
    pCb->rcvdKeys |= pMsg->keyInd.type;
    
    /* store key in record */
    AppDbSetKey(pCb->dbHdl, &pMsg->keyInd);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appSecPairCmpl
 *        
 *  \brief  Handle pairing complete.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSecPairCmpl(dmEvt_t *pMsg, appConnCb_t *pCb)
{

  /* if bonding */
  if (pCb->bondByPairing)
  {
    /* set bonded state */
    pCb->bonded = TRUE;

    /* validate record and received keys */
    if (pCb->dbHdl != APP_DB_HDL_NONE)
    {
      AppDbValidateRecord(pCb->dbHdl, pCb->rcvdKeys);
    }    

    /* if bonded clear bondable mode */
    appSlaveCb.bondable = FALSE;
    
    /* if discoverable switch to connectable mode when connection closes */
    if (appSlaveCb.discMode == APP_MODE_DISCOVERABLE)
    {
      appSlaveCb.setConnectable = TRUE;
    }

    /*  if bonded and device is using static or public address add device to white list */
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appSecPairFailed
 *        
 *  \brief  Handle pairing failed
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSecPairFailed(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  return;
}

/*************************************************************************************************/
/*!
 *  \fn     appSecEncryptInd
 *        
 *  \brief  Handle encryption indication
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSecEncryptInd(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* check if bonding state should be set */
  if (pCb->bondByLtk && pMsg->encryptInd.usingLtk)
  {
    pCb->bonded = TRUE;
    pCb->bondByLtk = FALSE;
  }
}


/*************************************************************************************************/
/*!
 *  \fn     appSecFindLtk
 *        
 *  \brief  Handle LTK request from peer.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSecFindLtk(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  dmSecKey_t  *pKey = NULL;
  uint8_t     secLevel;
  
  /* if device record is not in place */
  if (pCb->dbHdl == APP_DB_HDL_NONE)
  {
    /* find record */
    pCb->dbHdl = AppDbFindByLtkReq(pMsg->ltkReqInd.encDiversifier, pMsg->ltkReqInd.randNum);
  }
  
  /* if there is a record */
  if (pCb->dbHdl != APP_DB_HDL_NONE)
  {
    /* get ltk */
    pKey = AppDbGetKey(pCb->dbHdl, DM_KEY_LOCAL_LTK, &secLevel);
  }
  
  if (pKey != NULL)
  {
    /* if not bonded we need to update bonding state when encrypted */
    pCb->bondByLtk = !pCb->bonded;

    /* we found the key */
    DmSecLtkRsp(pCb->connId, TRUE, secLevel, pKey->ltk.key);
  }
  else
  {
    pCb->bondByLtk = FALSE;
    
    /* key not found */
    DmSecLtkRsp(pCb->connId, FALSE, 0, NULL);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appSlaveConnUpdateTimeout
 *        
 *  \brief  Handle a connection update timeout.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appSlaveConnUpdateTimeout(wsfMsgHdr_t *pMsg, appConnCb_t *pCb)
{
  hciConnSpec_t connSpec;
  bool_t        idle;
  
  /* check if connection is idle */
  idle = (DmConnCheckIdle(pCb->connId) == 0);
  
  /* if connection is idle and was also idle on last check */
  if (idle && appSlaveCb.connWasIdle)
  {
    /* do update */
    appSlaveCb.attempts++;
    connSpec.connIntervalMin = pAppUpdateCfg->connIntervalMin;
    connSpec.connIntervalMax = pAppUpdateCfg->connIntervalMax;
    connSpec.connLatency = pAppUpdateCfg->connLatency;
    connSpec.supTimeout = pAppUpdateCfg->supTimeout;
    DmConnUpdate(pCb->connId, &connSpec);
  }
  else
  {
    appSlaveCb.connWasIdle = idle;
    appConnUpdateTimerStart(pCb->connId);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appSlaveProcMsg
 *        
 *  \brief  Process app framework messages for a slave.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void appSlaveProcMsg(wsfMsgHdr_t *pMsg)
{
  appConnCb_t *pCb;
  
  /* look up app connection control block from DM connection ID */
  pCb = &appConnCb[pMsg->param - 1];

  switch(pMsg->event)
  {
    case APP_CONN_UPDATE_TIMEOUT_IND:
      appSlaveConnUpdateTimeout(pMsg, pCb);
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppSlaveInit
 *        
 *  \brief  Initialize app framework slave.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppSlaveInit(void)
{
  /* initialize advertising state */
  appSlaveCb.advState = APP_ADV_STOPPED;
  
  /* set up callback from main */
  appCb.slaveCback = appSlaveProcMsg;
}

/*************************************************************************************************/
/*!
 *  \fn     AppSlaveProcDmMsg
 *        
 *  \brief  Process connection-related DM messages for a slave.  This function should be called
 *          from the application's event handler.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppSlaveProcDmMsg(dmEvt_t *pMsg)
{
  appConnCb_t *pCb = NULL;
  
  /* look up app connection control block from DM connection ID */
  if (pMsg->hdr.event != DM_ADV_STOP_IND)
  {
    pCb = &appConnCb[pMsg->hdr.param - 1];
  }
  
  switch(pMsg->hdr.event)
  {
    case DM_ADV_STOP_IND:
      appSlaveNextAdvState(pMsg);
      break;
      
    case DM_CONN_OPEN_IND:
      appSlaveConnOpen(pMsg, pCb);
      break;

    case DM_CONN_CLOSE_IND:
      appSlaveConnClose(pMsg, pCb);
      break;

    case DM_CONN_UPDATE_IND:
      appSlaveConnUpdate(pMsg, pCb);
      break;
      
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppSlaveSecProcDmMsg
 *        
 *  \brief  Process security-related DM messages for a slave.  This function should be called
 *          from the application's event handler.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppSlaveSecProcDmMsg(dmEvt_t *pMsg)
{
  appConnCb_t *pCb;
  
  /* look up app connection control block from DM connection ID */
  pCb = &appConnCb[pMsg->hdr.param - 1];

  switch(pMsg->hdr.event)
  {
    case DM_CONN_OPEN_IND:
      appSlaveSecConnOpen(pMsg, pCb);
      break;

    case DM_CONN_CLOSE_IND:
      appSecConnClose(pMsg, pCb);
      break;

    case DM_SEC_PAIR_CMPL_IND:
      appSecPairCmpl(pMsg, pCb);
      break;
      
    case DM_SEC_PAIR_FAIL_IND:
      appSecPairFailed(pMsg, pCb);
      break;

    case DM_SEC_ENCRYPT_IND:
      appSecEncryptInd(pMsg, pCb);
      break;
      
    case DM_SEC_ENCRYPT_FAIL_IND:
      break;

    case DM_SEC_KEY_IND:
      appSecStoreKey(pMsg, pCb);
      break;

    case DM_SEC_PAIR_IND:
      appSecPairInd(pMsg, pCb);
      break;
      
    case DM_SEC_LTK_REQ_IND:
      appSecFindLtk(pMsg, pCb);
      break;
      
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppAdvSetData
 *        
 *  \brief  Set advertising data.
 *
 *  \param  location  Data location.
 *  \param  len       Length of the data.  Maximum length is 31 bytes.
 *  \param  pData     Pointer to the data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppAdvSetData(uint8_t location, uint8_t len, uint8_t *pData)
{
  /* store data for location */
  appSlaveCb.pAdvData[location] = pData;
  appSlaveCb.advDataLen[location] = len;
  
  /* Set the data now if we are in the right mode */
  if ((appSlaveCb.advState != APP_ADV_STOPPED) &&
      (APP_LOC_2_MODE(location) == appSlaveCb.discMode))
  {    
    DmAdvSetData(APP_LOC_2_DM_LOC(location), len, pData);
  }
  /* Otherwise set it when advertising is started or mode changes */
  else
  {
    appSlaveCb.advDataSynced = FALSE;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppAdvStart
 *        
 *  \brief  Start advertising using the parameters for the given mode.
 *
 *  \param  mode      Discoverable/connectable mode.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppAdvStart(uint8_t mode)
{
  uint8_t prevMode = appSlaveCb.discMode;
  
  /* initialize advertising state */
  appSlaveCb.advState = APP_ADV_STATE1;

  /* handle auto init mode */
  if (mode == APP_MODE_AUTO_INIT)
  {
    if (AppDbCheckBonded() == FALSE)
    {
      AppSetBondable(TRUE);    
      appSlaveCb.discMode = APP_MODE_DISCOVERABLE;
    }
    else
    {
      AppSetBondable(FALSE);    
      appSlaveCb.discMode = APP_MODE_CONNECTABLE;
      
      /* init white list with bonded device addresses */
    }    
  }
  else
  {  
    appSlaveCb.discMode = mode;
  }

  /* if mode changed force update of advertising data */
  if (prevMode != appSlaveCb.discMode)
  {
    appSlaveCb.advDataSynced = FALSE;
  }
  
  /* start advertising */
  appSlaveAdvStart();              
}

/*************************************************************************************************/
/*!
 *  \fn     AppAdvStop
 *        
 *  \brief  Stop advertising.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppAdvStop(void)
{
  appSlaveCb.advState = APP_ADV_STOPPED;
  
  DmAdvStop();
}

/*************************************************************************************************/
/*!
 *  \fn     AppAdvSetAdValue
 *        
 *  \brief  Set the value of an advertising data element in the advertising or scan
 *          response data.  If the element already exists in the data then it is replaced
 *          with the new value.  If the element does not exist in the data it is appended
 *          to it, space permitting.
 *
 *          There is special handling for the device name (AD type DM_ADV_TYPE_LOCAL_NAME).
 *          If the name can only fit in the data if it is shortened, the name is shortened
 *          and the AD type is changed to DM_ADV_TYPE_SHORT_NAME.
 *
 *  \param  location  Data location.
 *  \param  adType    Advertising data element type. 
 *  \param  len       Length of the value.  Maximum length is 31 bytes.
 *  \param  pValue    Pointer to the value.
 *
 *  \return TRUE if the element was successfully added to the data, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t AppAdvSetAdValue(uint8_t location, uint8_t adType, uint8_t len, uint8_t *pValue)
{
  uint8_t *pAdvData;
  uint8_t advDataLen;
  bool_t  valueSet;
  
  /* get pointer and length for location */
  pAdvData = appSlaveCb.pAdvData[location];
  advDataLen = appSlaveCb.advDataLen[location];
  
  if (pAdvData != NULL)
  {
    /* set the new element value in the advertising data */
    if (adType == DM_ADV_TYPE_LOCAL_NAME)
    {
      valueSet = DmAdvSetName(len, pValue, &advDataLen, pAdvData);
    }
    else
    {
      valueSet = DmAdvSetAdValue(adType, len, pValue, &advDataLen, pAdvData);
    }
    
    if (valueSet)
    {
      /* if new value set update advertising data */
      AppAdvSetData(location, advDataLen, pAdvData);
      return TRUE;
    }
  }
  return FALSE;
}

/*************************************************************************************************/
/*!
 *  \fn     AppSetBondable
 *        
 *  \brief  Set bondable mode of device.
 *
 *  \param  bondable  TRUE to set device to bondable, FALSE to set to non-bondable.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppSetBondable(bool_t bondable)
{
  appSlaveCb.bondable = bondable;
}

/*************************************************************************************************/
/*!
 *  \fn     AppSlaveSecurityReq
 *        
 *  \brief  Initiate a request for security as a slave device.  This function will send a 
 *          message to the master peer device requesting security.  The master device should
 *          either initiate encryption or pairing.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppSlaveSecurityReq(dmConnId_t connId)
{
  if (DmConnSecLevel(connId) == DM_SEC_LEVEL_NONE)
  {
    DmSecSlaveReq(connId, pAppSecCfg->auth);
  }
}
