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
 
#include "wsf_types.h"
#include "hci_drv.h"
#include "hci_tr.h"
#include "hci_cfg.h"

#define SPI_ENABLE_MASK       0x01  // SPICNn register bit mask
#define SPI_INT_ENABLE_MASK   0x80  // SPICFn register bit mask
#define XFER_COMPLETE_FLAG    0x40  // SPICNn register bit mask
#define STBY_FLAG             0x80  // SPICNn register bit mask

static volatile int reading;

#define spi_enable()    HCI_SPICN |= SPI_ENABLE_MASK
#define spi_disable()   HCI_SPICN &= ~SPI_ENABLE_MASK

/******************************************************************************/
static uint16_t spi_sendrecv(uint16_t data)
{
  HCI_SPICN &= ~XFER_COMPLETE_FLAG;  // clear SPIC flag
  HCI_SPIB = data; // data to send

  while (HCI_SPICN & STBY_FLAG); // wait for transfer to complete

  data = HCI_SPIB; // receive data from buffer
  HCI_SPICN &= ~XFER_COMPLETE_FLAG;  // clear SPIC flag

  return data;
}

/******************************************************************************/
void hciDrvInit(void)
{
  HCI_RST_POn &= ~HCI_RST_MASK;   // set low
  HCI_RST_PDn |= HCI_RST_MASK;    // set as output

  HCI_IRQ_POn &= ~HCI_IRQ_MASK;   // disable pull-up
  HCI_IRQ_PDn &= ~HCI_IRQ_MASK;   // configure as input
  HCI_IRQ_EIESn &= ~HCI_IRQ_MASK; // positive edge triggered
  HCI_IRQ_EIEn &= ~HCI_IRQ_MASK;  // disable interrupt

  HCI_CSN_POn |= HCI_CSN_MASK;    // set high
  HCI_CSN_PDn |= HCI_CSN_MASK;    // set as output

  HCI_MOSI_POn |= HCI_MOSI_MASK;  // set high
  HCI_MOSI_PDn |= HCI_MOSI_MASK;  // set as output

  HCI_SCK_PDn |= HCI_SCK_MASK; // set as output
  
  HCI_SPICN = 0x02;  // configure SPI in master mode, clear all flags
  HCI_SPICF = 0x00;  // configure for 8-bit mode 0
  HCI_SPICK = 1;

  reading = 0;
}

/******************************************************************************/
uint16_t hciDrvWrite(uint8_t type, uint16_t len, uint8_t *pData)
{
  uint16_t cnt;

  // disable interrupt to prevent further reads
  HCI_IRQ_EIEn &= ~HCI_IRQ_MASK;

  // wait for in progress transactions
  while (reading);

  // Set MOSI to '1'
  HCI_MOSI_POn |= HCI_MOSI_MASK;
  while (!(HCI_MOSI_PIn & HCI_MOSI_MASK));

  // There is no need for an explicit 100ns delay here. The normal code executes takes more time than that

  HCI_CSN_POn &= ~HCI_CSN_MASK;
  spi_enable();


  while (!(HCI_MISO_PIn & HCI_MISO_MASK));
  spi_sendrecv(type);

  for (cnt = 0; cnt < len; cnt++) {
    while (!(HCI_MISO_PIn & HCI_MISO_MASK));
    spi_sendrecv(*pData);
    pData++;
  }

  HCI_CSN_POn |= HCI_CSN_MASK;
  spi_disable();

  // enable new reads
  HCI_IRQ_EIEn |= HCI_IRQ_MASK;

  return len;
}

/******************************************************************************/
#pragma vector = HCI_IRQ_VECTOR
__interrupt void hciDrvIsr(void)
{
  uint8_t rxdata;

  if (HCI_IRQ_EIFn & HCI_IRQ_MASK) {

    // Clear interrupt
    HCI_IRQ_EIFn &= ~HCI_IRQ_MASK;

    // 1) The controller sets IRQ line to '1'. This means that the controller has at least 1 byte of data to transmit.
    if (!(HCI_IRQ_PIn & HCI_IRQ_MASK)) {
      return;
    }

    reading = 1;

    // 2) The host shall pull down MOSI signal.
    HCI_MOSI_POn &= ~HCI_MOSI_MASK;
    while (HCI_MOSI_PIn & HCI_MOSI_MASK);

    // 3) The host shall activate CSN and after 100 ns
    HCI_CSN_POn &= ~HCI_CSN_MASK;

    // There is no need for an explicit 100ns delay here. The normal code executes takes more time than that
    spi_enable();

    // 4) The host starts a SPI transaction by sending a data byte equal to 0x00
    // Read as many characters are available
    do {
      // 5) The host reads data sent by the controller on MISO line.
      rxdata = spi_sendrecv(0);

      hciTrSerialRxIncoming(&rxdata, 1);

      // 6) If IRQ is set to ‘0’ during an SPI transaction then the controller has no other data to transmit. Once all bit of the transaction are read, the host can stop sending a clock
    } while(HCI_IRQ_PIn & HCI_IRQ_MASK);

    HCI_CSN_POn |= HCI_CSN_MASK;
    spi_disable();

    reading = 0;
  }
}

/******************************************************************************/
void hciDrvReset(void)
{
  volatile unsigned int counter;

  // disable interrupt to prevent read operations
  HCI_IRQ_EIEn &= ~HCI_IRQ_MASK;

  // assert reset
  HCI_RST_POn |= HCI_RST_MASK;

  // Hold EM3901 in reset for at least 1ms
  for (counter = 0; counter < 2000; counter++);

  // clear reset
  HCI_RST_POn &= ~HCI_RST_MASK;

  // enable new reads
  HCI_IRQ_EIEn |= HCI_IRQ_MASK;
}
