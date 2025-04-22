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
#include "mxc_config.h"
#include "spi.h"
#include "gpio.h"
#include "mxc_pins.h"
#include "delay.h"

/***** Definitions *****/
#define delay100ns()  __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();

/***** Global Data *****/
extern const gpio_cfg_t gpio_cfg_em9301_rst;
extern const gpio_cfg_t gpio_cfg_em9301_wu;
#define gpio_cfg_em9301_csn gpio_cfg_em9301_wu
extern const gpio_cfg_t gpio_cfg_em9301_irq;
extern const gpio_int_cfg_t int_cfg_em9301_irq;
extern const gpio_cfg_t gpio_cfg_em9301_miso;
extern const gpio_cfg_t gpio_cfg_em9301_mosi;

/***** File Scope Data *****/
static volatile int reading;
static const spi_cfg_t spi_cfg = { 1, 0, 0, 8, 0, 1000000 };

/***** Function Prototypes *****/
void hciDrvIsr(void*);

/******************************************************************************/
void hciDrvInit(void)
{
  // Setup RST pin
  gpio_out_clr(&gpio_cfg_em9301_rst);
  gpio_cfg(&gpio_cfg_em9301_rst);

  // Setup IRQ pin
  NVIC_DisableIRQ(GPIO0_IRQn);
  gpio_cfg(&gpio_cfg_em9301_irq);
  gpio_int_cfg(&gpio_cfg_em9301_irq, &int_cfg_em9301_irq);
  gpio_register_callback(&gpio_cfg_em9301_irq, hciDrvIsr, NULL);

  // Setup CSN pin
  gpio_out_set(&gpio_cfg_em9301_csn);
  gpio_cfg(&gpio_cfg_em9301_csn);

  // Configure the SPI
  NVIC_DisableIRQ(SPI0_IRQn);
  gpio_cfg(&gpio_cfg_spi0);
  spi_init(MXC_SPI0, &spi_cfg);

  gpio_int_enable(&gpio_cfg_em9301_irq);
  NVIC_EnableIRQ(GPIO0_IRQn);

  reading = 0;
}

/******************************************************************************/
uint16_t hciDrvWrite(uint8_t type, uint16_t len, uint8_t *pData)
{
  uint16_t cnt;

  // disable interrupt to prevent further reads
  gpio_int_disable(&gpio_cfg_em9301_irq);

  // wait for in progress transactions
  while (reading);

  // Set MOSI to '1'
  gpio_out_set(&gpio_cfg_em9301_mosi);
  gpio_cfg(&gpio_cfg_em9301_mosi);

  delay100ns();

  gpio_out_clr(&gpio_cfg_em9301_csn);

  // Switch MOSI to SPI mode
  gpio_cfg(&gpio_cfg_spi0);

  while (!gpio_in_get(&gpio_cfg_em9301_miso));
  if (spi_transfer(MXC_SPI0, &type, NULL, 1, 0) != E_NO_ERROR) {
    return 0;
  }

  for (cnt = 0; cnt < len; cnt++) {
    while (!gpio_in_get(&gpio_cfg_em9301_miso));
    if (spi_transfer(MXC_SPI0, pData, NULL, 1, 0) != E_NO_ERROR) {
      return cnt;
    }
    pData++;
  }

  gpio_out_set(&gpio_cfg_em9301_csn);

  // enable new reads
  gpio_int_enable(&gpio_cfg_em9301_irq);

  return len;
}

/******************************************************************************/
void hciDrvIsr(void *null)
{
  uint8_t txdata, rxdata;

  gpio_int_clr(&gpio_cfg_em9301_irq);

  // 1) The controller sets IRQ line to '1'. This means that the controller has at least 1 byte of data to transmit.
  if (!gpio_in_get(&gpio_cfg_em9301_irq)) {
    return;
  }

  reading = 1;

  // 2) The host shall pull down MOSI signal.
  gpio_out_clr(&gpio_cfg_em9301_mosi);
  gpio_cfg(&gpio_cfg_em9301_mosi);

  // 3) The host shall activate CSN and after 100 ns
  gpio_out_clr(&gpio_cfg_em9301_csn);

  // There is no need for an explicit 100ns delay here. The normal code executes takes more time than that

  // 4) The host starts a SPI transaction by sending a data byte equal to 0x00
  txdata = 0;

  // Read as many characters are available
  do {
    // 5) The host reads data sent by the controller on MISO line.
    if (spi_transfer(MXC_SPI0, &txdata, &rxdata, 1, 0) == E_NO_ERROR) {
      hciTrSerialRxIncoming(&rxdata, 1);
    }
    // 6) If IRQ is set to ‘0’ during an SPI transaction then the controller has no other data to transmit. Once all bit of the transaction are read, the host can stop sending a clock
  } while(gpio_in_get(&gpio_cfg_em9301_irq));

  gpio_out_set(&gpio_cfg_em9301_csn);

  reading = 0;
}

/******************************************************************************/
void hciDrvReset(void)
{
  // disable interrupt to prevent read operations
  gpio_int_disable(&gpio_cfg_em9301_irq);

  // assert reset
  gpio_out_set(&gpio_cfg_em9301_rst);

  // Hold EM3901 in reset for at least 1ms
  delay(MSEC(1));

  // clear reset
  gpio_out_clr(&gpio_cfg_em9301_rst);

  // enable new reads
  gpio_int_enable(&gpio_cfg_em9301_irq);
}
