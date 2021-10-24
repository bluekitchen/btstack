/*
*********************************************************************************************************
*
*	模块名称 : RA8875芯片和MCU接口模块
*	文件名称 : bsp_ra8875_port.c
*	版    本 : V1.6
*	说    明 : RA8875底层驱动函数集。
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-02-01 armfly  正式发布
*		V1.1    2014-09-04 armfly  同时支持SPI和8080两种接口，自动识别。不采用宏控制。
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"


uint8_t g_RA8875_IF = RA_HARD_8080_16;	/*　接口类型　*/

/*
*********************************************************************************************************
*	函 数 名: RA8875_ConfigGPIO
*	功能说明: 配置CPU访问接口，如FSMC. SPI等
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_ConfigGPIO(void)
{
	static uint8_t s_run_first = 0;
	
	/* 如果已经运行过，则不再执行 */
	if (s_run_first == 1)
	{
		return;
	}
	
	s_run_first = 1;
	
	/* FSMC在 bsp_tft_lcd.c中已经配置好 */
	
	
	/* RA8875按照SPI接口设置后，通过总线方式依然可以读到0X75的特征，因此不能用来自动识别SPI模式 */
	{
		uint8_t value;
		
		g_RA8875_IF = RA_HARD_8080_16;	
		RA8875_WriteReg(0x60, 0x1A);	/* 60H寄存器背景色寄存器红色[4:0]低5位有效 */
		value = RA8875_ReadReg(0x60);
		if (value != 0x1A)
		{
			RA8875_InitSPI();				/* 配置好SPI接口  */
			g_RA8875_IF = RA_HARD_SPI;		/* 识别为 SPI总线 */
		}
	}
	
	#ifdef USE_WAIT_PIN
	{
		GPIO_InitTypeDef GPIO_InitStructure;
		
		/* 使能 GPIO时钟 */
		RCC_APB2PeriphClockCmd(RCC_WAIT, ENABLE);
		
		/* 连接到RA8875的BUSY引脚，用GPIO 识别芯片内忙 */
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;		/*　输入上拉　*/
		GPIO_InitStructure.GPIO_Pin = PIN_WAIT;
		GPIO_Init(PORT_WAIT, &GPIO_InitStructure);
	}
	#endif
}

#ifdef USE_WAIT_PIN
/*
*********************************************************************************************************
*	函 数 名: RA8875_ReadBusy
*	功能说明: 判断RA8875的WAIT引脚状态
*	形    参: 无
*	返 回 值: 1 表示正忙, 0 表示内部不忙
*********************************************************************************************************
*/
uint8_t RA8875_ReadBusy(void)
{
	if ((PORT_WAIT->IDR & PIN_WAIT) == 0) 
		return 1;
	else 
		return 0;
}
#endif

/*
*********************************************************************************************************
*	函 数 名: RA8875_Delaly1us
*	功能说明: 延迟函数, 不准, 主要用于RA8875 PLL启动之前发送指令间的延迟
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_Delaly1us(void)
{
	uint8_t i;

	for (i = 0; i < 10; i++);	/* 延迟, 不准 */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_Delaly1ms
*	功能说明: 延迟函数.  主要用于RA8875 PLL启动之前发送指令间的延迟
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_Delaly1ms(void)
{
	uint16_t i;

	for (i = 0; i < 5000; i++);	/* 延迟, 不准 */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_WriteCmd
*	功能说明: 写RA8875指令寄存器
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_WriteCmd(uint8_t _ucRegAddr)
{
	if (g_RA8875_IF == RA_HARD_SPI)	/* 硬件SPI接口 */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_WRITE_CMD);
		SPI_ShiftByte(_ucRegAddr);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080硬件总线16bit */
	{
		RA8875_REG = _ucRegAddr;	/* 设置寄存器地址 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_WriteData
*	功能说明: 写RA8875指令寄存器
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_WriteData(uint8_t _ucRegValue)
{
	if (g_RA8875_IF == RA_HARD_SPI)	/* 硬件SPI接口 */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_WRITE_DATA);
		SPI_ShiftByte(_ucRegValue);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080硬件总线16bit */
	{
		RA8875_RAM = _ucRegValue;	/* 设置寄存器地址 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_ReadData
*	功能说明: 读RA8875寄存器值
*	形    参: 无
*	返 回 值: 寄存器值
*********************************************************************************************************
*/
uint8_t RA8875_ReadData(void)
{
	uint8_t value = 0;
	
	if (g_RA8875_IF == RA_HARD_SPI)	/* 硬件SPI接口 */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_READ_DATA);
		value = SPI_ShiftByte(0x00);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080硬件总线16bit */
	{
		value = RA8875_RAM;		/* 读取寄存器值 */
	}

	return value;	
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_WriteData16
*	功能说明: 写RA8875数据总线，16bit，用于RGB显存写入
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_WriteData16(uint16_t _usRGB)
{
	if (g_RA8875_IF == RA_HARD_SPI)	/* 硬件SPI接口 */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_WRITE_DATA);
		SPI_ShiftByte(_usRGB >> 8);
		RA8875_CS_1();
		
		/* 必须增加一些延迟，否则连续写像素可能出错 */
		RA8875_CS_1();
		RA8875_CS_1();
		
		RA8875_CS_0();
		SPI_ShiftByte(SPI_WRITE_DATA);
		SPI_ShiftByte(_usRGB);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080硬件总线16bit */
	{
		RA8875_RAM = _usRGB;	/* 设置寄存器地址 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_ReadData16
*	功能说明: 读RA8875显存，16bit RGB
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t RA8875_ReadData16(void)
{
	uint16_t value;
	
	if (g_RA8875_IF == RA_HARD_SPI)	/* 硬件SPI接口 */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_READ_DATA);
		value = SPI_ShiftByte(0x00);
		value <<= 8;
		value += SPI_ShiftByte(0x00);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080硬件总线16bit */
	{
		value = RA8875_RAM;		/* 读取寄存器值 */
	}

	return value;	
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_ReadStatus
*	功能说明: 读RA8875状态寄存器
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t RA8875_ReadStatus(void)
{
	uint8_t value = 0;
	
	if (g_RA8875_IF == RA_HARD_SPI)	/* 硬件SPI接口 */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_READ_STATUS);
		value = SPI_ShiftByte(0x00);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080硬件总线16bit */
	{
		value = RA8875_REG;
	}
	return value;	
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_GetDispMemAddr
*	功能说明: 获得显存地址。
*	形    参: 无
*	返 回 值: 显存地址
*********************************************************************************************************
*/
uint32_t RA8875_GetDispMemAddr(void)
{
	if (g_RA8875_IF == RA_HARD_SPI)	/* 硬件SPI接口 */
	{
		return 0;
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080硬件总线16bit */
	{
		return RA8875_RAM_ADDR;
	}
	return 0;
}

#ifdef RA_HARD_SPI_EN	   /* 四线SPI接口模式 */

/*
*********************************************************************************************************
*	函 数 名: RA8875_InitSPI
*	功能说明: 配置STM32和RA8875的SPI口线，使能硬件SPI1, 片选由软件控制
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_InitSPI(void)
{
	/*
		安富莱STM32-V5 开发板口线分配：  串行Flash型号为 W25Q64BVSSIG (80MHz)
		PA5/SPI1_SCK
		PA6/SPI1_MISO
		PA7/SPI1_MOSI
		PG11/TP_NCS			--- 触摸芯片的片选		(RA8875屏无需SPI接口触摸芯片）
	*/
	{
		GPIO_InitTypeDef GPIO_InitStructure;

		/* 使能GPIO 时钟 */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOG, ENABLE);

		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
		GPIO_Init(GPIOA, &GPIO_InitStructure);

		/* 配置片选口线为推挽输出模式 */
		RA8875_CS_1();		/* 片选置高，不选中 */
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
		GPIO_Init(GPIOG, &GPIO_InitStructure);
	}
	
	RA8875_LowSpeedSPI();
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_LowSpeedSPI
*	功能说明: 配置STM32内部SPI硬件的工作模式、速度等参数，用于访问SPI接口的RA8875. 低速。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_LowSpeedSPI(void)
{
	SPI_InitTypeDef  SPI_InitStructure;

	/* 打开SPI时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

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
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;

	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* 数据位传输次序：高位先传 */
	SPI_InitStructure.SPI_CRCPolynomial = 7;			/* CRC多项式寄存器，复位后为7。本例程不用 */
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, DISABLE);			/* 先禁止SPI  */

	SPI_Cmd(SPI1, ENABLE);				/* 使能SPI  */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_HighSpeedSPI
*	功能说明: 配置STM32内部SPI硬件的工作模式、速度等参数，用于访问SPI接口的RA8875.
*			  初始化完成后，可以将SPI切换到高速模式，以提高刷屏效率。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_HighSpeedSPI(void)
{
	SPI_InitTypeDef  SPI_InitStructure;

	/* 打开SPI时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

	/* 配置SPI硬件参数 */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;	/* 数据方向：2线全双工 */
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;		/* STM32的SPI工作模式 ：主机模式 */
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;	/* 数据位长度 ： 8位 */
	/* SPI_CPOL和SPI_CPHA结合使用决定时钟和数据采样点的相位关系、
	   本例配置: 总线空闲是高电平,第2个边沿（上升沿采样数据)
	*/
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;			/* 时钟上升沿采样数据 */
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;		/* 时钟的第2个边沿采样数据 */
	//SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;			/* 时钟上升沿采样数据 */
	//SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;		/* 时钟的第2个边沿采样数据 */	
	
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;			/* 片选控制方式：软件控制 */

	/*
	
		示波器实测频率 (STM32F103ZE 上测试)
		SPI_BaudRatePrescaler_4 时， SCK = 18M  (显示正常，触摸不正常)
		SPI_BaudRatePrescaler_8 时， SCK = 9M   (显示和触摸都正常)
		
		F407 的 SP1时钟=84M, 需要 8分频 = 10.5M
	*/
	
	/* 设置波特率预分频系数 */
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;

	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* 数据位传输次序：高位先传 */
	SPI_InitStructure.SPI_CRCPolynomial = 7;			/* CRC多项式寄存器，复位后为7。本例程不用 */
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, DISABLE);			/* 先禁止SPI  */

	SPI_Cmd(SPI1, ENABLE);				/* 使能SPI  */
}

/*
*********************************************************************************************************
*	函 数 名: SPI_ShiftByte
*	功能说明: 向SPI总线发送一个字节，同时返回接收到的字节
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static uint8_t SPI_ShiftByte(uint8_t _ucByte)
{
	uint8_t ucRxByte;

	/* 等待发送缓冲区空 */
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);

	/* 发送一个字节 */
	SPI_I2S_SendData(SPI1, _ucByte);

	/* 等待数据接收完毕 */
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);

	/* 读取接收到的数据 */
	ucRxByte = SPI_I2S_ReceiveData(SPI1);

	/* 返回读到的数据 */
	return ucRxByte;
}

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
