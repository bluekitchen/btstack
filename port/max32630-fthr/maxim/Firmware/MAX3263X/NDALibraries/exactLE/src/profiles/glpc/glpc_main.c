/*************************************************************************************************/
/*!
 *  \file   glpc_main.c
 *
 *  \brief  Glucose profile collector.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
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
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "svc_ch.h"
#include "app_api.h"
#include "glpc_api.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Length of response data contained in a received RACP message */
#define GLPC_GLS_RACP_RSP_LEN         4

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*!
 *  Glucose service characteristics for discovery
 */

/*! Glucose measurement */
static const attcDiscChar_t glpcGlsGlm = 
{
  attGlmChUuid,
  ATTC_SET_REQUIRED
};

/*! Glucose measurement CCC descriptor */
static const attcDiscChar_t glpcGlsGlmCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! Glucose measurement context */
static const attcDiscChar_t glpcGlsGlmc = 
{
  attGlmcChUuid,
  0
};

/*! Glucose measurement context CCC descriptor */
static const attcDiscChar_t glpcGlsGlmcCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_DESCRIPTOR
};

/*! Glucose feature */
static const attcDiscChar_t glpcGlsGlf = 
{
  attGlfChUuid,
  ATTC_SET_REQUIRED
};

/*! Record access control point */
static const attcDiscChar_t glpcGlsRacp = 
{
  attRacpChUuid,
  ATTC_SET_REQUIRED
};

/*! Record access control point CCC descriptor */
static const attcDiscChar_t glpcGlsRacpCcc = 
{
  attCliChCfgUuid,
  ATTC_SET_REQUIRED | ATTC_SET_DESCRIPTOR
};

/*! List of characteristics to be discovered; order matches handle index enumeration  */
static const attcDiscChar_t *glpcGlsDiscCharList[] =
{
  &glpcGlsGlm,                    /*! Glucose measurement */
  &glpcGlsGlmCcc,                 /*! Glucose measurement CCC descriptor */
  &glpcGlsGlmc,                   /*! Glucose measurement context */
  &glpcGlsGlmcCcc,                /*! Glucose measurement context CCC descriptor */
  &glpcGlsGlf,                    /*! Glucose feature */
  &glpcGlsRacp,                   /*! Record access control point */
  &glpcGlsRacpCcc                 /*! Record access control point CCC descriptor */
};

/* sanity check:  make sure handle list length matches characteristic list length */
WSF_CT_ASSERT(GLPC_GLS_HDL_LIST_LEN == ((sizeof(glpcGlsDiscCharList) / sizeof(attcDiscChar_t *))));

/*! Control block */
static struct
{
  uint16_t    lastSeqNum;         /*! Last received glucose measurement sequence number */
} glpcCb;

/*************************************************************************************************/
/*!
 *  \fn     glpcGlsParseGlm
 *        
 *  \brief  Parse a glucose measurement.
 *
 *  \param  pValue    Pointer to buffer containing value.
 *  \param  len       length of buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void glpcGlsParseGlm(uint8_t *pValue, uint16_t len)
{
  uint8_t   flags;
  uint16_t  seqNum;
  uint16_t  year;
  uint8_t   month, day, hour, min, sec;
  int16_t   timeOffset;
  uint16_t  glucose;
  int16_t   mantissa;
  int8_t    exponent;
  uint8_t   typeLoc;
  uint16_t  sensorStatus;
  uint16_t  minLen = CH_GLM_FLAGS_LEN + CH_GLM_SEQNUM_LEN + CH_GLM_TIMESTAMP_LEN;
  
  if (len > 0)
  {
    /* get flags */
    BSTREAM_TO_UINT8(flags, pValue);

    /* determine expected minimum length based on flags */
    if (flags & CH_GLM_FLAG_TIME_OFFSET)
    {
      minLen += CH_GLM_TIME_OFFSET_LEN;
    }
    if (flags & CH_GLM_FLAG_CONC_TYPE_LOC)
    {
      minLen += CH_GLM_CONC_TYPE_LOC_LEN;
    }
    if (flags & CH_GLM_FLAG_SENSOR_STATUS)
    {
      minLen += CH_GLM_SENSOR_STATUS_LEN;
    }
  }
    
  /* verify length */
  if (len < minLen)
  {
    APP_TRACE_INFO2("Glucose meas len:%d minLen:%d", len, minLen);
    return;
  }
  
  /* sequence number */
  BSTREAM_TO_UINT16(seqNum, pValue);
  glpcCb.lastSeqNum = seqNum;
  
  /* base time */
  BSTREAM_TO_UINT16(year, pValue);
  BSTREAM_TO_UINT8(month, pValue);
  BSTREAM_TO_UINT8(day, pValue);
  BSTREAM_TO_UINT8(hour, pValue);
  BSTREAM_TO_UINT8(min, pValue);
  BSTREAM_TO_UINT8(sec, pValue);
  APP_TRACE_INFO3("  Date: %d/%d/%d", month, day, year);
  APP_TRACE_INFO3("  Time: %02d:%02d:%02d", hour, min, sec);
 
  /* time offset */
  if (flags & CH_GLM_FLAG_TIME_OFFSET)
  {
    BSTREAM_TO_UINT16(timeOffset, pValue);
    APP_TRACE_INFO1("  Time offset: %d", timeOffset);
  }
  
  /* glucose concentration, type, and location */
  if (flags & CH_GLM_FLAG_CONC_TYPE_LOC)
  {
    BSTREAM_TO_UINT16(glucose, pValue);
    UINT16_TO_SFLT(mantissa, exponent, glucose);
    APP_TRACE_INFO2("  Glucose concentration: %de%d", mantissa, exponent);
    BSTREAM_TO_UINT8(typeLoc, pValue);
    APP_TRACE_INFO2("  Type: %d Location: %d", (typeLoc >> 4), (typeLoc & 0x0F));
  }
  
  /* sensor status */
  if (flags & CH_GLM_FLAG_SENSOR_STATUS)
  {
    BSTREAM_TO_UINT16(sensorStatus, pValue);
    APP_TRACE_INFO1("  Sensor status: 0x%04x", sensorStatus);
  }
  
  APP_TRACE_INFO2("  Flags: 0x%02x Sequence Number: %d", flags, seqNum);
}

/*************************************************************************************************/
/*!
 *  \fn     glpcGlsParseGlmc
 *        
 *  \brief  Parse a glucose measurement context.
 *
 *  \param  pValue    Pointer to buffer containing value.
 *  \param  len       length of buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void glpcGlsParseGlmc(uint8_t *pValue, uint16_t len)
{
  uint8_t   flags;
  uint16_t  seqNum;
  uint8_t   extFlags;     
  uint8_t   carbId;       
  uint16_t  carb;         
  uint8_t   meal;         
  uint8_t   testerHealth; 
  uint16_t  exerDuration; 
  uint8_t   exerIntensity;
  uint8_t   medicationId; 
  uint16_t  medication;   
  uint16_t  hba1c;          
  int16_t   mantissa;
  int8_t    exponent;
  uint16_t  minLen = CH_GLMC_FLAGS_LEN + CH_GLMC_SEQNUM_LEN;
  
  if (len > 0)
  {
    /* get flags */
    BSTREAM_TO_UINT8(flags, pValue);

    /* determine expected minimum length based on flags */
    if (flags & CH_GLMC_FLAG_CARB)
    {
      minLen += CH_GLMC_CARB_LEN;
    }
    if (flags & CH_GLMC_FLAG_MEAL)
    {
      minLen += CH_GLMC_MEAL_LEN;
    }
    if (flags & CH_GLMC_FLAG_TESTER)
    {
      minLen += CH_GLMC_TESTER_LEN;
    }
    if (flags & CH_GLMC_FLAG_EXERCISE)
    {
      minLen += CH_GLMC_EXERCISE_LEN;
    }
    if (flags & CH_GLMC_FLAG_MED)
    {
      minLen += CH_GLMC_MED_LEN;
    }
    if (flags & CH_GLMC_FLAG_HBA1C)
    {
      minLen += CH_GLMC_HBA1C_LEN;
    }
    if (flags & CH_GLMC_FLAG_EXT)
    {
      minLen += CH_GLMC_EXT_LEN;
    }
  }
    
  /* verify length */
  if (len < minLen)
  {
    APP_TRACE_INFO2("Glucose meas context len:%d minLen:%d", len, minLen);
    return;
  }

  /* sequence number */
  BSTREAM_TO_UINT16(seqNum, pValue);  
  
  /* extended flags */
  if (flags & CH_GLMC_FLAG_EXT)
  {
    BSTREAM_TO_UINT8(extFlags, pValue);
    APP_TRACE_INFO1("  Extended Flags: 0x%02x", extFlags);  
  }

  /* carbohydrate id and carbohydrate */
  if (flags & CH_GLMC_FLAG_CARB)
  {
    BSTREAM_TO_UINT8(carbId, pValue);
    BSTREAM_TO_UINT16(carb, pValue);
    UINT16_TO_SFLT(mantissa, exponent, carb);
    APP_TRACE_INFO3("  Carb Id: %d Carb (kg): %de%d", carbId, mantissa, exponent);  
  }
  
  /* meal */
  if (flags & CH_GLMC_FLAG_MEAL)
  {
    BSTREAM_TO_UINT8(meal, pValue);
    APP_TRACE_INFO1("  Meal: %d", meal);  
  }
  
  /* tester-health */
  if (flags & CH_GLMC_FLAG_TESTER)
  {
    BSTREAM_TO_UINT8(testerHealth, pValue);
    APP_TRACE_INFO2("  Tester: %d Health: %d", (testerHealth >> 4), (testerHealth & 0x0F));  
  }
  
  /* exercise duration and exercise intensity */
  if (flags & CH_GLMC_FLAG_EXERCISE)
  {
    BSTREAM_TO_UINT16(exerDuration, pValue);
    BSTREAM_TO_UINT8(exerIntensity, pValue);
    APP_TRACE_INFO2("  Exercise Duration: %d Intensity: %d", exerDuration, exerIntensity);  
  }
  
  /* medication ID and medication */
  if (flags & CH_GLMC_FLAG_MED)
  {
    BSTREAM_TO_UINT8(medicationId, pValue);
    BSTREAM_TO_UINT16(medication, pValue);
    UINT16_TO_SFLT(mantissa, exponent, medication);
    APP_TRACE_INFO3("  Medication ID: %d Units: %de%d", medicationId, mantissa, exponent);    
  }
  
  /* hba1c */
  if (flags & CH_GLMC_FLAG_HBA1C)
  {
    BSTREAM_TO_UINT16(hba1c, pValue);
    UINT16_TO_SFLT(mantissa, exponent, hba1c);
    APP_TRACE_INFO2("  HbA1c: %de%d", mantissa, exponent);      
  }

  APP_TRACE_INFO2("  Flags: 0x%02x Sequence Number: %d", flags, seqNum);
}

/*************************************************************************************************/
/*!
 *  \fn     glpcGlsProcRacp
 *        
 *  \brief  Process a message received from the sensor on the record access control point.
 *
 *  \param  pValue    Pointer to buffer containing value.
 *  \param  len       length of buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void glpcGlsProcRacp(uint8_t *pValue, uint16_t len)
{
  uint8_t   opcode;
  uint16_t  numRecords;
  uint16_t  reqOpcode;
  uint16_t  status;
  
  /* verify length */
  if (len != GLPC_GLS_RACP_RSP_LEN)
  {
    APP_TRACE_INFO1("Unexpected RACP message length: %d", len);
    return;
  }
  
  /* parse message */
  BSTREAM_TO_UINT8(opcode, pValue);
  pValue++;
  if (opcode == CH_RACP_OPCODE_NUM_RSP)
  {
    BSTREAM_TO_UINT16(numRecords, pValue);
    APP_TRACE_INFO1("Number of records: %d", numRecords);
  }
  else if (opcode == CH_RACP_OPCODE_RSP)
  {
    BSTREAM_TO_UINT8(reqOpcode, pValue);
    BSTREAM_TO_UINT8(status, pValue);
    APP_TRACE_INFO2("Response opcode: %d status: %d", reqOpcode, status);    
  }
}

/*************************************************************************************************/
/*!
 *  \fn     GlpcGlsDiscover
 *        
 *  \brief  Perform service and characteristic discovery for Glucose service.  Parameter 
 *          pHdlList must point to an array of length GLPC_GLS_HDL_LIST_LEN.  If discovery is 
 *          successful the handles of discovered characteristics and descriptors will be set
 *          in pHdlList.
 *
 *  \param  connId    Connection identifier.
 *  \param  pHdlList  Characteristic handle list.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlpcGlsDiscover(dmConnId_t connId, uint16_t *pHdlList)
{
  AppDiscFindService(connId, ATT_16_UUID_LEN, (uint8_t *) attGlsSvcUuid,
                     GLPC_GLS_HDL_LIST_LEN, (attcDiscChar_t **) glpcGlsDiscCharList, pHdlList);
}

/*************************************************************************************************/
/*!
 *  \fn     GlpcGlsValueUpdate
 *        
 *  \brief  Process a value received in an ATT read response, notification, or indication 
 *          message.  Parameter pHdlList must point to an array of length GLPC_GLS_HDL_LIST_LEN. 
 *          If the attribute handle of the message matches a handle in the handle list the value
 *          is processed, otherwise it is ignored.
 *
 *  \param  pHdlList  Characteristic handle list.
 *  \param  pMsg      ATT callback message.
 *
 *  \return ATT_SUCCESS if handle is found, ATT_ERR_NOT_FOUND otherwise.
 */
/*************************************************************************************************/
uint8_t GlpcGlsValueUpdate(uint16_t *pHdlList, attEvt_t *pMsg)
{
  uint16_t  feature;
  uint8_t   *p;
  uint8_t   status = ATT_SUCCESS;

  /* glucose measurement */
  if (pMsg->handle == pHdlList[GLPC_GLS_GLM_HDL_IDX])
  {
    APP_TRACE_INFO0("Glucose meas.");

    /* parse value */
    glpcGlsParseGlm(pMsg->pValue, pMsg->valueLen);
  }
  /* glucose measurement context  */
  else if (pMsg->handle == pHdlList[GLPC_GLS_GLMC_HDL_IDX])
  {
    APP_TRACE_INFO0("Glucose meas. context");

    /* parse value */
    glpcGlsParseGlmc(pMsg->pValue, pMsg->valueLen);
  }
  /* record access control point */
  else if (pMsg->handle == pHdlList[GLPC_GLS_RACP_HDL_IDX])
  {
    glpcGlsProcRacp(pMsg->pValue, pMsg->valueLen);
  }
  /* glucose feature  */
  else if (pMsg->handle == pHdlList[GLPC_GLS_GLF_HDL_IDX])
  {
    /* parse value */
    p = pMsg->pValue;
    BSTREAM_TO_UINT16(feature, p);

    APP_TRACE_INFO1("Glucose feature:0x%04x", feature);
  }
  else
  {
    status = ATT_ERR_NOT_FOUND;
  }
  
  return status;
}

/*************************************************************************************************/
/*!
 *  \fn     GlpcGlsRacpSend
 *        
 *  \brief  Send a command to the glucose service record access control point.
 *
 *  \param  connId  Connection identifier.
 *  \param  handle  Attribute handle. 
 *  \param  opcode  Command opcode.
 *  \param  oper    Command operator or 0 if no operator required.
 *  \param  pFilter Command filter parameters or NULL of no parameters required.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlpcGlsRacpSend(dmConnId_t connId, uint16_t handle, uint8_t opcode, uint8_t oper,
                     glpcFilter_t *pFilter)
{
  uint8_t   buf[ATT_DEFAULT_PAYLOAD_LEN];
  uint8_t   *p = buf;
  
  if (handle != ATT_HANDLE_NONE)
  {
    /* build RACP command */
    UINT8_TO_BSTREAM(p, opcode);
    UINT8_TO_BSTREAM(p, oper);
    if (pFilter != NULL)
    {
      UINT8_TO_BSTREAM(p, pFilter->type);
      if (pFilter->type == CH_RACP_GLS_FILTER_SEQ)
      {
        UINT16_TO_BSTREAM(p, pFilter->param.seqNum);
      }
    }
     
    AttcWriteReq(connId, handle, (uint16_t) (p - buf), buf);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     GlpcGlsSetLastSeqNum
 *        
 *  \brief  Set the last received glucose measurement sequence number.
 *
 *  \param  seqNum   Glucose measurement sequence number.
 *
 *  \return None.
 */
/*************************************************************************************************/
void GlpcGlsSetLastSeqNum(uint16_t seqNum)
{
  glpcCb.lastSeqNum = seqNum;
}

/*************************************************************************************************/
/*!
 *  \fn     GlpcGlsGetLastSeqNum
 *        
 *  \brief  Get the last received glucose measurement sequence number.
 *
 *  \return Last received glucose measurement sequence number.
 */
/*************************************************************************************************/
uint16_t GlpcGlsGetLastSeqNum(void)
{
  return glpcCb.lastSeqNum;
}