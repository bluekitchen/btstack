/*************************************************************************************************/
/*!
 *  \file   app_hw.h
 *
 *  \brief  Application framework hardware interfaces.
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
#ifndef APP_HW_H
#define APP_HW_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Heart rate measurement structure */
typedef struct
{
  uint16_t          *pRrInterval;         /*! Array of RR intervals */
  uint8_t           numIntervals;         /*! Length of RR interval array */
  uint16_t          energyExp;            /*! Energy expended value */
  uint8_t           heartRate;            /*! Heart rate */
  uint8_t           flags;                /*! Heart rate measurement flags */
} appHrm_t;

/*! Date and time structure */
typedef struct
{
  uint16_t          year;                 /*! Year */
  uint8_t           month;                /*! Month */
  uint8_t           day;                  /*! Day */
  uint8_t           hour;                 /*! Hour */
  uint8_t           min;                  /*! Minutes */
  uint8_t           sec;                  /*! Seconds */
} appDateTime_t;

/*! Blood pressure measurement structure */
typedef struct
{
  appDateTime_t     timestamp;            /*! Date-time */
  uint16_t          systolic;             /*! Systolic pressure */
  uint16_t          diastolic;            /*! Diastolic pressure */
  uint16_t          map;                  /*! Mean arterial pressure */
  uint16_t          pulseRate;            /*! Pulse rate */
  uint16_t          measStatus;           /*! Measurement status */
  uint8_t           flags;                /*! Flags */
  uint8_t           userId;               /*! User ID */
} appBpm_t;

/*! Weight scale measurement structure */
typedef struct
{
  appDateTime_t     timestamp;            /*! Date-time */
  uint32_t          weight;               /*! Weight */
  uint8_t           flags;                /*! Weight measurement flags */
} appWsm_t;

/*! Temperature measurement structure */
typedef struct
{
  appDateTime_t     timestamp;            /*! Date-time */
  uint32_t          temperature;          /*! Temperature */
  uint8_t           flags;                /*! Flags */
  uint8_t           tempType;             /*! Temperature type */
} appTm_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     AppHwBattRead
 *        
 *  \brief  Read the battery level.  The battery level value returned in pLevel is the
 *          percentage of remaining battery capacity (0-100%).
 *
 *  \param  pLevel   Battery level return value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHwBattRead(uint8_t *pLevel);

/*************************************************************************************************/
/*!
 *  \fn     AppHwBattTest
 *        
 *  \brief  Set the battery level, for test purposes.
 *
 *  \param  level   Battery level (0-100%).
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHwBattTest(uint8_t level);

/*************************************************************************************************/
/*!
 *  \fn     AppHwHrmRead
 *        
 *  \brief  Perform a heart rate measurement.  Return the heart rate along with any RR interval
 *          data.
 *
 *  \param  pHrm   Heart rate measurement return value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHwHrmRead(appHrm_t *pHrm);

/*************************************************************************************************/
/*!
 *  \fn     AppHwHrmTest
 *        
 *  \brief  Set the heart rate, for test purposes.
 *
 *  \param  heartRate Heart rate.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHwHrmTest(uint8_t heartRate);

/*************************************************************************************************/
/*!
 *  \fn     AppHwBpmRead
 *        
 *  \brief  Perform a blood pressure measurement.  Return the measurement data.
 *
 *  \param  intermed  TRUE if this is an intermediate measurement.
 *  \param  pBpm      Blood pressure measurement return value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHwBpmRead(bool_t intermed, appBpm_t *pBpm);

/*************************************************************************************************/
/*!
 *  \fn     AppHwWsmRead
 *        
 *  \brief  Perform a weight scale measurement.  Return the measurement data.
 *
 *  \param  pWsm      Weight scale measurement return value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHwWsmRead(appWsm_t *pWsm);

/*************************************************************************************************/
/*!
 *  \fn     AppHwTmRead
 *        
 *  \brief  Perform a temperature measurement.  Return the measurement data.
 *
 *  \param  intermed  TRUE if this is an intermediate measurement.
 *  \param  pBpm      Temperature measurement return value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHwTmRead(bool_t intermed, appTm_t *pTm);

/*************************************************************************************************/
/*!
 *  \fn     AppHwTmSetUnits
 *        
 *  \brief  Set the temperature measurement units.
 *
 *  \param  units     CH_TM_FLAG_UNITS_C or CH_TM_FLAG_UNITS_F.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppHwTmSetUnits(uint8_t units);

#ifdef __cplusplus
};
#endif

#endif /* APP_HW_H */
