/*
*********************************************************************************************************
*	                                  
*	模块名称 : 中断模块
*	文件名称 : stm32f10x_it.h
*	版    本 : V2.0
*	说    明 : 头文件
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2011-05-13 armfly  ST固件库V3.4.0版本
*		v2.0    2011-10-16 armfly  ST固件库V3.5.0版本
*
*	Copyright (C), 2010-2011, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __STM32F10x_IT_H
#define __STM32F10x_IT_H

#include "stm32f10x.h"

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

#endif


