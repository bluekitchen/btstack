/*
*********************************************************************************************************
*
*	模块名称 : AM/FM收音机Si4730 驱动模块
*	文件名称 : bsp_Si730.c
*	版    本 : V1.0
*	说    明 : 驱动Si4730  Si4704收音机芯片，通过I2C总线控制该芯片，实现AM/FM接收。
*				Si4730和Si4704软件兼容的。Si4704支持FM,不支持AM;  Si4730支持AM，不支持FM,
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
	参考文档:
		AN332 Si47xx Programming Guide.pdf		软件编程指南
		Si4730-31-34-35-D60.pdf					芯片数据手册（不含软件部分）

	安富莱STM32-V5 开发板Si4730口线分配：

	I2C总线控制Si4730, 地址为 （0x22）, 通过宏 I2C_ADDR_SI4730 定义
		PH4/I2C2_SCL
 		PH5/I2C2_SDA

	I2C总线底层驱动在 bsp_i2c_gpio.c
	需要调用 bsp_InitI2C() 函数配置I2C的GPIO


	检查Si4730是否就绪，可以调用 i2c_CheckDevice(I2C_ADDR_SI4730)函数，返回0表示芯片正常。
*/

/*
	FM (64–108 MHz)
	AM (520–1710 kHz)

*/

/*
	i2c 总线时序，见 AN332 page = 226

	每个命令必须有完整的START + STOP信号，例如: [] 表示读取器件返回
	START ADDR+W [ACK] CMD  [ACK] ARG1 [ACK] ARG2 [ACK] ARG3 [ACK] STOP
	START  0x22    0  0x30    0   0x00   0   0x27   0   0x7E   0  STOP

	循环读取器件返回的状态，只到 STARTUS = 0x80
	START ADDR+R [ACK] [STATUS] NACK STOP
	START  0x23    0    0x00    1   STOP

	读取器件返回的数据
	START ADDR+R [ACK] STATUS ACK RESP1 ACK RESP2 ACK RESP3 NACK STOP
	START  0x23    0   0x80   0  0x00   0  0x00   0  0x00   1   STOP

	备注: [ACK] 是CPU发送一个SCL, 然后读取SDA
		  ACK   是CPU设置SDA=0 ,然后发送一个SCL
*/

/*
	AN223 page = 271    FM 收音机模式配置流程
	12.2. Programming Example for the FM/RDS Receiver
*/

/*
*********************************************************************************************************
*	函 数 名: bsp_InitSi4730
*	功能说明: 配置Si4703工作模式
*	形    参:无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitSi4730(void)
{
	;
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_Delay
*	功能说明: 延迟一段时间
*	形    参: n 循环次数
*	返 回 值: 无
*********************************************************************************************************
*/
void SI4730_Delay(uint32_t n)
{
	uint32_t i;

	for (i = 0; i < n; i++);
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_SendCmd
*	功能说明: 向Si4730发送CMD
*	形    参: _pCmdBuf : 命令数组
*			 _CmdLen : 命令串字节数
*	返 回 值: 0 失败(器件无应答)， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_SendCmd(uint8_t *_pCmdBuf, uint8_t _ucCmdLen)
{
	uint8_t ack;
	uint8_t i;

	i2c_Start();
	i2c_SendByte(I2C_ADDR_SI4730_W);
	ack = i2c_WaitAck();
	if (ack != 0)
	{
		goto err;
	}

	for (i = 0; i < _ucCmdLen; i++)
	{
		i2c_SendByte(_pCmdBuf[i]);
		ack = i2c_WaitAck();
		if (ack != 0)
		{
			goto err;
		}
	}

	i2c_Stop();
	return 1;

err:
	i2c_Stop();
	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_WaitStatus80
*	功能说明: 读取Si4730的状态，等于0x80时返回。
*	形    参: _uiTimeOut : 轮询次数
*			  _ucStopEn : 状态0x80检测成功后，是否发送STOP
*	返 回 值: 0 失败(器件无应答)， > 1 成功, 数字表示实际轮询次数
*********************************************************************************************************
*/
uint32_t SI4730_WaitStatus80(uint32_t _uiTimeOut, uint8_t _ucStopEn)
{
	uint8_t ack;
	uint8_t status;
	uint32_t i;

	/* 等待器件状态为 0x80 */
	for (i = 0; i < _uiTimeOut; i++)
	{
		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_R);	/* 读 */
		ack = i2c_WaitAck();
		if (ack == 1)
		{
			i2c_NAck();
			i2c_Stop();
			return 0;	/* 器件无应答，失败 */
		}
		status = i2c_ReadByte();
		if ((status == 0x80) || (status == 0x81))	/* 0x81 是为了执行0x23指令 读取信号质量 */
		{
			break;
		}
	}
	if (i == _uiTimeOut)
	{
		i2c_NAck();
		i2c_Stop();
		return 0;	/* 超时了，失败 */
	}

	/* 成功了， 处理一下第1次就成功的情况 */
	if (i == 0)
	{
		i = 1;

	}

	/* 因为有些命令还需要读取返回值，因此此处根据形参决定是否发送STOP */
	if  (_ucStopEn == 1)
	{
		i2c_NAck();
		i2c_Stop();
	}
	return i;
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_SetProperty
*	功能说明: 设置Si4730属性参数
*	形    参: _usPropNumber : 参数号
*			  _usPropValue : 参数值
*	返 回 值: 0 失败(器件无应答)， > 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_SetProperty(uint16_t _usPropNumber, uint16_t _usPropValue)
{
	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;

	ucCmdBuf[0] = 0x12;
	ucCmdBuf[1] = 0x00;

	ucCmdBuf[2] = _usPropNumber >> 8;
	ucCmdBuf[3] = _usPropNumber;

	ucCmdBuf[4] = _usPropValue >> 8;
	ucCmdBuf[5] = _usPropValue;
	SI4730_SendCmd(ucCmdBuf, 6);

	uiTimeOut = SI4730_WaitStatus80(100000, 1);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	return 1;
}

/* 下面2个函数是按I2C总线时序书写。可以发现很多代码是可以共用的。因此我们对部分代码进行封装，已便于实现其他命令 */
#if 0
	/*
	*********************************************************************************************************
	*	函 数 名: SI4730_PowerUp_FM_Revice
	*	功能说明: 配置Si4703为FM接收模式， 模拟模式（非数字模式)
	*	形    参:无
	*	返 回 值: 0 失败， 1 成功
	*********************************************************************************************************
	*/
	uint8_t SI4730_PowerUp_FM_Revice(void)
	{
		uint8_t ack;
		uint8_t status;

		/* AN332  page = 277
			Powerup in Analog Mode
			CMD      0x01     POWER_UP
			ARG1     0xC0     Set to FM Receive. Enable interrupts.
			ARG2     0x05     Set to Analog Audio Output
			STATUS   →0x80   Reply Status. Clear-to-send high.
		*/
		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_W);
		ack = i2c_WaitAck();
		i2c_SendByte(0x01);
		ack = i2c_WaitAck();
		i2c_SendByte(0xC0);
		ack = i2c_WaitAck();
		i2c_SendByte(0x05);
		ack = i2c_WaitAck();
		i2c_Stop();

		/* 等待器件返回状态 0x80 */
		{
			uint32_t i;

			for (i = 0; i < 2500; i++)
			{
				i2c_Start();
				i2c_SendByte(I2C_ADDR_SI4730_R);	/* 读 */
				ack = i2c_WaitAck();
				status = i2c_ReadByte();
				i2c_NAck();
				i2c_Stop();

				if (status == 0x80)
				{
					break;
				}
			}

			/* 实测 535 次循环应该正常退出 */
			if (i == 2500)
			{
				return 0;
			}
		}

		return 1;
	}

	/*
	*********************************************************************************************************
	*	函 数 名: SI4730_GetRevision
	*	功能说明: 读取器件、固件信息。 返回8字节数据
	*	形    参:_ReadBuf  返回结果存放在此缓冲区，请保证缓冲区大小大于等于8
	*	返 回 值: 0 失败， 1 成功
	*********************************************************************************************************
	*/
	uint8_t SI4730_GetRevision(uint8_t *_ReadBuf)
	{
		uint8_t ack;
		uint8_t status;
		uint32_t i;

		/* AN223 page = 67 */

		/* 发送 0x10 命令 */
		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_W);
		ack = i2c_WaitAck();
		i2c_SendByte(0x10);
		ack = i2c_WaitAck();
		i2c_Stop();

		/* 等待器件状态为 0x80 */
		for (i = 0; i < 50; i++)
		{
			i2c_Start();
			i2c_SendByte(I2C_ADDR_SI4730_R);	/* 读 */
			ack = i2c_WaitAck();
			status = i2c_ReadByte();
			if (status == 0x80)
			{
				break;
			}
		}
		/* 实测 2 次循环应该正常退出 */
		if (i == 50)
		{
			i2c_NAck();
			i2c_Stop();
			return 0;
		}

		/* 连续读取8个字节的器件返回信息 */
		for (i = 0; i < 8; i++)
		{
			i2c_Ack();
			_ReadBuf[i] = i2c_ReadByte();
		}
		i2c_NAck();
		i2c_Stop();
		return 1;
	}
#endif

/*
*********************************************************************************************************
*	函 数 名: SI4730_PowerUp_FM_Revice
*	功能说明: 配置Si4703为FM接收模式， 模拟模式（非数字模式)
*	形    参:无
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_PowerUp_FM_Revice(void)
{
	/* AN332  page = 277
		Powerup in Analog Mode
		CMD      0x01     POWER_UP
		ARG1     0xC0     Set to FM Receive. Enable interrupts.
		ARG2     0x05     Set to Analog Audio Output
		STATUS   →0x80   Reply Status. Clear-to-send high.
	*/

	uint8_t ucCmdBuf[3];
	uint32_t uiTimeOut;

	ucCmdBuf[0] = 0x01;
	ucCmdBuf[1] = 0xD0; //0xC0;
	ucCmdBuf[2] = 0x05;
	SI4730_SendCmd(ucCmdBuf, 3);

	/*
		第1个形参表示最大轮询次数； 如果成功，返回值uiTimeOut > 0 表示实际轮询次数
		第2个形参1表示结束后发送STOP
	*/
	uiTimeOut = SI4730_WaitStatus80(1000, 1);
	if (uiTimeOut > 0)
	{
		return 1;
	}

	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_PowerUp_AM_Revice
*	功能说明: 配置Si4703为AM接收模式， 模拟模式（非数字模式)
*	形    参:无
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_PowerUp_AM_Revice(void)
{
	/* AN332  page = 277
		Powerup in Analog Mode
		CMD      0x01     POWER_UP
		ARG1     0xC0     Set to FM Receive. Enable interrupts.
		ARG2     0x05     Set to Analog Audio Output
		STATUS   →0x80   Reply Status. Clear-to-send high.
	*/

	uint8_t ucCmdBuf[3];
	uint32_t uiTimeOut;

	ucCmdBuf[0] = 0x01;
	ucCmdBuf[1] = 0x91;
	ucCmdBuf[2] = 0x05;
	SI4730_SendCmd(ucCmdBuf, 3);

	/*
		第1个形参表示最大轮询次数； 如果成功，返回值uiTimeOut > 0 表示实际轮询次数
		第2个形参1表示结束后发送STOP
	*/
	uiTimeOut = SI4730_WaitStatus80(1000, 1);
	if (uiTimeOut > 0)
	{
		return 1;
	}


    SI4730_SetProperty(0x3403, 5);
    SI4730_SetProperty(0x3404, 25);

    SI4730_SetProperty(0x3402, 10); // Set spacing to 10kHz
    SI4730_SetProperty(0x3400, 520); // Set the band bottom to 520kHz
    SI4730_SetProperty(0x3401, 1710);   // Set the band top to 1710kHz

	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_PowerDown
*	功能说明: 关闭 Si470电源，模拟输出关闭
*	形    参:无
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_PowerDown(void)
{
	/*
		AN332  page = 15

		Moves the device from powerup to powerdown mode.
			Command 0x11. POWER_DOWN
	*/

	uint8_t ucCmdBuf[3];
	uint32_t uiTimeOut;

	ucCmdBuf[0] = 0x11;
	SI4730_SendCmd(ucCmdBuf, 1);

	/*
		第1个形参表示最大轮询次数； 如果成功，返回值uiTimeOut > 0 表示实际轮询次数
		第2个形参1表示结束后发送STOP
	*/
	uiTimeOut = SI4730_WaitStatus80(1000, 1);
	if (uiTimeOut > 0)
	{
		return 1;
	}

	return 0;
}


/*
*********************************************************************************************************
*	函 数 名: SI4730_GetRevision
*	功能说明: 读取器件、固件信息。 返回8字节数据
*	形    参:_ReadBuf  返回结果存放在此缓冲区，请保证缓冲区大小大于等于8
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_GetRevision(uint8_t *_ReadBuf)
{
	uint8_t ucCmdBuf[2];
	uint32_t uiTimeOut;
	uint8_t i;

	/* AN223 page = 67 */

	/* 发送 0x10 命令 */
	ucCmdBuf[0] = 0x10;
	SI4730_SendCmd(ucCmdBuf, 1);

	/*
		第1个形参表示最大轮询次数； 如果成功，返回值uiTimeOut > 0 表示实际轮询次数
		第2个形参0表示结束后不发送STOP， 因为还需要读取器件返回数据
	*/
	uiTimeOut = SI4730_WaitStatus80(10, 0);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* 连续读取8个字节的器件返回信息 */
	for (i = 0; i < 8; i++)
	{
		i2c_Ack();
		_ReadBuf[i] = i2c_ReadByte();
	}
	i2c_NAck();
	i2c_Stop();
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: SI4704_SetFMAntIntput
*	功能说明: 设置FM天线输入
*	形    参: _ch : 0 表示FM引脚输入(长天线)  1 表示LPI天线输入(PCB短天线)
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4704_SetFMIntput(uint8_t _ch)
{
	/* AN332 - PAGE 91 
		Property 0x1107. FM_ANTENNA_INPUT */

	return SI4730_SetProperty(0x1107, _ch);	
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_SetFMFreq
*	功能说明: 设置FM调谐频率
*	形    参:_uiFreq : 频率值, 单位 10kHz
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_SetFMFreq(uint32_t _uiFreq)
{
	/* AN332 page = 70 */

	/*
		CMD		 0x20 	FM_TUNE_FREQ
		ARG1     0x00
		ARG2     0x27	Set frequency to 102.3 MHz = 0x27F6
		ARG3     0xF6
		ARG4     0x00   Set antenna tuning capacitor to auto.
		STATUS   ?0x80	Reply Status. Clear-to-send high.
	*/

	/* 64 and 108 MHz in 10 kHz units. */

	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;
	uint8_t status;

	ucCmdBuf[0] = 0x20;
	ucCmdBuf[1] = 0x00;
	ucCmdBuf[2] = _uiFreq >> 8;
	ucCmdBuf[3] = _uiFreq;
	ucCmdBuf[4] = 0;
	SI4730_SendCmd(ucCmdBuf, 5);

	uiTimeOut = SI4730_WaitStatus80(1000, 1);
	if (uiTimeOut == 0)
	{
		return 0;
	}



	/* 等待器件状态为 0x81 */
	for (i = 0; i < 5000; i++)
	{
		/* 0x14. GET_INT_STATUS */
		ucCmdBuf[0] = 0x14;
		SI4730_SendCmd(ucCmdBuf, 1);

		SI4730_Delay(10000);

		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_R);	/* 读 */
		i2c_WaitAck();
		status = i2c_ReadByte();
		i2c_Stop();
		if (status == 0x81)
		{
			break;
		}
	}

	if (i == 5000)
	{
		return 0;	/* 失败 */
	}
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_SetAMFreq
*	功能说明: 设置AM调谐频率
*	形    参:_uiFreq : 频率值, 单位 10kHz
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_SetAMFreq(uint32_t _uiFreq)
{
	/* AN332 page = 70 */

	/*
		CMD       0x40        AM_TUNE_FREQ
		ARG1      0x00
		ARG2      0x03        Set frequency to 1000 kHz = 0x03E8
		ARG3      0xE8
		ARG4      0x00        Automatically select tuning capacitor
		ARG5      0x00
		STATUS    ?0x80       Reply Status. Clear-to-send high.
	*/

	/* 64 and 108 MHz in 10 kHz units. */

	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;
	uint8_t status;

	ucCmdBuf[0] = 0x40;
	ucCmdBuf[1] = 0x00;
	ucCmdBuf[2] = _uiFreq >> 8;
	ucCmdBuf[3] = _uiFreq;
	ucCmdBuf[4] = 0x00;
	ucCmdBuf[5] = 0x00;
	SI4730_SendCmd(ucCmdBuf, 6);

	uiTimeOut = SI4730_WaitStatus80(10000, 1);
	if (uiTimeOut == 0)
	{
		return 0;
	}



	/* 等待器件状态为 0x81 */
	for (i = 0; i < 5000; i++)
	{
		/* 0x14. GET_INT_STATUS */
		ucCmdBuf[0] = 0x14;
		SI4730_SendCmd(ucCmdBuf, 1);

		SI4730_Delay(10000);

		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_R);	/* 读 */
		i2c_WaitAck();
		status = i2c_ReadByte();
		i2c_Stop();
		if (status == 0x81)
		{
			break;
		}
	}

	if (i == 5000)
	{
		return 0;	/* 失败 */
	}
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_SetAMFreqCap
*	功能说明: 设置AM调谐频率
*	形    参:_uiFreq : 频率值, 单位 10kHz    _usCap : 调谐电容
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_SetAMFreqCap(uint32_t _uiFreq, uint16_t _usCap)
{
	/* AN332 page = 70 */

	/*
		CMD       0x40        AM_TUNE_FREQ
		ARG1      0x00
		ARG2      0x03        Set frequency to 1000 kHz = 0x03E8
		ARG3      0xE8
		ARG4      0x00        Automatically select tuning capacitor
		ARG5      0x00
		STATUS    ?0x80       Reply Status. Clear-to-send high.
	*/

	/* 64 and 108 MHz in 10 kHz units. */

	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;
	uint8_t status;

	ucCmdBuf[0] = 0x40;
	ucCmdBuf[1] = 0x00;
	ucCmdBuf[2] = _uiFreq >> 8;
	ucCmdBuf[3] = _uiFreq;
	ucCmdBuf[4] = _usCap >> 8;
	ucCmdBuf[5] = _usCap;
	SI4730_SendCmd(ucCmdBuf, 6);

	uiTimeOut = SI4730_WaitStatus80(10000, 1);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* 等待器件状态为 0x81 */
	for (i = 0; i < 5000; i++)
	{
		/* 0x14. GET_INT_STATUS */
		ucCmdBuf[0] = 0x14;
		SI4730_SendCmd(ucCmdBuf, 1);

		SI4730_Delay(10000);

		i2c_Start();
		i2c_SendByte(I2C_ADDR_SI4730_R);	/* 读 */
		i2c_WaitAck();
		status = i2c_ReadByte();
		i2c_Stop();
		if (status == 0x81)
		{
			break;
		}
	}

	if (i == 5000)
	{
		return 0;	/* 失败 */
	}
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_GetAMTuneStatus
*	功能说明: 读取AM调谐状态
*	形    参: 返回结果存放在此缓冲区，请保证缓冲区大小大于等于7
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_GetAMTuneStatus(uint8_t *_ReadBuf)
{
	/*
		CMD       0x42           AM_TUNE_STATUS
		ARG1      0x01           Clear STC interrupt.
		STATUS    ?0x80          Reply Status. Clear-to-send high.

		RESP1     ?0x01          Channel is valid, AFC is not railed, and seek did not wrap at AM band boundary
		RESP2     ?0x03
		RESP3     ?0xE8          Frequency = 0x03E8 = 1000 kHz
		RESP4     ?0x2A          RSSI = 0x2A = 42d = 42 dBμV
		RESP5     ?0x1A          SNR = 0x1A = 26d = 26 dB
		RESP6     ?0x0D          Value the antenna tuning capacitor is set to.
		RESP7     ?0x95          0x0D95 = 3477 dec.
	
		电容计算 The tuning capacitance is 95 fF x READANTCAP + 7 pF	
	*/
	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;

	ucCmdBuf[0] = 0x42;
	ucCmdBuf[1] = 0x01;
	SI4730_SendCmd(ucCmdBuf, 2);

	uiTimeOut = SI4730_WaitStatus80(100, 0);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* 连续读取7个字节的器件返回信息 */
	for (i = 0; i < 7; i++)
	{
		i2c_Ack();
		_ReadBuf[i] = i2c_ReadByte();
	}
	i2c_NAck();
	i2c_Stop();
	return 1;

}

/*
*********************************************************************************************************
*	函 数 名: SI4730_GetFMTuneStatus
*	功能说明: 读取FM调谐状态
*	形    参: 返回结果存放在此缓冲区，请保证缓冲区大小大于等于7
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_GetFMTuneStatus(uint8_t *_ReadBuf)
{
	/*
		CMD      0x22     FM_TUNE_STATUS
		ARG1     0x01     Clear STC interrupt.
		STATUS   ?0x80    Reply Status. Clear-to-send high.

		RESP1    ?0x01    Valid Frequency.
		RESP2    ?0x27    Frequency = 0x27F6 = 102.3 MHz
		RESP3    ?0xF6
		RESP4    ?0x2D    RSSI = 45 dBμV
		RESP5    ?0x33    SNR = 51 dB
		RESP6    ?0x00    MULT[7:0]
		RESP7    ?0x00    Antenna tuning capacitor = 0 (range = 0–191)  READANTCAP[7:0] (Si4704/05/06/2x only)
	*/
	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;

	ucCmdBuf[0] = 0x22;
	ucCmdBuf[1] = 0x01;
	SI4730_SendCmd(ucCmdBuf, 2);

	uiTimeOut = SI4730_WaitStatus80(100, 0);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* 连续读取7个字节的器件返回信息 */
	for (i = 0; i < 7; i++)
	{
		i2c_Ack();
		_ReadBuf[i] = i2c_ReadByte();
	}
	i2c_NAck();
	i2c_Stop();
	return 1;

}

/*
*********************************************************************************************************
*	函 数 名: SI4730_GetAMSignalQuality
*	功能说明: 读取AM接收信号质量
*	形    参: _ReadBuf 返回结果存放在此缓冲区，请保证缓冲区大小大于等于5
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_GetAMSignalQuality(uint8_t *_ReadBuf)
{
	/*
		AM_RSQ_STATUS
		Queries the status of the Received Signal Quality (RSQ) for
		the current channel.

		CMD        0x43      AM_RSQ_STATUS
		ARG1       0x01      Clear STC interrupt.
		STATUS     ?0x80     Reply Status. Clear-to-send high.

		RESP1      ?0x00     No SNR high, low, RSSI high, or low interrupts.
		RESP2      ?0x01     Channel is valid, soft mute is not activated, and AFC is not railed
		RESP3      ?0x00
		RESP4      ?0x2A     RSSI = 0x2A = 42d = 42 dBμV
		RESP5      ?0x1A     SNR = 0x1A = 26d = 26 dB
	*/
	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;

	ucCmdBuf[0] = 0x43;
	ucCmdBuf[1] = 0x01;
	SI4730_SendCmd(ucCmdBuf, 2);

	uiTimeOut = SI4730_WaitStatus80(100, 0);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* 连续读取5个字节的器件返回信息 */
	for (i = 0; i < 5; i++)
	{
		i2c_Ack();
		_ReadBuf[i] = i2c_ReadByte();
	}
	i2c_NAck();
	i2c_Stop();
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_GetFMSignalQuality
*	功能说明: 读取FM接收信号质量
*	形    参: _ReadBuf 返回结果存放在此缓冲区，请保证缓冲区大小大于等于7
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_GetFMSignalQuality(uint8_t *_ReadBuf)
{
	/*
		FM_RSQ_STATUS
		Queries the status of the Received Signal Quality (RSQ) for
		the current channel.

		CMD      0x23    FM_RSQ_STATUS
		ARG1     0x01    Clear RSQINT
		STATUS   ?0x80   Reply Status. Clear-to-send high.
		RESP1    ?0x00   No blend, SNR high, low, RSSI high or low interrupts.
		RESP2    ?0x01   Soft mute is not engaged, no AFC rail, valid frequency.
		RESP3    ?0xD9   Pilot presence, 89% blend
		RESP4    ?0x2D   RSSI = 45 dBμV
		RESP5    ?0x33   SNR = 51 dB
		RESP6    ?0x00
		RESP7    ?0x00   Freq offset = 0 kHz
	*/
	uint8_t ucCmdBuf[32];
	uint32_t uiTimeOut;
	uint32_t i;

	ucCmdBuf[0] = 0x23;
	ucCmdBuf[1] = 0x01;
	SI4730_SendCmd(ucCmdBuf, 2);

	uiTimeOut = SI4730_WaitStatus80(1000, 0);
	if (uiTimeOut == 0)
	{
		return 0;
	}

	/* 连续读取7个字节的器件返回信息 */
	for (i = 0; i < 7; i++)
	{
		i2c_Ack();
		_ReadBuf[i] = i2c_ReadByte();
	}
	i2c_NAck();
	i2c_Stop();
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: SI4730_SetOutVlomue
*	功能说明: 设置Si4730输出音量
*	形    参: _ucVolume; 值域[0-63];
*	返 回 值: 0 失败， 1 成功
*********************************************************************************************************
*/
uint8_t SI4730_SetOutVolume(uint8_t _ucVolume)
{
	/* AN332 page = 123 */

	/*
		Property 0x4000. RX_VOLUME

		Sets the output volume level, 63 max, 0 min. Default is 63.
	*/

	if (_ucVolume > 63)
	{
		_ucVolume = 63;
	}

	return SI4730_SetProperty(0x4000, _ucVolume);
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
