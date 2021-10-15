/*
*********************************************************************************************************
*
*	模块名称 : 步进电机驱动模块
*	文件名称 : bsp_step_moto.h
*	说    明 : 头文件
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_STEMP_MOTO_H
#define _BSP_STEMP_MOTO_H

typedef struct
{
	uint8_t Dir;		/* 0 表示正转  1 表示反转 */
	uint32_t StepFreq;	/* 步进频率 */
	uint32_t StepCount;	/* 剩余步数 */
	uint8_t Running;	/* 1表示正在旋转 0 表示停机 */
	uint8_t Pos;		/* 线圈通电的相序，0-7 */
}MOTO_T;

void bsp_InitStepMoto(void);
void MOTO_Start(uint32_t _speed, uint8_t _dir, uint32_t _stpes);
void MOTO_Stop(void);
void MOTO_Pause(void);
void MOTO_ShangeSpeed(uint32_t _speed);
uint32_t MOTO_RoudToStep(void);

extern MOTO_T g_tMoto;

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
