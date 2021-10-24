/*
*********************************************************************************************************
*
*	ģ������ : ��ѹǿ�ȴ�����BMP085����ģ��
*	�ļ����� : bsp_bmp085.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2012-2013, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_BMP085_H
#define _BSP_BH1750_H

#define BMP085_SLAVE_ADDRESS    0xEE		/* I2C�ӻ���ַ */

typedef struct
{
	/* ���ڱ���оƬ�ڲ�EEPROM��У׼���� */
	int16_t AC1;
	int16_t AC2;
	int16_t AC3;
	uint16_t AC4;
	uint16_t AC5;
	uint16_t AC6;
	int16_t B1;
	int16_t B2;
	int16_t MB;
	int16_t MC;
	int16_t MD;
	
	uint8_t OSS;	/* ������ֵ�������û��Լ��趨 */

	/* ����2����Ԫ���ڴ�ż������ʵֵ */
	int32_t Temp;	/* �¶�ֵ�� ��λ 0.1���϶� */
	int32_t Press;	/* ѹ��ֵ�� ��λ Pa */
}BMP085_T;

extern BMP085_T g_tBMP085;

void bsp_InitBMP085(void);
void BMP085_ReadTempPress(void);

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
