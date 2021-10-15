/*
*********************************************************************************************************
*
*	模块名称 : STM32内部RTC
*	文件名称 : bsp_cpu_rtc.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2015-08-08 armfly  首版
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_CPU_RTC_H
#define __BSP_CPU_RTC_H
#include "stm32f10x.h"

typedef struct
{
	uint16_t Year;
	uint8_t Mon;
	uint8_t Day;	
	uint8_t Hour;		
	uint8_t Min;				
	uint8_t Sec;					
	uint8_t Week;	
}RTC_T;

void bsp_InitRTC(void);
uint8_t RTC_WriteClock(uint16_t _year, uint8_t _mon, uint8_t _day, uint8_t _hour, uint8_t _min, uint8_t _sec);
void RTC_ReadClock(void);
uint8_t RTC_CalcWeek(uint16_t _year, uint8_t _mon, uint8_t _day);

extern RTC_T g_tRTC;

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/

