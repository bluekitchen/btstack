/*
*********************************************************************************************************
*
*	模块名称 : RA8875芯片外挂的串行Flash驱动模块
*	文件名称 : bsp_ra8875_flash.c
*	版    本 : V1.1
*	说    明 : 访问RA8875外挂的串行Flash （字库芯片和图库芯片），支持 SST25VF016B、MX25L1606E 和
*			   W25Q64BVSSIG。 通过TFT显示接口中SPI总线和PWM口线控制7寸新屏上的串行Flash。
*				【备注： RA8875本身不支持外挂串行Flash的写操作，必须增加额外的电子开关电路才能实现】
*	修改记录 :
*		版本号  日期       作者    说明
*		V1.0    2012-06-25 armfly  发布首版
*		V1.1	2014-06-27 armfly  增加支持 W25Q128  (16M字节)
*		V1.2    2014-06-28 armfly  初始化函数添加 w25_ReadInfo(), 识别串行FLASH。LCD_RA8875.c中的
*							RA8875_DispBmpInFlash （） 显示图库芯片的图片的函数会用到串行FLASH型号
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	安富莱STM32-V5 开发板TFT接口中的SPI口线分配： 串行Flash型号为 W25Q64BVSSIG (80MHz)
	PA5/SPI1_SCK
	PA6/SPI1_MISO
	PA7/SPI1_MOSI
	PG11/TP_NCS
	PB1/PWM	

	由于SPI1的时钟源是84M
*/

/* CS 和 PWM 口线对应的GPIO时钟 */
#define	RCC_W25		(RCC_APB2Periph_GPIOG | RCC_APB2Periph_GPIOB)

/* 片选GPIO端口  */
#define W25_CS_GPIO			GPIOG
#define W25_CS_PIN			GPIO_Pin_11

#define W25_PWM_GPIO		GPIOB
#define W25_PWM_PIN			GPIO_Pin_1

/* 片选口线置低选中  */
#define W25_CS_0()     W25_CS_GPIO->BRR = W25_CS_PIN
/* 片选口线置高不选中 */
#define W25_CS_1()     W25_CS_GPIO->BSRR = W25_CS_PIN

/*
	PWM口线置低选中
	PWM = 1  这个模式支持STM32读写RA8875外挂的串行Flash
	PWM = 0 这是正常工作模式，由RA8875 DMA读取外挂的串行Flash
*/
#define W25_PWM_0()     W25_PWM_GPIO->BRR = W25_PWM_PIN
#define W25_PWM_1()     W25_PWM_GPIO->BSRR = W25_PWM_PIN


#define CMD_AAI       0xAD  	/* AAI 连续编程指令(FOR SST25VF016B) */
#define CMD_DISWR	  0x04		/* 禁止写, 退出AAI状态 */
#define CMD_EWRSR	  0x50		/* 允许写状态寄存器的命令 */
#define CMD_WRSR      0x01  	/* 写状态寄存器命令 */
#define CMD_WREN      0x06		/* 写使能命令 */
#define CMD_READ      0x03  	/* 读数据区命令 */
#define CMD_RDSR      0x05		/* 读状态寄存器命令 */
#define CMD_RDID      0x9F		/* 读器件ID命令 */
#define CMD_SE        0x20		/* 擦除扇区命令 */
#define CMD_BE        0xC7		/* 批量擦除命令 */
#define DUMMY_BYTE    0xA5		/* 哑命令，可以为任意值，用于读操作 */

#define WIP_FLAG      0x01		/* 状态寄存器中的正在编程标志（WIP) */

W25_T g_tW25;

void w25_ReadInfo(void);
static void w25_WriteEnable(void);
static void w25_WriteStatus(uint8_t _ucValue);
static void w25_WaitForWriteEnd(void);
void bsp_CfgSPIForW25(void);
static void w25_ConfigGPIO(void);
void w25_SetCS(uint8_t _level);

/*
*********************************************************************************************************
*	函 数 名: bsp_InitRA8875Flash
*	功能说明: 初始化串行Flash硬件接口（配置STM32的SPI时钟、GPIO)
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitRA8875Flash(void)
{
	w25_ConfigGPIO();

	/* 配置SPI硬件参数用于访问串行Flash , 在 bsp_spi_bus.c 中配置*/
	//bsp_CfgSPIForW25();

	/* 识别串行FLASH型号 */
	w25_CtrlByMCU();	/* (必须先执行w25_CtrlByMCU()设置PWM=1切换SPI控制权)  */
	w25_ReadInfo();
	w25_CtrlByRA8875();
}

/*
*********************************************************************************************************
*	函 数 名: w25_ConfigGPIO
*	功能说明: 配置GPIO。 不包括 SCK  MOSI  MISO 共享的SPI总线。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void w25_ConfigGPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 使能GPIO 时钟 */
	RCC_APB2PeriphClockCmd(RCC_W25, ENABLE);

	/* 配置片选口线为推挽输出模式 */
	w25_SetCS(1);		/* 片选置高，不选中 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	GPIO_InitStructure.GPIO_Pin = W25_CS_PIN;
	GPIO_Init(W25_CS_GPIO, &GPIO_InitStructure);

	/* 配置TFT接口中的PWM脚为为推挽输出模式，PWM = 1时 用于写RA8875外挂的串行Flash */
	/* PF6/LCD_PWM  不用于调节RA8875屏的背光 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	GPIO_InitStructure.GPIO_Pin = W25_PWM_PIN;
	GPIO_Init(W25_PWM_GPIO, &GPIO_InitStructure);
}


/*
*********************************************************************************************************
*	函 数 名: bsp_CfgSPIForW25
*	功能说明: 配置STM32内部SPI硬件的工作模式、速度等参数，用于访问SPI接口的串行Flash。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_CfgSPIForW25(void)
{
	SPI_InitTypeDef  SPI_InitStructure;

	/* 配置SPI硬件参数 */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;	/* 数据方向：2线全双工 */
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;		/* STM32的SPI工作模式 ：主机模式 */
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;	/* 数据位长度 ： 8位 */
	/* SPI_CPOL和SPI_CPHA结合使用决定时钟和数据采样点的相位关系、
	   本例配置: 总线空闲是高电平,第2个边沿（上升沿采样数据)
	*/
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;			/* 时钟上升沿采样数据 */
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;		/* 时钟的第2个边沿采样数据 */
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;			/* 片选控制方式：软件控制 */

	/* 设置波特率预分频系数 */
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;

	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* 数据位传输次序：高位先传 */
	SPI_InitStructure.SPI_CRCPolynomial = 7;			/* CRC多项式寄存器，复位后为7。本例程不用 */
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, DISABLE);			/* 先禁止SPI  */

	SPI_Cmd(SPI1, ENABLE);				/* 使能SPI  */
}


/*
*********************************************************************************************************
*	函 数 名: w25_SetCS
*	功能说明: 设置CS。 用于运行中SPI共享。
*	形    参: 无
	返 回 值: 无
*********************************************************************************************************
*/
void w25_SetCS(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* 占用SPI总线， 用于总线共享 */

		#ifdef SOFT_SPI		/* 软件SPI */
			bsp_SetSpiSck(1);
			W25_CS_0();
		#endif

		#ifdef HARD_SPI		/* 硬件SPI */
			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_High | SPI_CPHA_2Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_8 | SPI_FirstBit_MSB);

			W25_CS_0();
		#endif
	}
	else
	{
		W25_CS_1();

		bsp_SpiBusExit();	/* 释放SPI总线， 用于总线共享 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: w25_CtrlByMCU
*	功能说明: 串行Flash控制权交给MCU （STM32）
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void w25_CtrlByMCU(void)
{
	/*
		PWM口线置低选中
		PWM = 1  这个模式支持STM32读写RA8875外挂的串行Flash
		PWM = 0 这是正常工作模式，由RA8875 DMA读取外挂的串行Flash
	*/
	W25_PWM_1();
}

/*
*********************************************************************************************************
*	函 数 名: w25_CtrlByRA8875
*	功能说明: 串行Flash控制权交给RA8875
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void w25_CtrlByRA8875(void)
{
	/*
		PWM口线置低选中
		PWM = 1  这个模式支持STM32读写RA8875外挂的串行Flash
		PWM = 0 这是正常工作模式，由RA8875 DMA读取外挂的串行Flash
	*/
	W25_PWM_0();
}

/*
*********************************************************************************************************
*	函 数 名: w25_SelectChip
*	功能说明: 选择即将操作的芯片
*	形    参: _idex = FONT_CHIP 表示字库芯片;  idex = BMP_CHIP 表示图库芯片
*	返 回 值: 无
*********************************************************************************************************
*/
void w25_SelectChip(uint8_t _idex)
{
	/*
		PWM = 1, KOUT3 = 0 写字库芯片
		PWM = 1, KOUT3 = 1 写图库芯片
	*/
	#if 1
		if (_idex == FONT_CHIP)
		{
			RA8875_CtrlGPO(3, 0);	/* RA8875 的 KOUT3 = 0 */
		}
		else	/* BMP图片芯片 */
		{
			RA8875_CtrlGPO(3, 1);	/* RA8875 的 KOUT3 = 1 */
		}
	#else
		/* 对于1片 W25Q128的屏，不需要RA8875的KOUT 引脚 */
		/* 对于外扩 图片阵列板的的屏，不需要RA8875的KOUT0 - KOUT3 四个引脚 */
	#endif


	w25_ReadInfo();				/* 自动识别芯片型号 */

	w25_SetCS(0);				/* 软件方式，使能串行Flash片选 */
	bsp_spiWrite1(CMD_DISWR);		/* 发送禁止写入的命令,即使能软件写保护 */
	w25_SetCS(1);				/* 软件方式，禁能串行Flash片选 */

	w25_WaitForWriteEnd();		/* 等待串行Flash内部操作完成 */

	w25_WriteStatus(0);			/* 解除所有BLOCK的写保护 */
}

/*
*********************************************************************************************************
*	函 数 名: w25_EraseSector
*	功能说明: 擦除指定的扇区
*	形    参:  _uiSectorAddr : 扇区地址
*	返 回 值: 无
*********************************************************************************************************
*/
void w25_EraseSector(uint32_t _uiSectorAddr)
{
	w25_WriteEnable();								/* 发送写使能命令 */

	/* 擦除扇区操作 */
	w25_SetCS(0);									/* 使能片选 */
	bsp_spiWrite1(CMD_SE);								/* 发送擦除命令 */
	bsp_spiWrite1((_uiSectorAddr & 0xFF0000) >> 16);	/* 发送扇区地址的高8bit */
	bsp_spiWrite1((_uiSectorAddr & 0xFF00) >> 8);		/* 发送扇区地址中间8bit */
	bsp_spiWrite1(_uiSectorAddr & 0xFF);				/* 发送扇区地址低8bit */
	w25_SetCS(1);									/* 禁能片选 */

	w25_WaitForWriteEnd();							/* 等待串行Flash内部写操作完成 */
}

/*
*********************************************************************************************************
*	函 数 名: w25_EraseChip
*	功能说明: 擦除整个芯片
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void w25_EraseChip(void)
{
	w25_WriteEnable();								/* 发送写使能命令 */

	/* 擦除扇区操作 */
	w25_SetCS(0);									/* 使能片选 */
	bsp_spiWrite1(CMD_BE);							/* 发送整片擦除命令 */
	w25_SetCS(1);									/* 禁能片选 */

	w25_WaitForWriteEnd();							/* 等待串行Flash内部写操作完成 */
}

/*
*********************************************************************************************************
*	函 数 名: w25_WritePage
*	功能说明: 向一个page内写入若干字节。字节个数不能超出页面大小（4K)
*	形    参:  	_pBuf : 数据源缓冲区；
*				_uiWriteAddr ：目标区域首地址
*				_usSize ：数据个数，不能超过页面大小
*	返 回 值: 无
*********************************************************************************************************
*/
void w25_WritePage(uint8_t * _pBuf, uint32_t _uiWriteAddr, uint16_t _usSize)
{
	uint32_t i, j;

	if (g_tW25.ChipID == SST25VF016B)
	{
		/* AAI指令要求传入的数据个数是偶数 */
		if ((_usSize < 2) && (_usSize % 2))
		{
			return ;
		}

		w25_WriteEnable();								/* 发送写使能命令 */

		w25_SetCS(0);									/* 使能片选 */
		bsp_spiWrite1(CMD_AAI);							/* 发送AAI命令(地址自动增加编程) */
		bsp_spiWrite1((_uiWriteAddr & 0xFF0000) >> 16);	/* 发送扇区地址的高8bit */
		bsp_spiWrite1((_uiWriteAddr & 0xFF00) >> 8);		/* 发送扇区地址中间8bit */
		bsp_spiWrite1(_uiWriteAddr & 0xFF);				/* 发送扇区地址低8bit */
		bsp_spiWrite1(*_pBuf++);							/* 发送第1个数据 */
		bsp_spiWrite1(*_pBuf++);							/* 发送第2个数据 */
		w25_SetCS(1);									/* 禁能片选 */

		w25_WaitForWriteEnd();							/* 等待串行Flash内部写操作完成 */

		_usSize -= 2;									/* 计算剩余字节数 */

		for (i = 0; i < _usSize / 2; i++)
		{
			w25_SetCS(0);								/* 使能片选 */
			bsp_spiWrite1(CMD_AAI);						/* 发送AAI命令(地址自动增加编程) */
			bsp_spiWrite1(*_pBuf++);						/* 发送数据 */
			bsp_spiWrite1(*_pBuf++);						/* 发送数据 */
			w25_SetCS(1);								/* 禁能片选 */
			w25_WaitForWriteEnd();						/* 等待串行Flash内部写操作完成 */
		}

		/* 进入写保护状态 */
		w25_SetCS(0);
		bsp_spiWrite1(CMD_DISWR);
		w25_SetCS(1);

		w25_WaitForWriteEnd();							/* 等待串行Flash内部写操作完成 */
	}
	else	/* for MX25L1606E 、 W25Q64BV */
	{
		for (j = 0; j < _usSize / 256; j++)
		{
			w25_WriteEnable();								/* 发送写使能命令 */

			w25_SetCS(0);									/* 使能片选 */
			bsp_spiWrite1(0x02);								/* 发送AAI命令(地址自动增加编程) */
			bsp_spiWrite1((_uiWriteAddr & 0xFF0000) >> 16);	/* 发送扇区地址的高8bit */
			bsp_spiWrite1((_uiWriteAddr & 0xFF00) >> 8);		/* 发送扇区地址中间8bit */
			bsp_spiWrite1(_uiWriteAddr & 0xFF);				/* 发送扇区地址低8bit */

			for (i = 0; i < 256; i++)
			{
				bsp_spiWrite1(*_pBuf++);					/* 发送数据 */
			}

			w25_SetCS(1);								/* 禁止片选 */

			w25_WaitForWriteEnd();						/* 等待串行Flash内部写操作完成 */

			_uiWriteAddr += 256;
		}

		/* 进入写保护状态 */
		w25_SetCS(0);
		bsp_spiWrite1(CMD_DISWR);
		w25_SetCS(1);

		w25_WaitForWriteEnd();							/* 等待串行Flash内部写操作完成 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: w25_ReadBuffer
*	功能说明: 连续读取若干字节。字节个数不能超出芯片容量。
*	形    参:  	_pBuf : 数据源缓冲区；
*				_uiReadAddr ：首地址
*				_usSize ：数据个数, 可以大于PAGE_SIZE,但是不能超出芯片总容量
*	返 回 值: 无
*********************************************************************************************************
*/
void w25_ReadBuffer(uint8_t * _pBuf, uint32_t _uiReadAddr, uint32_t _uiSize)
{
	/* 如果读取的数据长度为0或者超出串行Flash地址空间，则直接返回 */
	if ((_uiSize == 0) ||(_uiReadAddr + _uiSize) > g_tW25.TotalSize)
	{
		return;
	}

	/* 擦除扇区操作 */
	w25_SetCS(0);									/* 使能片选 */
	bsp_spiWrite1(CMD_READ);							/* 发送读命令 */
	bsp_spiWrite1((_uiReadAddr & 0xFF0000) >> 16);	/* 发送扇区地址的高8bit */
	bsp_spiWrite1((_uiReadAddr & 0xFF00) >> 8);		/* 发送扇区地址中间8bit */
	bsp_spiWrite1(_uiReadAddr & 0xFF);				/* 发送扇区地址低8bit */
	while (_uiSize--)
	{
		*_pBuf++ = bsp_spiRead1();			/* 读一个字节并存储到pBuf，读完后指针自加1 */
	}
	w25_SetCS(1);									/* 禁能片选 */
}

/*
*********************************************************************************************************
*	函 数 名: w25_ReadID
*	功能说明: 读取器件ID
*	形    参:  无
*	返 回 值: 32bit的器件ID (最高8bit填0，有效ID位数为24bit）
*********************************************************************************************************
*/
uint32_t w25_ReadID(void)
{
	uint32_t uiID;
	uint8_t id1, id2, id3;

	w25_SetCS(0);									/* 使能片选 */
	bsp_spiWrite1(CMD_RDID);								/* 发送读ID命令 */
	id1 = bsp_spiRead1();					/* 读ID的第1个字节 */
	id2 = bsp_spiRead1();					/* 读ID的第2个字节 */
	id3 = bsp_spiRead1();					/* 读ID的第3个字节 */
	w25_SetCS(1);									/* 禁能片选 */

	uiID = ((uint32_t)id1 << 16) | ((uint32_t)id2 << 8) | id3;

	return uiID;
}

/*
*********************************************************************************************************
*	函 数 名: w25_ReadInfo
*	功能说明: 读取器件ID,并填充器件参数
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void w25_ReadInfo(void)
{
	/* 自动识别串行Flash型号 */
	{
		g_tW25.ChipID = w25_ReadID();	/* 芯片ID */

		switch (g_tW25.ChipID)
		{
			case SST25VF016B:
				g_tW25.TotalSize = 2 * 1024 * 1024;	/* 总容量 = 2M */
				g_tW25.PageSize = 4 * 1024;			/* 页面大小 = 4K */
				break;

			case MX25L1606E:
				g_tW25.TotalSize = 2 * 1024 * 1024;	/* 总容量 = 2M */
				g_tW25.PageSize = 4 * 1024;			/* 页面大小 = 4K */
				break;

			case W25Q64BV:
				g_tW25.TotalSize = 8 * 1024 * 1024;	/* 总容量 = 8M */
				g_tW25.PageSize = 4 * 1024;			/* 页面大小 = 4K */
				break;

			case W25Q128:
				g_tW25.TotalSize = 16 * 1024 * 1024;	/* 总容量 = 16M */
				g_tW25.PageSize = 4 * 1024;			/* 页面大小 = 4K */
				break;

			default:		/* 集通字库不支持ID读取 */
				g_tW25.TotalSize = 2 * 1024 * 1024;
				g_tW25.PageSize = 4 * 1024;
				break;
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: w25_WriteEnable
*	功能说明: 向器件发送写使能命令
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void w25_WriteEnable(void)
{
	w25_SetCS(0);									/* 使能片选 */
	bsp_spiWrite1(CMD_WREN);								/* 发送命令 */
	w25_SetCS(1);									/* 禁能片选 */
}

/*
*********************************************************************************************************
*	函 数 名: w25_WriteStatus
*	功能说明: 写状态寄存器
*	形    参:  _ucValue : 状态寄存器的值
*	返 回 值: 无
*********************************************************************************************************
*/
static void w25_WriteStatus(uint8_t _ucValue)
{

	if (g_tW25.ChipID == SST25VF016B)
	{
		/* 第1步：先使能写状态寄存器 */
		w25_SetCS(0);									/* 使能片选 */
		bsp_spiWrite1(CMD_EWRSR);							/* 发送命令， 允许写状态寄存器 */
		w25_SetCS(1);									/* 禁能片选 */

		/* 第2步：再写状态寄存器 */
		w25_SetCS(0);									/* 使能片选 */
		bsp_spiWrite1(CMD_WRSR);							/* 发送命令， 写状态寄存器 */
		bsp_spiWrite1(_ucValue);							/* 发送数据：状态寄存器的值 */
		w25_SetCS(1);									/* 禁能片选 */
	}
	else
	{
		w25_SetCS(0);									/* 使能片选 */
		bsp_spiWrite1(CMD_WRSR);							/* 发送命令， 写状态寄存器 */
		bsp_spiWrite1(_ucValue);							/* 发送数据：状态寄存器的值 */
		w25_SetCS(1);									/* 禁能片选 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: w25_WaitForWriteEnd
*	功能说明: 采用循环查询的方式等待器件内部写操作完成
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void w25_WaitForWriteEnd(void)
{
	w25_SetCS(0);									/* 使能片选 */
	bsp_spiWrite1(CMD_RDSR);							/* 发送命令， 读状态寄存器 */
	while((bsp_spiRead1() & WIP_FLAG) == SET);	/* 判断状态寄存器的忙标志位 */
	w25_SetCS(1);									/* 禁能片选 */
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
