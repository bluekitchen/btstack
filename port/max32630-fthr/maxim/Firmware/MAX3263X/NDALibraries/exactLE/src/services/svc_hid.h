/*************************************************************************************************/
/*!
 *  \file   svc_hid.h
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

#ifndef SVC_HID_H
#define SVC_HID_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

#define HID_PROTOCOL_MODE_BOOT      0x00
#define HID_PROTOCOL_MODE_REPORT    0x01

/**************************************************************************************************
 Handle Ranges
**************************************************************************************************/

/*
 * Per Core Bluetooth Specification Version 4.0 (30 June 2010):
 *
 * "An attribute handle is a 16-bit value that is assigned by each server to its own
 *  attributes to allow a client to reference those attributes. An attribute handle
 *  shall not be reused while an ATT Bearer exists between a client and its server."
 *
 * "Attribute handles on any given server shall have unique, non-zero values."
 *
 * "An attribute handle of value 0x0000 is reserved, and shall not be used. An attri-
 *  bute handle of value 0xFFFF is known as the maximum attribute handle."
 *
 * Each service (svc_XXX.h) in the Wicentric examples uses its own range. Maxim picked 0x70 for
 * the HID Service.
 *
 */

/* Human Interface Device Service */
#define HID_START_HDL               0x70
#define HID_END_HDL                 (HID_MAX_HDL - 1)

/**************************************************************************************************
 Handles
**************************************************************************************************/

/* HID Service Handles */
/* NOTE: Does not support keyboard, mice, or Boot Protocol devices */
enum
{
  HID_SVC_HDL = HID_START_HDL,      /* HID service declaration */
  HID_RPT_IN_CH_HDL,                /* HID IN report characteristic */ 
  HID_RPT_IN_HDL,                   /* HID IN report */
  HID_RPT_IN_CH_CCC_HDL,            /* HID IN report client characteristic configuration */
  HID_RPT_IN_REF_HDL,               /* HID IN report reference */
  HID_RPT_OUT_CH_HDL,               /* HID OUT report characteristic */ 
  HID_RPT_OUT_HDL,                  /* HID OUT report */
  HID_RPT_OUT_REF_HDL,              /* HID OUT report reference */
  HID_RPT_MAP_CH_HDL,               /* HID report map point characteristic */
  HID_RPT_MAP_HDL,                  /* HID report map point */
  HID_INFO_CH_HDL,                  /* HID information characteristic */
  HID_INFO_HDL,                     /* HID information */
  HID_CP_CH_HDL,                    /* HID control point characteristic */
  HID_CP_HDL,                       /* HID control point */
  HID_MAX_HDL
};

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

void SvcHIDAddGroup(void);
void SvcHIDRemoveGroup(void);
void SvcHIDCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback);
void SvcHidCbackRegister(attsReadCback_t readCback, attsWriteCback_t writeCback);
void SvcHidAddGroup(void);
#ifdef __cplusplus
};
#endif

#endif /* SVC_HID_H */
