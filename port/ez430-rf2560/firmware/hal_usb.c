/** 
 * @file  hal_usb.c
 * 
 * Copyright 2008 Texas Instruments, Inc.
***************************************************************************/
#include  <msp430x54x.h>
#include "hal_usb.h"


#ifdef USE_IRQ_RX
static char halUsbReceiveBuffer[255];
static unsigned char bufferSize=0;
#endif

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

#ifdef USE_IRQ_RX
  volatile unsigned char i;
  for (i = 0;i < 255; i++){
    halUsbReceiveBuffer[i]='\0';
  }
  bufferSize = 0;
#endif
	  
  USB_PORT_SEL |= USB_PIN_RXD + USB_PIN_TXD;
  USB_PORT_DIR |= USB_PIN_TXD;
  USB_PORT_DIR &= ~USB_PIN_RXD;
  
  UCA3CTL1 |= UCSWRST;                          //Reset State                      
  UCA3CTL0 = UCMODE_0;
  
  UCA3CTL0 &= ~UC7BIT;                      // 8bit char
  UCA3CTL1 |= UCSSEL_2;
    
  // 115200 on 16 Mhz
  // UCA3BR0 = 16;                             // 8Mhz/57600=138
  // UCA3BR1 = 1;
  //  UCA3MCTL = 0xE;

  // 9600 on 16 Mhz - from family user guide 
  UCA3BR1 =  0x06; 
  UCA3BR0 =  0x82;
  UCA3MCTL = 0xC;

  UCA3CTL1 &= ~UCSWRST;  
  // UCA3IE |= UCRXIE;
  // __bis_SR_register(GIE);                   // Enable Interrupts
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
  UCA3IE &= ~UCRXIE;
  UCA3CTL1 = UCSWRST;                          //Reset State                         
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
  while (!(UCA3IFG & UCTXIFG));  
  UCA3TXBUF = character;
}

char halUsbRecvChar(void){
    while (!(UCA3IFG & UCRXIFG));  
    return UCA3RXBUF;
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

#ifdef USE_IRQ_RX
/************************************************************************/
interrupt(USCI_A1_VECTOR) USCI_A1_ISR (void)
{
  halUsbReceiveBuffer[bufferSize++] = UCA3RXBUF;
   __bic_SR_register_on_exit(LPM3_bits);   
}
#endif

// provide putchar used by printf
int putchar(int c){
    halUsbSendChar(c);
    return 1;
}