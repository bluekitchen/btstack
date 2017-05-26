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
 * $Date: 2016-03-17 14:27:29 -0700 (Thu, 17 Mar 2016) $
 * $Revision: 21966 $
 *
 ******************************************************************************/

#include <stdio.h>
#include "mxc_config.h"
#include "mxc_assert.h"
#include "board.h"
#include "gpio.h"
#include "uart.h"
#include "spim.h"
#include "max14690n.h"

/***** Global Variables *****/

// LEDs
// Note: EvKit board uses 3.3v supply so these must be open-drain.
const gpio_cfg_t led_pin[] = {
    { PORT_2, PIN_4, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN },
    { PORT_2, PIN_5, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN },
    { PORT_2, PIN_6, GPIO_FUNC_GPIO, GPIO_PAD_OPEN_DRAIN },
};
const unsigned int num_leds = (sizeof(led_pin) / sizeof(gpio_cfg_t));

// Pushbuttons
const gpio_cfg_t pb_pin[] = {
    { PORT_2, PIN_3, GPIO_FUNC_GPIO, GPIO_PAD_INPUT_PULLUP },
};
const unsigned int num_pbs = (sizeof(pb_pin) / sizeof(gpio_cfg_t));

// Console UART configuration
const uart_cfg_t console_uart_cfg = {
    .parity = UART_PARITY_DISABLE,
    .size = UART_DATA_SIZE_8_BITS,
    .extra_stop = 0,
    .cts = 0,
    .rts = 0,
    .baud = CONSOLE_BAUD,
};
const sys_cfg_uart_t console_sys_cfg = {
    .clk_scale = CLKMAN_SCALE_AUTO,
    .io_cfg = IOMAN_UART(CONSOLE_UART, IOMAN_MAP_A, IOMAN_MAP_UNUSED, IOMAN_MAP_UNUSED, 1, 0, 0)
};

// MAX14690 PMIC
const ioman_cfg_t max14690_io_cfg = IOMAN_I2CM2(IOMAN_MAP_A, 1);
const gpio_cfg_t max14690_int   = { PORT_3, PIN_7, GPIO_FUNC_GPIO, GPIO_PAD_INPUT_PULLUP };
const gpio_cfg_t max14690_mpc0  = { PORT_2, PIN_7, GPIO_FUNC_GPIO, GPIO_PAD_NORMAL };

/***** File Scope Variables *****/

/******************************************************************************/
void mxc_assert(const char *expr, const char *file, int line)
{
    printf("MXC_ASSERT %s #%d: (%s)\n", file, line, expr);
    while (1);
}

/******************************************************************************/
int Board_Init(void)
{
    int err;

    if ((err = Console_Init()) != E_NO_ERROR) {
        MXC_ASSERT_FAIL();
        return err;
    }

    if ((err = LED_Init()) != E_NO_ERROR) {
        MXC_ASSERT_FAIL();
        return err;
    }

    if ((err = PB_Init()) != E_NO_ERROR) {
        MXC_ASSERT_FAIL();
        return err;
    }

    /* On the Pegasus board MPC1 is connected to CAP which is high when VBUS is present.
     * The LDO_OUTPUT_MPC1 setting will automatically enable the output when VBUS is present.
     * The LDO_OUTPUT_MPC1 setting will also disable the output when powered from the battery.
     * The Pegasus board uses LDO2 for VDDB (USB), LEDs and the SD card connector.
     * Use the MAX14690_LDO2setMode(mode) function to enable LDO2 when needed.
     */
    if ((err = MAX14690N_Init(3.3, LDO_OUTPUT_MPC1, 3.3, LDO_OUTPUT_DISABLED)) != E_NO_ERROR) {
        MXC_ASSERT_FAIL();
        return err;
    }

    return E_NO_ERROR;
}

/******************************************************************************/
int Console_Init(void)
{
    int err;

    if ((err = UART_Init(MXC_UART_GET_UART(CONSOLE_UART), &console_uart_cfg, &console_sys_cfg)) != E_NO_ERROR) {
        MXC_ASSERT_FAIL();
        return err;
    }

    return E_NO_ERROR;
}

/******************************************************************************/
int Console_PrepForSleep(void)
{
    fflush(stdout);
    return UART_PrepForSleep(MXC_UART_GET_UART(CONSOLE_UART));
}
