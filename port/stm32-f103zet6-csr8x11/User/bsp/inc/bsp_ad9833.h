/*
*********************************************************************************************************
*
*	ģ������ : AD9833 ����ģ��(��ͨ����16λDAC)
*	�ļ����� : bsp_adc9833.c
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_AD9833_H
#define _BSP_AD9833_H

#define AD9833_MAX_FREQ		125000000	/* ���Ƶ�� 12.5MHz�� ��λ 0.1Hz */

/* ���岨������ */
typedef  enum
{
	NONE_WAVE = 0,	/* ����� */
	TRI_WAVE,		/* ������ǲ� */
	SINE_WAVE,		/* ������Ҳ� */
	SQU_WAVE		/* ������� */	
}AD9833_WAVE_E;

void bsp_InitAD9833(void);

void AD9833_SetWaveFreq(uint32_t _WaveFreq);
void AD9833_SelectWave(AD9833_WAVE_E _type);

void AD9833_WriteFreqReg(uint8_t _mode, uint32_t _freq_reg);
void AD9833_WritePhaseReg(uint8_t _mode, uint32_t _phase_reg);

#endif


/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
