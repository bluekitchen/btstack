/*************************************************************************************************/
/*!
 *  \file   app_master.c
 *
 *  \brief  Application framework module for master.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
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

#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_timer.h"
#include "wsf_trace.h"
#include "wsf_assert.h"
#include "dm_api.h"
#include "app_api.h"
#include "app_main.h"
#include "app_cfg.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Constant used in the address type indicating value not present */
#define APP_ADDR_NONE             0xFF

/**************************************************************************************************
  Data Types
**************************************************************************************************/

typedef struct
{
  appDevInfo_t  scanResults[APP_SCAN_RESULT_MAX]; /*! Scan result storage */
  uint8_t       numScanResults;                   /*! Number of scan results */
} appMasterCb_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

static appMasterCb_t appMasterCb;

/*************************************************************************************************/
/*!
 *  \fn     appMasterInitiateSec
 *        
 *  \brief  Initiate security
 *
 *  \param  connId           Connection ID.
 *  \param  initiatePairing  TRUE to initiate pairing.
 *  \param  pCb              Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterInitiateSec(dmConnId_t connId, bool_t initiatePairing, appConnCb_t *pCb)
{
  uint8_t     rKeyDist;
  uint8_t     secLevel;
  dmSecKey_t  *pKey;

  /* if we have an LTK for peer device */
  if ((pCb->dbHdl != APP_DB_HDL_NONE) &&
      ((pKey = AppDbGetKey(pCb->dbHdl, DM_KEY_PEER_LTK, &secLevel)) != NULL))
  {
    pCb->bondByLtk = TRUE;
    pCb->initiatingSec = TRUE;
          
    /* encrypt with LTK */
    DmSecEncryptReq(connId, secLevel, &pKey->ltk);
  }
  /* no key; initiate pairing only if requested */
  else if (initiatePairing)
  {
    /* store bonding state */
    pCb->bondByPairing = (pAppSecCfg->auth & DM_AUTH_BOND_FLAG) == DM_AUTH_BOND_FLAG;
    
    /* if bonding and no device record */
    if (pCb->bondByPairing && pCb->dbHdl == APP_DB_HDL_NONE)
    {
      /* create a device record if none exists */      
      pCb->dbHdl = AppDbNewRecord(DmConnPeerAddrType(connId), DmConnPeerAddr(connId));
    }

    /* initialize stored keys */
    pCb->rcvdKeys = 0;

    /* if peer is using random address request IRK */
    rKeyDist = pAppSecCfg->rKeyDist;
    if (DmConnPeerAddrType(connId) == DM_ADDR_RANDOM)
    {
      rKeyDist |= DM_KEY_DIST_IRK;
    }
    
    pCb->initiatingSec = TRUE;
    
    /* initiate pairing */
    DmSecPairReq(connId, pAppSecCfg->oob, pAppSecCfg->auth, pAppSecCfg->iKeyDist, rKeyDist);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appScanResultsClear
 *        
 *  \brief  Clear all scan results.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appScanResultsClear(void)
{
  uint8_t       i;
  appDevInfo_t  *pDev = appMasterCb.scanResults;
  
  appMasterCb.numScanResults = 0;
  for (i = APP_SCAN_RESULT_MAX; i > 0; i--, pDev++)
  {
    pDev->addrType = APP_ADDR_NONE;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appScanResultAdd
 *        
 *  \brief  Add a scan report to the scan result list.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appScanResultAdd(dmEvt_t *pMsg)
{
  uint8_t       i;
  appDevInfo_t  *pDev = appMasterCb.scanResults;
  
  /* see if device is in list already */
  for (i = 0; i < APP_SCAN_RESULT_MAX; i++, pDev++)
  {
    /* if address matches list entry */
    if ((pDev->addrType == pMsg->scanReport.addrType) &&
        BdaCmp(pDev->addr, pMsg->scanReport.addr))
    {
      /* device already exists in list; we are done */
      break;
    }
    /* if entry is free end then of list has been reached */
    else if (pDev->addrType == APP_ADDR_NONE)
    {
      /* add device to list */
      pDev->addrType = pMsg->scanReport.addrType;
      BdaCpy(pDev->addr, pMsg->scanReport.addr);
      appMasterCb.numScanResults++;
      break;
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterScanStart
 *        
 *  \brief  Handle a DM_SCAN_START_IND event.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterScanStart(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* clear current scan results */
  appScanResultsClear();
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterScanStop
 *        
 *  \brief  Handle a DM_SCAN_STOP_IND event.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterScanStop(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  APP_TRACE_INFO1("Scan results: %d", AppScanGetNumResults());
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterScanReport
 *        
 *  \brief  Handle a DM_SCAN_REPORT_IND event.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterScanReport(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* add to scan result list */
  appScanResultAdd(pMsg);
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterConnOpen
 *        
 *  \brief  Handle a DM_CONN_OPEN_IND event.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterConnOpen(dmEvt_t *pMsg, appConnCb_t *pCb)
{

}

/*************************************************************************************************/
/*!
 *  \fn     appMasterConnClose
 *        
 *  \brief  Handle a DM_CONN_CLOSE_IND event.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterConnClose(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* clear connection ID */
  pCb->connId = DM_CONN_ID_NONE;
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterSecConnOpen
 *        
 *  \brief  Perform master security procedures on connection open.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterSecConnOpen(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* initialize state variables */
  pCb->bonded = FALSE;
  pCb->bondByLtk = FALSE;
  pCb->bondByPairing = FALSE;
  pCb->initiatingSec = FALSE;
  
  /* if master initiates security on connection open */
  appMasterInitiateSec((dmConnId_t) pMsg->hdr.param, pAppSecCfg->initiateSec, pCb);
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterSecConnClose
 *        
 *  \brief  Perform security procedures on connection close.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterSecConnClose(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* if a device record was created check if it is valid */
  if (pCb->dbHdl != APP_DB_HDL_NONE)
  {
    AppDbCheckValidRecord(pCb->dbHdl);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterSecSlaveReq
 *        
 *  \brief  Handle a slave security request.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterSecSlaveReq(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* if master is not initiating security and not already secure */
  if (!pAppSecCfg->initiateSec && !pCb->initiatingSec &&
      (DmConnSecLevel((dmConnId_t) pMsg->hdr.param) == DM_SEC_LEVEL_NONE))
  {
    appMasterInitiateSec((dmConnId_t) pMsg->hdr.param, TRUE, pCb);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterSecStoreKey
 *        
 *  \brief  Store security key.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterSecStoreKey(dmEvt_t *pMsg, appConnCb_t *pCb)
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
 *  \fn     appMasterSecPairCmpl
 *        
 *  \brief  Handle pairing complete.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterSecPairCmpl(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* if bonding */
  if (pMsg->pairCmpl.auth & DM_AUTH_BOND_FLAG)
  {
    /* set bonded state */
    pCb->bonded = TRUE;

    /* validate record and received keys */
    if (pCb->dbHdl != APP_DB_HDL_NONE)
    {
      AppDbValidateRecord(pCb->dbHdl, pCb->rcvdKeys);
    }    
  }
  
  pCb->initiatingSec = FALSE;
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterSecPairFailed
 *        
 *  \brief  Handle pairing failed
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterSecPairFailed(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  pCb->initiatingSec = FALSE;
  return;
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterSecEncryptInd
 *        
 *  \brief  Handle encryption indication
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *  \param  pCb     Connection control block.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appMasterSecEncryptInd(dmEvt_t *pMsg, appConnCb_t *pCb)
{
  /* check if bonding state should be set */
  if (pCb->bondByLtk && pMsg->encryptInd.usingLtk)
  {
    pCb->bonded = TRUE;
    pCb->bondByLtk = FALSE;
    pCb->initiatingSec = FALSE;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appMasterProcMsg
 *        
 *  \brief  Process app framework messages for a master.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void appMasterProcMsg(wsfMsgHdr_t *pMsg)
{
  switch(pMsg->event)
  {
    case APP_CONN_UPDATE_TIMEOUT_IND:
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppMasterInit
 *        
 *  \brief  Initialize app framework master.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppMasterInit(void)
{
  /* set up callback from main */
  appCb.masterCback = appMasterProcMsg;
}

/*************************************************************************************************/
/*!
 *  \fn     AppMasterProcDmMsg
 *        
 *  \brief  Process connection-related DM messages for a master.  This function should be called
 *          from the application's event handler.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppMasterProcDmMsg(dmEvt_t *pMsg)
{
  appConnCb_t *pCb = NULL;
  
  /* look up app connection control block from DM connection ID */
  if (pMsg->hdr.event == DM_CONN_OPEN_IND ||
      pMsg->hdr.event == DM_CONN_CLOSE_IND)
  {
    pCb = &appConnCb[pMsg->hdr.param - 1];
  }
  
  switch(pMsg->hdr.event)
  {
    case DM_SCAN_START_IND:
      appMasterScanStart(pMsg, pCb);
      break;  

    case DM_SCAN_STOP_IND:
      appMasterScanStop(pMsg, pCb);
      break;  

    case DM_SCAN_REPORT_IND:
      appMasterScanReport(pMsg, pCb);
      break;  

    case DM_CONN_OPEN_IND:
      appMasterConnOpen(pMsg, pCb);
      break;

    case DM_CONN_CLOSE_IND:
      appMasterConnClose(pMsg, pCb);
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppMasterSecProcDmMsg
 *        
 *  \brief  Process security-related DM messages for a master.  This function should be called
 *          from the application's event handler.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppMasterSecProcDmMsg(dmEvt_t *pMsg)
{
  appConnCb_t *pCb;
  
  /* look up app connection control block from DM connection ID */
  pCb = &appConnCb[pMsg->hdr.param - 1];

  switch(pMsg->hdr.event)
  {
    case DM_CONN_OPEN_IND:
      appMasterSecConnOpen(pMsg, pCb);
      break;

    case DM_CONN_CLOSE_IND:
      appMasterSecConnClose(pMsg, pCb);
      break;

    case DM_SEC_PAIR_CMPL_IND:
      appMasterSecPairCmpl(pMsg, pCb);
      break;
      
    case DM_SEC_PAIR_FAIL_IND:
      appMasterSecPairFailed(pMsg, pCb);
      break;

    case DM_SEC_ENCRYPT_IND:
      appMasterSecEncryptInd(pMsg, pCb);
      break;
      
    case DM_SEC_ENCRYPT_FAIL_IND:
      break;

    case DM_SEC_KEY_IND:
      appMasterSecStoreKey(pMsg, pCb);
      break;

    case DM_SEC_SLAVE_REQ_IND:
      appMasterSecSlaveReq(pMsg, pCb);
      break;

    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppScanStart
 *        
 *  \brief  Start scanning.   A scan is performed using the given discoverability mode,
 *          scan type, and duration.
 *
 *  \param  mode      Discoverability mode.
 *  \param  scanType  Scan type.
 *  \param  duration  The scan duration, in milliseconds.  If set to zero, scanning will
 *                    continue until AppScanStop() is called.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppScanStart(uint8_t mode, uint8_t scanType, uint16_t duration)
{
  DmScanSetInterval(pAppMasterCfg->scanInterval, pAppMasterCfg->scanWindow);
  
  DmScanStart(mode, scanType, TRUE, duration);
}

/*************************************************************************************************/
/*!
 *  \fn     AppScanStop
 *        
 *  \brief  Stop scanning.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppScanStop(void)
{
  DmScanStop();
}

/*************************************************************************************************/
/*!
 *  \fn     AppScanGetResult
 *        
 *  \brief  Get a stored scan result from the scan result list.
 *
 *  \param  idx     Index of result in scan result list.
 *
 *  \return Pointer to scan result device info or NULL if index contains no result.
 */
/*************************************************************************************************/
appDevInfo_t *AppScanGetResult(uint8_t idx)
{
  if (idx < APP_SCAN_RESULT_MAX && appMasterCb.scanResults[idx].addrType != APP_ADDR_NONE)
  {
    return &appMasterCb.scanResults[idx];
  }
  else
  {
    return NULL;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppScanGetNumResults
 *        
 *  \brief  Get the number of stored scan results.
 *
 *  \return Number of stored scan results.
 */
/*************************************************************************************************/
uint8_t AppScanGetNumResults(void)
{
  return appMasterCb.numScanResults;
}

/*************************************************************************************************/
/*!
 *  \fn     AppConnOpen
 *        
 *  \brief  Open a connection to a peer device with the given address.
 *
 *  \param  addrType  Address type.
 *  \param  pAddr     Peer device address.
 *
 *  \return Connection identifier.
 */
/*************************************************************************************************/
dmConnId_t AppConnOpen(uint8_t addrType, uint8_t *pAddr)
{
  dmConnId_t  connId;
  appConnCb_t *pCb;
  
  /* open connection */
  connId = DmConnOpen(DM_CLIENT_ID_APP, addrType, pAddr);
  
  /* set up conn. control block */
  pCb = &appConnCb[connId - 1];
  pCb->connId = connId;
  
  /* set device db handle */
  pCb->dbHdl = AppDbFindByAddr(addrType, pAddr);
  
  return connId;
}

/*************************************************************************************************/
/*!
 *  \fn     AppMasterSecurityReq
 *        
 *  \brief  Initiate security as a master device.  If there is a stored encryption key
 *          for the peer device this function will initiate encryption, otherwise it will
 *          initiate pairing.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppMasterSecurityReq(dmConnId_t connId)
{
  appConnCb_t *pCb;

  WSF_ASSERT((connId > 0) && (connId <= DM_CONN_MAX));

  /* look up app connection control block from DM connection ID */
  pCb = &appConnCb[connId - 1];

  /* if master is not initiating security and not already secure */
  if (!pAppSecCfg->initiateSec && !pCb->initiatingSec &&
      (DmConnSecLevel(connId) == DM_SEC_LEVEL_NONE))
  {
    appMasterInitiateSec(connId, TRUE, pCb);
  }
}
