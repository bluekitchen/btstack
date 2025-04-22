/*************************************************************************************************/
/*!
 *  \file   svc_hid.c
 *        
 *  \brief  Example Human Interface Device (over GATT) service implementation.
 *
 *          $Date: 2015-04-30 09:09:16 -0500 (Thu, 30 Apr 2015) $
 *          $Revision: 17666 $
 *  
 *  Copyright (c) 2013 Maxim Integrated Products, all rights reserved.  
 *  
 *  Derived from code licensed under:
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
#include "att_api.h"
#include "wsf_trace.h"
#include "bstream.h"
#include "svc_ch.h"
#include "svc_hid.h"
#include "svc_cfg.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Characteristic read permissions */
#ifndef HID_SEC_PERMIT_READ
#define HID_SEC_PERMIT_READ SVC_SEC_PERMIT_READ
#endif

/*! Characteristic write permissions */
#ifndef HID_SEC_PERMIT_WRITE
#define HID_SEC_PERMIT_WRITE SVC_SEC_PERMIT_WRITE
#endif

/* FIXME: Do we need "Write w/o response" permissions? */
#define HID_REPORT_IN_SIZE 17
#define HID_REPORT_OUT_SIZE 17

/**************************************************************************************************
 Static Variables
**************************************************************************************************/

/* UUIDs */
static const uint8_t svcRptUuid[] = {UINT16_TO_BYTES(ATT_UUID_REPORT)}; // 0x2a4d
static const uint8_t svcRptRefUuid[] = {UINT16_TO_BYTES(ATT_UUID_HID_REPORT_ID_MAPPING)}; /* "Report Reference" */ // 0x2908
static const uint8_t svcRptMapUuid[] = {UINT16_TO_BYTES(ATT_UUID_REPORT_MAP)}; // 0x2a4b
#ifdef EXTERNAL_REF
static const uint8_t svcExtRptRefUuid[] = {UINT16_TO_BYTES(ATT_UUID_EXTERNAL_REPORT_REF)}; // 0x2907
#endif
static const uint8_t svcInfoUuid[] = {UINT16_TO_BYTES(ATT_UUID_HID_INFO)}; // 0x2a4a
static const uint8_t svcCpUuid[] = {UINT16_TO_BYTES(ATT_UUID_HID_CP)}; // 0x2a4c

/**************************************************************************************************
 Service variables
**************************************************************************************************/

/* Human Interface Device service declaration */
#ifdef BLE_DEMO_MAC
static const uint8_t hidValSvc[] = {UINT16_TO_BYTES(ATT_UUID_HID_SERVICE)};
#else /* BLE_DEMO_IOS */
static const uint8_t hidValSvc[] = {UINT16_TO_BYTES(ATT_UUID_HEALTH_THERM_SERVICE)};
#endif
static const uint16_t hidLenSvc = sizeof(hidValSvc);

/* -- Input group -- */

/* HID Input Report characteristic */ 
static const uint8_t hidValRptInCh[] = {ATT_PROP_READ | ATT_PROP_NOTIFY, 
                                        UINT16_TO_BYTES(HID_RPT_IN_HDL), 
                                        UINT16_TO_BYTES(ATT_UUID_REPORT)};
static const uint16_t hidLenRptInCh = sizeof(hidValRptInCh);

/* HID Input Report */
/* Dummy data. Read should provide Features, Notify is used to send actual Report */
uint8_t hidValRptIn[HID_REPORT_IN_SIZE] = { 0x00, 'I', 'N', 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00};
static const uint16_t hidLenRptIn = HID_REPORT_IN_SIZE;

/* HID Input Report client characteristic configuration */
static uint8_t hidValRptInChCcc[] = {UINT16_TO_BYTES(0x0000)};
static const uint16_t hidLenRptInChCcc = sizeof(hidValRptInChCcc);

/* HID Report Reference for Report Input */
/* This maps the report declared above as the Input (from host viewpoint) pipe */
static const uint8_t hidValRptInRef[] = {0x00, 0x01}; /* No report ID, Input Report */
static const uint16_t hidLenRptInRef = sizeof(hidValRptInRef);

/* -- Output group -- */

/* HID Output Report characteristic */ 
static const uint8_t hidValRptOutCh[] = {ATT_PROP_READ | ATT_PROP_WRITE | ATT_PROP_WRITE_NO_RSP, 
                                         UINT16_TO_BYTES(HID_RPT_OUT_HDL), 
                                         UINT16_TO_BYTES(ATT_UUID_REPORT)};
static const uint16_t hidLenRptOutCh = sizeof(hidValRptOutCh);

/* HID Output Report */ 
/* Dummy data. Read should provide Features, Notify is used to send actual Report */
static const uint8_t hidValRptOut[HID_REPORT_OUT_SIZE] = {0x00,
                                                          'O', 'U', 'T', 0x00,
                                                          0x00, 0x00, 0x00, 0x00,
                                                          0x00, 0x00, 0x00, 0x00,
                                                          0x00, 0x00, 0x00, 0x00};

static const uint16_t hidLenRptOut = HID_REPORT_OUT_SIZE;

/* 
 * Per Bluetooth HID Service spec 2.5.1: 
 *  "No Client Characteristic Configuration descriptor shall exist for a Report characteristic 
 *   containing Output Report data or Feature Report data."
 */

/* HID Report Reference for Report Output */
/* This maps the report declared above as the Output (from host viewpoint) pipe */
static const uint8_t hidValRptOutRef[] = {0x00, 0x02}; /* No report ID, Output Report */
static const uint16_t hidLenRptOutRef = sizeof(hidValRptOutRef);

/* -- Report Map -- */

/* HID Report Map characteristic */ 
static const uint8_t hidValRptMapCh[] = {ATT_PROP_READ, 
                                         UINT16_TO_BYTES(HID_RPT_MAP_HDL), 
                                         UINT16_TO_BYTES(ATT_UUID_REPORT_MAP)};
static const uint16_t hidLenRptMapCh = sizeof(hidValRptMapCh);

/* HID Report Map */ 
static const uint8_t hidValRptMap[] = {
    0x06, 0xA0, 0xFF,   /* Usage Page ( 0xFFA0 )   */
    0x0A, 0x00, 0x01,   /* Usage ( 0x0100 )        */
    0xA1, 0x01,         /* Collection ( 1 )        */
    0x75, 0x11,         /* Report Size             */
    0x15, 0x00,         /* Logical Minimum ( 0 )   */
    0x26, 0xFF, 0x00,   /* Logical Maximum ( 255 ) */
    0x95, 0x08,         /* Report Count            */
    0x09, 0x01,         /* Usage ( 1 )             */
    0x81, 0x02,         /* Input ( ARRAY )         */
    0x95, 0x08,         /* Report Count            */
    0x09, 0x02,         /* Usage ( 2 )             */
    0x91, 0x02,         /* Output ( ARRAY )        */
    0xC0                /* End Collection          */         
};
static const uint16_t hidLenRptMap = sizeof(hidValRptMap);

#ifdef EXTERNAL_REF
/* External Report Reference */
/* Includes the Battery Service into this Characteristic group (per HIDS spec Table 7.1) */
static const uint8_t hidValExtRptRef[] = {UINT16_TO_BYTES(ATT_UUID_BATTERY_LEVEL)};
static const uint16_t hidLenExtRptRef = sizeof(hidValExtRptRef);
#endif

/* -- Information -- */

/* HID Information characteristic */ 
static const uint8_t hidValInfoCh[] = { ATT_PROP_READ, 
                                        UINT16_TO_BYTES(HID_INFO_HDL), 
                                        UINT16_TO_BYTES(ATT_UUID_HID_INFO)};
static const uint16_t hidLenInfoCh = sizeof(hidValInfoCh);

/* HID Information */ 
static const uint8_t hidValInfo[] = {UINT16_TO_BYTES(0x0200),  /* USB 2.0 */
                                     0x00,  /* No bCountryCode */
                                     0x02}; /* NormallyConnectable=1 */
static const uint16_t hidLenInfo = sizeof(hidValInfo);

/* -- Control Point -- */

/* HID Control Point characteristic */
static const uint8_t hidValCpCh[] = { ATT_PROP_WRITE, /* 0x08 */
                                      UINT16_TO_BYTES(HID_CP_HDL), 
                                      UINT16_TO_BYTES(ATT_UUID_HID_CP)};
static const uint16_t hidLenCpCh = sizeof(hidValCpCh);

/* HID Control Point */
static const uint8_t hidValCp[] = {0};
static const uint16_t hidLenCp = sizeof(hidValCp);

/*
 * (the canonical reference is stack/include/att_api.h, but duplicated here for reference) 
 *
 * -- ATT server attribute settings --
 *
 * ATTS_SET_UUID_128           0x01     Set if the UUID is 128 bits in length 
 * ATTS_SET_WRITE_CBACK        0x02     Set if the group callback is executed when
 *                                               this attribute is written by a client device 
 * ATTS_SET_READ_CBACK         0x04     Set if the group callback is executed when
 *                                               this attribute is read by a client device 
 * ATTS_SET_VARIABLE_LEN       0x08     Set if the attribute has a variable length 
 * ATTS_SET_ALLOW_OFFSET       0x10     Set if writes are allowed with an offset 
 * ATTS_SET_CCC                0x20     Set if the attribute is a client characteristic
 *                                               configuration descriptor 
 * ATTS_SET_ALLOW_SIGNED       0x40     Set if signed writes are allowed 
 * ATTS_SET_REQ_SIGNED         0x80     Set if signed writes are required if link
 *                                               is not encrypted 
 *
 * -- ATT server attribute permissions --
 *
 * ATTS_PERMIT_READ            0x01     Set if attribute can be read 
 * ATTS_PERMIT_READ_AUTH       0x02     Set if attribute read requires authentication 
 * ATTS_PERMIT_READ_AUTHORIZ   0x04     Set if attribute read requires authorization 
 * ATTS_PERMIT_READ_ENC        0x08     Set if attribute read requires encryption 
 * ATTS_PERMIT_WRITE           0x10     Set if attribute can be written 
 * ATTS_PERMIT_WRITE_AUTH      0x20     Set if attribute write requires authentication 
 * ATTS_PERMIT_WRITE_AUTHORIZ  0x40     Set if attribute write requires authorization 
 * ATTS_PERMIT_WRITE_ENC       0x80     Set if attribute write requires encryption 
 *
 */

/* Attribute list for HID group */
static const attsAttr_t hidList[] =
    {
        /* HID Service itself */
        {
            attPrimSvcUuid,                   /*! Pointer to the attribute's UUID */
            (uint8_t *) hidValSvc,            /*! Pointer to the attribute's value */
            (uint16_t *) &hidLenSvc,          /*! Pointer to the length of the attribute's value */
            sizeof(hidValSvc),                /*! Maximum length of attribute's value */
            0,                                /*! Attribute settings */
            ATTS_PERMIT_READ                  /*! Attribute permissions */
        },
        /* HID Input Report */
        {
            attChUuid,
            (uint8_t *) hidValRptInCh,
            (uint16_t *) &hidLenRptInCh,
            sizeof(hidValRptInCh),
            0,
            ATTS_PERMIT_READ
        },
        {
            svcRptUuid, NULL, NULL, 0,
            /*(uint8_t *) hidValRptIn,
              (uint16_t *) &hidLenRptIn, // FIXME: to avoid IN packet 
              sizeof(hidValRptIn),*/
            ATTS_SET_READ_CBACK,
            ATTS_PERMIT_READ
        },
        {
            attCliChCfgUuid,
            (uint8_t *) hidValRptInChCcc,
            (uint16_t *) &hidLenRptInChCcc,
            sizeof(hidValRptInChCcc),
            ATTS_SET_CCC,
            (ATTS_PERMIT_READ | HID_SEC_PERMIT_WRITE)
        },
        {
            svcRptRefUuid,
            (uint8_t *) hidValRptInRef,
            (uint16_t *) &hidLenRptInRef,
            sizeof(hidLenRptInRef),
            0,
            ATTS_PERMIT_READ
        },
        /* HID Output Report */
        {
            attChUuid,
            (uint8_t *) hidValRptOutCh,
            (uint16_t *) &hidLenRptOutCh,
            sizeof(hidValRptOutCh),
            0,
            HID_SEC_PERMIT_READ
        },
        {
            svcRptUuid,
            (uint8_t *) hidValRptOut,
            (uint16_t *) &hidLenRptOut,
            sizeof(hidValRptOut),
            ATTS_SET_WRITE_CBACK,
            HID_SEC_PERMIT_READ | HID_SEC_PERMIT_WRITE
        },
        {
            svcRptRefUuid,
            (uint8_t *) hidValRptOutRef,
            (uint16_t *) &hidLenRptOutRef,
            sizeof(hidLenRptOutRef),
            0,
            ATTS_PERMIT_READ
        },
        /* HID Report Map */
        {
            attChUuid,
            (uint8_t *) hidValRptMapCh,
            (uint16_t *) &hidLenRptMapCh,
            sizeof(hidValRptMapCh),
            0,
            ATTS_PERMIT_READ
        },
        {
            svcRptMapUuid,
            (uint8_t *) hidValRptMap,
            (uint16_t *) &hidLenRptMap,
            sizeof(hidValRptMap),
            ATTS_SET_READ_CBACK,
            ATTS_PERMIT_READ
        },  
    #ifdef EXTERNAL_REF
        {
            svcExtRptRefUuid,
            (uint8_t *) hidValExtRptRef,
            (uint16_t *) &hidLenExtRptRef,
            sizeof(hidValExtRptRef),
            0,
            ATTS_PERMIT_READ
        },  
    #endif
        /* HID Information */
        {
            attChUuid,
            (uint8_t *) hidValInfoCh,
            (uint16_t *) &hidLenInfoCh,
            sizeof(hidValInfoCh),
            0,
            ATTS_PERMIT_READ
        },
        {
            svcInfoUuid,
            (uint8_t *) hidValInfo,
            (uint16_t *) &hidLenInfo,
            sizeof(hidValInfo),
            0,
            ATTS_PERMIT_READ
        },  
        /* HID Control Point */
        {
            attChUuid,
            (uint8_t *) hidValCpCh,
            (uint16_t *) &hidLenCpCh,
            sizeof(hidValCpCh),
            0,
            ATTS_PERMIT_READ
        },
        {
            svcCpUuid,
            (uint8_t *) hidValCp,
            (uint16_t *) &hidLenCp,
            sizeof(hidValCp),
            ATTS_SET_WRITE_CBACK,
            HID_SEC_PERMIT_WRITE
        }
    };

/* HID group structure */
static attsGroup_t svcHidGroup =
    {
        NULL,
        (attsAttr_t *) hidList,
        NULL,
        NULL,
        HID_START_HDL,
        HID_END_HDL
    };

/*************************************************************************************************/
/*!
 *  \fn     SvcHidAddGroup
 *        
 *  \brief  Add the services to the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcHidAddGroup(void)
{
    AttsAddGroup(&svcHidGroup);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcHidRemoveGroup
 *        
 *  \brief  Remove the services from the attribute server.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcHidRemoveGroup(void)
{
    AttsRemoveGroup(HID_START_HDL);
}

/*************************************************************************************************/
/*!
 *  \fn     SvcHidCbackRegister
 *        
 *  \brief  Register callbacks for the service.
 *
 *  \param  readCback   Read callback function.
 *  \param  writeCback  Write callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void SvcHidCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback)
{
    svcHidGroup.readCback = readCback;
    svcHidGroup.writeCback = writeCback;
}
