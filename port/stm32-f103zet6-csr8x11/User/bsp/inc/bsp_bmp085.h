/*
*********************************************************************************************************
*
*	模块名称 : 气压强度传感器BMP085驱动模块
*	文件名称 : bsp_bmp085.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2012-2013, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_BMP085_H
#define _BSP_BH1750_H

#define BMP085_SLAVE_ADDRESS    0xEE		/* I2C从机地址 */

typedef struct
{
	/* 用于保存芯片内部EEPROM的校准参数 */
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
	
	uint8_t OSS;	/* 过采样值，可有用户自己设定 */

	/* 下面2个单元用于存放计算的真实值 */
	int32_t Temp;	/* 温度值， 单位 0.1摄氏度 */
	int32_t Press;	/* 压力值， 单位 Pa */
}BMP085_T;

extern BMP085_T g_tBMP085;

void bsp_InitBMP085(void);
void BMP085_ReadTempPress(void);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
