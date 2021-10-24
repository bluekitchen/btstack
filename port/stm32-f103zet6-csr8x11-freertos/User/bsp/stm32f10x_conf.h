/*
*********************************************************************************************************
*	                                  
*	ģ������ : STM32�̼��������ļ���
*	�ļ����� : stm32f10x_conf.h 
*	��    �� : V3.5.0
*	˵    �� :	����ST�̼����ṩ���ļ����û����Ը�����Ҫ����ST�̼��������ģ�顣Ϊ�˷������ǰ��������й̼�
*				��ģ�顣
*
*			   ����ļ��� Libraries\CMSIS\CM3\DeviceSupport\ST\STM32F10x\stm32f10x.h ���������
*			   ������.c�ļ���ֻ��Ҫ include "stm32f10x.h"���ɣ����ص�����include stm32f10x_conf.h�ļ�
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		v1.0    2011-09-20 armfly  ST�̼���������V3.4.0�汾��
*		v2.0    2011-11-16 armfly  ST�̼���������V3.5.0�汾��
*
*	Copyright (C), 2010-2011, ���������� www.armfly.com
*
*********************************************************************************************************
*/  

#ifndef __STM32F10x_CONF_H
#define __STM32F10x_CONF_H

/* δע�͵��б�ʾ������Ӧ������ͷ�ļ� */
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
#include "misc.h"   /* ����NVIC��SysTick�ĸ߼�����(��CMSIS���) */

/*
	�û�����ѡ���Ƿ�ʹ��ST�̼���Ķ��Թ��ܡ�ʹ�ܶ��Եķ��������֣�
	(1) ��C��������Ԥ�����ѡ���ж���USE_FULL_ASSERT��
	(2) �ڱ��ļ�ȡ��"#define USE_FULL_ASSERT    1"�е�ע�͡�
*/
/* ȡ����������е�ע����̼�������չ��assert_param����ж��� */
#define USE_FULL_ASSERT    1 

#ifdef  USE_FULL_ASSERT
	/* 
		assert_param�����ں����βμ�顣���expr��false����������assert_failed()�������淢�������Դ�ļ�
		���кš����expr��true������ִ���κβ�����
		
		assert_failed() ������stm32f10x_assert.c�ļ�(���ǰ������������ļ�)
	*/
	#define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))

	void assert_failed(uint8_t* file, uint32_t line);
#else
	#define assert_param(expr) ((void)0)
#endif

#endif
