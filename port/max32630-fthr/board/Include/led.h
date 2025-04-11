
/**
 * @file
 * @brief   LED driver API.
 */
/* ****************************************************************************
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
 *************************************************************************** */

#ifndef _LED_H_
#define _LED_H_

#include "mxc_assert.h"
#include "board.h"
#include "gpio.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @ingroup bsp
 * @defgroup led_bsp LED Board Support API.
 * @{
 */
/* **** Definitions **** */
#ifndef LED_OFF
#define LED_OFF     1   /**< Define to turn off the LED. */
#endif

#ifndef LED_ON
#define LED_ON      0   /**< Define to turn on the LED. */
#endif

/* **** Global Variables **** */
extern const gpio_cfg_t led_pin[];
extern const unsigned int num_leds;

/* **** Function Prototypes **** */

/**
 * @brief      Initialize all LED pins.
 * @retval     #E_NO_ERROR   Push buttons intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int LED_Init(void);

/**
 * @brief      Turn the specified LED on.
 * @param      idx   LED index
 */
__STATIC_INLINE void LED_On(unsigned int idx)
{
    MXC_ASSERT(idx < num_leds);
#if (LED_ON == 0)
    GPIO_OutClr(&led_pin[idx]);
#else
    GPIO_OutSet(&led_pin[idx]);
#endif
}

/**
 * @brief      Turn the specified LED off.
 * @param      idx   LED index
 */
__STATIC_INLINE void LED_Off(unsigned int idx)
{
    MXC_ASSERT(idx < num_leds);
#if (LED_ON == 0)
    GPIO_OutSet(&led_pin[idx]);
#else
    GPIO_OutClr(&led_pin[idx]);
#endif
}

/**
 * @brief      Toggle the state of the specified LED.
 * @param      idx   LED index
 */
__STATIC_INLINE void LED_Toggle(unsigned int idx)
{
    MXC_ASSERT(idx < num_leds);
    GPIO_OutToggle(&led_pin[idx]);
}

#ifdef __cplusplus
}
#endif
/**@}*/
#endif /* _LED_H_ */
