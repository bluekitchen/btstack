/*
*********************************************************************************************************
*
*	模块名称 : DHT11 驱动模块(1-wire 数字温湿度传感器）
*	文件名称 : bsp_dht11.h
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_DHT11_H
#define _BSP_DHT11_H

typedef struct
{
	uint8_t Buf[5];
	uint8_t Temp;		/* Temperature 温度 摄氏度 */
	uint8_t Hum;		/* Humidity 湿度 百分比 */
}DHT11_T;

void bsp_InitDHT11(void);
uint8_t DHT11_ReadData(DHT11_T *_pDHT);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
