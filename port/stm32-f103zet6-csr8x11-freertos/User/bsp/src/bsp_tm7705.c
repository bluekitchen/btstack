/*
*********************************************************************************************************
*
*	模块名称 : TM7705 驱动模块(2通道带PGA的16位ADC)
*	文件名称 : bsp_tm7705.c
*	版    本 : V1.0
*	说    明 : TM7705模块和CPU之间采用SPI接口。本驱动程序支持硬件SPI接口和软件SPI接口。
*			  通过宏切换。
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-10-20  armfly  正式发布
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* 通道1和通道2的增益,输入缓冲，极性 */
#define __CH1_GAIN_BIPOLAR_BUF	(GAIN_1 | UNIPOLAR | BUF_EN)
#define __CH2_GAIN_BIPOLAR_BUF	(GAIN_1 | UNIPOLAR | BUF_EN)

/*
	TM7705模块可以直接插到STM32-V4开发板nRF24L01模块的排母接口上。

    TM7705模块   STM32-V4开发板
      SCK   ------  PA5/SPI1_SCK
      DOUT  ------  PA6/SPI1_MISO
      DIN   ------  PA7/SPI1_MOSI
      CS    ------  PC3/ADC123_IN13/NRF24L01_CSN
      DRDY  ------  PB0/MPU-6050_INT/NRF24L01_IRQ
      RST   ------  PB3/TDO/NRF24L01_CE	(复位 RESET)

*/

/* SPI总线的SCK、MOSI、MISO 在 bsp_spi_bus.c中配置  */

/* TM7705 */
#define TM7705_RCC_CS 		RCC_APB2Periph_GPIOC
#define TM7705_PORT_CS		GPIOC
#define TM7705_PIN_CS		GPIO_Pin_3

#define TM7705_CS_1()		TM7705_PORT_CS->BSRR = TM7705_PIN_CS
#define TM7705_CS_0()		TM7705_PORT_CS->BRR = TM7705_PIN_CS

/* RESET */
#define RCC_RESET 	RCC_APB2Periph_GPIOB
#define PORT_RESET	GPIOB
#define PIN_RESET	GPIO_Pin_3

#define RESET_0()	GPIO_ResetBits(PORT_RESET, PIN_RESET)
#define RESET_1()	GPIO_SetBits(PORT_RESET, PIN_RESET)

/* DRDY */
#define RCC_DRDY 	RCC_APB2Periph_GPIOB
#define PORT_DRDY	GPIOB
#define PIN_DRDY	GPIO_Pin_0

#define DRDY_IS_LOW()	(GPIO_ReadInputDataBit(PORT_DRDY, PIN_DRDY) == Bit_RESET)

/* 通信寄存器bit定义 */
enum
{
	/* 寄存器选择  RS2 RS1 RS0  */
	REG_COMM	= 0x00,	/* 通信寄存器 */
	REG_SETUP	= 0x10,	/* 设置寄存器 */
	REG_CLOCK	= 0x20,	/* 时钟寄存器 */
	REG_DATA	= 0x30,	/* 数据寄存器 */
	REG_ZERO_CH1	= 0x60,	/* CH1 偏移寄存器 */
	REG_FULL_CH1	= 0x70,	/* CH1 满量程寄存器 */
	REG_ZERO_CH2	= 0x61,	/* CH2 偏移寄存器 */
	REG_FULL_CH2	= 0x71,	/* CH2 满量程寄存器 */

	/* 读写操作 */
	WRITE 		= 0x00,	/* 写操作 */
	READ 		= 0x08,	/* 读操作 */

	/* 通道 */
	CH_1		= 0,	/* AIN1+  AIN1- */
	CH_2		= 1,	/* AIN2+  AIN2- */
	CH_3		= 2,	/* AIN1-  AIN1- */
	CH_4		= 3		/* AIN1-  AIN2- */
};

/* 设置寄存器bit定义 */
enum
{
	MD_NORMAL		= (0 << 6),	/* 正常模式 */
	MD_CAL_SELF		= (1 << 6),	/* 自校准模式 */
	MD_CAL_ZERO		= (2 << 6),	/* 校准0刻度模式 */
	MD_CAL_FULL		= (3 << 6),	/* 校准满刻度模式 */

	GAIN_1			= (0 << 3),	/* 增益 */
	GAIN_2			= (1 << 3),	/* 增益 */
	GAIN_4			= (2 << 3),	/* 增益 */
	GAIN_8			= (3 << 3),	/* 增益 */
	GAIN_16			= (4 << 3),	/* 增益 */
	GAIN_32			= (5 << 3),	/* 增益 */
	GAIN_64			= (6 << 3),	/* 增益 */
	GAIN_128		= (7 << 3),	/* 增益 */

	/* 无论双极性还是单极性都不改变任何输入信号的状态，它只改变输出数据的代码和转换函数上的校准点 */
	BIPOLAR			= (0 << 2),	/* 双极性输入 */
	UNIPOLAR		= (1 << 2),	/* 单极性输入 */

	BUF_NO			= (0 << 1),	/* 输入无缓冲（内部缓冲器不启用) */
	BUF_EN			= (1 << 1),	/* 输入有缓冲 (启用内部缓冲器) */

	FSYNC_0			= 0,
	FSYNC_1			= 1		/* 不启用 */
};

/* 时钟寄存器bit定义 */
enum
{
	CLKDIS_0	= 0x00,		/* 时钟输出使能 （当外接晶振时，必须使能才能振荡） */
	CLKDIS_1	= 0x10,		/* 时钟禁止 （当外部提供时钟时，设置该位可以禁止MCK_OUT引脚输出时钟以省电 */

	/*
		2.4576MHz（CLKDIV=0 ）或为 4.9152MHz （CLKDIV=1 ），CLK 应置 “0”。
		1MHz （CLKDIV=0 ）或 2MHz   （CLKDIV=1 ），CLK 该位应置  “1”
	*/
	CLK_4_9152M = 0x08,
	CLK_2_4576M = 0x00,
	CLK_1M 		= 0x04,
	CLK_2M 		= 0x0C,

	FS_50HZ		= 0x00,
	FS_60HZ		= 0x01,
	FS_250HZ	= 0x02,
	FS_500HZ	= 0x04,

	/*
		四十九、电子秤应用中提高TM7705 精度的方法
			当使用主时钟为 2.4576MHz 时，强烈建议将时钟寄存器设为 84H,此时数据输出更新率为10Hz,即每0.1S 输出一个新数据。
			当使用主时钟为 1MHz 时，强烈建议将时钟寄存器设为80H, 此时数据输出更新率为4Hz, 即每0.25S 输出一个新数据
	*/
	ZERO_0		= 0x00,
	ZERO_1		= 0x80
};

static void TM7705_SyncSPI(void);
static void TM7705_WriteByte(uint8_t _data);
static void TM7705_Write3Byte(uint32_t _data);
static uint8_t TM7705_ReadByte(void);
static uint16_t TM7705_Read2Byte(void);
static uint32_t TM7705_Read3Byte(void);
static void TM7705_WaitDRDY(void);
static void TM7705_ResetHard(void);

static void TM7705_ConfigGPIO(void);
void TM7705_CfgSpiHard(void);

uint8_t g_TM7705_OK = 0;	/* 全局标志，表示TM7705芯片是否连接正常  */

uint16_t g_TM7705_Adc1;
uint16_t g_TM7705_Adc2;

/*
*********************************************************************************************************
*	函 数 名: TM7705_ConfigGPIO
*	功能说明: 配置GPIO。 不包括 SCK  MOSI  MISO 共享的SPI总线。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void TM7705_ConfigGPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 打开GPIO时钟 */
	RCC_APB2PeriphClockCmd(RCC_DRDY | RCC_RESET | TM7705_RCC_CS, ENABLE);

	/* 配置几个推完输出IO */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* 推挽输出模式 */

	GPIO_InitStructure.GPIO_Pin = PIN_RESET;
	GPIO_Init(PORT_RESET, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = TM7705_PIN_CS;
	GPIO_Init(TM7705_PORT_CS, &GPIO_InitStructure);

	/* 配置GPIO为浮动输入模式(实际上CPU复位后就是输入状态) */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	/* 输入浮空模式 */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	/* IO口最大速度 */

	GPIO_InitStructure.GPIO_Pin = PIN_DRDY;
	GPIO_Init(PORT_DRDY, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_CfgSpiHard
*	功能说明: 配置STM32内部SPI硬件的工作模式、速度等参数，用于访问TM7705
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void TM7705_CfgSpiHard(void)
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

	/* 设置波特率预分频系数 SPI_BaudRatePrescaler_64 实测SCK周期 800ns ，12.5MHz */
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;

	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* 数据位传输次序：高位先传 */
	SPI_InitStructure.SPI_CRCPolynomial = 7;			/* CRC多项式寄存器，复位后为7。本例程不用 */
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, ENABLE);				/* 使能SPI  */
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_SetCS(0)
*	功能说明: 设置CS。 用于运行中SPI共享。
*	形    参: 无
	返 回 值: 无
*********************************************************************************************************
*/
void TM7705_SetCS(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* 占用SPI总线， 用于总线共享 */

		#ifdef SOFT_SPI		/* 软件SPI */
			bsp_SetSpiSck(1);
			TM7705_CS_0();
		#endif

		#ifdef HARD_SPI		/* 硬件SPI */
			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_High | SPI_CPHA_2Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_64 | SPI_FirstBit_MSB);

			TM7705_CS_0();
		#endif
	}
	else
	{
		TM7705_CS_1();

		bsp_SpiBusExit();	/* 释放SPI总线， 用于总线共享 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: uint8_t
*	功能说明: 配置STM32的GPIO和SPI接口，用于连接 TM7705
*	形    参: 无
*	返 回 值: 0 表示失败; 1 表示成功
*********************************************************************************************************
*/
void bsp_InitTM7705(void)
{
	TM7705_ConfigGPIO();
	//TM7705_CfgSpiHard();

	bsp_DelayMS(10);

	TM7705_ResetHard();		/* 硬件复位 */

	/*
		在接口序列丢失的情况下，如果在DIN 高电平的写操作持续了足够长的时间（至少 32个串行时钟周期），
		TM7705 将会回到默认状态。
	*/
	bsp_DelayMS(5);

	TM7705_SyncSPI();		/* 同步SPI接口时序 */

	bsp_DelayMS(5);

	/* 复位之后, 时钟寄存器应该是 0x05 再进入时 08*/
	{
		uint8_t reg;
		
		reg = TM7705_ReadReg(REG_CLOCK);
		if ((reg == 0x05) || (reg == 0x08))
		{
			g_TM7705_OK = 1;
		}
		else
		{
			g_TM7705_OK = 0;
		}
	}

	/* 配置时钟寄存器 */
	TM7705_WriteByte(REG_CLOCK | WRITE | CH_1);			/* 先写通信寄存器，下一步是写时钟寄存器 */

	TM7705_WriteByte(CLKDIS_0 | CLK_4_9152M | FS_50HZ);	/* 刷新速率50Hz */
	//TM7705_WriteByte(CLKDIS_0 | CLK_4_9152M | FS_500HZ);	/* 刷新速率500Hz */

	/* 每次上电进行一次自校准 */
	TM7705_CalibSelf(1);	/* 内部自校准 CH1 */
	bsp_DelayMS(5);
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_ResetHard
*	功能说明: 硬件复位 TM7705芯片
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void TM7705_ResetHard(void)
{
	RESET_1();
	bsp_DelayMS(1);
	RESET_0();
	bsp_DelayMS(2);
	RESET_1();
	bsp_DelayMS(1);
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_SyncSPI
*	功能说明: 同步TM7705芯片SPI接口时序
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void TM7705_SyncSPI(void)
{
	/* AD7705串行接口失步后将其复位。复位后要延时500us再访问 */
	TM7705_SetCS(0);
	bsp_spiWrite1(0xFF);
	bsp_spiWrite1(0xFF);
	bsp_spiWrite1(0xFF);
	bsp_spiWrite1(0xFF);
	TM7705_SetCS(1);
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_WriteByte
*	功能说明: 写入1个字节。带CS控制
*	形    参: _data ：将要写入的数据
*	返 回 值: 无
*********************************************************************************************************
*/
static void TM7705_WriteByte(uint8_t _data)
{
	TM7705_SetCS(0);
	bsp_spiWrite1(_data);
	TM7705_SetCS(1);
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_Write3Byte
*	功能说明: 写入3个字节。带CS控制
*	形    参: _data ：将要写入的数据
*	返 回 值: 无
*********************************************************************************************************
*/
static void TM7705_Write3Byte(uint32_t _data)
{
	TM7705_SetCS(0);
	bsp_spiWrite1((_data >> 16) & 0xFF);
	bsp_spiWrite1((_data >> 8) & 0xFF);
	bsp_spiWrite1(_data);
	TM7705_SetCS(1);
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_ReadByte
*	功能说明: 从AD芯片读取一个字（16位）
*	形    参: 无
*	返 回 值: 读取的字（16位）
*********************************************************************************************************
*/
static uint8_t TM7705_ReadByte(void)
{
	uint8_t read;

	TM7705_SetCS(0);
	read = bsp_spiRead1();
	TM7705_SetCS(1);

	return read;
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_Read2Byte
*	功能说明: 读2字节数据
*	形    参: 无
*	返 回 值: 读取的数据（16位）
*********************************************************************************************************
*/
static uint16_t TM7705_Read2Byte(void)
{
	uint16_t read;

	TM7705_SetCS(0);
	read = bsp_spiRead1();
	read <<= 8;
	read += bsp_spiRead1();
	TM7705_SetCS(1);

	return read;
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_Read3Byte
*	功能说明: 读3字节数据
*	形    参: 无
*	返 回 值: 读取到的数据（24bit) 高8位固定为0.
*********************************************************************************************************
*/
static uint32_t TM7705_Read3Byte(void)
{
	uint32_t read;

	TM7705_SetCS(0);
	read = bsp_spiRead1();
	read <<= 8;
	read += bsp_spiRead1();
	read <<= 8;
	read += bsp_spiRead1();
	TM7705_SetCS(1);
	return read;
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_WaitDRDY
*	功能说明: 等待内部操作完成。 自校准时间较长，需要等待。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void TM7705_WaitDRDY(void)
{
	uint32_t i;

	/* 如果初始化时未检测到芯片则快速返回，避免影响主程序响应速度 */
	if (g_TM7705_OK == 0)
	{
		return;
	}

	for (i = 0; i < 4000000; i++)
	{
		if (DRDY_IS_LOW())
		{
			break;
		}
	}
	if (i >= 4000000)
	{
		printf("TM7705_WaitDRDY() Time Out ...\r\n");		/* 调试语句. 用语排错 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_WriteReg
*	功能说明: 写指定的寄存器
*	形    参:  _RegID : 寄存器ID
*			  _RegValue : 寄存器值。 对于8位的寄存器，取32位形参的低8bit
*	返 回 值: 无
*********************************************************************************************************
*/
void TM7705_WriteReg(uint8_t _RegID, uint32_t _RegValue)
{
	uint8_t bits;

	switch (_RegID)
	{
		case REG_COMM:		/* 通信寄存器 */
		case REG_SETUP:		/* 设置寄存器 8bit */
		case REG_CLOCK:		/* 时钟寄存器 8bit */
			bits = 8;
			break;

		case REG_ZERO_CH1:	/* CH1 偏移寄存器 24bit */
		case REG_FULL_CH1:	/* CH1 满量程寄存器 24bit */
		case REG_ZERO_CH2:	/* CH2 偏移寄存器 24bit */
		case REG_FULL_CH2:	/* CH2 满量程寄存器 24bit*/
			bits = 24;
			break;

		case REG_DATA:		/* 数据寄存器 16bit */
		default:
			return;
	}

	TM7705_WriteByte(_RegID | WRITE);	/* 写通信寄存器, 指定下一步是写操作，并指定写哪个寄存器 */

	if (bits == 8)
	{
		TM7705_WriteByte((uint8_t)_RegValue);
	}
	else	/* 24bit */
	{
		TM7705_Write3Byte(_RegValue);
	}
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_ReadReg
*	功能说明: 写指定的寄存器
*	形    参:  _RegID : 寄存器ID
*			  _RegValue : 寄存器值。 对于8位的寄存器，取32位形参的低8bit
*	返 回 值: 读到的寄存器值。 对于8位的寄存器，取32位形参的低8bit
*********************************************************************************************************
*/
uint32_t TM7705_ReadReg(uint8_t _RegID)
{
	uint8_t bits;
	uint32_t read;

	switch (_RegID)
	{
		case REG_COMM:		/* 通信寄存器 */
		case REG_SETUP:		/* 设置寄存器 8bit */
		case REG_CLOCK:		/* 时钟寄存器 8bit */
			bits = 8;
			break;

		case REG_ZERO_CH1:	/* CH1 偏移寄存器 24bit */
		case REG_FULL_CH1:	/* CH1 满量程寄存器 24bit */
		case REG_ZERO_CH2:	/* CH2 偏移寄存器 24bit */
		case REG_FULL_CH2:	/* CH2 满量程寄存器 24bit*/
			bits = 24;
			break;

		case REG_DATA:		/* 数据寄存器 16bit */
		default:
			return 0xFFFFFFFF;
	}

	TM7705_WriteByte(_RegID | READ);	/* 写通信寄存器, 指定下一步是写操作，并指定写哪个寄存器 */

	if (bits == 16)
	{
		read = TM7705_Read2Byte();
	}
	else if (bits == 8)
	{
		read = TM7705_ReadByte();
	}
	else	/* 24bit */
	{
		read = TM7705_Read3Byte();
	}
	return read;
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_CalibSelf
*	功能说明: 启动自校准. 内部自动短接AIN+ AIN-校准0位，内部短接到Vref 校准满位。此函数执行过程较长，
*			  实测约 180ms
*	形    参:  _ch : ADC通道，1或2
*	返 回 值: 无
*********************************************************************************************************
*/
void TM7705_CalibSelf(uint8_t _ch)
{
	if (_ch == 1)
	{
		/* 自校准CH1 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_1);	/* 写通信寄存器，下一步是写设置寄存器，通道1 */
		TM7705_WriteByte(MD_CAL_SELF | __CH1_GAIN_BIPOLAR_BUF | FSYNC_0);/* 启动自校准 */
		TM7705_WaitDRDY();	/* 等待内部操作完成 --- 时间较长，约180ms */
	}
	else if (_ch == 2)
	{
		/* 自校准CH2 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_2);	/* 写通信寄存器，下一步是写设置寄存器，通道2 */
		TM7705_WriteByte(MD_CAL_SELF | __CH2_GAIN_BIPOLAR_BUF | FSYNC_0);	/* 启动自校准 */
		TM7705_WaitDRDY();	/* 等待内部操作完成  --- 时间较长，约180ms */
	}
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_SytemCalibZero
*	功能说明: 启动系统校准零位. 请将AIN+ AIN-短接后，执行该函数。校准应该由主程序控制并保存校准参数。
*			 执行完毕后。可以通过 TM7705_ReadReg(REG_ZERO_CH1) 和  TM7705_ReadReg(REG_ZERO_CH2) 读取校准参数。
*	形    参: _ch : ADC通道，1或2
*	返 回 值: 无
*********************************************************************************************************
*/
void TM7705_SytemCalibZero(uint8_t _ch)
{
	if (_ch == 1)
	{
		/* 校准CH1 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_1);	/* 写通信寄存器，下一步是写设置寄存器，通道1 */
		TM7705_WriteByte(MD_CAL_ZERO | __CH1_GAIN_BIPOLAR_BUF | FSYNC_0);/* 启动自校准 */
		TM7705_WaitDRDY();	/* 等待内部操作完成 */
	}
	else if (_ch == 2)
	{
		/* 校准CH2 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_2);	/* 写通信寄存器，下一步是写设置寄存器，通道1 */
		TM7705_WriteByte(MD_CAL_ZERO | __CH2_GAIN_BIPOLAR_BUF | FSYNC_0);	/* 启动自校准 */
		TM7705_WaitDRDY();	/* 等待内部操作完成 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_SytemCalibFull
*	功能说明: 启动系统校准满位. 请将AIN+ AIN-接最大输入电压源，执行该函数。校准应该由主程序控制并保存校准参数。
*			 执行完毕后。可以通过 TM7705_ReadReg(REG_FULL_CH1) 和  TM7705_ReadReg(REG_FULL_CH2) 读取校准参数。
*	形    参:  _ch : ADC通道，1或2
*	返 回 值: 无
*********************************************************************************************************
*/
void TM7705_SytemCalibFull(uint8_t _ch)
{
	if (_ch == 1)
	{
		/* 校准CH1 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_1);	/* 写通信寄存器，下一步是写设置寄存器，通道1 */
		TM7705_WriteByte(MD_CAL_FULL | __CH1_GAIN_BIPOLAR_BUF | FSYNC_0);/* 启动自校准 */
		TM7705_WaitDRDY();	/* 等待内部操作完成 */
	}
	else if (_ch == 2)
	{
		/* 校准CH2 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_2);	/* 写通信寄存器，下一步是写设置寄存器，通道1 */
		TM7705_WriteByte(MD_CAL_FULL | __CH2_GAIN_BIPOLAR_BUF | FSYNC_0);	/* 启动自校准 */
		TM7705_WaitDRDY();	/* 等待内部操作完成 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_ReadAdc1
*	功能说明: 读通道1或2的ADC数据。 该函数执行较慢，但是可以读到正确结果。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t TM7705_ReadAdc(uint8_t _ch)
{
	uint8_t i;
	uint16_t read = 0;

	/* 为了避免通道切换造成读数失效，读2次 */
	for (i = 0; i < 2; i++)
	{
		TM7705_WaitDRDY();		/* 等待DRDY口线为0 */

		if (_ch == 1)
		{
			TM7705_WriteByte(0x38);
		}
		else if (_ch == 2)
		{
			TM7705_WriteByte(0x39);
		}

		read = TM7705_Read2Byte();
	}
	return read;
}


/*
*********************************************************************************************************
*	函 数 名: TM7705_Scan2
*	功能说明: 扫描2个通道。插入到主程序循环，空闲时采集ADC值放到全局变量。方便读取。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void TM7705_Scan2(void)
{
	static uint8_t s_ch = 1;

	/* 如果初始化时未检测到芯片则快速返回，避免影响主程序响应速度 */
	if (g_TM7705_OK == 0)
	{
		return;
	}

	if (DRDY_IS_LOW())
	{
		/* 双通道交替读 */
		if (s_ch == 1)
		{
			TM7705_WriteByte(0x38);
			g_TM7705_Adc2 = TM7705_Read2Byte();

			s_ch = 2;
		}
		else
		{
			TM7705_WriteByte(0x39);
			g_TM7705_Adc1 = TM7705_Read2Byte();

			s_ch = 1;
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_Scan1
*	功能说明: 扫描通道1。插入到主程序循环，空闲时采集ADC值放到全局变量。方便读取。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void TM7705_Scan1(void)
{
	/* 如果初始化时未检测到芯片则快速返回，避免影响主程序响应速度 */
	if (g_TM7705_OK == 0)
	{
		return;
	}

	if (DRDY_IS_LOW())
	{
		TM7705_WriteByte(0x38);
		g_TM7705_Adc1 = TM7705_Read2Byte();
	}
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_GetAdc1
*	功能说明: 读取扫描的结果。需要结合 TM7705_Scan2() 或 TM7705_Scan1() 使用
*	形    参: 无
*	返 回 值: 通道1的ADC值
*********************************************************************************************************
*/
uint16_t TM7705_GetAdc1(void)
{
	/* 如果初始化时未检测到芯片则快速返回，避免影响主程序响应速度 */
	if (g_TM7705_OK == 0)
	{
		return 0;
	}

	return g_TM7705_Adc1;
}

/*
*********************************************************************************************************
*	函 数 名: TM7705_GetAdc2
*	功能说明: 读取扫描的结果。
*	形    参: 无
*	返 回 值: 通道2的ADC值
*********************************************************************************************************
*/
uint16_t TM7705_GetAdc2(void)
{
	/* 如果初始化时未检测到芯片则快速返回，避免影响主程序响应速度 */
	if (g_TM7705_OK == 0)
	{
		return 0;
	}

	return g_TM7705_Adc2;
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
