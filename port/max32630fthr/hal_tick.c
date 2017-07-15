/*******************************************************************************
* Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
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
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/

#include <stdio.h>
#include "board.h"

/***** Definitions *****/
#define USE_RTC_SYSTEM_CLK 0
#define SYSTICK_PERIOD_EXT_CLK 32767

/* Trigger interrupt every second */
static const uint32_t sysTicks = SYSTICK_PERIOD_EXT_CLK;
static volatile uint32_t sys_tick_sec = 0;

int32_t hal_tick_init(void)
{
	uint32_t ret;
	ret = SYS_SysTick_Config(sysTicks, USE_RTC_SYSTEM_CLK);
    printf("SysTick Clock = %d Hz\n", SYS_SysTick_GetFreq());
    if(ret != E_NO_ERROR) {
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
	uint32_t sys_freq = SYS_SysTick_GetFreq();

	usec_tick = ((uint64_t)(sysTicks - systick_val) * 1000000) / sys_freq;
	if (systick_val == 0) // to protect time overflow
		_sys_tick_sec -= 1;
	tick_sec = _sys_tick_sec * 1000000 + usec_tick;
	return tick_sec;
}

void hal_delay_ms(unsigned int ms)
{
	uint64_t prev_tick = hal_get_tick();
	uint64_t wait_time = ms * 1000;
	uint64_t curr_tick;
	int64_t diff;

	while(1) {
		curr_tick = hal_get_tick();
		diff = curr_tick - prev_tick;
		if (diff > wait_time) {
			break;
		}
	}
}

void hal_delay_us(unsigned int us)
{
	uint64_t prev_tick = hal_get_tick();
	uint64_t wait_time = us;
	uint64_t curr_tick;
	int64_t diff;

	while(1) {
		curr_tick = hal_get_tick();
		diff = curr_tick - prev_tick;
		if (diff > wait_time) {
			break;
		}
	}
}

uint32_t hal_get_time_ms(void)
{
	uint32_t usec_tick;
	uint64_t tick_sec;
	uint32_t systick_val = SysTick->VAL;
	uint32_t _sys_tick_sec = sys_tick_sec;
	uint32_t sys_freq = SYS_SysTick_GetFreq();

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
