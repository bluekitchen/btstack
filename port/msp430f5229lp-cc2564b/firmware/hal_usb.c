/** 
 * @file  hal_usb.c
 * 
 * Copyright 2008 Texas Instruments, Inc.
***************************************************************************/
#include  <msp430.h>
#include "hal_usb.h"

/************************************************************************
 * @brief  Initializes the serial communications peripheral and GPIO ports 
 *         to communicate with the TUSB3410.
 * 
 * @param  none
 * 
 * @return none
 **************************************************************************/
void halUsbInit(void)
{
	  
  USB_PORT_SEL |= USB_PIN_RXD + USB_PIN_TXD;
  USB_PORT_DIR |= USB_PIN_TXD;
  USB_PORT_DIR &= ~USB_PIN_RXD;
  
  UCA1CTL1 |= UCSWRST;                          //Reset State                      
  UCA1CTL0 = UCMODE_0;
  
  UCA1CTL0 &= ~UC7BIT;                      // 8bit char
  UCA1CTL1 |= UCSSEL_2;
#if 0
  // 57600 @ 16 Mhz
  UCA1BR0 = 16;
  UCA1BR1 = 1;
#else
  // 115200 @ 16 Mhz
  UCA1BR0 = 138;
  UCA1BR1 = 0;
#endif
  UCA1MCTL = 0xE;
  UCA1CTL1 &= ~UCSWRST;  
}

/************************************************************************
 * @brief  Disables the serial communications peripheral and clears the GPIO
 *         settings used to communicate with the TUSB3410.
 * 
 * @param  none
 * 
 * @return none
 **************************************************************************/
void halUsbShutDown(void)
{
  UCA1IE &= ~UCRXIE;
  UCA1CTL1 = UCSWRST;                          //Reset State                         
  USB_PORT_SEL &= ~( USB_PIN_RXD + USB_PIN_TXD );
  USB_PORT_DIR |= USB_PIN_TXD;
  USB_PORT_DIR |= USB_PIN_RXD;
  USB_PORT_OUT &= ~(USB_PIN_TXD + USB_PIN_RXD);
}

/************************************************************************
 * @brief  Sends a character over UART to the TUSB3410.
 * 
 * @param  character The character to be sent. 
 * 
 * @return none
 **************************************************************************/
void halUsbSendChar(char character)
{
  while (!(UCA1IFG & UCTXIFG));  
  UCA1TXBUF = character;
}

char halUsbRecvChar(void){
    while (!(UCA1IFG & UCRXIFG));  
    return UCA1RXBUF;
}

/************************************************************************
 * @brief  Sends a string of characters to the TUSB3410
 * 
 * @param  string[] The array of characters to be transmit to the TUSB3410.
 * 
 * @param  length   The length of the string.
 * 
 * @return none
 **************************************************************************/
void halUsbSendString(char string[], unsigned char length)
{
  volatile unsigned char i;
  for (i=0; i < length; i++)
    halUsbSendChar(string[i]);  
}

// provide putchar used by printf
int putchar(int c){
    halUsbSendChar(c);
    return 1;
}