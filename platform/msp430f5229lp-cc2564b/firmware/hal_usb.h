/*******************************************************************************
    @file hal_usb.h

    Copyright 2008 Texas Instruments, Inc.
***************************************************************************/
#ifndef HAL_USB_H
#define HAL_USB_H


#define USB_PORT_OUT      P4OUT
#define USB_PORT_SEL      P4SEL
#define USB_PORT_DIR      P4DIR
#define USB_PORT_REN      P4REN
#define USB_PIN_TXD       BIT4
#define USB_PIN_RXD       BIT5

/*-------------------------------------------------------------
 *                  Function Prototypes 
 * ------------------------------------------------------------*/ 
void halUsbInit(void);
void halUsbShutDown(void);
void halUsbSendChar(char character);
void halUsbSendString(char string[], unsigned char length);
char halUsbRecvChar();

#endif
