/*******************************************************************************
    @file hal_usb.h

    Copyright 2008 Texas Instruments, Inc.
***************************************************************************/
#ifndef HAL_USB_H
#define HAL_USB_H


#define USB_PORT_OUT      P5OUT
#define USB_PORT_SEL      P5SEL
#define USB_PORT_DIR      P5DIR
#define USB_PORT_REN      P5REN
#define USB_PIN_TXD       BIT6
#define USB_PIN_RXD       BIT7

/*-------------------------------------------------------------
 *                  Function Prototypes 
 * ------------------------------------------------------------*/ 
void halUsbInit(void);
void halUsbShutDown(void);
void halUsbSendChar(char character);
void halUsbSendString(char string[], unsigned char length);
char halUsbRecvChar();

#endif
