/*
*********************************************************************************************************
*	                                  
*	模块名称 : STM32固件库配置文件。
*	文件名称 : stm32f10x_conf.h 
*	版    本 : V3.5.0
*	说    明 :	这是ST固件库提供的文件。用户可以根据需要包含ST固件库的外设模块。为了方便我们包含了所有固件
*				库模块。
*
*			   这个文件被 Libraries\CMSIS\CM3\DeviceSupport\ST\STM32F10x\stm32f10x.h 包含，因此
*			   我们在.c文件中只需要 include "stm32f10x.h"即可，不必单独再include stm32f10x_conf.h文件
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2011-09-20 armfly  ST固件库升级到V3.4.0版本。
*		v2.0    2011-11-16 armfly  ST固件库升级到V3.5.0版本。
*
*	Copyright (C), 2010-2011, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/  

#ifndef __STM32F10x_CONF_H
#define __STM32F10x_CONF_H

/* 未注释的行表示包含对应的外设头文件 */
#include "stm32f10x_adc.h" 
#include "stm32f10x_bkp.h"
#include "stm32f10x_can.h"
#include "stm32f10x_cec.h"
#include "stm32f10x_crc.h"
#include "stm32f10x_dac.h"
#include "stm32f10x_dbgmcu.h"
#include "stm32f10x_dma.h" 
#include "stm32f10x_exti.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_fsmc.h"
#include "stm32f10x_gpio.h" 
#include "stm32f10x_i2c.h"
#include "stm32f10x_iwdg.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h" 
#include "stm32f10x_rtc.h"
#include "stm32f10x_sdio.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_wwdg.h"
#include "misc.h"   /* 用于NVIC和SysTick的高级函数(与CMSIS相关) */

/*
	用户可以选择是否使能ST固件库的断言供能。使能断言的方法有两种：
	(1) 在C编译器的预定义宏选项中定义USE_FULL_ASSERT。
	(2) 在本文件取消"#define USE_FULL_ASSERT    1"行的注释。
*/
/* 取消下面代码行的注释则固件库代码会展开assert_param宏进行断言 */
#define USE_FULL_ASSERT    1 

#ifdef  USE_FULL_ASSERT
	/* 
		assert_param宏用于函数形参检查。如果expr是false，它将调用assert_failed()函数报告发生错误的源文件
		和行号。如果expr是true，将不执行任何操作。
		
		assert_failed() 函数在stm32f10x_assert.c文件(这是安富莱建立的文件)
	*/
	#define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))

	void assert_failed(uint8_t* file, uint32_t line);
#else
	#define assert_param(expr) ((void)0)
#endif

#endif
