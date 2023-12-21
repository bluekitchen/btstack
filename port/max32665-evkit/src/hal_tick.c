/*******************************************************************************
 * Copyright (C) Analog Devices, Inc., All Rights Reserved.
 * Author: Ismail H. Kose <ismail.kose@maximintegrated.com>
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
 * property whatsoever. Analog Devices, Inc. retains all
 * ownership rights.
 *******************************************************************************
 */

#include <stdio.h>
#include <stdint.h>
#include "max32665.h"
#include "core_cm4.h"
#include "mxc_delay.h"
#include "mxc_errors.h"
#include "tmr_regs.h"
#include "board.h"
#include "rtc.h"

/***** Definitions *****/
#define USE_RTC_SYSTEM_CLK 0
#define SYSTICK_PERIOD_EXT_CLK 32768

/* Trigger interrupt every second */
static const uint32_t sysTicks = SYSTICK_PERIOD_EXT_CLK;
static volatile uint32_t sys_tick_sec = 0;
/* ************************************************************************** */
int MXC_SYS_SysTick_Config(uint32_t ticks, int clk_src)
{
    if (ticks == 0)
        return E_BAD_PARAM;

    /* If SystemClock, call default CMSIS config and return */
    if (clk_src)
    {
        return SysTick_Config(ticks);
    }
    else
    { /* External clock source requested. Enable RTC clock in run mode*/
        MXC_RTC_Init(0, 0);
        MXC_RTC_Start();

        /* Disable SysTick Timer */
        SysTick->CTRL = 0;
        /* Check reload value for valid */
        if ((ticks - 1) > SysTick_LOAD_RELOAD_Msk)
        {
            /* Reload value impossible */
            return E_BAD_PARAM;
        }
        /* set reload register */
        SysTick->LOAD = ticks - 1;

        /* set Priority for Systick Interrupt */
        NVIC_SetPriority(SysTick_IRQn, (1 << __NVIC_PRIO_BITS) - 1);

        /* Load the SysTick Counter Value */
        SysTick->VAL = 0;

        /* Enable SysTick IRQ and SysTick Timer leaving clock source as external */
        SysTick->CTRL = SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

        /* Function successful */
        return E_NO_ERROR;
    }
}
uint32_t MXC_SYS_SysTick_GetFreq(void)
{
    /* Determine is using internal (SystemCoreClock) or external (32768) clock */
    return SYSTICK_PERIOD_EXT_CLK;
}

int32_t hal_tick_init(void)
{
    uint32_t ret;
    ret = MXC_SYS_SysTick_Config(sysTicks, USE_RTC_SYSTEM_CLK);
    printf("SysTick Clock = %d Hz\n", MXC_SYS_SysTick_GetFreq());
    if (ret != E_NO_ERROR)
    {
        printf("ERROR: Ticks is not valid");
    }

    return ret;
}

void SysTick_Handler(void)
{
    sys_tick_sec++;
}

uint64_t hal_get_tick(void)
{
    uint32_t usec_tick;
    uint64_t tick_sec;
    uint32_t systick_val = SysTick->VAL;
    uint32_t _sys_tick_sec = sys_tick_sec;
    uint32_t sys_freq = MXC_SYS_SysTick_GetFreq();

    usec_tick = ((uint64_t)(sysTicks - systick_val) * 1000000) / sys_freq;
    if (systick_val == 0) // to protect time overflow
        _sys_tick_sec -= 1;
    tick_sec = _sys_tick_sec * 1000000 + usec_tick;
    return tick_sec;
}

void hal_delay_ms(unsigned int ms)
{
    MXC_Delay(1000 * ms);
}

void hal_delay_us(unsigned int us)
{
    MXC_Delay(us);
}

uint32_t hal_get_time_ms(void)
{
    uint32_t usec_tick;
    uint64_t tick_sec;
    uint32_t systick_val = SysTick->VAL;
    uint32_t _sys_tick_sec = sys_tick_sec;
    uint32_t sys_freq = MXC_SYS_SysTick_GetFreq();

    usec_tick = ((uint64_t)(sysTicks - systick_val) * 1000) / sys_freq;
    if (systick_val == 0) // to protect time overflow
        _sys_tick_sec -= 1;
    tick_sec = _sys_tick_sec * 1000 + usec_tick;
    return tick_sec;
}

uint32_t hal_time_ms(void)
{
    return hal_get_time_ms();
}
