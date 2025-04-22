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
 
#ifndef HCI_DRV_H
#define HCI_DRV_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     hciDrvInit
 *
 *  \brief  Initialize the driver.
 */
/*************************************************************************************************/
void hciDrvInit(void);

/*************************************************************************************************/
/*!
 *  \fn     hciDrvWrite
 *
 *  \brief  Write data to the driver.
 *
 *  \param  type     HCI packet type
 *  \param  len      Number of bytes to write.
 *  \param  pData    Byte array to write.
 *
 *  \return Return actual number of data bytes written.
 *
 *  \note   The type parameter allows the driver layer to prepend the data with a header on the
 *          same write transaction.
 */
/*************************************************************************************************/
uint16_t hciDrvWrite(uint8_t type, uint16_t len, uint8_t *pData);

/*************************************************************************************************/
/*!
 *  \fn     hciDrvRead
 *
 *  \brief  Read data bytes from the driver.
 *
 *  \param  len      Number of bytes to read.
 *  \param  pData    Byte array to store data.
 *
 *  \return Return actual number of data bytes read.
 */
/*************************************************************************************************/
uint16_t hciDrvRead(uint16_t len, uint8_t *pData, bool_t last);

/*************************************************************************************************/
/*!
 *  \fn     hciDrvIsr
 *
 *  \brief  Interrupt service routine for IRQ
 */
/*************************************************************************************************/
void hciDrvIsr(void *unused);

/*************************************************************************************************/
/*!
 *  \fn     hciDrvReset
 *
 *  \brief  Resets the controller
 */
/*************************************************************************************************/
void hciDrvReset(void);

#ifdef __cplusplus
};
#endif

#endif /* HCI_DRV_H */
