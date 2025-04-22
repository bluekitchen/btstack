/*************************************************************************************************/
/*!
 *  \file   app_api.h
 *        
 *  \brief  Application framework API.
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
#ifndef APP_API_H
#define APP_API_H

#include "wsf_os.h"
#include "dm_api.h"
#include "att_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Discoverable/connectable mode used by function AppAdvStart()  */
#define APP_MODE_CONNECTABLE        0     /*! Connectable mode */
#define APP_MODE_DISCOVERABLE       1     /*! Discoverable mode */
#define APP_MODE_AUTO_INIT          2     /*! Automatically configure mode based on bonding info */

/*! Number of Discoverable/connectable modes */
#define APP_NUM_MODES               2

/*! Advertising states */
enum
{
  APP_ADV_STATE1,                         /*! Advertising state 1 */
  APP_ADV_STATE2,                         /*! Advertising state 2 */
  APP_ADV_STATE3,                         /*! Advertising state 3 */
  APP_ADV_STOPPED                         /*! Advertising stopped */
};

/*! Advertising and scan data storage locations */
enum
{
  APP_ADV_DATA_CONNECTABLE,               /*! Advertising data for connectable mode */
  APP_SCAN_DATA_CONNECTABLE,              /*! Scan data for connectable mode */
  APP_ADV_DATA_DISCOVERABLE,              /*! Advertising data for discoverable mode */
  APP_SCAN_DATA_DISCOVERABLE,             /*! Scan data for discoverable mode */
  APP_NUM_DATA_LOCATIONS
};

/*! Number of advertising configurations */
#define APP_ADV_NUM_CFG             APP_ADV_STOPPED

/*! Service discovery and configuration client status */
enum
{
  APP_DISC_INIT,                          /*! No discovery or configuration complete */
  APP_DISC_SEC_REQUIRED,                  /*! Security required to complete configuration */
  APP_DISC_START,                         /*! Service discovery started */  
  APP_DISC_CMPL,                          /*! Service discovery complete */
  APP_DISC_FAILED,                        /*! Service discovery failed */
  APP_DISC_CFG_START,                     /*! Service configuration started */
  APP_DISC_CFG_CONN_START,                /*! Configuration for connection setup started */
  APP_DISC_CFG_CMPL,                      /*! Service configuration complete */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Configurable parameters for slave */
typedef struct
{
  uint16_t    advDuration[APP_ADV_NUM_CFG];  /*! Advertising durations */
  uint16_t    advInterval[APP_ADV_NUM_CFG];  /*! Advertising intervals */
} appSlaveCfg_t;

/*! Configurable parameters for master */
typedef struct
{
  uint16_t    scanInterval;               /*! The scan interval, in 0.625 ms units */
  uint16_t    scanWindow;                 /*! The scan window, in 0.625 ms units.   Must be
                                              less than or equal to scan interval. */
  uint16_t    scanDuration;               /*! The scan duration in ms.  Set to zero to scan
                                              until stopped. */
  uint8_t     discMode;                   /*! The GAP discovery mode (general, limited, or none) */
  uint8_t     scanType;                   /*! The scan type (active or passive) */
} appMasterCfg_t;

/*! Configurable parameters for security */
typedef struct
{
  uint8_t       auth;                     /*! Authentication and bonding flags */
  uint8_t       iKeyDist;                 /*! Initiator key distribution flags */
  uint8_t       rKeyDist;                 /*! Responder key distribution flags */
  bool_t        oob;                      /*! TRUE if Out-of-band pairing data is present */
  bool_t        initiateSec;              /*! TRUE to initiate security upon connection */
} appSecCfg_t;

/*! Configurable parameters for connection parameter update */
typedef struct
{
  wsfTimerTicks_t idlePeriod;             /*! Connection idle period in ms before attempting
                                              connection parameter update; set to zero to disable */
  uint16_t        connIntervalMin;        /*! Minimum connection interval in 1.25ms units */
  uint16_t        connIntervalMax;        /*! Maximum connection interval in 1.25ms units */
  uint16_t        connLatency;            /*! Connection latency */
  uint16_t        supTimeout;             /*! Supervision timeout in 10ms units */
  uint8_t         maxAttempts;            /*! Number of update attempts before giving up */
} appUpdateCfg_t;

/*! Configurable parameters for service and characteristic discovery */
typedef struct
{
  bool_t          waitForSec;             /*! TRUE to wait for a secure connection before initiating discovery */
} appDiscCfg_t;

/*! Device information data type */
typedef struct
{
  bdAddr_t        addr;                   /*! Peer device address */
  uint8_t         addrType;               /*! Peer address type */
} appDevInfo_t;

/*!
 *  \fn     appDiscCback_t
 *        
 *  \brief  Service discovery and configuration callback.
 *
 *  \param  connId    Connection identifier.
 *  \param  status    Service or configuration status.
 *
 *  \return None.
 */
typedef void (*appDiscCback_t)(dmConnId_t connId, uint8_t status);

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! Configuration pointer for slave */
extern appSlaveCfg_t *pAppSlaveCfg;

/*! Configuration pointer for master */
extern appMasterCfg_t *pAppMasterCfg;

/*! Configuration pointer for security */
extern appSecCfg_t *pAppSecCfg;

/*! Configuration pointer for connection parameter update */
extern appUpdateCfg_t *pAppUpdateCfg;

/*! Configuration pointer for discovery */
extern appDiscCfg_t *pAppDiscCfg;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     AppSlaveInit
 *        
 *  \brief  Initialize the App Framework for operation as a Bluetooth LE slave.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppSlaveInit(void);

/*************************************************************************************************/
/*!
 *  \fn     AppMasterInit
 *        
 *  \brief  Initialize the App Framework for operation as a Bluetooth LE master.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppMasterInit(void);

/*************************************************************************************************/
/*!
 *  \fn     AppAdvSetData
 *        
 *  \brief  Set advertising or scan data.  Separate advertising and scan data can be set for 
 *          connectable and discoverable modes.  The application must allocate and maintain
 *          the memory pointed to by pData while the device is advertising.
 *
 *  \param  location  Data location.
 *  \param  len       Length of the data.  Maximum length is 31 bytes.
 *  \param  pData     Pointer to the data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppAdvSetData(uint8_t location, uint8_t len, uint8_t *pData);

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
void AppAdvStart(uint8_t mode);

/*************************************************************************************************/
/*!
 *  \fn     AppAdvStop
 *        
 *  \brief  Stop advertising.  The device will no longer be connectable or discoverable.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppAdvStop(void);

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
 *  \param  len       Length of the value.  Maximum length is 29 bytes.
 *  \param  pValue    Pointer to the value.
 *
 *  \return TRUE if the element was successfully added to the data, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t AppAdvSetAdValue(uint8_t location, uint8_t adType, uint8_t len, uint8_t *pValue);

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
void AppScanStart(uint8_t mode, uint8_t scanType, uint16_t duration);

/*************************************************************************************************/
/*!
 *  \fn     AppScanStop
 *        
 *  \brief  Stop scanning.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppScanStop(void);

/*************************************************************************************************/
/*!
 *  \fn     AppScanGetResult
 *        
 *  \brief  Get a stored scan result from the scan result list.  The first result is at
 *          index zero.
 *
 *  \param  idx     Index of result in scan result list.
 *
 *  \return Pointer to scan result device info or NULL if index contains no result.
 */
/*************************************************************************************************/
appDevInfo_t *AppScanGetResult(uint8_t idx);

/*************************************************************************************************/
/*!
 *  \fn     AppScanGetNumResults
 *        
 *  \brief  Get the number of stored scan results.
 *
 *  \return Number of stored scan results.
 */
/*************************************************************************************************/
uint8_t AppScanGetNumResults(void);

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
dmConnId_t AppConnOpen(uint8_t addrType, uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \fn     AppConnClose
 *        
 *  \brief  Close a connection with the given connection identifier.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppConnClose(dmConnId_t connId);

/*************************************************************************************************/
/*!
 *  \fn     AppConnIsOpen
 *        
 *  \brief  Check if a connection is open.
 *
 *  \return Connection ID of open connection or DM_CONN_ID_NONE if no open connections.
 */
/*************************************************************************************************/
dmConnId_t AppConnIsOpen(void);

/*************************************************************************************************/
/*!
 *  \fn     AppHandlePasskey
 *        
 *  \brief  Handle a passkey request during pairing.  If the passkey is to be displayed, a
 *          random passkey is generated and displayed.  If the passkey is to be entered
 *          the user is prompted to enter the passkey.
 *
 *  \param  pAuthReq  DM authentication requested event structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHandlePasskey(dmSecAuthReqIndEvt_t *pAuthReq);

/*************************************************************************************************/
/*!
 *  \fn     AppSetBondable
 *        
 *  \brief  Set the bondable mode of the device.
 *
 *  \param  bondable  TRUE to set device to bondable, FALSE to set to non-bondable.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppSetBondable(bool_t bondable);

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
void AppSlaveSecurityReq(dmConnId_t connId);

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
void AppMasterSecurityReq(dmConnId_t connId);

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
void AppSlaveProcDmMsg(dmEvt_t *pMsg);

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
void AppSlaveSecProcDmMsg(dmEvt_t *pMsg);

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
void AppMasterProcDmMsg(dmEvt_t *pMsg);

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
void AppMasterSecProcDmMsg(dmEvt_t *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     AppServerConnCback
 *        
 *  \brief  ATT connection callback for app framework.  This function is used when the application
 *          is operating as an ATT server and it uses notifications or indications.  This
 *          function can be called by the application's ATT connection callback or it can be
 *          installed as the ATT connection callback. 
 *
 *  \param  pDmEvt  DM callback event.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppServerConnCback(dmEvt_t *pDmEvt);

/*************************************************************************************************/
/*!
 *  \fn     AppDiscInit
 *        
 *  \brief  Initialize app framework discovery.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscInit(void);

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
void AppDiscRegister(appDiscCback_t cback);

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
void AppDiscSetHdlList(dmConnId_t connId, uint8_t hdlListLen, uint16_t *pHdlList);

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
void AppDiscComplete(dmConnId_t connId, uint8_t status);

/*************************************************************************************************/
/*!
 *  \fn     AppDiscFindService
 *        
 *  \brief  Perform service and characteristic discovery for a given service.
 *
 *  \param  connId    Connection identifier.
 *  \param  uuidLen   Length of service UUID (2 or 16).
 *  \param  pUuid     Pointer to service UUID.
 *  \param  listLen   Length of characteristic and handle lists.
 *  \param  pCharList Characterisic list for discovery.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDiscFindService(dmConnId_t connId, uint8_t uuidLen, uint8_t *pUuid, uint8_t listLen,
                        attcDiscChar_t **pCharList, uint16_t *pHdlList);

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
                      attcDiscCfg_t *pCfgList, uint8_t hdlListLen, uint16_t *pHdlList);

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
void AppDiscServiceChanged(attEvt_t *pMsg);

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
void AppDiscProcDmMsg(dmEvt_t *pMsg);

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
void AppDiscProcAttMsg(attEvt_t *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     AppHandlerInit
 *        
 *  \brief  App framework handler init function called during system initialization.
 *
 *  \param  handlerID  WSF handler ID for App.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHandlerInit(wsfHandlerId_t handlerId);

/*************************************************************************************************/
/*!
 *  \fn     AppHandler
 *        
 *  \brief  WSF event handler for app framework.
 *
 *  \param  event   WSF event mask.
 *  \param  pMsg    WSF message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg);

#ifdef __cplusplus
};
#endif

#endif /* APP_API_H */
