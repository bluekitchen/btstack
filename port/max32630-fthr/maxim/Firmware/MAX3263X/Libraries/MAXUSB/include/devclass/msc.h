/*******************************************************************************
 * Copyright (C) 2017 Maxim Integrated Products, Inc., All Rights Reserved.
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
 * $Date: 2017-10-17 15:36:13 -0500 (Tue, 17 Oct 2017) $
 * $Revision: 31422 $
 *
 ******************************************************************************/

#ifndef _MSC_H_
#define _MSC_H_

/**
 * @file  MSC.h
 * @brief Mass Storage Class over USB.
 */
 
#define MAX_PACKET_SIZE                     512     /* The maximum size block (in bytes) handled by this driver. */
                                                    /* LBAs are typically 512 bytes so that is a reasonable choice for this value. */
 
#define VENDOR_ID_MAX_LEN                   8
#define PRODUCT_ID_MAX_LEN                  16
#define PRODUCT_REV_MAX_LEN                 4
 
/// Configuration structure
typedef struct {
    unsigned int out_ep;           /* endpoint to be used for OUT packets   */
    unsigned int out_maxpacket;    /* max packet size for OUT endpoint      */
    unsigned int in_ep;            /* endpoint to be used for IN packets    */
    unsigned int in_maxpacket;     /* max packet size for IN endpoint       */
} msc_cfg_t;

typedef struct {
    uint8_t vendor[VENDOR_ID_MAX_LEN];      /* Vendor string.  Maximum of 8 bytes */
    uint8_t product[PRODUCT_ID_MAX_LEN];    /* Product string.  Maximum of 16 bytes */
    uint8_t revision[PRODUCT_REV_MAX_LEN];  /* Version string.  Maximum of 4 bytes */
} msc_idstrings_t;
    
typedef struct {    
    int (*init)(void);                  /* Function to initialize the memory exposed by the MSC.    */
                                        /*   Returns 0 for success, non-zero for failure.           */
    int (*start)(void);                 /* Function to "wake" the memory.  This is called when the  */
                                        /*   device has been enumerated on the bus and the host is  */
                                        /*   to begin sending transactions to it.  Returns 0 for    */
                                        /*   success, non-zero for failure.                         */ 
    int (*stop)(void);                  /* Function to stop the memory.  This function is called    */
                                        /*   when the "disk" has been ejected.  This function       */
                                        /*   should flush any pending writes to the memory.         */
                                        /*   Returns 0 for success, non-zero for failure.           */ 
    int (*ready)(void);                 /* Function to test the readiness of the memory.  Returns 0 */
                                        /*   for ready, non-zero for not ready.                     */ 
    uint32_t (*size)(void);             /* Function to get the memory's size.  Returns the number   */
                                        /*   of 512 byte blocks supported by the memory.            */
    int (*read)(uint32_t, uint8_t*);    /* Function to read the memory.  The first paramter is the  */
                                        /*   block number to read, the second is a buffer where the */
                                        /*   read data should be stored. Returns 0 for success,     */
                                        /*   non-zero for failure.                                  */ 
    int (*write)(uint32_t, uint8_t*);   /* Function to write the memory.  Set to NULL if the memory */
                                        /*   is not writable.  The first paramter is the block      */
                                        /*   number to write, the second is a buffer containing the */
                                        /*   data to write. Returns 0 for success, non-zero for     */
                                        /*   failure.                                               */ 
} msc_mem_t;

/**
 *  \brief    Initialize the class driver
 *  \details  Initialize the class driver.
 *  \param    ids list of application specific stringstream
 *  \param    funcs set of functions used by the driver to control the underlying memory
 *  \return   Zero (0) for success, non-zero for failure
 */
int msc_init(const msc_idstrings_t* ids, const msc_mem_t* funcs);

/**
 *  \brief    Set the specified configuration
 *  \details  Configures the class and endpoints and starts operation. This function should be
 *            called upon configuration from the host.
 *  \param    cfg   configuration to be set
 *  \return   Zero (0) for success, non-zero for failure
 */
int msc_configure(const msc_cfg_t *cfg);

/**
 *  \brief    Clear the current configuration and resets endpoints
 *  \details  Clear the current configuration and resets endpoints.
 *  \return   Zero (0) for success, non-zero for failure
 */
int msc_deconfigure(void);

#endif /* _MSC_H_ */
