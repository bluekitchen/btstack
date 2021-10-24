/*
*********************************************************************************************************
*
*	ģ������ : �����������ģ��
*	�ļ����� : bsp_step_moto.h
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_STEMP_MOTO_H
#define _BSP_STEMP_MOTO_H

typedef struct
{
	uint8_t Dir;		/* 0 ��ʾ��ת  1 ��ʾ��ת */
	uint32_t StepFreq;	/* ����Ƶ�� */
	uint32_t StepCount;	/* ʣ�ಽ�� */
	uint8_t Running;	/* 1��ʾ������ת 0 ��ʾͣ�� */
	uint8_t Pos;		/* ��Ȧͨ�������0-7 */
}MOTO_T;

void bsp_InitStepMoto(void);
void MOTO_Start(uint32_t _speed, uint8_t _dir, uint32_t _stpes);
void MOTO_Stop(void);
void MOTO_Pause(void);
void MOTO_ShangeSpeed(uint32_t _speed);
uint32_t MOTO_RoudToStep(void);

extern MOTO_T g_tMoto;

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
