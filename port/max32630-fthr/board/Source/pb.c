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
 * $Date: 2017-02-28 17:26:15 -0600 (Tue, 28 Feb 2017) $
 * $Revision: 26771 $
 *
 ******************************************************************************/

#include <stddef.h>
#include "mxc_config.h"
#include "mxc_assert.h"
#include "pb.h"

/******************************************************************************/
int PB_Init(void)
{
    int retval = E_NO_ERROR;
    unsigned int i;

    // Enable pushbutton inputs
    for (i = 0; i < num_pbs; i++) {
        if (GPIO_Config(&pb_pin[i]) != E_NO_ERROR) {
            retval = E_UNKNOWN;
        }
    }

    return retval;
}

/******************************************************************************/
int PB_RegisterCallback(unsigned int pb, pb_callback callback)
{
    MXC_ASSERT(pb < num_pbs);

    if (callback) {
        // Register callback
        GPIO_RegisterCallback(&pb_pin[pb], callback, (void*)pb);

        // Configure and enable interrupt
        GPIO_IntConfig(&pb_pin[pb], GPIO_INT_FALLING_EDGE);
        GPIO_IntEnable(&pb_pin[pb]);
        NVIC_EnableIRQ(MXC_GPIO_GET_IRQ(pb_pin[pb].port));
    } else {
        // Disable interrupt and clear callback
        GPIO_IntDisable(&pb_pin[pb]);
        GPIO_RegisterCallback(&pb_pin[pb], NULL, NULL);
    }

    return E_NO_ERROR;
}

//******************************************************************************
void GPIO_P0_IRQHandler(void)
{
    GPIO_Handler(0);
}
void GPIO_P1_IRQHandler(void)
{
    GPIO_Handler(1);
}
void GPIO_P2_IRQHandler(void)
{
    GPIO_Handler(2);
}
void GPIO_P3_IRQHandler(void)
{
    GPIO_Handler(3);
}
void GPIO_P4_IRQHandler(void)
{
    GPIO_Handler(4);
}
void GPIO_P5_IRQHandler(void)
{
    GPIO_Handler(5);
}
void GPIO_P6_IRQHandler(void)
{
    GPIO_Handler(6);
}
void GPIO_P7_IRQHandler(void)
{
    GPIO_Handler(7);
}
void GPIO_P8_IRQHandler(void)
{
    GPIO_Handler(8);
}
