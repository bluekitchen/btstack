/*
*********************************************************************************************************
*
*	模块名称 : 蜂鸣器模块
*	文件名称 : bsp_beep.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2012-2013, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_BEEP_H
#define __BSP_BEEP_H

typedef struct _BEEP_T
{
	uint8_t ucEnalbe;
	uint8_t ucState;
	uint16_t usBeepTime;
	uint16_t usStopTime;
	uint16_t usCycle;
	uint16_t usCount;
	uint16_t usCycleCount;
}BEEP_T;

/* 供外部调用的函数声明 */
void BEEP_InitHard(void);
void BEEP_Start(uint16_t _usBeepTime, uint16_t _usStopTime, uint16_t _usCycle);
void BEEP_Stop(void);
void BEEP_KeyTone(void);
void BEEP_Pro(void);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
