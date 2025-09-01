/*************************************************************************************************/
/*!
 *  \file   app_db.h
 *
 *  \brief  Application framework device database.
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
#ifndef APP_DB_H
#define APP_DB_H

#include "wsf_os.h"
#include "dm_api.h"

#ifdef __cplusplus
extern "C" {
#endif


/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! No device database record handle  */
#define APP_DB_HDL_NONE                   NULL

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Device database record handle type */
typedef void *appDbHdl_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     AppDbInit()
 *        
 *  \brief  Initialize the device database.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDbInit(void);

/*************************************************************************************************/
/*!
 *  \fn     AppDbNewRecord
 *        
 *  \brief  Create a new device database record.
 *
 *  \param  addrType  Address type.
 *  \param  pAddr     Peer device address.
 *
 *  \return Database record handle.
 */
/*************************************************************************************************/
appDbHdl_t AppDbNewRecord(uint8_t addrType, uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \fn     AppDbDeleteRecord
 *        
 *  \brief  Delete a new device database record.
 *
 *  \param  hdl       Database record handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDbDeleteRecord(appDbHdl_t hdl);

/*************************************************************************************************/
/*!
 *  \fn     AppDbValidateRecord
 *        
 *  \brief  Validate a new device database record.  This function is called when pairing is
 *          successful and the devices are bonded.
 *
 *  \param  hdl       Database record handle.
 *  \param  keyMask   Bitmask of keys to validate.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDbValidateRecord(appDbHdl_t hdl, uint8_t keyMask);

/*************************************************************************************************/
/*!
 *  \fn     AppDbCheckValidRecord
 *        
 *  \brief  Check if a record has been validated.  If it has not, delete it.  This function
 *          is typically called when the connection is closed.
 *
 *  \param  hdl       Database record handle.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDbCheckValidRecord(appDbHdl_t hdl);

/*************************************************************************************************/
/*!
 *  \fn     AppDbCheckBonded
 *        
 *  \brief  Check if there is a stored bond with any device.
 *
 *  \return TRUE if a bonded device is found, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t AppDbCheckBonded(void);

/*************************************************************************************************/
/*!
 *  \fn     AppDbDeleteAllRecords
 *        
 *  \brief  Delete all database records.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDbDeleteAllRecords(void);

/*************************************************************************************************/
/*!
 *  \fn     AppDbFindByAddr
 *        
 *  \brief  Find a device database record by peer address.
 *
 *  \param  addrType  Address type.
 *  \param  pAddr     Peer device address.
 *
 *  \return Database record handle or APP_DB_HDL_NONE if not found.
 */
/*************************************************************************************************/
appDbHdl_t AppDbFindByAddr(uint8_t addrType, uint8_t *pAddr);

/*************************************************************************************************/
/*!
 *  \fn     AppDbFindByLtkReq
 *        
 *  \brief  Find a device database record from data in an LTK request.
 *
 *  \param  encDiversifier  Encryption diversifier associated with key.
 *  \param  pRandNum        Pointer to random number associated with key.
 *
 *  \return Database record handle or APP_DB_HDL_NONE if not found.
 */
/*************************************************************************************************/
appDbHdl_t AppDbFindByLtkReq(uint16_t encDiversifier, uint8_t *pRandNum);

/*************************************************************************************************/
/*!
 *  \fn     AppDbGetHdl
 *        
 *  \brief  Get the device database record handle associated with an open connection.
 *
 *  \param  connId    Connection identifier.
 *
 *  \return Database record handle or APP_DB_HDL_NONE.
 */
/*************************************************************************************************/
appDbHdl_t AppDbGetHdl(dmConnId_t connId);

/*************************************************************************************************/
/*!
 *  \fn     AppDbGetKey
 *        
 *  \brief  Get a key from a device database record.
 *
 *  \param  hdl       Database record handle.
 *  \param  type      Type of key to get.
 *  \param  pSecLevel If the key is valid, returns the security level of the key.
 *
 *  \return Pointer to the key if the key is valid or NULL if not valid.
 */
/*************************************************************************************************/
dmSecKey_t *AppDbGetKey(appDbHdl_t hdl, uint8_t type, uint8_t *pSecLevel);

/*************************************************************************************************/
/*!
 *  \fn     AppDbSetKey
 *        
 *  \brief  Set a key in a device database record.
 *
 *  \param  hdl       Database record handle.
 *  \param  pKey      Key data.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDbSetKey(appDbHdl_t hdl, dmSecKeyIndEvt_t *pKey);

/*************************************************************************************************/
/*!
 *  \fn     AppDbGetCccTbl
 *        
 *  \brief  Get the client characteristic configuration descriptor table.
 *
 *  \param  hdl       Database record handle.
 *
 *  \return Pointer to client characteristic configuration descriptor table.
 */
/*************************************************************************************************/
uint16_t *AppDbGetCccTbl(appDbHdl_t hdl);

/*************************************************************************************************/
/*!
 *  \fn     AppDbSetCccTblValue
 *        
 *  \brief  Set a value in the client characteristic configuration table.
 *
 *  \param  hdl       Database record handle.
 *  \param  idx       Table index.
 *  \param  value     Client characteristic configuration value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDbSetCccTblValue(appDbHdl_t hdl, uint16_t idx, uint16_t value);

/*************************************************************************************************/
/*!
 *  \fn     AppDbGetDiscStatus
 *        
 *  \brief  Get the discovery status.
 *
 *  \param  hdl       Database record handle.
 *
 *  \return Discovery status.
 */
/*************************************************************************************************/
uint8_t AppDbGetDiscStatus(appDbHdl_t hdl);

/*************************************************************************************************/
/*!
 *  \fn     AppDbSetDiscStatus
 *        
 *  \brief  Set the discovery status.
 *
 *  \param  hdl       Database record handle.
 *  \param  status    Discovery status.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDbSetDiscStatus(appDbHdl_t hdl, uint8_t status);

/*************************************************************************************************/
/*!
 *  \fn     AppDbGetHdlList
 *        
 *  \brief  Get the cached handle list.
 *
 *  \param  hdl       Database record handle.
 *
 *  \return Pointer to handle list.
 */
/*************************************************************************************************/
uint16_t *AppDbGetHdlList(appDbHdl_t hdl);

/*************************************************************************************************/
/*!
 *  \fn     AppDbSetHdlList
 *        
 *  \brief  Set the cached handle list.
 *
 *  \param  hdl       Database record handle.
 *  \param  pHdlList  Pointer to handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDbSetHdlList(appDbHdl_t hdl, uint16_t *pHdlList);

/*************************************************************************************************/
/*!
 *  \fn     AppDbGetDevName
 *        
 *  \brief  Get the device name.
 *
 *  \param  pLen      Returned device name length.
 *
 *  \return Pointer to UTF-8 string containing the device name or NULL if not set.
 */
/*************************************************************************************************/
char *AppDbGetDevName(uint8_t *pLen);

/*************************************************************************************************/
/*!
 *  \fn     AppDbSetDevName
 *        
 *  \brief  Set the device name.
 *
 *  \param  len       Device name length.
 *  \param  pStr      UTF-8 string containing the device name.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppDbSetDevName(uint8_t len, char *pStr);

#ifdef __cplusplus
};
#endif

#endif /* APP_DB_H */
