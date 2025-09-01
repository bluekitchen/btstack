/*************************************************************************************************/
/*!
 *  \file   glps_db.c
 *
 *  \brief  Glucose profile example record database and access functions.
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

#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "att_api.h"
#include "svc_ch.h"
#include "svc_gls.h"
#include "app_api.h"
#include "glps_api.h"
#include "glps_main.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Number of records in example database */
#define GLPS_DB_NUM_RECORDS     3

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/*! Control block */
static struct
{
  uint8_t     numRec;
} glpsDbCb;

/*! Example record database */
static const glpsRec_t glpsDb[GLPS_DB_NUM_RECORDS] =
{
  /* record 1 */
  {
    /* measurement */
    {
      CH_GLM_FLAG_TIME_OFFSET | CH_GLM_FLAG_CONC_TYPE_LOC |     /* Flags */
      CH_GLM_FLAG_UNITS_MOL_L | CH_GLM_FLAG_SENSOR_STATUS |
      CH_GLM_FLAG_CONTEXT_INFO,                 
      0x0001,                                                   /* Sequence number */
      {2012, 6, 15, 20, 52, 36},                                /* Base time */
      60,                                                       /* Time offset */
      SFLT_TO_UINT16(50, -3),                                   /* Glucose concentration (SFLOAT) */
      CH_GLM_LOC_FINGER | (CH_GLM_TYPE_ART_BLOOD << 4),         /* Sample type and sample location */
      CH_GLM_STATUS_BATT_LOW                                    /* Sensor status annunciation */
    },
    
    /* context */
    {
      CH_GLMC_FLAG_CARB |  CH_GLMC_FLAG_MEAL |                  /* Flags */
      CH_GLMC_FLAG_TESTER | CH_GLMC_FLAG_EXERCISE |                                              
      CH_GLMC_FLAG_MED | CH_GLMC_FLAG_MED_KG |                                                   
      CH_GLMC_FLAG_HBA1C,                                                     
      0x0001,                                                   /* Sequence number */
      0,                                                        /* Extended Flags */
      CH_GLMC_CARB_DINNER,                                      /* Carbohydrate ID */                                                          
      SFLT_TO_UINT16(12, -3),                                   /* Carbohydrate (SFLOAT) */
      CH_GLMC_MEAL_POSTPRANDIAL,                                /* Meal */
      CH_GLMC_HEALTH_NONE | (CH_GLMC_TESTER_SELF << 4),         /* Tester and health */
      300,                                                      /* Exercise Duration */
      99,                                                       /* Exercise Intensity */
      CH_GLMC_MED_LONG,                                         /* Medication ID */
      SFLT_TO_UINT16(50, -6),                                   /* Medication (SFLOAT) */
      SFLT_TO_UINT16(10, 0)                                     /* HbA1c */
    },
  },
  
  /* record 2 */
  {
    /* measurement */
    {
      CH_GLM_FLAG_CONC_TYPE_LOC | CH_GLM_FLAG_UNITS_KG_L |      /* Flags */
      CH_GLM_FLAG_CONTEXT_INFO,                 
      0x0002,                                                   /* Sequence number */
      {2012, 6, 16, 6, 45, 20},                                 /* Base time */
      0,                                                        /* Time offset */
      SFLT_TO_UINT16(20, -5),                                   /* Glucose concentration (SFLOAT) */
      CH_GLM_LOC_FINGER | (CH_GLM_TYPE_ART_BLOOD << 4),         /* Sample type and sample location */
      0                                                         /* Sensor status annunciation */
    },
    
    /* context */
    {
      CH_GLMC_FLAG_CARB |  CH_GLMC_FLAG_MEAL |                  /* Flags */
      CH_GLMC_FLAG_TESTER | CH_GLMC_FLAG_EXERCISE |                                              
      CH_GLMC_FLAG_MED | CH_GLMC_FLAG_MED_L |                                                   
      CH_GLMC_FLAG_HBA1C,                                                     
      0x0002,                                                   /* Sequence number */
      0,                                                        /* Extended Flags */
      CH_GLMC_CARB_BREAKFAST,                                   /* Carbohydrate ID */                                                          
      SFLT_TO_UINT16(3, -3),                                    /* Carbohydrate (SFLOAT) */
      CH_GLMC_MEAL_PREPRANDIAL,                                 /* Meal */
      CH_GLMC_HEALTH_NONE | (CH_GLMC_TESTER_SELF << 4),         /* Tester and health */
      1000,                                                     /* Exercise Duration */
      25,                                                       /* Exercise Intensity */
      CH_GLMC_MED_LONG,                                         /* Medication ID */
      SFLT_TO_UINT16(10, -3),                                   /* Medication (SFLOAT) */
      SFLT_TO_UINT16(11, 0)                                     /* HbA1c */
    },
  },  

  /* record 3 */
  {
    /* measurement */
    {
      CH_GLM_FLAG_CONC_TYPE_LOC | CH_GLM_FLAG_UNITS_KG_L |      /* Flags */
      CH_GLM_FLAG_CONTEXT_INFO,                 
      0x0003,                                                   /* Sequence number */
      {2012, 6, 16, 18, 45, 20},                                /* Base time */
      0,                                                        /* Time offset */
      SFLT_TO_UINT16(20, -5),                                   /* Glucose concentration (SFLOAT) */
      CH_GLM_LOC_FINGER | (CH_GLM_TYPE_ART_BLOOD << 4),         /* Sample type and sample location */
      0                                                         /* Sensor status annunciation */
    },
    
    /* context */
    {                
      CH_GLMC_FLAG_TESTER | CH_GLMC_FLAG_EXERCISE |             /* Flags */                             
      CH_GLMC_FLAG_MED | CH_GLMC_FLAG_MED_L |                                                   
      CH_GLMC_FLAG_HBA1C,                                                     
      0x0003,                                                   /* Sequence number */
      0,                                                        /* Extended Flags */
      0,                                                        /* Carbohydrate ID */                                                          
      SFLT_TO_UINT16(0, 0),                                     /* Carbohydrate (SFLOAT) */
      0,                                                        /* Meal */
      CH_GLMC_HEALTH_NONE | (CH_GLMC_TESTER_SELF << 4),         /* Tester and health */
      1001,                                                     /* Exercise Duration */
      26,                                                       /* Exercise Intensity */
      CH_GLMC_MED_LONG,                                         /* Medication ID */
      SFLT_TO_UINT16(15, -3),                                   /* Medication (SFLOAT) */
      SFLT_TO_UINT16(12, 0)                                     /* HbA1c */
    },
  }
};

/*************************************************************************************************/
/*!
 *  \fn     glpsDbGetEnd
 *        
 *  \brief  Get last database record.
 *
 *  \return Pointer to record or NULL if no record.
 */
/*************************************************************************************************/
static glpsRec_t *glpsDbGetEnd(void)
{
  if (glpsDbCb.numRec == 0)
  {
    return NULL;
  }
  
  return (glpsRec_t *) &glpsDb[GLPS_DB_NUM_RECORDS - 1];
}

/*************************************************************************************************/
/*!
 *  \fn     glpsDbGetNext
 *        
 *  \brief  Get the next database record after the given current record.
 *
 *  \param  pCurrRec    Pointer to current record.
 *
 *  \return Pointer to record or NULL if no record.
 */
/*************************************************************************************************/
static glpsRec_t *glpsDbGetNext(glpsRec_t *pCurrRec)
{
  if (glpsDbCb.numRec == 0 || pCurrRec == glpsDbGetEnd())
  {
    return NULL;
  }
  else if (pCurrRec == NULL)
  {
    return (glpsRec_t *) glpsDb;
  }
  else
  {
    return (pCurrRec + 1);
  }
}

/*************************************************************************************************/
/*!
 *  \fn     glpsDbOpAll
 *        
 *  \brief  Get the next record filtered for "all".
 *
 *  \param  pCurrRec    Pointer to current record.
 *  \param  pRec        Return pointer to next record, if found.
 *
 *  \return CH_RACP_RSP_SUCCESS if a record is found, otherwise an error status is returned.
 */
/*************************************************************************************************/
static uint8_t glpsDbOpAll(glpsRec_t *pCurrRec,  glpsRec_t **pRec)
{
  *pRec = glpsDbGetNext(pCurrRec);
  return (*pRec != NULL) ? CH_RACP_RSP_SUCCESS : CH_RACP_RSP_NO_RECORDS;
}

/*************************************************************************************************/
/*!
 *  \fn     glpsDbOpGteqSeqNum
 *        
 *  \brief  Get the next record filtered for "greater than or equal to sequence number".
 *
 *  \param  pFilter     Glucose service RACP filter parameters.
 *  \param  pCurrRec    Pointer to current record.
 *  \param  pRec        Return pointer to next record, if found.
 *
 *  \return CH_RACP_RSP_SUCCESS if a record is found, otherwise an error status is returned.
 */
/*************************************************************************************************/
static uint8_t glpsDbOpGteqSeqNum(uint8_t *pFilter, glpsRec_t *pCurrRec,  glpsRec_t **pRec)
{
  uint16_t seqNum;
  
  /* parse seq number */
  pFilter++;
  BYTES_TO_UINT16(seqNum, pFilter);
  
  /* find record */
  while ((*pRec = glpsDbGetNext(pCurrRec)) != NULL)
  {
    if ((*pRec)->meas.seqNum >= seqNum)
    {
      return CH_RACP_RSP_SUCCESS;
    }
    pCurrRec = *pRec;
  }
  
  return CH_RACP_RSP_NO_RECORDS;
}

/*************************************************************************************************/
/*!
 *  \fn     glpsDbOpLast
 *        
 *  \brief  Get the next record filtered for "last".
 *
 *  \param  pCurrRec    Pointer to current record.
 *  \param  pRec        Return pointer to next record, if found.
 *
 *  \return CH_RACP_RSP_SUCCESS if a record is found, otherwise an error status is returned.
 */
/*************************************************************************************************/
static uint8_t glpsDbOpLast(glpsRec_t *pCurrRec, glpsRec_t **pRec)
{
  /* if current record is already last return failure */
  *pRec = glpsDbGetEnd();
  if (*pRec != NULL && *pRec != pCurrRec)
  {
    return CH_RACP_RSP_SUCCESS;
  }
  else
  {
    return CH_RACP_RSP_NO_RECORDS;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     glpsDbInit
 *        
 *  \brief  Initialize the glucose record database.
 *
 *  \return None.
 */
/*************************************************************************************************/
void glpsDbInit(void)
{
  glpsDbCb.numRec = GLPS_DB_NUM_RECORDS;
}

/*************************************************************************************************/
/*!
 *  \fn     glpsDbGetNextRecord
 *        
 *  \brief  Get the next record that matches the given filter parameters that follows
 *          the given current record.
 *
 *  \param  oper        Operator. 
 *  \param  pFilter     Glucose service RACP filter parameters.
 *  \param  pCurrRec    Pointer to current record.
 *  \param  pRec        Return pointer to next record, if found.
 *
 *  \return CH_RACP_RSP_SUCCESS if a record is found, otherwise an error status is returned.
 */
/*************************************************************************************************/
uint8_t glpsDbGetNextRecord(uint8_t oper, uint8_t *pFilter, glpsRec_t *pCurrRec,  glpsRec_t **pRec)
{
  uint8_t status;

  switch (oper)
  {
    case CH_RACP_OPERATOR_ALL:
      status = glpsDbOpAll(pCurrRec, pRec);
      break;

    case CH_RACP_OPERATOR_GTEQ:
      /* check filter type */
      if (*pFilter == CH_RACP_GLS_FILTER_SEQ)
      {
        status = glpsDbOpGteqSeqNum(pFilter, pCurrRec, pRec);
      }
      else
      {
        status = CH_RACP_RSP_OPERAND_NOT_SUP;
      }
      break;

    case CH_RACP_OPERATOR_LAST:
      status = glpsDbOpLast(pCurrRec, pRec);
      break;

    default:
      status = CH_RACP_RSP_OPERATOR_NOT_SUP;
      break;

  }
  
  return status;
}

/*************************************************************************************************/
/*!
 *  \fn     glpsDbDeleteRecords
 *        
 *  \brief  Delete records that match the given filter parameters.
 *
 *  \param  oper        Operator. 
 *  \param  pFilter     Glucose service RACP filter parameters.
 *
 *  \return CH_RACP_RSP_SUCCESS if records deleted, otherwise an error status is returned.
 */
/*************************************************************************************************/
uint8_t glpsDbDeleteRecords(uint8_t oper, uint8_t *pFilter)
{
  /* only 'all records' is supported */
  if (oper == CH_RACP_OPERATOR_ALL)
  {
    glpsDbCb.numRec = 0;
    
    return CH_RACP_RSP_SUCCESS;
  }
  else
  {
    return CH_RACP_RSP_OPERATOR_NOT_SUP;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     glpsDbGetNumRecords
 *        
 *  \brief  Get the number of records matching the filter parameters.
 *
 *  \param  oper        Operator. 
 *  \param  pFilter     Glucose service RACP filter parameters.
 *  \param  pNumRec     Returns number of records which match filter parameters.

 *
 *  \return RACP status.
 */
/*************************************************************************************************/
uint8_t glpsDbGetNumRecords(uint8_t oper, uint8_t *pFilter, uint8_t *pNumRec)
{
  glpsRec_t *pCurrRec = NULL;
  uint8_t   status;
   
  *pNumRec = 0;
  while ((status = glpsDbGetNextRecord(oper, pFilter, pCurrRec, &pCurrRec)) == CH_RACP_RSP_SUCCESS)
  {
    (*pNumRec)++;
  }
  
  if (status == CH_RACP_RSP_NO_RECORDS)
  {
    status = CH_RACP_RSP_SUCCESS;
  }
  
  return status;
}
