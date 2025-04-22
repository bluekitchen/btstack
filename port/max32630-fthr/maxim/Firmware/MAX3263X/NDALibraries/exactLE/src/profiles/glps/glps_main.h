/*************************************************************************************************/
/*!
 *  \file   glps_main.h
 *
 *  \brief  Glucose profile sensor internal interfaces.
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
#ifndef GLPS_MAIN_H
#define GLPS_MAIN_H

#include "app_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Minimum RACP write length */
#define GLPS_RACP_MIN_WRITE_LEN       2

/*! RACP response length */
#define GLPS_RACP_RSP_LEN             4

/*! Glucose RACP number of stored records response length */
#define GLPS_RACP_NUM_REC_RSP_LEN     4

/*! RACP operand maximum length */
#define GLPS_OPERAND_MAX              ((CH_RACP_GLS_FILTER_TIME_LEN * 2) + 1)

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Glucose measurement structure */
typedef struct
{
  uint8_t         flags;            /*! Flags */
  uint16_t        seqNum;           /*! Sequence number */
  appDateTime_t   baseTime;         /*! Base time */
  int16_t         timeOffset;       /*! Time offset */
  uint16_t        concentration;    /*! Glucose concentration (SFLOAT) */
  uint8_t         typeSampleLoc;    /*! Sample type and sample location */      
  uint16_t        sensorStatus;     /*! Sensor status annunciation */
} glpsGlm_t;

/*! Glucose measurement context structure */
typedef struct
{
  uint8_t         flags;            /*! Flags */
  uint16_t        seqNum;           /*! Sequence number */
  uint8_t         extFlags;         /*! Extended Flags */
  uint8_t         carbId;           /*! Carbohydrate ID */
  uint16_t        carb;             /*! Carbohydrate (SFLOAT) */
  uint8_t         meal;             /*! Meal */
  uint8_t         testerHealth;     /*! Tester and health */
  uint16_t        exerDuration;     /*! Exercise Duration */
  uint8_t         exerIntensity;    /*! Exercise Intensity */
  uint8_t         medicationId;     /*! Medication ID */
  uint16_t        medication;       /*! Medication (SFLOAT) */
  uint16_t        hba1c;            /*! HbA1c */
} glpsGlmc_t;

/*! Glucose measurement record */
typedef struct
{
  glpsGlm_t     meas;               /*! Glucose measurement */
  glpsGlmc_t    context;            /*! Glucose measurement context */
} glpsRec_t;

/*! Measurement database interface */
void glpsDbInit(void);
uint8_t glpsDbGetNextRecord(uint8_t oper, uint8_t *pFilter, glpsRec_t *pCurrRec,  glpsRec_t **pRec);
uint8_t glpsDbDeleteRecords(uint8_t oper, uint8_t *pFilter);
uint8_t glpsDbGetNumRecords(uint8_t oper, uint8_t *pFilter, uint8_t *pNumRec);

#ifdef __cplusplus
};
#endif

#endif /* GLPS_MAIN_H */
