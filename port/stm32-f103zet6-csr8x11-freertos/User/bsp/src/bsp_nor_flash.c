/*
*********************************************************************************************************
*
*	模块名称 : NOR Flash驱动模块
*	文件名称 : bsp_nor_flash.c
*	版    本 : V1.1
*	说    明 : 安富莱STM32-V5开发板标配的NOR Flash 为 S29GL128P10TFI01  容量16M字节，16Bit，100ns速度
*				物理地址 : 0x6400 0000
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-02-01  armfly  正式发布
*		V1.1	2015-08-03  armfly  修改NOR_ReadBuffer()的BUG
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

/*
	S29GL128P 内存组织结构： 每个扇区128K字节，一共128个扇区。总容量为 16M字节。按16Bit访问。

	挂在STM32上，对应的物理地址范围为 ： 0x6400 0000 - 0x64FF FFFF.  只能按16Bit模式访问。

*/

#include "bsp.h"

#define ADDR_SHIFT(A) 	(NOR_FLASH_ADDR + (2 * (A)))
#define NOR_WRITE(Address, Data)  (*(__IO uint16_t *)(Address) = (Data))

/* 判忙时的执行语句循环次数 */
#define BlockErase_Timeout    ((uint32_t)0x00A00000)
#define ChipErase_Timeout     ((uint32_t)0x30000000)
#define Program_Timeout       ((uint32_t)0x00001400)

/* PD6 是NOR Flash输出到STM32的忙信号, 通过GPIO查询方式判忙 */
#define NOR_IS_BUSY()	(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_6) == RESET)

static void NOR_QuitToReadStatus(void);
static uint8_t NOR_GetStatus(uint32_t Timeout);


/*
*********************************************************************************************************
*	函 数 名: bsp_InitNorFlash
*	功能说明: 配置连接外部NOR Flash的GPIO和FSMC
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitNorFlash(void)
{
	FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef  p;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE | 
						 RCC_APB2Periph_GPIOF | RCC_APB2Periph_GPIOG, ENABLE);

	/*-- GPIO Configuration ------------------------------------------------------*/
	/* NOR Data lines configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9 |
								GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
								GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 |
								GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* NOR Address lines configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
								GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_12 | GPIO_Pin_13 |
								GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
								GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;                            
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* NOE and NWE configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* NE2 configuration */
	/* armfly : STM32F103ZE-EK 必须使能NE3,NE4, 否访问NOR 4M空间以上会出现异常 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_12;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	/*-- FSMC Configuration ----------------------------------------------------*/
	p.FSMC_AddressSetupTime = 0x05;
	//p.FSMC_AddressSetupTime = 0x07;
	p.FSMC_AddressHoldTime = 0x00;
	p.FSMC_DataSetupTime = 0x07;
	//p.FSMC_DataSetupTime = 0x09;
	p.FSMC_BusTurnAroundDuration = 0x00;
	p.FSMC_CLKDivision = 0x00;
	p.FSMC_DataLatency = 0x00;
	p.FSMC_AccessMode = FSMC_AccessMode_B;

	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM2;
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_NOR;
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
	//FSMC_NORSRAMInitStructure.FSMC_AsyncWait = FSMC_AsyncWait_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p;

	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);

	/* Enable FSMC Bank1_NOR Bank */
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM2, ENABLE);
}

/*
*********************************************************************************************************
*	函 数 名: NOR_ReadID
*	功能说明: 读取NOR Flash的器件ID
*	形    参: 无
*	返 回 值: 器件ID,32Bit, 高8bit 是Manufacturer_Code, 低24bit是器件ID
*********************************************************************************************************
*/
uint32_t NOR_ReadID(void)
{
	uint32_t uiID;
	uint8_t id1, id2, id3, id4;

	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0090);

	id1 = *(__IO uint16_t *) ADDR_SHIFT(0x0000);
	id2 = *(__IO uint16_t *) ADDR_SHIFT(0x0001);
	id3 = *(__IO uint16_t *) ADDR_SHIFT(0x000E);
	id4 = *(__IO uint16_t *) ADDR_SHIFT(0x000F);

	uiID = ((uint32_t)id1 << 24) | ((uint32_t)id2 << 16)  | ((uint32_t)id3 << 8) | id4;

	NOR_WRITE(NOR_FLASH_ADDR, 0x00F0 );		/* 退出ID模式 */

	return uiID;
}

/*
*********************************************************************************************************
*	函 数 名: NOR_QuitToReadStatus
*	功能说明: 复位NOR，退到读状态
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void NOR_QuitToReadStatus(void)
{
	NOR_WRITE(ADDR_SHIFT(0x00555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x002AA), 0x0055);
	NOR_WRITE(NOR_FLASH_ADDR, 0x00F0 );
}

/*
*********************************************************************************************************
*	函 数 名: NOR_GetStatus
*	功能说明: 读取NOR的操作状态
*	形    参: 无
*	返 回 值: 0表示成功.   NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
static uint8_t NOR_GetStatus(uint32_t Timeout)
{
	uint16_t val1 = 0x00;
	uint16_t val2 = 0x00;
	uint8_t status = NOR_ONGOING;
	uint32_t timeout = Timeout;

	/* 等待NOR输出忙信号，高电平时等待。避免NOR的忙信号还未反映过来导致CPU提前认为不忙了 */
	while ((!NOR_IS_BUSY()) && (timeout > 0))
	{
		timeout--;
	}

	/* 等待NOR忙信号结束，低电平时等待 */
	timeout = Timeout;
	while(NOR_IS_BUSY() && (timeout > 0))
	{
		timeout--;
	}

	/*
		- DQ 6 编程时跳变
		- DQ 6 和 DQ 2 在擦除时跳变
		- DQ 2 在擦除挂起时跳变
		- DQ 1 在编程错误时置1
		- DQ 5 在超时时置1
	*/
	/* 通过读取DQ6, DQ5 的数据位是否存在翻转现象判断NOR 内部操作是否完成。如果正忙，则第2次读和第1次读的数据不同 */
	while ((Timeout != 0x00) && (status != NOR_SUCCESS))
	{
		Timeout--;

		/* Read DQ6 */
		val1 = *(__IO uint16_t *)(NOR_FLASH_ADDR);
		val2 = *(__IO uint16_t *)(NOR_FLASH_ADDR);

		/* If DQ6 did not toggle between the two reads then return NOR_Success */
		if ((val1 & 0x0040) == (val2 & 0x0040))
		{
			return NOR_SUCCESS;
		}

		/* Read DQ2 */
		if((val1 & 0x0020) != 0x0020)
		{
			status = NOR_ONGOING;
		}

		val1 = *(__IO uint16_t *)(NOR_FLASH_ADDR);
		val2 = *(__IO uint16_t *)(NOR_FLASH_ADDR);

		if((val1 & 0x0040) == (val2 & 0x0040))
		{
			return NOR_SUCCESS;
		}
		else if ((val1 & 0x0020) == 0x0020)
		{
			status = NOR_ERROR;
			NOR_QuitToReadStatus();
		}
	}

	if (Timeout == 0x00)
	{
		status = NOR_TIMEOUT;
		NOR_QuitToReadStatus();
	}

	/* 返回操作状态 */
	return (status);
}

/*
*********************************************************************************************************
*	函 数 名: NOR_EraseChip
*	功能说明: 擦除NOR Flash整个芯片
*	形    参: 无
*	返 回 值: 0表示成功
*********************************************************************************************************
*/
uint8_t NOR_EraseChip(void)
{
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0080);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0010);

	return (NOR_GetStatus(ChipErase_Timeout));
}

/*
*********************************************************************************************************
*	函 数 名: NOR_StartEraseChip
*	功能说明: 开始擦除NOR Flash整个芯片， 不等待结束
*	形    参: 无
*	返 回 值: 0表示成功
*********************************************************************************************************
*/
void NOR_StartEraseChip(void)
{
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0080);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0010);
	
	NOR_GetStatus(1000);
}

/*
*********************************************************************************************************
*	函 数 名: NOR_CheckComplete
*	功能说明: 检测擦除是否完成
*	形    参: 无
*	返 回 值: 0表示成功   NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_CheckStatus(void)
{
	uint16_t val1 = 0x00;
	uint16_t val2 = 0x00;
	uint8_t status = NOR_ONGOING;
	uint32_t timeout = 10;
	
	/*
		- DQ 6 编程时跳变
		- DQ 6 和 DQ 2 在擦除时跳变
		- DQ 2 在擦除挂起时跳变
		- DQ 1 在编程错误时置1
		- DQ 5 在超时时置1
	*/
	/* 通过读取DQ6, DQ5 的数据位是否存在翻转现象判断NOR 内部操作是否完成。如果正忙，则第2次读和第1次读的数据不同 */
	while ((timeout != 0x00) && (status != NOR_SUCCESS))
	{
		timeout--;

		/* Read DQ6 */
		val1 = *(__IO uint16_t *)(NOR_FLASH_ADDR);
		val2 = *(__IO uint16_t *)(NOR_FLASH_ADDR);

		/* If DQ6 did not toggle between the two reads then return NOR_Success */
		if ((val1 & 0x0040) == (val2 & 0x0040))
		{
			return NOR_SUCCESS;
		}

		/* Read DQ2 */
		if((val1 & 0x0020) != 0x0020)
		{
			status = NOR_ONGOING;
		}

		val1 = *(__IO uint16_t *)(NOR_FLASH_ADDR);
		val2 = *(__IO uint16_t *)(NOR_FLASH_ADDR);

		if((val1 & 0x0040) == (val2 & 0x0040))
		{
			return NOR_SUCCESS;
		}
		else if ((val1 & 0x0020) == 0x0020)
		{
			status = NOR_ERROR;
			NOR_QuitToReadStatus();
		}
	}

	if (timeout == 0x00)
	{
		status = NOR_TIMEOUT;
		//NOR_QuitToReadStatus();
	}

	/* 返回操作状态 */
	return (status);
}

/*
*********************************************************************************************************
*	函 数 名: NOR_EraseSector
*	功能说明: 擦除NOR Flash指定的扇区
*	形    参: 扇区地址
*	返 回 值: NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_EraseSector(uint32_t _uiBlockAddr)
{
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0080);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE((NOR_FLASH_ADDR + _uiBlockAddr), 0x30);

	return (NOR_GetStatus(BlockErase_Timeout));
}

/*
*********************************************************************************************************
*	函 数 名: NOR_ReadByte
*	功能说明: 读取单字节数据
*	形    参: 	_uiWriteAddr : 偏移地址[0, 16*1024*1024 - 2]; 编程地址可以为偶数，也可以为奇数。
*	返 回 值: 读取到的数据
*********************************************************************************************************
*/
uint8_t NOR_ReadByte(uint32_t _uiWriteAddr)
{
	uint16_t usHalfWord;

	if (_uiWriteAddr % 2)	/* 奇数地址 */
	{
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr - 1);
		return (usHalfWord >> 8);	/* 取高8Bit */
	}
	else	/* 偶数地址 */
	{
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr);
		return usHalfWord;	/* 取低8Bit */
	}
}

/*
*********************************************************************************************************
*	函 数 名: NOR_ReadBuffer
*	功能说明: 连续读取NOR Flash
*	形    参: 	_pBuf : 字节型数据缓冲区，用于存放读出的数据
*				_uiWriteAddr : 偏移地址[0, 16*1024*1024 - 2]; 编程地址可以为偶数，也可以为奇数。
*				_uiBytes : 字节大小
*	返 回 值: 读取到的数据
*********************************************************************************************************
*/
void NOR_ReadBuffer(uint8_t *_pBuf, uint32_t _uiWriteAddr, uint32_t _uiBytes)
{
	uint16_t usHalfWord;
	uint16_t *pNor16;
	uint32_t i;
	uint32_t uiNum;

	uiNum = _uiBytes;
	/* 处理首字节 */
	if (_uiWriteAddr % 2)	/* 奇数地址 */
	{
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr - 1);
		*_pBuf++ = (usHalfWord >> 8);	/* 取高8Bit */
		uiNum--;
		_uiWriteAddr++;		/* 变为偶数 */
	}

	/* 按照双字节模式连续读取NOR数据至缓冲区_pBuf */
	pNor16 = (uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr);
	// for (i = 0; i < uiNum / 2; i++)
	for (i = 0; i < _uiBytes / 2; i++)
	{
		usHalfWord = *pNor16++;
		*_pBuf++ = usHalfWord;
		*_pBuf++ = usHalfWord >> 8;
		uiNum -= 2;
	}

	/* 处理最后1个字节 */
	if (uiNum == 1)
	{
		*_pBuf++ = *pNor16;
	}
}

/*
*********************************************************************************************************
*	函 数 名: NOR_WriteHalfWord
*	功能说明: 半字编程. 编程前执行解锁命令序列。编程完毕后，自动退到读取模式。半字编程可以是随机地址。
*				编程前需要保证存储单元是全0xFF状态。可以重复编程相同的数据。
*	形    参: 	_uiWriteAddr : 偏移地址[0, 16*1024*1024 - 2]; 编程地址必须为偶数
*				_usData      : 数据 16Bit
*
*	返 回 值: NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_WriteHalfWord(uint32_t _uiWriteAddr, uint16_t _usData)
{
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00A0);
	NOR_WRITE(NOR_FLASH_ADDR + _uiWriteAddr, _usData);

	return (NOR_GetStatus(Program_Timeout));
}

/*
*********************************************************************************************************
*	函 数 名: NOR_WriteByte
*	功能说明: 字节编程. 编程前需要保证存储单元是全0xFF状态。可以重复编程相同的数据。
*	形    参: 	_uiWriteAddr : 偏移地址[0, 16*1024*1024 - 1]; 编程地址可以为奇数也可以为偶数
*				_usData      : 数据 16Bit
*
*	返 回 值: NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_WriteByte(uint32_t _uiWriteAddr, uint8_t _ucByte)
{
	uint16_t usHalfWord;

	if (_uiWriteAddr % 2)	/* 奇数地址 */
	{
		/* 读出2字节数据，然后改写高字节，维持以前的低字节数据不变 */
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr - 1);
		usHalfWord &= 0x00FF;
		usHalfWord |= (_ucByte << 8);
	}
	else
	{
		/* 读取NOR原始数据，保留高字节 */
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr);
		usHalfWord &= 0xFF00;
		usHalfWord |= _ucByte;
	}
	return NOR_WriteHalfWord(_uiWriteAddr, usHalfWord);
}

/*
*********************************************************************************************************
*	函 数 名: NOR_WriteInPage.    
*	功能说明: 页面内编程（64字节一个页面）. 编程前需要保证存储单元是全0xFF状态。可以重复编程相同的数据。
*	形    参: 	pBuffer : 数据存放在此缓冲区
*				_uiWriteAddr : 偏移地址, 必须是偶数开始
*				_usNumHalfword      : 数据格式，双字节为1个单位. 值域: 1-32
*
*	返 回 值: NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_WriteInPage(uint16_t *pBuffer, uint32_t _uiWriteAddr,  uint16_t _usNumHalfword)
{
	uint32_t lastloadedaddress;
	uint32_t currentaddress;
	uint32_t endaddress;

	/* pdf 表7.7 写入缓冲器编程

		写入缓冲器编程允许系统在一个编程操作中写入最多32 个字。与标准的“ 字” 编程算法相比，这可以有效地
		加快字编程速度。
	*/
	
	if (_usNumHalfword > 32)
	{
		return NOR_ERROR;
	}
	
	if ((_uiWriteAddr % 2) != 0)
	{
		return NOR_ERROR;
	}
	
	_uiWriteAddr = _uiWriteAddr / 2;

	currentaddress = _uiWriteAddr;
	endaddress = _uiWriteAddr + _usNumHalfword - 1;
	lastloadedaddress = _uiWriteAddr;

	/* 解锁命令序列 */
	NOR_WRITE(ADDR_SHIFT(0x00555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);

	/* Write Write Buffer Load Command */
	NOR_WRITE(ADDR_SHIFT(_uiWriteAddr), 0x0025);
	NOR_WRITE(ADDR_SHIFT(_uiWriteAddr), (_usNumHalfword - 1));

	/*  Load Data into NOR Buffer */
	while (currentaddress <= endaddress)
	{
		/* Store last loaded address & data value (for polling) */
		lastloadedaddress = currentaddress;

		NOR_WRITE(ADDR_SHIFT(currentaddress), *pBuffer++);
		currentaddress += 1;
	}

	NOR_WRITE(ADDR_SHIFT(lastloadedaddress), 0x29);

	return (NOR_GetStatus(Program_Timeout));
}

/*
*********************************************************************************************************
*	函 数 名: NOR_WriteBuffer
*	功能说明: 连续编程操作。采取半字编程模式。
*			  S29GL 支持64字节页面大小的连续编程。本函数暂时不支持页面编程。
*	形    参: _pBuf : 8位数据缓冲区
*			 _uiWriteAddr : 写入的存储单元首地址, 必须为偶数
*			 _uiBytes : 字节个数
*	返 回 值: NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_WriteBuffer(uint8_t *_pBuf, uint32_t _uiWriteAddr, uint32_t _uiBytes)
{
	uint16_t usHalfWord;
	uint32_t i;
	uint32_t uiNum;
	uint8_t ucStatus;

	uiNum = _uiBytes;
	/* 处理首字节 */
	if (_uiWriteAddr % 2)	/* 奇数地址 */
	{
		/* 读出2字节数据，然后改写高字节，维持以前的低字节数据不变 */
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr - 1);
		usHalfWord &= 0x00FF;
		usHalfWord |= ((*_pBuf++) << 8);

		ucStatus = NOR_WriteHalfWord(_uiWriteAddr - 1, usHalfWord);
		if (ucStatus != NOR_SUCCESS)
		{
			goto err_quit;
		}

		uiNum--;
		_uiWriteAddr++;		/* 变为偶数 */
	}

	/* 按照双字节模式连续编程NOR数据 */
	for (i = 0; i < uiNum / 2; i++)
	{
		usHalfWord = *_pBuf++;
		usHalfWord |= ((*_pBuf++) << 8);

		ucStatus = NOR_WriteHalfWord(_uiWriteAddr, usHalfWord);
		if (ucStatus != NOR_SUCCESS)
		{
			goto err_quit;
		}

		_uiWriteAddr += 2;
	}

	/* 处理最后1个字节 */
	if (uiNum % 2)
	{
		/* 读取NOR原始数据，保留高字节 */
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr);
		usHalfWord &= 0xFF00;
		usHalfWord |= (*_pBuf++);

		ucStatus = NOR_WriteHalfWord(_uiWriteAddr, usHalfWord);
		if (ucStatus != NOR_SUCCESS)
		{
			goto err_quit;
		}
	}
	ucStatus = NOR_SUCCESS;
err_quit:
	return 	ucStatus;
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
