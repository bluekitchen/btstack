/*
*********************************************************************************************************
*
*	ģ������ : ����ʽ����������ģ��
*	�ļ����� : bsp_touch.h
*	��    �� : V1.6
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_TOUCH_H
#define __BSP_TOUCH_H

#define CALIB_POINT_COUNT	2		/* 2 = 2��У׼�� 4 = �ĵ�У׼ */

#define TOUCH_FIFO_SIZE		20

typedef struct
{
	/* 2��У׼ �� 4��У׼ */
	uint16_t usAdcX1;	/* ���Ͻ� */
	uint16_t usAdcY1;
	uint16_t usAdcX2;	/* ���½� */
	uint16_t usAdcY2;
	uint16_t usAdcX3;	/* ���½� */
	uint16_t usAdcY3;
	uint16_t usAdcX4;	/* ���Ͻ� */
	uint16_t usAdcY4;
	
	uint16_t usLcdX1;	/* ���Ͻ� */
	uint16_t usLcdY1;
	uint16_t usLcdX2;	/* ���½� */
	uint16_t usLcdY2;
	uint16_t usLcdX3;	/* ���½� */
	uint16_t usLcdY3;
	uint16_t usLcdX4;	/* ���Ͻ� */
	uint16_t usLcdY4;	

	uint16_t XYChange;	/* X, Y �Ƿ񽻻�  */

	uint16_t usMaxAdc;	/* ���������ADCֵ��������Ч���ж�. ��СADC = 0  */
	uint16_t usAdcNowX;
	uint16_t usAdcNowY;

	uint8_t Enable;		/* �������ʹ�ܱ�־ */

	uint8_t Event[TOUCH_FIFO_SIZE];	/* �����¼� */
	int16_t XBuf[TOUCH_FIFO_SIZE];	/* �������껺���� */
	int16_t YBuf[TOUCH_FIFO_SIZE];	/* �������껺���� */
	uint8_t Read;					/* ��������ָ�� */
	uint8_t Write;					/* ������дָ�� */
}TOUCH_T;

/* �����¼� */
enum
{
	TOUCH_NONE = 0,		/* �޴��� */
	TOUCH_DOWN = 1,		/* ���� */
	TOUCH_MOVE = 2,		/* �ƶ� */
	TOUCH_RELEASE = 3	/* �ͷ� */
};

/* ���ⲿ���õĺ������� */
void TOUCH_InitHard(void);
void TOUCH_Calibration(void);

uint16_t TOUCH_ReadAdcX(void);
uint16_t TOUCH_ReadAdcY(void);

int16_t TOUCH_GetX(void);
int16_t TOUCH_GetY(void);

uint8_t TOUCH_GetKey(int16_t *_pX, int16_t *_pY);
void TOUCH_CelarFIFO(void);

uint8_t TOUCH_InRect(uint16_t _usX, uint16_t _usY,
uint16_t _usRectX, uint16_t _usRectY, uint16_t _usRectH, uint16_t _usRectW);

void TOUCH_Scan(void);
int32_t TOUCH_Abs(int32_t x);
void TOUCH_PutKey(uint8_t _ucEvent, uint16_t _usX, uint16_t _usY);

extern TOUCH_T g_tTP;

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
