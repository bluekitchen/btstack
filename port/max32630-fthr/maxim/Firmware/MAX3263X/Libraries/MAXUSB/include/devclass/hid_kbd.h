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
 
#ifndef _HID_KBD_H_
#define _HID_KBD_H_

/**
 * @file  hid_kbd.h
 * @brief Human Interface Device Class (Keyboard) over USB
 */

#include "hid.h"

/** 
 *  \brief    Initialize the class driver
 *  \details  Initialize the class driver.
 *  \param    hid_descriptor      pointer to the descriptor to be used in response to getdescriptor requests
 *  \param    report_descriptor   pointer to the descriptor to be used in response to getdescriptor requests
 *  \return   Zero (0) for success, non-zero for failure
 */
int hidkbd_init(const hid_descriptor_t *hid_descriptor, const uint8_t *report_descriptor);

/** 
 *  \brief    Set the specified configuration
 *  \details  Configures the class and endpoint(s) and starts operation. This function should be
 *            called upon configuration from the host.
 *  \param    cfg   configuration to be set
 *  \return   Zero (0) for success, non-zero for failure
 */
int hidkbd_configure(unsigned int ep);

/**
 *  \brief    Clear the current configuration and resets endpoint(s)
 *  \details  Clear the current configuration and resets endpoint(s).
 *  \return   Zero (0) for success, non-zero for failure
 */
int hidkbd_deconfigure(void);

/** 
 *  \brief    Send one ASCII value (key) encoded as a keyboard code to the host
 *  \param    key   ASCII value
 *  \return   Zero (0) for success, non-zero for failure
 */
int hidkbd_keypress(uint8_t key);

/**
 *  \brief    ASCII-to-HID mapping (partial)
 *  \details  HID Keyboard/Keypad Page codes do not map directly to ASCII, unfortunately. This partial mapping of
 *            the most commonly used codes is provided for demonstration. Please refer to the HID Usage Tables
 *            1.12 section 10 "Keyboard/Keypad Page (0x07)" section for a complete listing.
 *  \param    rpt   Encoded report. Must point to 3 * num bytes of storage, as each report is 3 bytes
 *  \param    ascii Characters to encode
 *  \param    num   Number of characters to encode
 *  \return   size of the encoded report
 */
int hidkbd_encode_report(uint8_t *rpt, uint8_t *ascii, int num);

#endif /* _HID_KBD_H_ */
