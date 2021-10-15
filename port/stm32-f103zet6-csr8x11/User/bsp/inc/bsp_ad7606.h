/*
*********************************************************************************************************
*
*	模块名称 : AD7606数据采集模块
*	文件名称 : bsp_ad7606.h
*	版    本 : V1.0
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_AD7606_H
#define _BSP_AD7606_H

/* 过采样倍率 */
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


/* AD数据采集缓冲区 FIFO */
#define ADC_FIFO_SIZE	(2*1024)	/* 总体样本数 */

typedef struct
{
	uint8_t ucOS;			/* 过采样倍率，0 - 6. 0表示无过采样 */
	uint8_t ucRange;		/* 输入量程，0表示正负5V, 1表示正负10V */
	int16_t sNowAdc[8];		/* 当前ADC值, 有符号数 */
}AD7606_VAR_T;

typedef struct
{
	/* FIFO 结构 */
	uint16_t usRead;		/* 读指针 */
	uint16_t usWrite;		/* 写指针 */

	uint16_t usCount;		/* 新数据个数 */
	uint8_t ucFull;			/* FIFO满标志 */

	int16_t  sBuf[ADC_FIFO_SIZE];
}AD7606_FIFO_T;

void bsp_InitAD7606(void);
void AD7606_SetOS(uint8_t _ucOS);
void AD7606_SetInputRange(uint8_t _ucRange);
void AD7606_Reset(void);
void AD7606_StartConvst(void);
void AD7606_ReadNowAdc(void);

/* 下面的函数用于FIFO操作模式 */
void AD7606_EnterAutoMode(uint32_t _ulFreq);
void AD7606_StartRecord(uint32_t _ulFreq);
void AD7606_StopRecord(void);
uint8_t AD7606_FifoNewData(void);
uint8_t AD7606_ReadFifo(uint16_t *_usReadAdc);
uint8_t AD7606_FifoFull(void);


/* 全局变量 */
extern AD7606_VAR_T g_tAD7606;
extern AD7606_FIFO_T g_tAdcFifo;

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
