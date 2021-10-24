/*
*********************************************************************************************************
*
*	ģ������ : WM8978��ƵоƬ����ģ��
*	�ļ����� : bsp_wm8978.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-03-01 armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_WM8978_H
#define _BSP_WM8978_H

#define WM8978_SLAVE_ADDRESS    0x34	/* WM8978 I2C�ӻ���ַ */

/* WM8978 ��Ƶ����ͨ������ѡ��, ����ѡ���·������ MIC_LEFT_ON | LINE_ON */
typedef enum
{
	IN_PATH_OFF		= 0x00,	/* ������ */
	MIC_LEFT_ON 	= 0x01,	/* LIN,LIP�ţ�MIC���������Ӱ�����ͷ�� */
	MIC_RIGHT_ON	= 0x02,	/* RIN,RIP�ţ�MIC����������GPRSģ����Ƶ����� */
	LINE_ON			= 0x04, /* L2,R2 ���������� */
	AUX_ON			= 0x08,	/* AUXL,AUXR ���������� */
	DAC_ON			= 0x10,	/* I2S����DAC (CPU������Ƶ�ź�) */
	ADC_ON			= 0x20	/* �������Ƶ����WM8978�ڲ�ADC ��I2S¼��) */
}IN_PATH_E;

/* WM8978 ��Ƶ���ͨ������ѡ��, ����ѡ���· */
typedef enum
{
	OUT_PATH_OFF	= 0x00,	/* ����� */
	EAR_LEFT_ON 	= 0x01,	/* LOUT1 ���������� */
	EAR_RIGHT_ON	= 0x02,	/* ROUT1 ���������� */
	SPK_ON			= 0x04,	/* LOUT2��ROUT2�������������,�������� */
	OUT3_4_ON		= 0x08,	/* OUT3 �� OUT4 �����������Ƶ�� ��GSMģ�����Ƶ���� */
}OUT_PATH_E;

/* ����������� */
#define VOLUME_MAX		63		/* ������� */
#define VOLUME_STEP		1		/* �������ڲ��� */

/* �������MIC���� */
#define GAIN_MAX		63		/* ������� */
#define GAIN_STEP		1		/* ���沽�� */


/* ���ⲿ���õĺ������� */
uint8_t wm8978_Init(void);

void wm8978_CfgAudioIF(uint16_t _usStandard, uint8_t _ucWordLen, uint16_t _usMode);
void wm8978_OutMute(uint8_t _ucMute);
void wm8978_PowerDown(void);

void wm8978_CfgAudioPath(uint16_t _InPath, uint16_t _OutPath);

void wm8978_SetMicGain(uint8_t _ucGain);
void wm8978_SetLineGain(uint8_t _ucGain);
void wm8978_SetSpkVolume(uint8_t _ucVolume);
void wm8978_SetEarVolume(uint8_t _ucVolume);
uint8_t wm8978_ReadEarVolume(void);
uint8_t wm8978_ReadSpkVolume(void);

void wm8978_NotchFilter(uint16_t _NFA0, uint16_t _NFA1);

void I2S_CODEC_Init(void);
void I2S_StartPlay(uint16_t _usStandard, uint16_t _usWordLen,  uint32_t _uiAudioFreq);
void I2S_StartRecord(uint16_t _usStandard, uint16_t _usWordLen,  uint32_t _uiAudioFreq);
void I2S_Stop(void);
#endif
