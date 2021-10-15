/*
*********************************************************************************************************
*
*	模块名称 : GPRS模块SIM800驱动程序
*	文件名称 : bsp_SIM800.c
*	版    本 : V1.1
*	说    明 : 封装了SIMCOM公司的SIM800模块相关的AT命令。
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2015-02-01  armfly  正式发布
*		V1.1    2015-06-21  armfly  
*							1.完善代码，修改PowerOn()函数返回值.
*							2.不用软件定时器判断超时。使用 bsp_GetRunTimer()函数实现
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	安富莱STM32-V4 开发板口线分配：
	SIM800_TERM_ON   ： PB4/TRST/GPRS_TERM_ON
	SIM800_RESET   	 ： PC2/ADC123_IN12/GPRS_RESET
	SIM800_RX   	 ： PA2/USART2_TX
	SIM800_TX   	 ： PA3/USART2_RX
*/

/*
	AT+CREG?  查询当前网络状态

	AT+CSQ 查询信号质量命令

	AT+CIMI 查询SIM 卡的IMSI 号。

	AT+CIND? 读取当前的指示状态

	ATA 接听命令
	ATH 挂断连接命令
	
	ATI 显示SIM800模块的硬件信息
	
	ATD10086; 拨打10086
*/

/* 按键口对应的RCC时钟 */
#define RCC_TERM_ON 	RCC_APB2Periph_GPIOB
#define PORT_TERM_ON	GPIOB
#define PIN_TERM_ON		GPIO_Pin_4

#define RCC_RESET 		RCC_APB2Periph_GPIOC
#define PORT_RESET		GPIOC
#define PIN_RESET		GPIO_Pin_2

/* STM32和SIM800的PWRKEY引脚间有1个NPN三极管，因此需要反相 */
#define PWRKEY_1()		GPIO_ResetBits(PORT_TERM_ON, PIN_TERM_ON);
#define PWRKEY_0()		GPIO_SetBits(PORT_TERM_ON, PIN_TERM_ON);

/* STM32和MG323的RESET引脚间有1个NPN三极管，因此需要反相. MG323的RESET脚是低脉冲复位 */
#define MG_RESET_1()	GPIO_ResetBits(PORT_RESET, PIN_RESET);
#define MG_RESET_0()	GPIO_SetBits(PORT_RESET, PIN_RESET);

/*
*********************************************************************************************************
*	函 数 名: bsp_InitSIM800
*	功能说明: 配置无线模块相关的GPIO,  该函数被 bsp_Init() 调用。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitSIM800(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 打开GPIO时钟 */
	RCC_APB2PeriphClockCmd(RCC_TERM_ON | RCC_RESET, ENABLE);

	
	/* Disable the Serial Wire Jtag Debug Port SWJ-DP 
		JTAG-DP Disabled and SW-DP Enabled 
	 PB4/TRST/GPRS_TERM_ON 缺省用于JTAG信号，需要重新映射为	
	*/
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
	
	PWRKEY_1();
	
	/* 配置几个推完输出IO */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* 推挽输出模式 */

	GPIO_InitStructure.GPIO_Pin = PIN_TERM_ON;
	GPIO_Init(PORT_TERM_ON, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_RESET;
	GPIO_Init(PORT_RESET, &GPIO_InitStructure);

	/* CPU的串口配置已经由 bsp_uart_fifo.c 中的 bsp_InitUart() 做了 */
}

/*
*********************************************************************************************************
*	函 数 名: SIM800_PrintRxData
*	功能说明: 打印STM32从SIM800收到的数据到COM1串口，主要用于跟踪调试
*	形    参: _ch : 收到的数据
*	返 回 值: 无
*********************************************************************************************************
*/
void SIM800_PrintRxData(uint8_t _ch)
{
	#ifdef SIM800_TO_COM1_EN
		comSendChar(COM1, _ch);		/* 将接收到数据打印到调试串口1 */
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: SIM800_PowerOn
*	功能说明: 模块上电. 函数内部先判断是否已经开机，如果已开机则直接返回1
*	形    参: 无
*	返 回 值: 1 表示上电成功  0: 表示异常
*********************************************************************************************************
*/
uint8_t SIM800_PowerOn(void)
{
	uint8_t ret_value = 0;
	uint8_t i;
	
	/* 判断是否开机 */
	for (i = 0; i < 5; i++)
	{
		SIM800_SendAT("AT");
		if (SIM800_WaitResponse("OK", 100))
		{
			return 1;
		}
	}
	
	comClearRxFifo(COM_SIM800);	/* 清零串口接收缓冲区 */

	/*  通过拉低 PWRKEY 引脚至少 1.2 秒然后释放，使模块开机。*/
	PWRKEY_0();
	bsp_DelayMS(2000);
	PWRKEY_1();

	/* 等待模块完成上电，如果是自动波特率则收不到RDT */
	//if (SIM800_WaitResponse("RDY", 5000) == 0)
	{	
		/* 开始同步波特率: 主机发送AT，只到接收到正确的OK 
		  当模块开机后建议延迟 2 至 3 秒后再发送同步字符，用户可发送“ AT” (大写、小写均可)来和模块
		  同步波特率，当主机收到模块返回“ OK”，			
		*/
		for (i = 0; i < 50; i++)
		{
			SIM800_SendAT("OK");
			if (SIM800_WaitResponse("OK", 100))
			{
				ret_value = 1;
				break;			/* 模块上电成功 */
			}
		}		
	}
	
	return ret_value;
}


/*
*********************************************************************************************************
*	函 数 名: SIM800_PowerOff
*	功能说明: 控制SIM800模块关机
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void SIM800_PowerOff(void)
{
	/*
	用户可以通过把PWRKEY 信号拉低1.5秒用来关机,拉低时间超过33秒模块会重新开机。
	
	关机过程中，模块首先从网络上注销，让内部软件进入安全状态并且保存相关数据，最后关闭内部电
	源。在最后断电前模块的串口将发送以下字符：
	NORMAL POWER DOWN
	这之后模块将不会执行AT命令。模块进入关机模式，仅RTC处于激活状态。关机模式可以通过
	VDD_EXT引脚来检测，在关机模式下此引脚输出为低电平。
	*/
	
	/* 硬件关机 */
	PWRKEY_0();
	bsp_DelayMS(1500);
	PWRKEY_1();	
		
}

/*
*********************************************************************************************************
*	函 数 名: SIM800_WaitResponse
*	功能说明: 等待SIM800返回指定的应答字符串. 比如等待 OK
*	形    参: _pAckStr : 应答的字符串， 长度不得超过255
*			 _usTimeOut : 命令执行超时，0表示一直等待. >０表示超时时间，单位1ms
*	返 回 值: 1 表示成功  0 表示失败
*********************************************************************************************************
*/
uint8_t SIM800_WaitResponse(char *_pAckStr, uint16_t _usTimeOut)
{
	uint8_t ucData;
	uint8_t ucRxBuf[256];
	uint16_t pos = 0;
	uint32_t len;
	uint8_t ret;

	len = strlen(_pAckStr);
	if (len > 255)
	{
		return 0;
	}

	/* _usTimeOut == 0 表示无限等待 */
	if (_usTimeOut > 0)
	{
		bsp_StartTimer(SIM800_TMR_ID, _usTimeOut);		/* 使用软件定时器3，作为超时控制 */
	}
	while (1)
	{
		bsp_Idle();				/* CPU空闲执行的操作， 见 bsp.c 和 bsp.h 文件 */

		if (_usTimeOut > 0)
		{
			if (bsp_CheckTimer(SIM800_TMR_ID))
			{
				ret = 0;	/* 超时 */
				break;
			}
		}

		if (comGetChar(COM_SIM800, &ucData))
		{
			SIM800_PrintRxData(ucData);		/* 将接收到数据打印到调试串口1 */

			if (ucData == '\n')
			{
				if (pos > 0)	/* 第2次收到回车换行 */
				{
					if (memcmp(ucRxBuf, _pAckStr,  len) == 0)
					{
						ret = 1;	/* 收到指定的应答数据，返回成功 */
						break;
					}
					else
					{
						pos = 0;
					}
				}
				else
				{
					pos = 0;
				}
			}
			else
			{
				if (pos < sizeof(ucRxBuf))
				{
					/* 只保存可见字符 */
					if (ucData >= ' ')
					{
						ucRxBuf[pos++] = ucData;
					}
				}
			}
		}
	}
	return ret;
}

/*
*********************************************************************************************************
*	函 数 名: SIM800_ReadResponse
*	功能说明: 读取SIM800返回应答字符串。该函数根据字符间超时判断结束。 本函数需要紧跟AT命令发送函数。
*	形    参: _pBuf : 存放模块返回的完整字符串
*			  _usBufSize : 缓冲区最大长度
*			 _usTimeOut : 命令执行超时，0表示一直等待. >0 表示超时时间，单位1ms
*	返 回 值: 0 表示错误（超时）  > 0 表示应答的数据长度
*********************************************************************************************************
*/
uint16_t SIM800_ReadResponse(char *_pBuf, uint16_t _usBufSize, uint16_t _usTimeOut)
{
	uint8_t ucData;
	uint16_t pos = 0;
	uint8_t ret;
	uint8_t status = 0;		/* 接收状态 */

	/* _usTimeOut == 0 表示无限等待 */
	if (_usTimeOut > 0)
	{
		bsp_StartTimer(SIM800_TMR_ID, _usTimeOut);		/* 使用软件定时器作为超时控制 */
	}
	while (1)
	{
		bsp_Idle();				/* CPU空闲执行的操作， 见 bsp.c 和 bsp.h 文件 */

		if (status == 2)		/* 正在接收有效应答阶段，通过字符间超时判断数据接收完毕 */
		{
			if (bsp_CheckTimer(SIM800_TMR_ID))
			{
				_pBuf[pos]	 = 0;	/* 结尾加0， 便于函数调用者识别字符串结束 */
				ret = pos;		/* 成功。 返回数据长度 */
				break;
			}
		}
		else
		{
			if (_usTimeOut > 0)
			{
				if (bsp_CheckTimer(SIM800_TMR_ID))
				{
					ret = 0;	/* 超时 */
					break;
				}
			}
		}
		
		if (comGetChar(COM_SIM800, &ucData))
		{			
			SIM800_PrintRxData(ucData);		/* 将接收到数据打印到调试串口1 */

			switch (status)
			{
				case 0:			/* 首字符 */
					if (ucData == AT_CR)		/* 如果首字符是回车，表示 AT命令不会显 */
					{
						_pBuf[pos++] = ucData;		/* 保存接收到的数据 */
						status = 2;	 /* 认为收到模块应答结果 */
					}
					else	/* 首字符是 A 表示 AT命令回显 */
					{
						status = 1;	 /* 这是主机发送的AT命令字符串，不保存应答数据，直到遇到 CR字符 */
					}
					break;
					
				case 1:			/* AT命令回显阶段, 不保存数据. 继续等待 */
					if (ucData == AT_CR)
					{
						status = 2;
					}
					break;
					
				case 2:			/* 开始接收模块应答结果 */
					/* 只要收到模块的应答字符，则采用字符间超时判断结束，此时命令总超时不起作用 */
					bsp_StartTimer(SIM800_TMR_ID, 5);
					if (pos < _usBufSize - 1)
					{
						_pBuf[pos++] = ucData;		/* 保存接收到的数据 */
					}
					break;
			}
		}
	}
	return ret;
}

/*
*********************************************************************************************************
*	函 数 名: SIM800_SendAT
*	功能说明: 向GSM模块发送AT命令。 本函数自动在AT字符串口增加<CR>字符
*	形    参: _Str : AT命令字符串，不包括末尾的回车<CR>. 以字符0结束
*	返 回 值: 无
*********************************************************************************************************
*/
void SIM800_SendAT(char *_Cmd)
{
	comClearRxFifo(COM_SIM800);	/* 清零串口接收缓冲区 */	
	
	comSendBuf(COM_SIM800, (uint8_t *)_Cmd, strlen(_Cmd));
	comSendBuf(COM_SIM800, "\r", 1);
}

/*
*********************************************************************************************************
*	函 数 名: SIM800_SetEarVolume
*	功能说明: 设置耳机音量
*	形    参: _ucVolume : 音量。 0 - 100, 0表示最小声音
*	返 回 值: 无
*********************************************************************************************************
*/
void SIM800_SetEarVolume(uint8_t _ucVolume)
{
	char CmdBuf[32];

	sprintf(CmdBuf, "AT+CLVL=%d", _ucVolume);
	SIM800_SendAT(CmdBuf);
}

/*
*********************************************************************************************************
*	函 数 名: SIM800_SetMicGain
*	功能说明: 设置MIC 增益 .    设置后对两路通道都起作用，但只能在有激活电话前使用。
*	形    参: _Channel : 0 主音频通道  1 辅助音频通道
*				_iGain : 增益。 0-15  ; 0表示0dB; 15表示+22.5dB
*	返 回 值: 无
*********************************************************************************************************
*/
void SIM800_SetMicGain(uint16_t _Channel, uint16_t _iGain)
{
	char CmdBuf[32];
		
	sprintf(CmdBuf, "AT+CMIC=%d,%d", _Channel, _iGain);
	SIM800_SendAT(CmdBuf);
	
	
}


/*
*********************************************************************************************************
*	函 数 名: SIM800_DialTel
*	功能说明: 拨打电话
*	形    参: _pTel 电话号码字符串
*	返 回 值: 无
*********************************************************************************************************
*/
void SIM800_DialTel(char *_pTel)
{
	char CmdBuf[32];

	sprintf(CmdBuf, "ATD%s;", _pTel);
	SIM800_SendAT(CmdBuf);
}

/*
*********************************************************************************************************
*	函 数 名: SIM800_Hangup
*	功能说明: 挂断电话
*	形    参: _pTel 电话号码字符串
*	返 回 值: 无
*********************************************************************************************************
*/
void SIM800_Hangup(void)
{
	SIM800_SendAT("ATH");
}

/*
*********************************************************************************************************
*	函 数 名: SIM800_AnswerIncall
*	功能说明: 接听来话
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void SIM800_AnswerIncall(void)
{
	SIM800_SendAT("ATA");
}

/*
*********************************************************************************************************
*	函 数 名: SIM800_GetHardInfo
*	功能说明: 读取模块的硬件信息. 分析 ATI 命令应答结果。
*	形    参: 存放结果的结构体指针
*	返 回 值: 1 表示成功， 0 表示失败
*********************************************************************************************************
*/
uint8_t SIM800_GetHardInfo(SIM800_INFO_T *_pInfo)
{
	/*
	ATI
	SIM800 R13.08

	OK
	
	AT+GSV
	SIMCOM_Ltd
	SIMCOM_SIM800
	Revision:1308B05SIM800M32
	*/	
	char buf[64];
	uint16_t len;
	char *p1, *p2;
	
	SIM800_SendAT("ATI");		/* 发送 ATI 命令 */	
	len = SIM800_ReadResponse(buf, sizeof(buf), 300);	/* 超时 300ms */
	if (len == 0)
	{
		return 0;		
	}
	
	/* 制造商信息规定填写 SIMCOM_Ltd */
	sprintf(_pInfo->Manfacture, "SIMCOM_Ltd");	
	_pInfo->Model[0] = 0;
	_pInfo->Revision[0] = 0;

	/* 应答数据前2个字符是 \r\n */
	p1 = buf;
	p2 = strchr(p1, '\n');
	
	/* 解析型号 */
	p1 = p2 + 1;
	p2 = strchr(p1, ' ');
	len = p2 - p1;
	if (len >= sizeof(_pInfo->Model))
	{
		len = sizeof(_pInfo->Model) - 1;
	}		
	memcpy(_pInfo->Model, p1, len);
	_pInfo->Model[len] = 0;

	/* 解析型号 */
	p1 = p2 + 1;
	p2 = strchr(p1, '\r');
	len = p2 - p1;
	if (len >= sizeof(_pInfo->Revision))
	{
		len = sizeof(_pInfo->Revision) - 1;
	}				
	memcpy(_pInfo->Revision, p1, len);
	_pInfo->Revision[len] = 0;	
	
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: MG323_GetNetStatus
*	功能说明: 查询当前网络状态
*	形    参: 无
*	返 回 值: 网络状态, CREG_NO_REG, CREG_LOCAL_OK 等。
*********************************************************************************************************
*/
uint8_t SIM800_GetNetStatus(void)
{
	/*
		AT+CREG?
		+CREG: 0,1
		
		OK				
	*/	
	char buf[128];
	uint16_t len, i;
	uint8_t status = 0;
	
	SIM800_SendAT("AT+CREG?");	/* 发送 AT 命令 */
	
	len = SIM800_ReadResponse(buf, sizeof(buf), 200);	/* 超时 200ms */
	if (len == 0)
	{
		return 0;		
	}
	
	for (i = 2; i < len; i++)
	{
		if (buf[i] == ',')
		{
			i++;
			
			status = buf[i] - '0';
			break;
		}
	}
	return status;
}


/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
