/*************************************************************************************************/
/*!
 *  \file   app_disc.c
 *
 *  \brief  Application framework service discovery and configuration.
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

#include <string.h>
#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_buf.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "dm_api.h"
#include "att_api.h"
#include "svc_ch.h"
#include "app_api.h"
#include "app_main.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! "In progress" values */
#define APP_DISC_IDLE               0
#define APP_DISC_IN_PROGRESS        1
#define APP_DISC_CFG_IN_PROGRESS    2

/**************************************************************************************************
  Data Types
**************************************************************************************************/

typedef struct
{
  attcDiscCb_t    *pDiscCb;                 /*! ATT discovery control block */
  uint16_t        *pHdlList;                /*! Handle list */
  appDiscCback_t  cback;                    /*! Discovery callback */
  uint8_t         connCfgStatus;            /*! Connection setup configuration status */
  uint8_t         cmplStatus;               /*! Discovery or configuration complete status */
  uint8_t         hdlListLen;               /*! Handle list length */
  uint8_t         inProgress;               /*! Discovery or configuration in progress */
  bool_t          alreadySecure;            /*! TRUE if connection was already secure */
  bool_t          secRequired;              /*! TRUE if security is required for configuration */
  bool_t          scPending;                /*! TRUE if service changed from peer is pending */
} appDiscCb_t;

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

static appDiscCb_t appDiscCb;

/*************************************************************************************************/
/*!
 *  \fn     appDiscStart
 *        
 *  \brief  Start discovery or configuration
 *
 *  \param  dmConnId_t  Connection ID.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appDiscStart(dmConnId_t connId)
{
  appDbHdl_t  hdl;
  uint8_t     status;
  
  if (appDiscCb.inProgress == APP_DISC_IDLE)
  {
    /* get discovery status */
    if ((hdl = AppDbGetHdl(connId)) != APP_DB_HDL_NONE)
    {
      status = AppDbGetDiscStatus(hdl);
    }
    else
    {
      status = appDiscCb.cmplStatus;
    }
  
    /* if discovery not complete */
    if (status < APP_DISC_CMPL)
    {
      /* notify application to start discovery */
      (*appDiscCb.cback)(connId, APP_DISC_START);      
    }
    /* else if discovery was completed successfully */
    else if (status != APP_DISC_FAILED)
    {
      /* get stored handle list if present */
      if (hdl != APP_DB_HDL_NONE && appDiscCb.pHdlList != NULL)
      {
        memcpy(appDiscCb.pHdlList, AppDbGetHdlList(hdl), (appDiscCb.hdlListLen * sizeof(uint16_t)));
      }

      /* if configuration not complete */
      if (status < APP_DISC_CFG_CMPL)
      {
        /* notify application to start configuration */
        (*appDiscCb.cback)(connId, APP_DISC_CFG_START);          
      }
      /* else if configuration complete start connection setup configuration */
      else if (status == APP_DISC_CFG_CMPL && appDiscCb.connCfgStatus == APP_DISC_INIT)
      {
        (*appDiscCb.cback)(connId, APP_DISC_CFG_CONN_START);
      }
    }
  }
}
/*************************************************************************************************/
/*!
 *  \fn     appDiscConnOpen
 *        
 *  \brief  Handle a DM_CONN_OPEN_IND event.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appDiscConnOpen(dmEvt_t *pMsg)
{
  appDiscCb.alreadySecure = FALSE;
  appDiscCb.connCfgStatus = APP_DISC_INIT;
  appDiscCb.cmplStatus = APP_DISC_INIT;
  appDiscCb.secRequired = FALSE;
  appDiscCb.scPending = FALSE;
  
  /* tell app to set up handle list */
  (*appDiscCb.cback)((dmConnId_t) pMsg->hdr.param, APP_DISC_INIT);
  
  /* initialize handle list */
  if (appDiscCb.pHdlList != NULL)
  {
    memset(appDiscCb.pHdlList, 0, (appDiscCb.hdlListLen * sizeof(uint16_t)));
  }
  
  /* if not waiting for security start discovery/configuration */
  if (!pAppDiscCfg->waitForSec)
  {
    appDiscStart((dmConnId_t) pMsg->hdr.param);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appDiscConnClose
 *        
 *  \brief  Handle a DM_CONN_CLOSE_IND event.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appDiscConnClose(dmEvt_t *pMsg)
{
  appDiscCb.inProgress = APP_DISC_IDLE;
  
  if (appDiscCb.pDiscCb != NULL)
  {
    WsfBufFree(appDiscCb.pDiscCb);
    appDiscCb.pDiscCb = NULL;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     appDiscPairCmpl
 *        
 *  \brief  Handle pairing complete.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appDiscPairCmpl(dmEvt_t *pMsg)
{
  appDbHdl_t hdl;

  /* procedures triggered by security are only executed once */
  if (appDiscCb.alreadySecure)
  {
    return;
  }
  
  /* if waiting for security start discovery now that connection is secure */
  if (pAppDiscCfg->waitForSec)
  {
    appDiscStart((dmConnId_t) pMsg->hdr.param);
  }
  else
  {
    /* if we are now bonded and discovery/configuration was performed before bonding */
    if (appCheckBonded((dmConnId_t) pMsg->hdr.param) && (appDiscCb.cmplStatus != APP_DISC_INIT))
    {
      if ((hdl = AppDbGetHdl((dmConnId_t) pMsg->hdr.param)) != APP_DB_HDL_NONE)
      {
        /* store discovery status */
        AppDbSetDiscStatus(hdl, appDiscCb.cmplStatus);

        /* store handle list */
        if (appDiscCb.cmplStatus == APP_DISC_CMPL || appDiscCb.cmplStatus == APP_DISC_CFG_CMPL)
        {
          if (appDiscCb.pHdlList != NULL)
          {
            AppDbSetHdlList(hdl, appDiscCb.pHdlList);
          }
        }
      }
      
      /* if configuration was waiting for security */
      if (appDiscCb.secRequired)
      {
        appDiscCb.secRequired = FALSE;

        /* resume configuration */
        if (appDiscCb.pDiscCb != NULL)
        {
          AttcDiscConfigResume((dmConnId_t) pMsg->hdr.param, appDiscCb.pDiscCb);
        }
      }
    }    
  }
    
  appDiscCb.alreadySecure = TRUE;
}

/*************************************************************************************************/
/*!
 *  \fn     appDiscEncryptInd
 *        
 *  \brief  Handle encryption indication
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
static void appDiscEncryptInd(dmEvt_t *pMsg)
{
  /* if encrypted with ltk */
  if (pMsg->encryptInd.usingLtk)
  {
    /* procedures triggered by security are only executed once */
    if (appDiscCb.alreadySecure)
    {
      return;
    }

    /* if we waiting for security start discovery now that connection is secure */
    if (pAppDiscCfg->waitForSec)
    {
      appDiscStart((dmConnId_t) pMsg->hdr.param);
    }
    /* else if configuration was waiting for security */
    else if (appDiscCb.secRequired)
    {
      appDiscCb.secRequired = FALSE;

      /* resume configuration */
      if (appDiscCb.pDiscCb != NULL)
      {
        AttcDiscConfigResume((dmConnId_t) pMsg->hdr.param, appDiscCb.pDiscCb);
      }
    }
      
    appDiscCb.alreadySecure = TRUE;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppDiscProcDmMsg
 *        
 *  \brief  Process discovery-related DM messages.  This function should be called
 *          from the application's event handler.
 *
 *  \param  pMsg    Pointer to DM callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscProcDmMsg(dmEvt_t *pMsg)
{
  switch(pMsg->hdr.event)
  {
    case DM_CONN_OPEN_IND:
      appDiscConnOpen(pMsg);
      break;

    case DM_CONN_CLOSE_IND:
      appDiscConnClose(pMsg);
      break;

    case DM_SEC_PAIR_CMPL_IND:
      appDiscPairCmpl(pMsg);
      break;
      
    case DM_SEC_ENCRYPT_IND:
      appDiscEncryptInd(pMsg);
      break;
      
    default:
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppDiscProcAttMsg
 *        
 *  \brief  Process discovery-related ATT messages.  This function should be called
 *          from the application's event handler.
 *
 *  \param  pMsg    Pointer to ATT callback event message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscProcAttMsg(attEvt_t *pMsg)
{
  uint8_t status;
  
  if (appDiscCb.inProgress == APP_DISC_IN_PROGRESS)
  {
    /* service discovery */
    if (pMsg->hdr.event == ATTC_FIND_BY_TYPE_VALUE_RSP)
    {
      /* continue with service discovery */
      status = AttcDiscServiceCmpl(appDiscCb.pDiscCb, pMsg);
      
      APP_TRACE_INFO1("AttcDiscServiceCmpl status 0x%02x", status);
      
      /* if discovery complete  and successful */
      if (status == ATT_SUCCESS)
      {
        /* proceed with characteristic discovery */
        AttcDiscCharStart((dmConnId_t) pMsg->hdr.param, appDiscCb.pDiscCb);
      }
      /* else if failed */
      else if (status != ATT_CONTINUING)
      {
        /* notify application of discovery failure */
        (*appDiscCb.cback)((dmConnId_t) pMsg->hdr.param, APP_DISC_FAILED);
      }
    }
    /* characteristic discovery */
    else if (pMsg->hdr.event == ATTC_READ_BY_TYPE_RSP ||
             pMsg->hdr.event == ATTC_FIND_INFO_RSP)
    {
      /* continue with characteristic discovery */
      status = AttcDiscCharCmpl(appDiscCb.pDiscCb, pMsg);

      APP_TRACE_INFO1("AttcDiscCharCmpl status 0x%02x", status);

      /* if discovery complete and successful */
      if (status == ATT_SUCCESS)
      {
        /* notify application of discovery success */
        (*appDiscCb.cback)((dmConnId_t) pMsg->hdr.param, APP_DISC_CMPL);
      }
      /* else if failed */
      else if (status != ATT_CONTINUING)
      {
        /* notify application of discovery failure */
        (*appDiscCb.cback)((dmConnId_t) pMsg->hdr.param, APP_DISC_FAILED);
      }     
    }
  }
  /* characteristic configuration */
  else if ((appDiscCb.inProgress == APP_DISC_CFG_IN_PROGRESS) &&
           (pMsg->hdr.event == ATTC_READ_RSP || pMsg->hdr.event == ATTC_WRITE_RSP))
  {
    /* if service changed is pending */
    if (appDiscCb.scPending)
    {
      /* clear pending flag */
      appDiscCb.scPending = FALSE;
      
      /* start discovery */
      appDiscCb.inProgress = APP_DISC_IDLE;
      appDiscStart((dmConnId_t) pMsg->hdr.param);
    }
    /* else if security failure */
    else if ((pMsg->hdr.status == ATT_ERR_AUTH || pMsg->hdr.status == ATT_ERR_ENC) &&
             (DmConnSecLevel((dmConnId_t) pMsg->hdr.param) == DM_SEC_LEVEL_NONE))
    {
      /* tell application to request security */
      appDiscCb.secRequired = TRUE;
      (*appDiscCb.cback)((dmConnId_t) pMsg->hdr.param, APP_DISC_SEC_REQUIRED);
    }
    else
    {
      status = AttcDiscConfigCmpl((dmConnId_t) pMsg->hdr.param, appDiscCb.pDiscCb);
      
      APP_TRACE_INFO1("AttcDiscConfigCmpl status 0x%02x", status);
      
      /* if configuration complete */
      if (status != ATT_CONTINUING)
      {
        /* notify application of config success */
        (*appDiscCb.cback)((dmConnId_t) pMsg->hdr.param, APP_DISC_CFG_CMPL);
      }
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppDiscInit
 *        
 *  \brief  Initialize app framework discovery.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscInit(void)
{
  appDiscCb.inProgress = APP_DISC_IDLE;
  appDiscCb.pDiscCb = NULL;
}

/*************************************************************************************************/
/*!
 *  \fn     AppDiscRegister
 *        
 *  \brief  Register a callback function to service discovery status.
 *
 *  \param  cback   Application service discovery callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscRegister(appDiscCback_t cback)
{
  appDiscCb.cback = cback;
}

/*************************************************************************************************/
/*!
 *  \fn     AppDiscSetHdlList
 *        
 *  \brief  Set the discovery cached handle list for a given connection.
 *
 *  \param  connId    Connection identifier.
 *  \param  listLen   Length of characteristic and handle lists.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscSetHdlList(dmConnId_t connId, uint8_t hdlListLen, uint16_t *pHdlList)
{
  appDiscCb.hdlListLen = hdlListLen;
  appDiscCb.pHdlList = pHdlList;
}

/*************************************************************************************************/
/*!
 *  \fn     AppDiscComplete
 *        
 *  \brief  Service discovery or configuration procedure complete.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    Service or configuration status.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscComplete(dmConnId_t connId, uint8_t status)
{
  appDbHdl_t hdl;

  /* set connection as idle */
  DmConnSetIdle(connId, DM_IDLE_APP_DISC, DM_CONN_IDLE);

  /* store status if not doing connection setup configuration */
  if (!(status == APP_DISC_CFG_CMPL && appDiscCb.connCfgStatus == APP_DISC_CFG_CONN_START))
  {
    appDiscCb.cmplStatus = status;
  }
  
  /* initialize control block */
  appDiscCb.inProgress = APP_DISC_IDLE;
  if (appDiscCb.pDiscCb != NULL)
  {
    WsfBufFree(appDiscCb.pDiscCb);
    appDiscCb.pDiscCb = NULL;
  }
  
  if ((hdl = AppDbGetHdl(connId)) != APP_DB_HDL_NONE)
  {
    /* store discovery status if not doing connection setup configuration */
    if (!(status == APP_DISC_CFG_CMPL && appDiscCb.connCfgStatus == APP_DISC_CFG_CONN_START))
    {
      AppDbSetDiscStatus(hdl, status);
    }

    if (appDiscCb.pHdlList != NULL)
    {
      /* if discovery complete store handles */
      if (status == APP_DISC_CMPL)
      {
        AppDbSetHdlList(hdl, appDiscCb.pHdlList);
      }      
    }    
  }
  
  /* set connection setup configuration status as complete if either discovery-initiated
   * configuration is complete or connection setup configuration is complete
   */
  if (status == APP_DISC_CFG_CMPL)
  {
    appDiscCb.connCfgStatus = APP_DISC_CFG_CMPL;
  }
  
  APP_TRACE_INFO2("AppDiscComplete connId:%d status:0x%02x", connId, status);
}

/*************************************************************************************************/
/*!
 *  \fn     AppDiscFindService
 *        
 *  \brief  Perform service and characteristic discovery for a given service.
 *
 *  \param  connId    Connection identifier.
 *  \param  uuidLen   Length of UUID (2 or 16).
 *  \param  pUuid     Pointer to UUID data.
 *  \param  listLen   Length of characteristic and handle lists.
 *  \param  pCharList Characterisic list for discovery.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscFindService(dmConnId_t connId, uint8_t uuidLen, uint8_t *pUuid, uint8_t listLen,
                        attcDiscChar_t **pCharList, uint16_t *pHdlList)
{
  if (appDiscCb.pDiscCb == NULL)
  {
    appDiscCb.pDiscCb = WsfBufAlloc(sizeof(attcDiscCb_t));
  }
  
  if (appDiscCb.pDiscCb != NULL)
  {
    /* set connection as busy */
    DmConnSetIdle(connId, DM_IDLE_APP_DISC, DM_CONN_BUSY);
    
    appDiscCb.inProgress = APP_DISC_IN_PROGRESS;
 
    appDiscCb.pDiscCb->pCharList = pCharList;
    appDiscCb.pDiscCb->pHdlList = pHdlList;  
    appDiscCb.pDiscCb->charListLen = listLen;
    AttcDiscService(connId, appDiscCb.pDiscCb, uuidLen, pUuid);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppDiscConfigure
 *        
 *  \brief  Configure characteristics for discovered services.
 *
 *  \param  connId      Connection identifier.
 *  \param  status      APP_DISC_CFG_START or APP_DISC_CFG_CONN_START.
 *  \param  cfgListLen  Length of characteristic configuration list.
 *  \param  pCfgList    Characteristic configuration list.
 *  \param  hdlListLen  Length of characteristic handle list.
 *  \param  pHdlList    Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscConfigure(dmConnId_t connId, uint8_t status, uint8_t cfgListLen,
                      attcDiscCfg_t *pCfgList, uint8_t hdlListLen, uint16_t *pHdlList)
{
  uint8_t ret;
  
  if (appDiscCb.pDiscCb == NULL)
  {
    appDiscCb.pDiscCb = WsfBufAlloc(sizeof(attcDiscCb_t));
  }
  
  if (appDiscCb.pDiscCb != NULL)
  {
    /* set connection as busy */
    DmConnSetIdle(connId, DM_IDLE_APP_DISC, DM_CONN_BUSY);
    
    appDiscCb.inProgress = APP_DISC_CFG_IN_PROGRESS;
 
    if (status == APP_DISC_CFG_CONN_START)
    {
      appDiscCb.connCfgStatus = APP_DISC_CFG_CONN_START;
    }

    /* start configuration */
    appDiscCb.pDiscCb->pCfgList = pCfgList;
    appDiscCb.pDiscCb->cfgListLen = cfgListLen;
    appDiscCb.pDiscCb->pHdlList = pHdlList;  
    appDiscCb.pDiscCb->charListLen = hdlListLen;
    ret = AttcDiscConfigStart(connId, appDiscCb.pDiscCb);
    
    /* nothing to configure; configuration complete */
    if (ret == ATT_SUCCESS)
    {
      (*appDiscCb.cback)(connId, APP_DISC_CFG_CMPL);
    }    
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppDiscServiceChanged
 *        
 *  \brief  Perform the GATT service changed procedure.  This function is called when an
 *          indication is received containing the GATT service changed characteristic.  This
 *          function may initialize the discovery state and initiate service discovery
 *          and configuration.
 *
 *  \param  pMsg    Pointer to ATT callback event message containing received indication.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscServiceChanged(attEvt_t *pMsg)
{
  uint16_t    startHdl;
  uint16_t    endHdl;
  uint8_t     *p;
  uint16_t    *pHdl;
  uint8_t     i;
  bool_t      foundHdl;
  appDbHdl_t  dbHdl;
  
  /* verify characteristic length */
  if (pMsg->valueLen != CH_SC_LEN)
  {
    return;
  }
  
  /* parse and verify handles */
  p = pMsg->pValue;
  BSTREAM_TO_UINT16(startHdl, p);
  BSTREAM_TO_UINT16(endHdl, p);
  if (startHdl == 0 || endHdl < startHdl)
  {
    return;
  }
  
  /* if we don't have any stored handles within service changed handle range, ignore */
  foundHdl = FALSE;
  if (appDiscCb.pHdlList != NULL)
  {
    pHdl = appDiscCb.pHdlList;
    for (i = appDiscCb.hdlListLen; i > 0; i--, pHdl++)
    {
      if (*pHdl >= startHdl && *pHdl <= endHdl)
      {
        foundHdl = TRUE;
        break;
      }
    }
  }
  if (foundHdl == FALSE)
  {
    return;
  }
  
  /* if discovery procedure already in progress */
  if (appDiscCb.inProgress == APP_DISC_IN_PROGRESS)
  {
    /* ignore service changed */
    return;
  }

  /* otherwise initialize discovery and configuration status */
  appDiscCb.connCfgStatus = APP_DISC_INIT;
  appDiscCb.cmplStatus = APP_DISC_INIT;
  appDiscCb.secRequired = FALSE;
  appDiscCb.scPending = FALSE;
  
  /* initialize handle list */
  if (appDiscCb.pHdlList != NULL)
  {
    memset(appDiscCb.pHdlList, 0, (appDiscCb.hdlListLen * sizeof(uint16_t)));
  }
 
  /* clear stored discovery status and handle list */
  if ((dbHdl = AppDbGetHdl((dmConnId_t) pMsg->hdr.param)) != APP_DB_HDL_NONE)
  {
      AppDbSetDiscStatus(dbHdl, APP_DISC_INIT);
      AppDbSetHdlList(dbHdl, appDiscCb.pHdlList);
  }
  
  /* if configuration in progress */
  if (appDiscCb.inProgress == APP_DISC_CFG_IN_PROGRESS)
  {
    /* set pending status to set up abort of configuration */
    appDiscCb.scPending = TRUE;
  }  
  /* else no procedure in progress */
  else
  {
    /* if not waiting for security or connection is already secure, then
     * initiate discovery now; otherwise discovery will be initiated after
     * security is done
     */
    if (!pAppDiscCfg->waitForSec || appDiscCb.alreadySecure)
    {
      appDiscStart((dmConnId_t) pMsg->hdr.param);
    }
  }
}