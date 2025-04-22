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
 
#ifndef _HID_RAW_H_
#define _HID_RAW_H_

/**
 * @file  hid_raw.h
 * @brief Raw Human Interface Device Class over USB
 */

#include "hid.h"

/// Configuration structure
typedef struct {
  uint8_t in_ep;            // endpoint to be used for IN packets
  uint8_t in_maxpacket;     // max packet size for IN endpoint
  uint8_t out_ep;           // endpoint to be used for OUT packets
  uint8_t out_maxpacket;    // max packet size for OUT endpoint
} hid_cfg_t;

/** 
 *  \brief    Initialize the class driver
 *  \details  Initialize the class driver.
 *  \param    hid_descriptor      pointer to the descriptor to be used in response to getdescriptor requests
 *  \param    report_descriptor   pointer to the descriptor to be used in response to getdescriptor requests
 *  \return   Zero (0) for success, non-zero for failure
 */
int hidraw_init(const hid_descriptor_t *hid_descriptor, const uint8_t *report_descriptor);

/** 
 *  \brief    Set the specified configuration
 *  \details  Configures the class and endpoint(s) and starts operation. This function should be
 *            called upon configuration from the host.
 *  \param    cfg   configuration to be set
 *  \return   Zero (0) for success, non-zero for failure
 */
int hidraw_configure(const hid_cfg_t *cfg);

/**
 *  \brief    Clear the current configuration and resets endpoint(s)
 *  \details  Clear the current configuration and resets endpoint(s).
 *  \return   Zero (0) for success, non-zero for failure
 */
int hidraw_deconfigure(void);

/**
 *  \brief    Register a callback to be called when read data is available.
 *  \details  Register a callback to be called when read data is available. To disable the
 *            callback, call this function with a NULL parameter.
 */
void hidraw_register_callback(int (*func)(void));

/**
 *  \brief    Get the number of bytes available to be read.
 *  \return   The number of bytes available to be read.
 */
int hidraw_canread(void);

/**
 *  \brief    Read the specified number of bytes.
 *  \details  Read the specified number of bytes. This function blocks until the specified
 *            number of bytes have been received.
 *  \param    buf   buffer to store the bytes in
 *  \param    len   number of bytes to read
 *  \return   number of bytes to read, non-zero for failure
 */
int hidraw_read(uint8_t *data, unsigned int len);

/**
 *  \brief    Write the specified number of bytes.
 *  \details  Write the specified number of bytes. The number of bytes must be less than or
 *            equal to the max packet size provided in hid_cfg_t.
 *  \param    data  buffer containing the data to be sent
 *  \param    len   number of bytes to write
 *  \return   Zero (0) for success, non-zero for failure
 *  \note     On some processors, the actually USB transaction is performed asynchronously, after
 *            this function returns. Successful return from this function does not guarantee
 *            successful reception of characters by the host.
 */
int hidraw_write(const uint8_t *data, unsigned int len);

#endif /* _HID_RAW_H_ */
