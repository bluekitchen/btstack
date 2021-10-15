/*
*********************************************************************************************************
*
*	模块名称 : 外部SRAM驱动模块
*	文件名称 : bsp_fsmc_sram.c
*	版    本 : V2.4
*	说    明 : 安富莱STM32-V4开发板标配的SRAM为 1M字节
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2015-05-23  armfly  正式发布
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
*********************************************************************************************************
*	函 数 名: bsp_InitExtSRAM
*	功能说明: 配置连接外部SRAM的GPIO和FSMC
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitExtSRAM(void)
{
	FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef  p;
	GPIO_InitTypeDef GPIO_InitStructure; 

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);
		
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOG | RCC_APB2Periph_GPIOE |
	                     RCC_APB2Periph_GPIOF, ENABLE);
	
	/* 配置SRAM数据线 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9 |
	                            GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure); 
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
	                            GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | 
	                            GPIO_Pin_15;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	
	/* 配置SRAM 地址线 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | 
	                            GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_12 | GPIO_Pin_13 | 
	                            GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOF, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | 
	                            GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOG, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13; 
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	
	/* 配置 NOE 和 NWE */  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 |GPIO_Pin_5;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	
	/* 配置 NE3 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; 
	GPIO_Init(GPIOG, &GPIO_InitStructure);
	
	/* 配置 NBL0, NBL1 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1; 
	GPIO_Init(GPIOE, &GPIO_InitStructure); 
	
	/* 配置FSMC */
	p.FSMC_AddressSetupTime = 0;
	p.FSMC_AddressHoldTime = 0;
	p.FSMC_DataSetupTime = 3;           /* 根据SRAM的最大速度进行调整  */
	p.FSMC_BusTurnAroundDuration = 0;
	p.FSMC_CLKDivision = 0;
	p.FSMC_DataLatency = 0;
	p.FSMC_AccessMode = FSMC_AccessMode_A;
	
	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM3;
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;  
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p;
	
	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure); 
	
	/*!< Enable FSMC Bank1_SRAM Bank */
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM3, ENABLE);  
}

/*
*********************************************************************************************************
*	函 数 名: bsp_TestExtSRAM
*	功能说明: 扫描测试外部SRAM
*	形    参: 无
*	返 回 值: 0 表示测试通过； 大于0表示错误单元的个数。
*********************************************************************************************************
*/
uint8_t bsp_TestExtSRAM(void)
{
	uint32_t i;
	uint32_t *pSRAM;
	uint8_t *pBytes;
	uint32_t err;
	const uint8_t ByteBuf[4] = {0x55, 0xA5, 0x5A, 0xAA};

	/* 写SRAM */
	pSRAM = (uint32_t *)EXT_SRAM_ADDR;
	for (i = 0; i < EXT_SRAM_SIZE / 4; i++)
	{
		*pSRAM++ = i;
	}

	/* 读SRAM */
	err = 0;
	pSRAM = (uint32_t *)EXT_SRAM_ADDR;
	for (i = 0; i < EXT_SRAM_SIZE / 4; i++)
	{
		if (*pSRAM++ != i)
		{
			err++;
		}
	}

	if (err >  0)
	{
		return  (4 * err);
	}

#if 0
	/* 对SRAM 的数据求反并写入 */
	pSRAM = (uint32_t *)EXT_SRAM_ADDR;
	for (i = 0; i < EXT_SRAM_SIZE / 4; i++)
	{
		*pSRAM = ~*pSRAM;
		pSRAM++;
	}

	/* 再次比较SRAM的数据 */
	err = 0;
	pSRAM = (uint32_t *)EXT_SRAM_ADDR;
	for (i = 0; i < EXT_SRAM_SIZE / 4; i++)
	{
		if (*pSRAM++ != (~i))
		{
			err++;
		}
	}

	if (err >  0)
	{
		return (4 * err);
	}
#endif

	/* 测试按字节方式访问, 目的是验证 FSMC_NBL0 、 FSMC_NBL1 口线 */
	pBytes = (uint8_t *)EXT_SRAM_ADDR;
	for (i = 0; i < sizeof(ByteBuf); i++)
	{
		*pBytes++ = ByteBuf[i];
	}

	/* 比较SRAM的数据 */
	err = 0;
	pBytes = (uint8_t *)EXT_SRAM_ADDR;
	for (i = 0; i < sizeof(ByteBuf); i++)
	{
		if (*pBytes++ != ByteBuf[i])
		{
			err++;
		}
	}
	if (err >  0)
	{
		return err;
	}
	return 0;
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
