/*
*********************************************************************************************************
*
*	模块名称 : DHT11 驱动模块(1-wire 数字温度传感器）
*	文件名称 : bsp_dht11.c
*	版    本 : V1.0
*	说    明 : DHT11和CPU之间采用1个GPIO接口。建议调用 DHT11_ReadData()的频率不要大于1Hz。
*
*	修改记录 :
*		版本号  日期         作者     说明
*		V1.0    2014-01-24  armfly  正式发布
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	DHT11 可以用杜邦线连接到开发板。

     DHT11      STM32F407开发板
	  VCC   ------  3.3V
	  DQ    ------  PG8   (开发板上有 4.7K 上拉电阻. 和 USER 键复用)
	  GND   ------  GND
*/

/* 定义GPIO端口 */
#define RCC_DQ		RCC_APB2Periph_GPIOG
#define PORT_DQ		GPIOG
#define PIN_DQ		GPIO_Pin_2

#define DQ_0()		PORT_DQ->BRR = PIN_DQ
#define DQ_1()		PORT_DQ->BSRR = PIN_DQ

/* 判断DQ输入是否为低 */
#define DQ_IS_LOW()	((PORT_DQ->IDR & PIN_DQ) == 0)


static uint8_t DHT11_ReadByte(void);

/*
*********************************************************************************************************
*	函 数 名: bsp_InitDHT11
*	功能说明: 配置STM32的GPIO和SPI接口，用于连接 DHT11
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitDHT11(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 打开GPIO时钟 */
	RCC_APB2PeriphClockCmd(RCC_DQ, ENABLE);

	DQ_1();

	/* 配置DQ为开漏输出 */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;	/* 开漏输出模式 */

	GPIO_InitStructure.GPIO_Pin = PIN_DQ;
	GPIO_Init(PORT_DQ, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	函 数 名: DHT11_ReadData
*	功能说明: 复位DHT11。 拉低DQ为低，持续最少480us，然后等待
*	形    参: 无
*	返 回 值: 0 失败； 1 表示成功
*********************************************************************************************************
*/
uint8_t DHT11_ReadData(DHT11_T *_pDTH)
{
	/*
		时序:
		1. MCU拉低QD持续时间大于 18ms, 然后释放QD = 1
	*/
	uint8_t i;
	uint8_t k;
	uint8_t sum;

	/* 1. 主机发送起始信号, DQ拉低时间 ＞ 18ms */
	DQ_0();		/* DQ = 0 */
	bsp_DelayMS(20);
	DQ_1();		/* DQ = 1 */

	bsp_DelayUS(15);	/* 等待15us */

	/* 2. 等待DQ电平变低 ( 超时100us) */
	for (k = 0; k < 10; k++)
	{
		if (DQ_IS_LOW())
		{
			break;
		}
		bsp_DelayUS(10);
	}
	if (k >= 10)
	{
		goto quit;		/* 超时无应答，失败 */
	}

	/* 3.等待DQ电平变高 (超时100us) */
	for (k = 0; k < 10; k++)
	{
		if (!DQ_IS_LOW())
		{
			break;
		}
		bsp_DelayUS(10);
	}
	if (k >= 10)
	{
		goto quit;		/* 超时无应答，失败 */
	}

	/* 4.等待DQ电平变低 (超时100us) */
	for (k = 0; k < 10; k++)
	{
		if (DQ_IS_LOW())
		{
			break;
		}
		bsp_DelayUS(10);
	}
	if (k >= 10)
	{
		goto quit;		/* 超时无应答，失败 */
	}

	/* 读40bit 数据 */
	for (i = 0; i < 5; i++)
	{
		_pDTH->Buf[i] = DHT11_ReadByte();
	}
	bsp_DelayUS(100);

	/* 计算校验和 */
	sum = _pDTH->Buf[0] + _pDTH->Buf[1] + _pDTH->Buf[2] + _pDTH->Buf[3];
	if (sum == _pDTH->Buf[4])
	{
		_pDTH->Temp = _pDTH->Buf[2];	/* 温度整数部分 */
		_pDTH->Hum = _pDTH->Buf[0];	/* 湿度整数部分 */
		return 1;
	}
quit:
	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: DHT11_ReadByte
*	功能说明: 向DHT11读取1字节数据
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static uint8_t DHT11_ReadByte(void)
{
	/*
		写数据时序, 见DHT11 page 16
	*/
	uint8_t i,k;
	uint8_t read = 0;

	for (i = 0; i < 8; i++)
	{
		read <<= 1;
		/* 等待DQ电平变高 (超时100us) */
		for (k = 0; k < 10; k++)
		{
			if (!DQ_IS_LOW())
			{
				break;
			}
			bsp_DelayUS(10);
		}
		if (k >= 10)
		{
			goto quit;		/* 超时无应答，失败 */
		}

		/* 等待DQ电平变低，统计DQ高电平时长 (超时100us) */
		for (k = 0; k < 10; k++)
		{
			if (DQ_IS_LOW())
			{
				break;
			}
			bsp_DelayUS(10);
		}

		if (k > 3)		/* 高脉冲持续时间大于40us ，认为是1，否则是0 */
		{
			read++;
		}
	}

	return read;

quit:
	return 0xFF;
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
