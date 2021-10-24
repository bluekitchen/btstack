/*
*********************************************************************************************************
*
*	模块名称 : DAC8501 驱动模块(单通道带16位DAC)
*	文件名称 : bsp_dac8501.c
*	版    本 : V1.0
*	说    明 : DAC8501模块和CPU之间采用SPI接口。本驱动程序支持硬件SPI接口和软件SPI接口。
*			  通过宏切换。
*
*	修改记录 :
*		版本号  日期         作者     说明
*		V1.0    2014-01-17  armfly  正式发布
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	DAC8501模块可以直接插到STM32-V4开发板CN29排母(2*4P 2.54mm)接口上

    DAC8501模块    开发板
	  VCC   ------  3.3V
	  GND   ------  GND
      SCLK  ------  PA5/SPI1_SCK
      MOSI  ------  PA6/SPI1_MOSI
      CS1   ------  PC3/NRF24L01_CSN
	  CS2   ------  PB3/NRF24L01_CE
			------  PA7/SPI1_MISO
			------  PB0/NRF24L01_IRQ

*/

/*
	DAC8501基本特性:
	1、供电2.7 - 5V;  【本例使用3.3V】
	4、参考电压2.5V (推荐缺省的，外置的）

	对SPI的时钟速度要求: 高达30MHz， 速度很快.
	SCLK下降沿读取数据, 每次传送24bit数据， 高位先传
*/

#define DAC8501_RCC_CS1 	RCC_APB2Periph_GPIOC
#define DAC8501_PORT_CS1	GPIOC
#define DAC8501_PIN_CS1		GPIO_Pin_3
#define DAC8501_CS1_1()		DAC8501_PORT_CS1->BSRR = DAC8501_PIN_CS1
#define DAC8501_CS1_0()		DAC8501_PORT_CS1->BRR = DAC8501_PIN_CS1

#define DAC8501_RCC_CS2 	RCC_APB2Periph_GPIOB
#define DAC8501_PORT_CS2	GPIOB
#define DAC8501_PIN_CS2		GPIO_Pin_3
#define DAC8501_CS2_1()		DAC8501_PORT_CS2->BSRR = DAC8501_PIN_CS2
#define DAC8501_CS2_0()		DAC8501_PORT_CS2->BRR = DAC8501_PIN_CS2

/* 定义电压和DAC值间的关系。 两点校准 x是dac y 是电压 0.1mV */
#define X1	100
#define Y1  50

#define X2	65000
#define Y2  49400

static void DAC8501_ConfigGPIO(void);

/*
*********************************************************************************************************
*	函 数 名: bsp_InitDAC8501
*	功能说明: 配置STM32的GPIO和SPI接口，用于连接 ADS1256
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitDAC8501(void)
{
	DAC8501_ConfigGPIO();

	DAC8501_SetDacData(0, 0);
	DAC8501_SetDacData(1, 0);
}

/*
*********************************************************************************************************
*	函 数 名: DAC8501_CfgSpiHard
*	功能说明: 配置STM32内部SPI硬件的工作模式、速度等参数，用于访问TM7705
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void DAC8501_CfgSpiHard(void)
{
	SPI_InitTypeDef  SPI_InitStructure;

	/* 配置 SPI1工作模式 */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft; 		/* 软件控制片选 */

	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_Init(SPI1,&SPI_InitStructure);

	/* 使能 SPI1 */
	SPI_Cmd(SPI1,ENABLE);
}

/*
*********************************************************************************************************
*	函 数 名: DAC8501_SetCS1(0)
*	功能说明: 设置CS1。 用于运行中SPI共享。
*	形    参: 无
	返 回 值: 无
*********************************************************************************************************
*/
void DAC8501_SetCS1(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* 占用SPI总线， 用于总线共享 */

		#ifdef SOFT_SPI		/* 软件SPI */
			bsp_SetSpiSck(0);
			DAC8501_CS1_0();
		#endif

		#ifdef HARD_SPI		/* 硬件SPI */
			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_Low | SPI_CPHA_1Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_8 | SPI_FirstBit_MSB);

			DAC8501_CS1_0();
		#endif
	}
	else
	{
		DAC8501_CS1_1();

		bsp_SpiBusExit();	/* 释放SPI总线， 用于总线共享 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: DAC8501_SetCS2(0)
*	功能说明: 设置CS2。 用于运行中SPI共享。
*	形    参: 无
	返 回 值: 无
*********************************************************************************************************
*/
void DAC8501_SetCS2(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* 占用SPI总线， 用于总线共享 */

		#ifdef SOFT_SPI		/* 软件SPI */
			bsp_SetSpiSck(0);
			DAC8501_CS2_0();
		#endif

		#ifdef HARD_SPI		/* 硬件SPI */
			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_Low | SPI_CPHA_1Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_8 | SPI_FirstBit_MSB);

			DAC8501_CS2_0();
		#endif
	}
	else
	{
		DAC8501_CS2_1();

		bsp_SpiBusExit();	/* 释放SPI总线， 用于总线共享 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: DAC8501_ConfigGPIO
*	功能说明: 配置GPIO。 不包括 SCK  MOSI  MISO 共享的SPI总线。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void DAC8501_ConfigGPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 打开GPIO时钟 */
	RCC_APB2PeriphClockCmd(DAC8501_RCC_CS1 | DAC8501_RCC_CS2, ENABLE);

	/* 配置几个推挽输出IO */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* 推挽输出模式 */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	

	GPIO_InitStructure.GPIO_Pin = DAC8501_PIN_CS1;
	GPIO_Init(DAC8501_PORT_CS1, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = DAC8501_PIN_CS2;
	GPIO_Init(DAC8501_PORT_CS2, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	函 数 名: DAC8501_SetDacData
*	功能说明: 设置DAC数据
*	形    参: _ch, 通道,
*		     _data : 数据
*	返 回 值: 无
*********************************************************************************************************
*/
void DAC8501_SetDacData(uint8_t _ch, uint16_t _dac)
{
	uint32_t data;

	/*
		DAC8501.pdf page 12 有24bit定义

		DB24:18 = xxxxx 保留
		DB17： PD1
		DB16： PD0

		DB15：0  16位数据

		其中 PD1 PD0 决定4种工作模式
		      0   0  ---> 正常工作模式
		      0   1  ---> 输出接1K欧到GND
		      1   0  ---> 输出100K欧到GND
		      1   1  ---> 输出高阻
	*/

	data = _dac; /* PD1 PD0 = 00 正常模式 */

	if (_ch == 0)
	{
		DAC8501_SetCS1(0);
	}
	else
	{
		DAC8501_SetCS2(0);
	}

	/*　DAC8501 SCLK时钟高达30M，因此可以不延迟 */
	bsp_spiWrite0(data >> 16);
	bsp_spiWrite0(data >> 8);
	bsp_spiWrite0(data);

	if (_ch == 0)
	{
		DAC8501_SetCS1(1);
	}
	else
	{
		DAC8501_SetCS2(1);
	}
}

/*
*********************************************************************************************************
*	函 数 名: DAC8501_DacToVoltage
*	功能说明: 将DAC值换算为电压值，单位0.1mV
*	形    参: _dac  16位DAC字
*	返 回 值: 电压。单位0.1mV
*********************************************************************************************************
*/
int32_t DAC8501_DacToVoltage(uint16_t _dac)
{
	int32_t y;

	/* CaculTwoPoint(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x);*/
	y =  CaculTwoPoint(X1, Y1, X2, Y2, _dac);
	if (y < 0)
	{
		y = 0;
	}
	return y;
}

/*
*********************************************************************************************************
*	函 数 名: DAC8501_DacToVoltage
*	功能说明: 将DAC值换算为电压值，单位 0.1mV
*	形    参: _volt 电压。单位0.1mV
*	返 回 值: 16位DAC字
*********************************************************************************************************
*/
uint32_t DAC8501_VoltageToDac(int32_t _volt)
{
	/* CaculTwoPoint(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x);*/
	return CaculTwoPoint(Y1, X1, Y2, X2, _volt);
}



/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
