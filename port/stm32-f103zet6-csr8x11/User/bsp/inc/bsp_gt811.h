/*
*********************************************************************************************************
*
*	ģ������ : GT811���ݴ���оƬ��������
*	�ļ����� : bsp_ct811.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_GT811_H
#define _BSP_GT811_H

#define GT811_I2C_ADDR	0xBA

typedef struct
{
	uint8_t Enable;
	uint8_t TimerCount;
	
	uint8_t TouchpointFlag;
	uint8_t Touchkeystate;

	uint16_t X0;
	uint16_t Y0;
	uint8_t P0;

	uint16_t X1;
	uint16_t Y1;
	uint8_t P1;

	uint16_t X2;
	uint16_t Y2;
	uint8_t P2;

	uint16_t X3;
	uint16_t Y3;
	uint8_t P3;

	uint16_t X4;
	uint16_t Y4;
	uint8_t P4;
}GT811_T;

void GT811_InitHard(void);
uint16_t GT811_ReadVersion(void);
void GT811_Scan(void);

extern GT811_T g_GT811;

#endif
