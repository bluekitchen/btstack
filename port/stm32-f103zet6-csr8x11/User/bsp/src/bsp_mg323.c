/*
*********************************************************************************************************
*
*	模块名称 : 华为GPRS模块MG323驱动程序
*	文件名称 : bsp_mg323.c
*	版    本 : V1.0
*	说    明 : 封装MG323模块相关的AT命令
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-02-01 armfly  正式发布
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	安富莱STM32-V4 开发板口线分配：
	GPRS_TERM_ON   :  PB4/TRST/GPRS_TERM_ON
	GPRS_RESET     :  PC2/ADC123_IN12/GPRS_RESET
	
	GPRS_TX        :  PA2/USART2_TX
	GPRS_RX        :  PA3/USART2_RX

	GPRS_CTS       :  不连接
	GPRS_RTS       :  不连接

*/

/*
	AT+CIND=<mode>[,<mode>[,<mode>...]] 设置指示事件是否上报
		+CIND: 5,99,1,0,1,0,0,0,4

	AT+CREG?  查询当前网络状态

	AT+CSQ 查询信号质量命令

	AT+CIMI 查询SIM 卡的IMSI 号。

	AT+CIND? 读取当前的指示状态

	ATA 接听命令
	ATH 挂断连接命令

	AT^SWSPATH=<n>  切换音频通道
*/

/* 按键口对应的RCC时钟 */
#define RCC_TERM_ON 	RCC_APB2Periph_GPIOB
#define PORT_TERM_ON	GPIOB
#define PIN_TERM_ON		GPIO_Pin_4

#define RCC_RESET 		RCC_APB2Periph_GPIOC
#define PORT_RESET		GPIOC
#define PIN_RESET		GPIO_Pin_2

/* STM32和MG323的TERM_ON引脚间有1个NPN三极管，因此需要反相 */
#define TERM_ON_1()		GPIO_ResetBits(PORT_TERM_ON, PIN_TERM_ON);
#define TERM_ON_0()		GPIO_SetBits(PORT_TERM_ON, PIN_TERM_ON);

/* STM32和MG323的RESET引脚间有1个NPN三极管，因此需要反相. MG323的RESET脚是低脉冲复位 */
#define MG_RESET_1()	GPIO_ResetBits(PORT_RESET, PIN_RESET);
#define MG_RESET_0()	GPIO_SetBits(PORT_RESET, PIN_RESET);

/*
*********************************************************************************************************
*	函 数 名: bsp_InitMG323
*	功能说明: 配置无线模块相关的GPIO,  该函数被 bsp_Init() 调用。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitMG323(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 打开GPIO时钟 */
	RCC_APB2PeriphClockCmd(RCC_TERM_ON | RCC_RESET, ENABLE);


	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* 推挽输出模式 */

	GPIO_InitStructure.GPIO_Pin = PIN_TERM_ON;
	GPIO_Init(PORT_TERM_ON, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_RESET;
	GPIO_Init(PORT_RESET, &GPIO_InitStructure);

	/* CPU的串口配置已经由 bsp_uart_fifo.c 中的 bsp_InitUart() 做了 */
	
	TERM_ON_0();
	MG_RESET_1();
}

/*
*********************************************************************************************************
*	函 数 名: MG323_PrintRxData
*	功能说明: 打印STM32从MG323收到的数据到COM1串口，主要用于跟踪调试
*	形    参: _ch : 收到的数据
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_PrintRxData(uint8_t _ch)
{
	#ifdef MG323_TO_COM1_EN
		comSendChar(COM1, _ch);		/* 将接收到数据打印到调试串口1 */
	#endif
}

/*
*********************************************************************************************************
*	函 数 名: MG323_PowerOn
*	功能说明: 给MG323模块上电
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_PowerOn(void)
{
	comClearRxFifo(COM_MG323);	/* 清零串口接收缓冲区 */
	
	MG323_Reset();

	/*
		根据MG323手册，模块上电后延迟250ms，然后驱动 TERM_ON口线为低电平 750ms 之后驱动为高，完成开机时序
	*/
	TERM_ON_1();
	bsp_DelayMS(250);

	TERM_ON_0();
	bsp_DelayMS(750);
	TERM_ON_1();	

	/* 等待模块完成上电，判断是否接收到 ^SYSSTART */
	MG323_WaitResponse("^SYSSTART", 5000);
}

/*
*********************************************************************************************************
*	函 数 名: MG323_PowerOn
*	功能说明: 给MG323模块上电
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_Reset(void)
{
	/*
		根据MG323手册，
		RESET 管脚用于实现模块硬件复位。当模块出现软件死机的情况时，通过拉低
		RESET 管脚 ≥ 10 ms 后，模块进行硬件复位。
	*/
	MG_RESET_0();
	bsp_DelayMS(20);
	MG_RESET_1();
	
	bsp_DelayMS(10);
}


/*
*********************************************************************************************************
*	函 数 名: MG323_PowerOff
*	功能说明: 控制MG323模块关机
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_PowerOff(void)
{
	/* 硬件关机 */
	TERM_ON_0();

	/* 也可以软件关机 */
	//MG323_SendAT("AT^SMSO");
}

/*
*********************************************************************************************************
*	函 数 名: MG323_WaitResponse
*	功能说明: 等待MG323返回指定的应答字符串. 比如等待 OK
*	形    参: _pAckStr : 应答的字符串， 长度不得超过255
*			 _usTimeOut : 命令执行超时，0表示一直等待. >０表示超时时间，单位1ms
*	返 回 值: 1 表示成功  0 表示失败
*********************************************************************************************************
*/
uint8_t MG323_WaitResponse(char *_pAckStr, uint16_t _usTimeOut)
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
		bsp_StartTimer(MG323_TMR_ID, _usTimeOut);		/* 使用软件定时器3，作为超时控制 */
	}
	while (1)
	{
		bsp_Idle();				/* CPU空闲执行的操作， 见 bsp.c 和 bsp.h 文件 */

		if (_usTimeOut > 0)
		{
			if (bsp_CheckTimer(MG323_TMR_ID))
			{
				ret = 0;	/* 超时 */
				break;
			}
		}

		if (comGetChar(COM_MG323, &ucData))
		{
			MG323_PrintRxData(ucData);		/* 将接收到数据打印到调试串口1 */

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
*	函 数 名: MG323_ReadResponse
*	功能说明: 读取MG323返回应答字符串。该函数根据字符间超时判断结束。 本函数需要紧跟AT命令发送函数。
*	形    参: _pBuf : 存放模块返回的完整字符串
*			  _usBufSize : 缓冲区最大长度
*			 _usTimeOut : 命令执行超时，0表示一直等待. >0 表示超时时间，单位1ms
*	返 回 值: 0 表示错误（超时）  > 0 表示应答的数据长度
*********************************************************************************************************
*/
uint16_t MG323_ReadResponse(char *_pBuf, uint16_t _usBufSize, uint16_t _usTimeOut)
{
	uint8_t ucData;
	uint16_t pos = 0;
	uint8_t ret;
	uint8_t status = 0;		/* 接收状态 */

	/* _usTimeOut == 0 表示无限等待 */
	if (_usTimeOut > 0)
	{
		bsp_StartTimer(MG323_TMR_ID, _usTimeOut);		/* 使用软件定时器作为超时控制 */
	}
	while (1)
	{
		bsp_Idle();				/* CPU空闲执行的操作， 见 bsp.c 和 bsp.h 文件 */

		if (status == 2)		/* 正在接收有效应答阶段，通过字符间超时判断数据接收完毕 */
		{
			if (bsp_CheckTimer(MG323_TMR_ID))
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
				if (bsp_CheckTimer(MG323_TMR_ID))
				{
					ret = 0;	/* 超时 */
					break;
				}
			}
		}
		
		if (comGetChar(COM_MG323, &ucData))
		{			
			MG323_PrintRxData(ucData);		/* 将接收到数据打印到调试串口1 */

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
					bsp_StartTimer(MG323_TMR_ID, 5);
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
*	函 数 名: MG323_SendAT
*	功能说明: 向GSM模块发送AT命令。 本函数自动在AT字符串口增加<CR>字符
*	形    参: _Str : AT命令字符串，不包括末尾的回车<CR>. 以字符0结束
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_SendAT(char *_Cmd)
{
	comSendBuf(COM_MG323, (uint8_t *)_Cmd, strlen(_Cmd));
	comSendBuf(COM_MG323, "\r", 1);
}

/*
*********************************************************************************************************
*	函 数 名: MG323_SetAutoReport
*	功能说明: 设置事件自动上报
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_SetAutoReport(void)
{
	MG323_SendAT("AT+CIND=1,1,1,1,1,1,1,1,1");
}

/*
*********************************************************************************************************
*	函 数 名: MG323_SwitchPath
*	功能说明: 切换音频通道
*	形    参: ch ： 0表示第1路音频（INTMIC, INTEAR)  1表示第2路音频（EXTMIC,EXTEAR)
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_SwitchPath(uint8_t ch)
{
	if (ch == 0)
	{
		MG323_SendAT("AT^SWSPATH=0");
	}
	else
	{
		MG323_SendAT("AT^SWSPATH=1");
	}
}

/*
*********************************************************************************************************
*	函 数 名: MG323_SetEarVolume
*	功能说明: 设置耳机音量
*	形    参: _ucVolume : 音量。 0 表示静音，1-5 表示音量大小。5最大。4是缺省。
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_SetEarVolume(uint8_t _ucVolume)
{
	char CmdBuf[32];

	sprintf(CmdBuf, "AT+CLVL=%d", _ucVolume);
	MG323_SendAT(CmdBuf);
}

/*
*********************************************************************************************************
*	函 数 名: MG323_SetMicGain
*	功能说明: 设置MIC 增益 .    设置后对两路通道都起作用，但只能在有激活电话前使用。
*	形    参: _iGain : 增益。 -12 最小，12最大，13表示静音
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_SetMicGain(int16_t _iGain)
{
	char CmdBuf[32];

	sprintf(CmdBuf, "AT+CMIC=%d", _iGain);
	MG323_SendAT(CmdBuf);
}


/*
*********************************************************************************************************
*	函 数 名: MG323_DialTel
*	功能说明: 拨打电话
*	形    参: _pTel 电话号码字符串
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_DialTel(char *_pTel)
{
	char CmdBuf[64];

	sprintf(CmdBuf, "ATD%s;", _pTel);
	MG323_SendAT(CmdBuf);
}

/*
*********************************************************************************************************
*	函 数 名: MG323_Hangup
*	功能说明: 挂断电话
*	形    参: _pTel 电话号码字符串
*	返 回 值: 无
*********************************************************************************************************
*/
void MG323_Hangup(void)
{
	MG323_SendAT("ATH");
}

/*
*********************************************************************************************************
*	函 数 名: MG323_GetHardInfo
*	功能说明: 读取模块的硬件信息. 分析 ATI 命令应答结果。
*	形    参: 存放结果的结构体指针
*	返 回 值: 1 表示成功， 0 表示失败
*********************************************************************************************************
*/
uint8_t MG323_GetHardInfo(MG_HARD_INFO_T *_pInfo)
{
	/*
		ATI
		Manufacture: HUAWEI
		Model: MG323
		Revision: 12.210.10.05.00
		IMEI: 351869045435933
		+GCAP: +CGSM
		
		OK	
	*/	
	char buf[255];
	uint16_t len, i, begin = 0, num;
	uint8_t status = 0;	
	
	comClearRxFifo(COM_MG323);	/* 清零串口接收缓冲区 */	
	
	MG323_SendAT("ATI");		/* 发送 ATI 命令 */
	
	len = MG323_ReadResponse(buf, sizeof(buf), 300);	/* 超时 300ms */
	if (len == 0)
	{
		return 0;		
	}
	
	_pInfo->Manfacture[0] = 0;
	_pInfo->Model[0] = 0;
	_pInfo->Revision[0] = 0;
	_pInfo->IMEI[0] = 0;
	
	for (i = 2; i < len; i++)
	{
		if (buf[i] == ':')
		{
			i += 2;
			begin = i;
		}
		if (buf[i] == AT_CR)
		{
			num = i - begin;
			
			if (status == 0)
			{
				if (num >= sizeof(_pInfo->Manfacture))
				{
					num = sizeof(_pInfo->Manfacture) - 1;
				}
				memcpy(_pInfo->Manfacture, &buf[begin], num);
				_pInfo->Manfacture[num] = 0;
			}
			else if (status == 1)
			{
				if (num >= sizeof(_pInfo->Model))
				{
					num = sizeof(_pInfo->Model) - 1;
				}				
				memcpy(_pInfo->Model, &buf[begin], num);
				_pInfo->Model[num] = 0;
			}
			else if (status == 2)
			{
				if (num >= sizeof(_pInfo->Revision))
				{
					num = sizeof(_pInfo->Revision) - 1;
				}				
				memcpy(_pInfo->Revision, &buf[begin], num);
				_pInfo->Revision[num] = 0;
			}
			else if (status == 3)
			{
				if (num >= sizeof(_pInfo->IMEI))
				{
					num = sizeof(_pInfo->IMEI) - 1;
				}					
				memcpy(_pInfo->IMEI, &buf[begin], num);
				_pInfo->IMEI[num] = 0;
				break;
			}
			status++;	
		}
	}
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
uint8_t MG323_GetNetStatus(void)
{
	/*
		AT+CREG?
		+CREG: 0,1
		
		OK				
	*/	
	char buf[128];
	uint16_t len, i;
	uint8_t status = 0;
	
	comClearRxFifo(COM_MG323);	/* 清零串口接收缓冲区 */	
	
	MG323_SendAT("AT+CREG?");	/* 发送 AT 命令 */
	
	len = MG323_ReadResponse(buf, sizeof(buf), 200);	/* 超时 200ms */
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
		}
	}
	return status;
}


/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
