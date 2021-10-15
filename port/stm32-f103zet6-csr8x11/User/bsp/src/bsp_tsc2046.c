/*
*********************************************************************************************************
*
*	模块名称 : TSC2046电阻触摸芯片驱动程序
*	文件名称 : bsp_tsc2046.c
*	版    本 : V1.0
*	说    明 : TSC2046触摸芯片驱动程序。
*	修改记录 :
*		版本号   日期        作者     说明
*		V1.0    2014-10-23  armfly   正式发布
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*********************************************************************************************************
*/

#include "bsp.h"

/*
【1】安富莱STM32-V4开发板 + 3.5寸显示模块， 显示模块上的触摸芯片为 TSC2046或其兼容芯片
	PG11           --> TP_CS
	               --> TP_BUSY   ( 不用)
	PA5/SPI1_SCK   --> TP_SCK
	PA6/SPI1_MISO  --> TP_MISO
	PA7/SPI1_MOSI  --> TP_MOSI

	PC5/TP_INT     --> TP_PEN_INT
*/

/* TSC2046 触摸芯片 */
#define TSC2046_RCC_CS 		RCC_APB2Periph_GPIOG
#define TSC2046_PORT_CS		GPIOG
#define TSC2046_PIN_CS		GPIO_Pin_11
#define TSC2046_CS_1()		TSC2046_PORT_CS->BSRR = TSC2046_PIN_CS
#define TSC2046_CS_0()		TSC2046_PORT_CS->BRR = TSC2046_PIN_CS

#define TSC2046_RCC_INT 	RCC_APB2Periph_GPIOC
#define TSC2046_PORT_INT	GPIOC
#define TSC2046_PIN_INT		GPIO_Pin_5

void TSC2046_CfgSpiHard(void);
static void TSC2046_SetCS(uint8_t _level);

/*
*********************************************************************************************************
*	函 数 名: bsp_InitTouch
*	功能说明: 配置STM32和触摸相关的口线，片选由软件控制. SPI总线的GPIO在bsp_spi_bus.c 中统一配置。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void TSC2046_InitHard(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	/* 打开GPIO时钟 */
	RCC_APB2PeriphClockCmd(TSC2046_RCC_CS | TSC2046_RCC_INT, ENABLE);

	/* 配置几个推完输出IO */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* 推挽输出模式 */

	GPIO_InitStructure.GPIO_Pin = TSC2046_PIN_CS;
	GPIO_Init(TSC2046_PORT_CS, &GPIO_InitStructure);

	/* 不用配置TP_BUSY  */

	/* 配置触笔中断IO */
	GPIO_InitStructure.GPIO_Pin = TSC2046_PIN_INT;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	/* 输入浮空模式 */
	GPIO_Init(TSC2046_PORT_INT, &GPIO_InitStructure);

	//TSC2046_CfgSpiHard();
}

/*
*********************************************************************************************************
*	函 数 名: TSC2046_ReadAdc
*	功能说明: 选择一个模拟通道，启动ADC，并返回ADC采样结果
*	形    参:  _ucCh = 0 表示X通道； 1表示Y通道
*	返 回 值: 12位ADC值
*********************************************************************************************************
*/
uint16_t TSC2046_ReadAdc(uint8_t _ucCh)
{
	uint16_t usAdc;

	TSC2046_SetCS(0);		/* 使能TS2046的片选 */

	/*
		TSC2046 控制字（8Bit）
		Bit7   = S     起始位，必须是1
		Bit6:4 = A2-A0 模拟输入通道选择A2-A0; 共有6个通道。
		Bit3   = MODE  ADC位数选择，0 表示12Bit;1表示8Bit
		Bit2   = SER/DFR 模拟输入形式，  1表示单端输入；0表示差分输入
		Bit1:0 = PD1-PD0 掉电模式选择位
	*/
	bsp_spiWrite0((1 << 7) | (_ucCh << 4));			/* 选择通道1, 测量X位置 */

	/* 读ADC结果, 12位ADC值的高位先传，前12bit有效，最后4bit填0 */
	usAdc = bsp_spiRead0();		/* 发送的0x00可以为任意值，无意义 */
	usAdc <<= 8;
	usAdc += bsp_spiRead0();		/* 获得12位的ADC采样值 */

	usAdc >>= 3;						/* 右移3位，保留12位有效数字.  */

	TSC2046_SetCS(1);					/* 禁能片选 */

	return (usAdc);
}

/*
*********************************************************************************************************
*	函 数 名: TSC2046_CfgSpiHard
*	功能说明: 配置STM32内部SPI硬件的工作模式、速度等参数，用于访问TSC2046。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void TSC2046_CfgSpiHard(void)
{
	SPI_InitTypeDef  SPI_InitStructure;

	/* 配置 SPI1工作模式 */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft; 		/* 软件控制片选 */
	/*
		SPI_BaudRatePrescaler_64 对应SCK时钟频率约1M
		TSC2046 对SCK时钟的要求，高电平和低电平最小200ns，周期400ns，也就是2.5M

		示波器实测频率
		SPI_BaudRatePrescaler_64 时，SCK时钟频率约 1.116M
		SPI_BaudRatePrescaler_32 时，SCK时钟频率月 2.232M
	*/
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_Init(SPI1,&SPI_InitStructure);

	/* 使能 SPI1 */
	SPI_Cmd(SPI1,ENABLE);
}

/*
*********************************************************************************************************
*	函 数 名: TSC2046_SetCS(0)
*	功能说明: 设置CS。 用于运行中SPI共享。
*	形    参: 无
	返 回 值: 无
*********************************************************************************************************
*/
static void TSC2046_SetCS(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* 占用SPI总线， 用于总线共享 */

		#ifdef SOFT_SPI		/* 软件SPI */
			bsp_SetSpiSck(0);
			TSC2046_CS_0();
		#endif

		#ifdef HARD_SPI		/* 硬件SPI */
			TSC2046_CS_0();

			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_Low | SPI_CPHA_1Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_128 | SPI_FirstBit_MSB);
		#endif
	}
	else
	{
		TSC2046_CS_1();

		bsp_SpiBusExit();	/* 释放SPI总线， 用于总线共享 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: TSC2046_PenInt
*	功能说明: 判断触摸笔中断
*	形    参: 无
*	返 回 值: 0表示触笔按下  1表示释放
*********************************************************************************************************
*/
uint8_t TSC2046_PenInt(void)
{
	if (GPIO_ReadInputDataBit(TSC2046_PORT_INT, TSC2046_PIN_INT) == Bit_RESET)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
