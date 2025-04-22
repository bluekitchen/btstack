/*******************************************************************************
 * Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 * $Date: 2016-03-11 11:46:37 -0600 (Fri, 11 Mar 2016) $
 * $Revision: 21839 $
 *
 ******************************************************************************/

#ifndef _HID_H_
#define _HID_H_

/**
 * @file  hid.h
 * @brief Human Interface Device Class
 */
#define HID_MAX_PACKET 	  64

/// USB HID class requests
#define HID_GET_REPORT    0x01
#define HID_GET_IDLE      0x02
#define HID_GET_PROTOCOL  0x03
#define HID_SET_REPORT    0x09
#define HID_SET_IDLE      0x0a
#define HID_SET_PROTOCOL  0x0b

/// Class-specific descriptor types for GET_DESCRIPTOR
#define DESC_HID          0x21
#define DESC_REPORT       0x22

/// HID Descriptor
#if defined(__GNUC__)
typedef struct __attribute__((packed)) {
#else
typedef __packed struct {
#endif
  uint8_t  bFunctionalLength;
  uint8_t  bDescriptorType;
  uint16_t bcdHID;
  uint8_t  bCountryCode;
  uint8_t  bNumDescriptors;
  uint8_t  bHIDDescriptorType;
  uint16_t wDescriptorLength;
} hid_descriptor_t;

#endif /* _HID_H_ */
