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
#include "uart.h"
#include "gpio.h"
#include "mxc_pins.h"
#include "delay.h"

/***** Global Data *****/
extern const gpio_cfg_t gpio_cfg_em9301_rst;
extern const gpio_cfg_t gpio_cfg_em9301_wu;

/***** File Scope Data *****/
static const uart_cfg_t uart_cfg = { MXC_E_UART_PAR_NONE, MXC_E_UART_SIZE_8, MXC_E_UART_STOP_1, MXC_E_UART_RTSCTSF_DIS, 115200 };

/******************************************************************************/
void hciDrvInit(void)
{
  // Setup RST pin
  gpio_out_clr(&gpio_cfg_em9301_rst);
  gpio_cfg(&gpio_cfg_em9301_rst);

  // Setup WU pin
  gpio_out_set(&gpio_cfg_em9301_wu);
  gpio_cfg(&gpio_cfg_em9301_wu);

  // Configure the UART
  NVIC_DisableIRQ(UART1_IRQn);
  gpio_cfg(&gpio_cfg_uart1);
  uart_init(MXC_UART1, &uart_cfg);

  // Override some UART settings for receive
  MXC_UART1->ctrl_f.rxthd = 1;
  MXC_UART1->int_en |= MXC_F_UART_INTEN_FFRXIE;

  NVIC_EnableIRQ(UART1_IRQn);
}

/******************************************************************************/
uint16_t hciDrvWrite(uint8_t type, uint16_t len, uint8_t *pData)
{
  int actlen;

  /* write packet type to hardware driver */
  if (uart_write(MXC_UART1, &type, 1, NULL) != E_NO_ERROR) {
    return 0;
  }

  /* write data to hardware driver */
  uart_write(MXC_UART1, pData, len, &actlen);

  return actlen;
}

/******************************************************************************/
void UART1_IRQHandler(void)
{
  unsigned char byte;

  if (MXC_UART1->int_stat & MXC_F_UART_INTSTAT_FFRXIS) {
    MXC_UART1->int_stat = ~MXC_F_UART_INTSTAT_FFRXIS;  // clear the interrupt flag

    // Read as many characters are available
    while (!(MXC_UART1->stat & MXC_F_UART_STAT_RXEMPTY)) {
      MXC_UART1->int_stat = ~MXC_F_UART_INTSTAT_FFRXIS;  // clear the interrupt flag
      byte = MXC_UART1->data;
      hciTrSerialRxIncoming(&byte, 1);
    }
  }
}

/******************************************************************************/
void hciDrvReset(void)
{
  // assert reset
  gpio_out_set(&gpio_cfg_em9301_rst);

  // Hold EM3901 in reset for at least 1ms
  delay(MSEC(1));

  // clear reset
  gpio_out_clr(&gpio_cfg_em9301_rst);
}
