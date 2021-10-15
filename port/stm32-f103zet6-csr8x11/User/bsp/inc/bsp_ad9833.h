/*
*********************************************************************************************************
*
*	模块名称 : AD9833 驱动模块(单通道带16位DAC)
*	文件名称 : bsp_adc9833.c
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_AD9833_H
#define _BSP_AD9833_H

#define AD9833_MAX_FREQ		125000000	/* 最大频率 12.5MHz， 单位 0.1Hz */

/* 定义波形类型 */
typedef  enum
{
	NONE_WAVE = 0,	/* 无输出 */
	TRI_WAVE,		/* 输出三角波 */
	SINE_WAVE,		/* 输出正弦波 */
	SQU_WAVE		/* 输出方波 */	
}AD9833_WAVE_E;

void bsp_InitAD9833(void);

void AD9833_SetWaveFreq(uint32_t _WaveFreq);
void AD9833_SelectWave(AD9833_WAVE_E _type);

void AD9833_WriteFreqReg(uint8_t _mode, uint32_t _freq_reg);
void AD9833_WritePhaseReg(uint8_t _mode, uint32_t _phase_reg);

#endif


/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
