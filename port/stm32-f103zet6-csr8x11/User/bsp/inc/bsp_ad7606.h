/*
*********************************************************************************************************
*
*	ģ������ : AD7606���ݲɼ�ģ��
*	�ļ����� : bsp_ad7606.h
*	��    �� : V1.0
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_AD7606_H
#define _BSP_AD7606_H

/* ���������� */
typedef enum
{
	AD_OS_NO = 0,
	AD_OS_X2 = 1,
	AD_OS_X4 = 2,
	AD_OS_X8 = 3,
	AD_OS_X16 = 4,
	AD_OS_X32 = 5,
	AD_OS_X64 = 6
}AD7606_OS_E;


/* AD���ݲɼ������� FIFO */
#define ADC_FIFO_SIZE	(2*1024)	/* ���������� */

typedef struct
{
	uint8_t ucOS;			/* ���������ʣ�0 - 6. 0��ʾ�޹����� */
	uint8_t ucRange;		/* �������̣�0��ʾ����5V, 1��ʾ����10V */
	int16_t sNowAdc[8];		/* ��ǰADCֵ, �з����� */
}AD7606_VAR_T;

typedef struct
{
	/* FIFO �ṹ */
	uint16_t usRead;		/* ��ָ�� */
	uint16_t usWrite;		/* дָ�� */

	uint16_t usCount;		/* �����ݸ��� */
	uint8_t ucFull;			/* FIFO����־ */

	int16_t  sBuf[ADC_FIFO_SIZE];
}AD7606_FIFO_T;

void bsp_InitAD7606(void);
void AD7606_SetOS(uint8_t _ucOS);
void AD7606_SetInputRange(uint8_t _ucRange);
void AD7606_Reset(void);
void AD7606_StartConvst(void);
void AD7606_ReadNowAdc(void);

/* ����ĺ�������FIFO����ģʽ */
void AD7606_EnterAutoMode(uint32_t _ulFreq);
void AD7606_StartRecord(uint32_t _ulFreq);
void AD7606_StopRecord(void);
uint8_t AD7606_FifoNewData(void);
uint8_t AD7606_ReadFifo(uint16_t *_usReadAdc);
uint8_t AD7606_FifoFull(void);


/* ȫ�ֱ��� */
extern AD7606_VAR_T g_tAD7606;
extern AD7606_FIFO_T g_tAdcFifo;

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
