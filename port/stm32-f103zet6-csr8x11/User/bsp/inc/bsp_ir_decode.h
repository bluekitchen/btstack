/*
*********************************************************************************************************
*
*	ģ������ : ����ң�ؽ���������ģ��
*	�ļ����� : bsp_ir_decode.h
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_IR_DECODE_H
#define _BSP_IR_DECODE_H

/* �������ң������������, ��bsp_key.h ������������ͳһ���� */
typedef enum
{
	IR_KEY_STRAT 	= 0x80,
	IR_KEY_POWER 	= IR_KEY_STRAT + 0x45,
	IR_KEY_MENU 	= IR_KEY_STRAT + 0x47, 
	IR_KEY_TEST 	= IR_KEY_STRAT + 0x44,
	IR_KEY_UP 		= IR_KEY_STRAT + 0x40,
	IR_KEY_RETURN	= IR_KEY_STRAT + 0x43,
	IR_KEY_LEFT		= IR_KEY_STRAT + 0x07,
	IR_KEY_OK		= IR_KEY_STRAT + 0x15,
	IR_KEY_RIGHT	= IR_KEY_STRAT + 0x09,
	IR_KEY_0		= IR_KEY_STRAT + 0x16,
	IR_KEY_DOWN		= IR_KEY_STRAT + 0x19,
	IR_KEY_C		= IR_KEY_STRAT + 0x0D,
	IR_KEY_1		= IR_KEY_STRAT + 0x0C,
	IR_KEY_2		= IR_KEY_STRAT + 0x18,
	IR_KEY_3		= IR_KEY_STRAT + 0x5E,
	IR_KEY_4		= IR_KEY_STRAT + 0x08,
	IR_KEY_5		= IR_KEY_STRAT + 0x1C,
	IR_KEY_6		= IR_KEY_STRAT + 0x5A,
	IR_KEY_7		= IR_KEY_STRAT + 0x42,
	IR_KEY_8		= IR_KEY_STRAT + 0x52,
	IR_KEY_9		= IR_KEY_STRAT + 0x4A,	
}IR_KEY_E;

typedef struct
{
	uint16_t LastCapture;
	uint8_t Status;
	uint8_t RxBuf[4];
	uint8_t RepeatCount;
	
	uint8_t WaitFallEdge;	/* 0 ��ʾ�ȴ������أ�1��ʾ�ȴ��½��أ������л����벶���� */
	uint16_t TimeOut;
}IRD_T;

void bsp_InitIRD(void);
void IRD_StartWork(void);
void IRD_StopWork(void);

extern IRD_T g_tIR;

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
