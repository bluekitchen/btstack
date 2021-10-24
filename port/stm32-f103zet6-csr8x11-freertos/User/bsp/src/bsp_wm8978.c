/*
*********************************************************************************************************
*
*	模块名称 : WM8978音频芯片驱动模块
*	文件名称 : bsp_wm8978.h
*	版    本 : V1.0
*	说    明 : WM8978音频芯片和STM32 I2S底层驱动。在安富莱STM32-V5开发板上调试通过。
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-02-01 armfly  正式发布
*		V1.1    2013-06-12 armfly  解决单独Line 输入不能放音的问题。修改 wm8978_CfgAudioPath() 函数
*		V1.1    2013-07-14 armfly  增加设置Line输入接口增益的函数： wm8978_SetLineGain()
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"

/*
*********************************************************************************************************
*
*	重要提示:
*	1、wm8978_ 开头的函数是操作WM8978寄存器，操作WM8978寄存器是通过I2C模拟总线进行的
*	2、I2S_ 开头的函数是操作STM32  I2S相关寄存器
*	3、实现录音或放音应用，需要同时操作WM8978和STM32的I2S。
*	4、部分函数用到的形参的定义在ST固件库中，比如：I2S_Standard_Phillips、I2S_Standard_MSB、I2S_Standard_LSB
*			  I2S_MCLKOutput_Enable、I2S_MCLKOutput_Disable
*			  I2S_AudioFreq_8K、I2S_AudioFreq_16K、I2S_AudioFreq_22K、I2S_AudioFreq_44K、I2S_AudioFreq_48
*			  I2S_Mode_MasterTx、I2S_Mode_MasterRx
*	5、注释中 pdf 指的是 wm8978.pdf 数据手册，wm8978de寄存器很多，用到的寄存器会注释pdf文件的页码，便于查询
*
*********************************************************************************************************
*/


/* 仅在本模块内部使用的局部函数 */
static uint16_t wm8978_ReadReg(uint8_t _ucRegAddr);
static uint8_t wm8978_WriteReg(uint8_t _ucRegAddr, uint16_t _usValue);
static void I2S_GPIO_Config(void);
static void I2S_Mode_Config(uint16_t _usStandard, uint16_t _usWordLen, uint32_t _uiAudioFreq, uint16_t _usMode);
static void I2S_NVIC_Config(void);
static void wm8978_Reset(void);

void wm8978_CtrlGPIO1(uint8_t _ucValue);

/*
	wm8978寄存器缓存
	由于WM8978的I2C两线接口不支持读取操作，因此寄存器值缓存在内存中，当写寄存器时同步更新缓存，读寄存器时
	直接返回缓存中的值。
	寄存器MAP 在WM8978.pdf 的第67页，寄存器地址是7bit， 寄存器数据是9bit
*/
static uint16_t wm8978_RegCash[] = {
	0x000, 0x000, 0x000, 0x000, 0x050, 0x000, 0x140, 0x000,
	0x000, 0x000, 0x000, 0x0FF, 0x0FF, 0x000, 0x100, 0x0FF,
	0x0FF, 0x000, 0x12C, 0x02C, 0x02C, 0x02C, 0x02C, 0x000,
	0x032, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x038, 0x00B, 0x032, 0x000, 0x008, 0x00C, 0x093, 0x0E9,
	0x000, 0x000, 0x000, 0x000, 0x003, 0x010, 0x010, 0x100,
	0x100, 0x002, 0x001, 0x001, 0x039, 0x039, 0x039, 0x039,
	0x001, 0x001
};

/*
*********************************************************************************************************
*	函 数 名: wm8978_Init
*	功能说明: 配置I2C GPIO，并检查I2C总线上的WM8978是否正常
*	形    参:  无
*	返 回 值: 1 表示初始化正常，0表示初始化不正常
*********************************************************************************************************
*/
uint8_t wm8978_Init(void)
{
	uint8_t re;

	if (i2c_CheckDevice(WM8978_SLAVE_ADDRESS) == 0)	/* 这个函数会配置STM32的GPIO用于软件模拟I2C时序 */
	{
		re = 1;
	}
	else
	{
		re = 0;
	}
	wm8978_Reset();			/* 硬件复位WM8978所有寄存器到缺省状态 */
	wm8978_CtrlGPIO1(1);	/* WM8978的GPIO1引脚输出1，表示缺省是放音(仅针对安富莱开发板硬件需要) */	
	return re;
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_SetEarVolume
*	功能说明: 修改耳机输出音量
*	形    参:  _ucLeftVolume ：左声道音量值, 0-63
*			  _ucLRightVolume : 右声道音量值,0-63
*	返 回 值: 无
*********************************************************************************************************
*/
void wm8978_SetEarVolume(uint8_t _ucVolume)
{
	uint16_t regL;
	uint16_t regR;

	if (_ucVolume > 0x3F)
	{
		_ucVolume = 0x3F;
	}

	regL = _ucVolume;
	regR = _ucVolume;

	/*
		R52	LOUT1 Volume control
		R53	ROUT1 Volume control
	*/
	/* 先更新左声道缓存值 */
	wm8978_WriteReg(52, regL | 0x00);

	/* 再同步更新左右声道的音量 */
	wm8978_WriteReg(53, regR | 0x100);	/* 0x180表示 在音量为0时再更新，避免调节音量出现的“嘎哒”声 */
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_SetSpkVolume
*	功能说明: 修改扬声器输出音量
*	形    参:  _ucLeftVolume ：左声道音量值, 0-63
*			  _ucLRightVolume : 右声道音量值,0-63
*	返 回 值: 无
*********************************************************************************************************
*/
void wm8978_SetSpkVolume(uint8_t _ucVolume)
{
	uint16_t regL;
	uint16_t regR;

	if (_ucVolume > 0x3F)
	{
		_ucVolume = 0x3F;
	}

	regL = _ucVolume;
	regR = _ucVolume;

	/*
		R54	LOUT2 (SPK) Volume control
		R55	ROUT2 (SPK) Volume control
	*/
	/* 先更新左声道缓存值 */
	wm8978_WriteReg(54, regL | 0x00);

	/* 再同步更新左右声道的音量 */
	wm8978_WriteReg(55, regR | 0x100);	/* 在音量为0时再更新，避免调节音量出现的“嘎哒”声 */
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_ReadEarVolume
*	功能说明: 读回耳机通道的音量.
*	形    参:  无
*	返 回 值: 当前音量值
*********************************************************************************************************
*/
uint8_t wm8978_ReadEarVolume(void)
{
	return (uint8_t)(wm8978_ReadReg(52) & 0x3F );
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_ReadSpkVolume
*	功能说明: 读回扬声器通道的音量.
*	形    参:  无
*	返 回 值: 当前音量值
*********************************************************************************************************
*/
uint8_t wm8978_ReadSpkVolume(void)
{
	return (uint8_t)(wm8978_ReadReg(54) & 0x3F );
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_OutMute
*	功能说明: 输出静音.
*	形    参:  _ucMute ：1是静音，0是不静音.
*	返 回 值: 当前音量值
*********************************************************************************************************
*/
void wm8978_OutMute(uint8_t _ucMute)
{
	uint16_t usRegValue;

	if (_ucMute == 1) /* 静音 */
	{
		usRegValue = wm8978_ReadReg(52); /* Left Mixer Control */
		usRegValue |= (1u << 6);
		wm8978_WriteReg(52, usRegValue);

		usRegValue = wm8978_ReadReg(53); /* Left Mixer Control */
		usRegValue |= (1u << 6);
		wm8978_WriteReg(53, usRegValue);

		usRegValue = wm8978_ReadReg(54); /* Right Mixer Control */
		usRegValue |= (1u << 6);
		wm8978_WriteReg(54, usRegValue);

		usRegValue = wm8978_ReadReg(55); /* Right Mixer Control */
		usRegValue |= (1u << 6);
		wm8978_WriteReg(55, usRegValue);
	}
	else	/* 取消静音 */
	{
		usRegValue = wm8978_ReadReg(52);
		usRegValue &= ~(1u << 6);
		wm8978_WriteReg(52, usRegValue);

		usRegValue = wm8978_ReadReg(53); /* Left Mixer Control */
		usRegValue &= ~(1u << 6);
		wm8978_WriteReg(53, usRegValue);

		usRegValue = wm8978_ReadReg(54);
		usRegValue &= ~(1u << 6);
		wm8978_WriteReg(54, usRegValue);

		usRegValue = wm8978_ReadReg(55); /* Left Mixer Control */
		usRegValue &= ~(1u << 6);
		wm8978_WriteReg(55, usRegValue);
	}
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_SetMicGain
*	功能说明: 设置MIC增益
*	形    参:  _ucGain ：音量值, 0-63
*	返 回 值: 无
*********************************************************************************************************
*/
void wm8978_SetMicGain(uint8_t _ucGain)
{
	if (_ucGain > GAIN_MAX)
	{
		_ucGain = GAIN_MAX;
	}

	/* PGA 音量控制  R45， R46   pdf 19页
		Bit8	INPPGAUPDATE
		Bit7	INPPGAZCL		过零再更改
		Bit6	INPPGAMUTEL		PGA静音
		Bit5:0	增益值，010000是0dB
	*/
	wm8978_WriteReg(45, _ucGain);
	wm8978_WriteReg(46, _ucGain | (1 << 8));
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_SetLineGain
*	功能说明: 设置Line输入通道的增益
*	形    参:  _ucGain ：音量值, 0-7. 7最大，0最小。 可衰减可放大。
*	返 回 值: 无
*********************************************************************************************************
*/
void wm8978_SetLineGain(uint8_t _ucGain)
{
	uint16_t usRegValue;

	if (_ucGain > 7)
	{
		_ucGain = 7;
	}

	/*
		Mic 输入信道的增益由 PGABOOSTL 和 PGABOOSTR 控制
		Aux 输入信道的输入增益由 AUXL2BOOSTVO[2:0] 和 AUXR2BOOSTVO[2:0] 控制
		Line 输入信道的增益由 LIP2BOOSTVOL[2:0] 和 RIP2BOOSTVOL[2:0] 控制
	*/
	/*	pdf 21页，R47（左声道），R48（右声道）, MIC 增益控制寄存器
		R47 (R48定义与此相同)
		B8		PGABOOSTL	= 1,   0表示MIC信号直通无增益，1表示MIC信号+20dB增益（通过自举电路）
		B7		= 0， 保留
		B6:4	L2_2BOOSTVOL = x， 0表示禁止，1-7表示增益-12dB ~ +6dB  （可以衰减也可以放大）
		B3		= 0， 保留
		B2:0`	AUXL2BOOSTVOL = x，0表示禁止，1-7表示增益-12dB ~ +6dB  （可以衰减也可以放大）
	*/

	usRegValue = wm8978_ReadReg(47);
	usRegValue &= 0x8F;/* 将Bit6:4清0   1000 1111*/
	usRegValue |= (_ucGain << 4);
	wm8978_WriteReg(47, usRegValue);	/* 写左声道输入增益控制寄存器 */

	usRegValue = wm8978_ReadReg(48);
	usRegValue &= 0x8F;/* 将Bit6:4清0   1000 1111*/
	usRegValue |= (_ucGain << 4);
	wm8978_WriteReg(48, usRegValue);	/* 写右声道输入增益控制寄存器 */
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_PowerDown
*	功能说明: 关闭wm8978，进入低功耗模式
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void wm8978_PowerDown(void)
{
	wm8978_Reset();			/* 硬件复位WM8978所有寄存器到缺省状态 */
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_CfgAudioIF
*	功能说明: 配置WM8978的音频接口(I2S)
*	形    参:
*			  _usStandard : 接口标准，I2S_Standard_Phillips, I2S_Standard_MSB 或 I2S_Standard_LSB
*			  _ucWordLen : 字长，16、24、32  （丢弃不常用的20bit格式）
*			  _usMode : CPU I2S的工作模式，I2S_Mode_MasterTx、I2S_Mode_MasterRx、
*						安富莱开发板硬件不支持 I2S_Mode_SlaveTx、I2S_Mode_SlaveRx 模式，这需要WM8978连接
*						外部振荡器
*	返 回 值: 无
*********************************************************************************************************
*/
void wm8978_CfgAudioIF(uint16_t _usStandard, uint8_t _ucWordLen, uint16_t _usMode)
{
	uint16_t usReg;

	/* pdf 67页，寄存器列表 */

	/*	REG R4, 音频接口控制寄存器
		B8		BCP	 = X, BCLK极性，0表示正常，1表示反相
		B7		LRCP = x, LRC时钟极性，0表示正常，1表示反相
		B6:5	WL = x， 字长，00=16bit，01=20bit，10=24bit，11=32bit （右对齐模式只能操作在最大24bit)
		B4:3	FMT = x，音频数据格式，00=右对齐，01=左对齐，10=I2S格式，11=PCM
		B2		DACLRSWAP = x, 控制DAC数据出现在LRC时钟的左边还是右边
		B1 		ADCLRSWAP = x，控制ADC数据出现在LRC时钟的左边还是右边
		B0		MONO	= 0，0表示立体声，1表示单声道，仅左声道有效
	*/
	usReg = 0;
	if (_usStandard == I2S_Standard_Phillips)	/* I2S飞利浦标准 */
	{
		usReg |= (2 << 3);
	}
	else if (_usStandard == I2S_Standard_MSB)	/* MSB对齐标准(左对齐) */
	{
		usReg |= (1 << 3);
	}
	else if (_usStandard == I2S_Standard_LSB)	/* LSB对齐标准(右对齐) */
	{
		usReg |= (0 << 3);
	}
	else	/* PCM标准(16位通道帧上带长或短帧同步或者16位数据帧扩展为32位通道帧) */
	{
		usReg |= (3 << 3);;
	}

	if (_ucWordLen == 24)
	{
		usReg |= (2 << 5);
	}
	else if (_ucWordLen == 32)
	{
		usReg |= (3 << 5);
	}
	else
	{
		usReg |= (0 << 5);		/* 16bit */
	}
	wm8978_WriteReg(4, usReg);

	/* R5  pdf 57页 */


	/*
		R6，时钟产生控制寄存器
		MS = 0,  WM8978被动时钟，由MCU提供MCLK时钟
	*/
	wm8978_WriteReg(6, 0x000);

	/* 如果是放音则需要设置  WM_GPIO1 = 1 ,如果是录音则需要设置WM_GPIO1 = 0 */
	if (_usMode == I2S_Mode_MasterTx)
	{
		wm8978_CtrlGPIO1(1);	/* 驱动WM8978的GPIO1引脚输出1, 用于放音 */		
	}
	else
	{
		wm8978_CtrlGPIO1(0);	/* 驱动WM8978的GPIO1引脚输出0, 用于录音 */		
	}	
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_ReadReg
*	功能说明: 从cash中读回读回wm8978寄存器
*	形    参:  _ucRegAddr ： 寄存器地址
*	返 回 值: 无
*********************************************************************************************************
*/
static uint16_t wm8978_ReadReg(uint8_t _ucRegAddr)
{
	return wm8978_RegCash[_ucRegAddr];
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_WriteReg
*	功能说明: 写wm8978寄存器
*	形    参:  _ucRegAddr ： 寄存器地址
*			  _usValue ：寄存器值
*	返 回 值: 无
*********************************************************************************************************
*/
static uint8_t wm8978_WriteReg(uint8_t _ucRegAddr, uint16_t _usValue)
{
	uint8_t ucAck;

	/* 发送起始位 */
	i2c_Start();

	/* 发送设备地址+读写控制bit（0 = w， 1 = r) bit7 先传 */
	i2c_SendByte(WM8978_SLAVE_ADDRESS | I2C_WR);

	/* 检测ACK */
	ucAck = i2c_WaitAck();
	if (ucAck == 1)
	{
		return 0;
	}

	/* 发送控制字节1 */
	i2c_SendByte(((_ucRegAddr << 1) & 0xFE) | ((_usValue >> 8) & 0x1));

	/* 检测ACK */
	ucAck = i2c_WaitAck();
	if (ucAck == 1)
	{
		return 0;
	}

	/* 发送控制字节2 */
	i2c_SendByte(_usValue & 0xFF);

	/* 检测ACK */
	ucAck = i2c_WaitAck();
	if (ucAck == 1)
	{
		return 0;
	}

	/* 发送STOP */
	i2c_Stop();

	wm8978_RegCash[_ucRegAddr] = _usValue;
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_CfgInOut
*	功能说明: 配置wm8978音频通道
*	形    参:
*			_InPath : 音频输入通道配置
*			_OutPath : 音频输出通道配置
*	返 回 值: 无
*********************************************************************************************************
*/
void wm8978_CfgAudioPath(uint16_t _InPath, uint16_t _OutPath)
{
	uint16_t usReg;

	/* 查看WM8978数据手册的 REGISTER MAP 章节， 第67页 */

	if ((_InPath == IN_PATH_OFF) && (_OutPath == OUT_PATH_OFF))
	{
		wm8978_PowerDown();
		return;
	}

	/* --------------------------- 第1步：根据输入通道参数配置寄存器 -----------------------*/
	/*
		R1 寄存器 Power manage 1
		Bit8    BUFDCOPEN,  Output stage 1.5xAVDD/2 driver enable
 		Bit7    OUT4MIXEN, OUT4 mixer enable
		Bit6    OUT3MIXEN, OUT3 mixer enable
		Bit5    PLLEN	.不用
		Bit4    MICBEN	,Microphone Bias Enable (MIC偏置电路使能)
		Bit3    BIASEN	,Analogue amplifier bias control 必须设置为1模拟放大器才工作
		Bit2    BUFIOEN , Unused input/output tie off buffer enable
		Bit1:0  VMIDSEL, 必须设置为非00值模拟放大器才工作
	*/
	usReg = (1 << 3) | (3 << 0);
	if (_OutPath & OUT3_4_ON) 	/* OUT3和OUT4使能输出到GSM模块 */
	{
		usReg |= ((1 << 7) | (1 << 6));
	}
	if ((_InPath & MIC_LEFT_ON) || (_InPath & MIC_RIGHT_ON))
	{
		usReg |= (1 << 4);
	}
	wm8978_WriteReg(1, usReg);	/* 写寄存器 */

	/*
		R2 寄存器 Power manage 2
		Bit8	ROUT1EN,	ROUT1 output enable 耳机右声道输出使能
		Bit7	LOUT1EN,	LOUT1 output enable 耳机左声道输出使能
		Bit6	SLEEP, 		0 = Normal device operation   1 = Residual current reduced in device standby mode
		Bit5	BOOSTENR,	Right channel Input BOOST enable 输入通道自举电路使能. 用到PGA放大功能时必须使能
		Bit4	BOOSTENL,	Left channel Input BOOST enable
		Bit3	INPGAENR,	Right channel input PGA enable 右声道输入PGA使能
		Bit2	INPGAENL,	Left channel input PGA enable
		Bit1	ADCENR,		Enable ADC right channel
		Bit0	ADCENL,		Enable ADC left channel
	*/
	usReg = 0;
	if (_OutPath & EAR_LEFT_ON)
	{
		usReg |= (1 << 7);
	}
	if (_OutPath & EAR_RIGHT_ON)
	{
		usReg |= (1 << 8);
	}
	if (_InPath & MIC_LEFT_ON)
	{
		usReg |= ((1 << 4) | (1 << 2));
	}
	if (_InPath & MIC_RIGHT_ON)
	{
		usReg |= ((1 << 5) | (1 << 3));
	}
	if (_InPath & LINE_ON)
	{
		usReg |= ((1 << 4) | (1 << 5));
	}
	if (_InPath & MIC_RIGHT_ON)
	{
		usReg |= ((1 << 5) | (1 << 3));
	}
	if (_InPath & ADC_ON)
	{
		usReg |= ((1 << 1) | (1 << 0));
	}
	wm8978_WriteReg(2, usReg);	/* 写寄存器 */

	/*
		R3 寄存器 Power manage 3
		Bit8	OUT4EN,		OUT4 enable
		Bit7	OUT3EN,		OUT3 enable
		Bit6	LOUT2EN,	LOUT2 output enable
		Bit5	ROUT2EN,	ROUT2 output enable
		Bit4	0,
		Bit3	RMIXEN,		Right mixer enable
		Bit2	LMIXEN,		Left mixer enable
		Bit1	DACENR,		Right channel DAC enable
		Bit0	DACENL,		Left channel DAC enable
	*/
	usReg = 0;
	if (_OutPath & OUT3_4_ON)
	{
		usReg |= ((1 << 8) | (1 << 7));
	}
	if (_OutPath & SPK_ON)
	{
		usReg |= ((1 << 6) | (1 << 5));
	}
	if (_OutPath != OUT_PATH_OFF)
	{
		usReg |= ((1 << 3) | (1 << 2));
	}
	if (_InPath & DAC_ON)
	{
		usReg |= ((1 << 1) | (1 << 0));
	}
	wm8978_WriteReg(3, usReg);	/* 写寄存器 */

	/*
		R44 寄存器 Input ctrl

		Bit8	MBVSEL, 		Microphone Bias Voltage Control   0 = 0.9 * AVDD   1 = 0.6 * AVDD
		Bit7	0
		Bit6	R2_2INPPGA,		Connect R2 pin to right channel input PGA positive terminal
		Bit5	RIN2INPPGA,		Connect RIN pin to right channel input PGA negative terminal
		Bit4	RIP2INPPGA,		Connect RIP pin to right channel input PGA amplifier positive terminal
		Bit3	0
		Bit2	L2_2INPPGA,		Connect L2 pin to left channel input PGA positive terminal
		Bit1	LIN2INPPGA,		Connect LIN pin to left channel input PGA negative terminal
		Bit0	LIP2INPPGA,		Connect LIP pin to left channel input PGA amplifier positive terminal
	*/
	usReg = 0 << 8;
	if (_InPath & LINE_ON)
	{
		usReg |= ((1 << 6) | (1 << 2));
	}
	if (_InPath & MIC_RIGHT_ON)
	{
		usReg |= ((1 << 5) | (1 << 4));
	}
	if (_InPath & MIC_LEFT_ON)
	{
		usReg |= ((1 << 1) | (1 << 0));
	}
	wm8978_WriteReg(44, usReg);	/* 写寄存器 */


	/*
		R14 寄存器 ADC Control
		设置高通滤波器（可选的） pdf 24、25页,
		Bit8 	HPFEN,	High Pass Filter Enable高通滤波器使能，0表示禁止，1表示使能
		BIt7 	HPFAPP,	Select audio mode or application mode 选择音频模式或应用模式，0表示音频模式，
		Bit6:4	HPFCUT，Application mode cut-off frequency  000-111选择应用模式的截止频率
		Bit3 	ADCOSR,	ADC oversample rate select: 0=64x (lower power) 1=128x (best performance)
		Bit2   	0
		Bit1 	ADC right channel polarity adjust:  0=normal  1=inverted
		Bit0 	ADC left channel polarity adjust:  0=normal 1=inverted
	*/
	if (_InPath & ADC_ON)
	{
		usReg = (1 << 3) | (0 << 8) | (4 << 0);		/* 禁止ADC高通滤波器, 设置截至频率 */
	}
	else
	{
		usReg = 0;
	}
	wm8978_WriteReg(14, usReg);	/* 写寄存器 */

	/* 设置陷波滤波器（notch filter），主要用于抑制话筒声波正反馈，避免啸叫.  暂时关闭
		R27，R28，R29，R30 用于控制限波滤波器，pdf 26页
		R7的 Bit7 NFEN = 0 表示禁止，1表示使能
	*/
	if (_InPath & ADC_ON)
	{
		usReg = (0 << 7);
		wm8978_WriteReg(27, usReg);	/* 写寄存器 */
		usReg = 0;
		wm8978_WriteReg(28, usReg);	/* 写寄存器,填0，因为已经禁止，所以也可不做 */
		wm8978_WriteReg(29, usReg);	/* 写寄存器,填0，因为已经禁止，所以也可不做 */
		wm8978_WriteReg(30, usReg);	/* 写寄存器,填0，因为已经禁止，所以也可不做 */
	}

	/* 自动增益控制 ALC, R32  - 34  pdf 19页 */
	{
		usReg = 0;		/* 禁止自动增益控制 */
		wm8978_WriteReg(32, usReg);
		wm8978_WriteReg(33, usReg);
		wm8978_WriteReg(34, usReg);
	}

	/*  R35  ALC Noise Gate Control
		Bit3	NGATEN, Noise gate function enable
		Bit2:0	Noise gate threshold:
	*/
	usReg = (3 << 1) | (7 << 0);		/* 禁止自动增益控制 */
	wm8978_WriteReg(35, usReg);

	/*
		Mic 输入信道的增益由 PGABOOSTL 和 PGABOOSTR 控制
		Aux 输入信道的输入增益由 AUXL2BOOSTVO[2:0] 和 AUXR2BOOSTVO[2:0] 控制
		Line 输入信道的增益由 LIP2BOOSTVOL[2:0] 和 RIP2BOOSTVOL[2:0] 控制
	*/
	/*	pdf 21页，R47（左声道），R48（右声道）, MIC 增益控制寄存器
		R47 (R48定义与此相同)
		B8		PGABOOSTL	= 1,   0表示MIC信号直通无增益，1表示MIC信号+20dB增益（通过自举电路）
		B7		= 0， 保留
		B6:4	L2_2BOOSTVOL = x， 0表示禁止，1-7表示增益-12dB ~ +6dB  （可以衰减也可以放大）
		B3		= 0， 保留
		B2:0`	AUXL2BOOSTVOL = x，0表示禁止，1-7表示增益-12dB ~ +6dB  （可以衰减也可以放大）
	*/
	usReg = 0;
	if ((_InPath & MIC_LEFT_ON) || (_InPath & MIC_RIGHT_ON))
	{
		usReg |= (1 << 8);	/* MIC增益取+20dB */
	}
	if (_InPath & AUX_ON)
	{
		usReg |= (3 << 0);	/* Aux增益固定取3，用户可以自行调整 */
	}
	if (_InPath & LINE_ON)
	{
		usReg |= (3 << 4);	/* Line增益固定取3，用户可以自行调整 */
	}
	wm8978_WriteReg(47, usReg);	/* 写左声道输入增益控制寄存器 */
	wm8978_WriteReg(48, usReg);	/* 写右声道输入增益控制寄存器 */

	/* 数字ADC音量控制，pdf 27页
		R15 控制左声道ADC音量，R16控制右声道ADC音量
		Bit8 	ADCVU  = 1 时才更新，用于同步更新左右声道的ADC音量
		Bit7:0 	增益选择； 0000 0000 = 静音
						   0000 0001 = -127dB
						   0000 0010 = -12.5dB  （0.5dB 步长）
						   1111 1111 = 0dB  （不衰减）
	*/
	usReg = 0xFF;
	wm8978_WriteReg(15, usReg);	/* 选择0dB，先缓存左声道 */
	usReg = 0x1FF;
	wm8978_WriteReg(16, usReg);	/* 同步更新左右声道 */

	/* 通过 wm8978_SetMicGain 函数设置mic PGA增益 */

	/*	R43 寄存器  AUXR – ROUT2 BEEP Mixer Function
		B8:6 = 0

		B5	 MUTERPGA2INV,	Mute input to INVROUT2 mixer
		B4	 INVROUT2,  Invert ROUT2 output 用于扬声器推挽输出
		B3:1 BEEPVOL = 7;	AUXR input to ROUT2 inverter gain
		B0	 BEEPEN = 1;	Enable AUXR beep input

	*/
	usReg = 0;
	if (_OutPath & SPK_ON)
	{
		usReg |= (1 << 4);	/* ROUT2 反相, 用于驱动扬声器 */
	}
	if (_InPath & AUX_ON)
	{
		usReg |= ((7 << 1) | (1 << 0));
	}
	wm8978_WriteReg(43, usReg);

	/* R49  Output ctrl
		B8:7	0
		B6		DACL2RMIX,	Left DAC output to right output mixer
		B5		DACR2LMIX,	Right DAC output to left output
		B4		OUT4BOOST,	0 = OUT4 output gain = -1; DC = AVDD / 2；1 = OUT4 output gain = +1.5；DC = 1.5 x AVDD / 2
		B3		OUT3BOOST,	0 = OUT3 output gain = -1; DC = AVDD / 2；1 = OUT3 output gain = +1.5；DC = 1.5 x AVDD / 2
		B2		SPKBOOST,	0 = Speaker gain = -1;  DC = AVDD / 2 ; 1 = Speaker gain = +1.5; DC = 1.5 x AVDD / 2
		B1		TSDEN,   Thermal Shutdown Enable  扬声器热保护使能（缺省1）
		B0		VROI,	Disabled Outputs to VREF Resistance
	*/
	usReg = 0;
	if (_InPath & DAC_ON)
	{
		usReg |= ((1 << 6) | (1 << 5));
	}
	if (_OutPath & SPK_ON)
	{
		usReg |=  ((1 << 2) | (1 << 1));	/* SPK 1.5x增益,  热保护使能 */
	}
	if (_OutPath & OUT3_4_ON)
	{
		usReg |=  ((1 << 4) | (1 << 3));	/* BOOT3  BOOT4  1.5x增益 */
	}
	wm8978_WriteReg(49, usReg);

	/*	REG 50    (50是左声道，51是右声道，配置寄存器功能一致) pdf 40页
		B8:6	AUXLMIXVOL = 111	AUX用于FM收音机信号输入
		B5		AUXL2LMIX = 1		Left Auxilliary input to left channel
		B4:2	BYPLMIXVOL			音量
		B1		BYPL2LMIX = 0;		Left bypass path (from the left channel input boost output) to left output mixer
		B0		DACL2LMIX = 1;		Left DAC output to left output mixer
	*/
	usReg = 0;
	if (_InPath & AUX_ON)
	{
		usReg |= ((7 << 6) | (1 << 5));
	}
	if ((_InPath & LINE_ON) || (_InPath & MIC_LEFT_ON) || (_InPath & MIC_RIGHT_ON))
	{
		usReg |= ((7 << 2) | (1 << 1));
	}
	if (_InPath & DAC_ON)
	{
		usReg |= (1 << 0);
	}
	wm8978_WriteReg(50, usReg);
	wm8978_WriteReg(51, usReg);

	/*	R56 寄存器   OUT3 mixer ctrl
		B8:7	0
		B6		OUT3MUTE,  	0 = Output stage outputs OUT3 mixer;  1 = Output stage muted – drives out VMID.
		B5:4	0
		B3		BYPL2OUT3,	OUT4 mixer output to OUT3  (反相)
		B4		0
		B2		LMIX2OUT3,	Left ADC input to OUT3
		B1		LDAC2OUT3,	Left DAC mixer to OUT3
		B0		LDAC2OUT3,	Left DAC output to OUT3
	*/
	usReg = 0;
	if (_OutPath & OUT3_4_ON)
	{
		usReg |= (1 << 3);
	}
	wm8978_WriteReg(56, usReg);

	/* R57 寄存器		OUT4 (MONO) mixer ctrl
		B8:7	0
		B6		OUT4MUTE,	0 = Output stage outputs OUT4 mixer  1 = Output stage muted – drives outVMID.
		B5		HALFSIG,	0 = OUT4 normal output	1 = OUT4 attenuated by 6dB
		B4		LMIX2OUT4,	Left DAC mixer to OUT4
		B3		LDAC2UT4,	Left DAC to OUT4
		B2		BYPR2OUT4,	Right ADC input to OUT4
		B1		RMIX2OUT4,	Right DAC mixer to OUT4
		B0		RDAC2OUT4,	Right DAC output to OUT4
	*/
	usReg = 0;
	if (_OutPath & OUT3_4_ON)
	{
		usReg |= ((1 << 4) |  (1 << 1));
	}
	wm8978_WriteReg(57, usReg);


	/* R11, 12 寄存器 DAC数字音量
		R11		Left DAC Digital Volume
		R12		Right DAC Digital Volume
	*/
	if (_InPath & DAC_ON)
	{
		wm8978_WriteReg(11, 255);
		wm8978_WriteReg(12, 255 | 0x100);
	}
	else
	{
		wm8978_WriteReg(11, 0);
		wm8978_WriteReg(12, 0 | 0x100);
	}

	/*	R10 寄存器 DAC Control
		B8	0
		B7	0
		B6	SOFTMUTE,	Softmute enable:
		B5	0
		B4	0
		B3	DACOSR128,	DAC oversampling rate: 0=64x (lowest power) 1=128x (best performance)
		B2	AMUTE,		Automute enable
		B1	DACPOLR,	Right DAC output polarity
		B0	DACPOLL,	Left DAC output polarity:
	*/
	if (_InPath & DAC_ON)
	{
		wm8978_WriteReg(10, 0);
	}
	;
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_NotchFilter
*	功能说明: 设置陷波滤波器（notch filter），主要用于抑制话筒声波正反馈，避免啸叫
*	形    参:  NFA0[13:0] and NFA1[13:0]
*	返 回 值: 无
*********************************************************************************************************
*/
void wm8978_NotchFilter(uint16_t _NFA0, uint16_t _NFA1)
{
	uint16_t usReg;

	/*  page 26
		A programmable notch filter is provided. This filter has a variable centre frequency and bandwidth,
		programmable via two coefficients, a0 and a1. a0 and a1 are represented by the register bits
		NFA0[13:0] and NFA1[13:0]. Because these coefficient values require four register writes to setup
		there is an NFU (Notch Filter Update) flag which should be set only when all four registers are setup.
	*/
	usReg = (1 << 7) | (_NFA0 & 0x3F);
	wm8978_WriteReg(27, usReg);	/* 写寄存器 */

	usReg = ((_NFA0 >> 7) & 0x3F);
	wm8978_WriteReg(28, usReg);	/* 写寄存器 */

	usReg = (_NFA1 & 0x3F);
	wm8978_WriteReg(29, usReg);	/* 写寄存器 */

	usReg = (1 << 8) | ((_NFA1 >> 7) & 0x3F);
	wm8978_WriteReg(30, usReg);	/* 写寄存器 */
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_CtrlGPIO1
*	功能说明: 控制WM8978的GPIO1引脚输出0或1
*	形    参:  _ucValue ：GPIO1输出值，0或1
*	返 回 值: 无
*********************************************************************************************************
*/
void wm8978_CtrlGPIO1(uint8_t _ucValue)
{
	uint16_t usRegValue;

	/* R8， pdf 62页 */
	if (_ucValue == 0) /* 输出0 */
	{
		usRegValue = 6; /* B2:0 = 110 */
	}
	else
	{
		usRegValue = 7; /* B2:0 = 111 */
	}
	wm8978_WriteReg(8, usRegValue);
}

/*
*********************************************************************************************************
*	函 数 名: wm8978_Reset
*	功能说明: 复位wm8978，所有的寄存器值恢复到缺省值
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void wm8978_Reset(void)
{
	/* wm8978寄存器缺省值 */
	const uint16_t reg_default[] = {
	0x000, 0x000, 0x000, 0x000, 0x050, 0x000, 0x140, 0x000,
	0x000, 0x000, 0x000, 0x0FF, 0x0FF, 0x000, 0x100, 0x0FF,
	0x0FF, 0x000, 0x12C, 0x02C, 0x02C, 0x02C, 0x02C, 0x000,
	0x032, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
	0x038, 0x00B, 0x032, 0x000, 0x008, 0x00C, 0x093, 0x0E9,
	0x000, 0x000, 0x000, 0x000, 0x003, 0x010, 0x010, 0x100,
	0x100, 0x002, 0x001, 0x001, 0x039, 0x039, 0x039, 0x039,
	0x001, 0x001
	};
	uint8_t i;

	wm8978_WriteReg(0x00, 0);

	for (i = 0; i < sizeof(reg_default) / 2; i++)
	{
		wm8978_RegCash[i] = reg_default[i];
	}
}

/*
*********************************************************************************************************
*	                     下面的代码是和STM32 I2S硬件相关的
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*	函 数 名: I2S_CODEC_Init
*	功能说明: 配置GPIO引脚和中断通道用于codec应用
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void I2S_CODEC_Init(void)
{
	/* 配置I2S中断通道 */
	I2S_NVIC_Config();

	/* 配置I2S2 GPIO口线 */
	I2S_GPIO_Config();

	/* 禁止I2S2 TXE中断(发送缓冲区空)，需要时再打开 */
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, DISABLE);

	/* 禁止I2S2 RXNE中断(接收不空)，需要时再打开 */
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, DISABLE);
}

/*
*********************************************************************************************************
*	函 数 名: I2S_StartPlay
*	功能说明: 配置I2S工作模式，启动I2S发送缓冲区空中断，开始放音
*	形    参:  _usStandard : 接口标准，I2S_Standard_Phillips, I2S_Standard_MSB 或 I2S_Standard_LSB
*			  _usMCLKOutput : 主时钟输出，I2S_MCLKOutput_Enable or I2S_MCLKOutput_Disable
*			  _uiAudioFreq : 采样频率，I2S_AudioFreq_8K、I2S_AudioFreq_16K、I2S_AudioFreq_22K、
*							I2S_AudioFreq_44K、I2S_AudioFreq_48
*	返 回 值: 无
*********************************************************************************************************
*/
void I2S_StartPlay(uint16_t _usStandard, uint16_t _usWordLen,  uint32_t _uiAudioFreq)
{
	/* 配置I2S为主发送模式，即STM32提供主时钟，I2S数据口是发送方向(放音) */
	I2S_Mode_Config(_usStandard, _usWordLen, _uiAudioFreq, I2S_Mode_MasterTx);

	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, ENABLE);		/* 使能发送中断 */
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, DISABLE);	/* 禁止接收中断 */
}

/*
*********************************************************************************************************
*	函 数 名: I2S_StartRecord
*	功能说明: 配置I2S工作模式，启动I2S接收缓冲区不空中断，开始录音。在调用本函数前，请先配置WM8978录音通道
*	形    参:  _usStandard : 接口标准，I2S_Standard_Phillips, I2S_Standard_MSB 或 I2S_Standard_LSB
*			  _usMCLKOutput : 主时钟输出，I2S_MCLKOutput_Enable or I2S_MCLKOutput_Disable
*			  _uiAudioFreq : 采样频率，I2S_AudioFreq_8K、I2S_AudioFreq_16K、I2S_AudioFreq_22K、
*							I2S_AudioFreq_44K、I2S_AudioFreq_48
*	返 回 值: 无
*********************************************************************************************************
*/
void I2S_StartRecord(uint16_t _usStandard, uint16_t _usWordLen, uint32_t _uiAudioFreq)
{
	/* 配置I2S为主发送模式，即STM32提供主时钟，I2S数据口是发送方向(放音) */
	I2S_Mode_Config(_usStandard, _usWordLen, _uiAudioFreq, I2S_Mode_MasterRx);
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, ENABLE);		/* 使能接收中断 */

	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, DISABLE);		/* 禁止发送中断 */
}

/*
*********************************************************************************************************
*	函 数 名: I2S_Stop
*	功能说明: 停止I2S工作
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void I2S_Stop(void)
{
	/* 禁止I2S2 TXE中断(发送缓冲区空)，需要时再打开 */
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, DISABLE);

	/* 禁止I2S2 RXNE中断(接收不空)，需要时再打开 */
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, DISABLE);

	/* 禁能 SPI2/I2S2 外设 */
	I2S_Cmd(SPI2, DISABLE);

	/* 关闭 I2S2 APB1 时钟 */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
}


/*
*********************************************************************************************************
*	函 数 名: I2S_GPIO_Config
*	功能说明: 配置GPIO引脚用于codec应用
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void I2S_GPIO_Config(void)
{
	/*
		安富莱STM32-V4开发板--- I2S总线传输音频数据口线
		PB12/I2S2_WS
		PB13/I2S2_CK
		             adcdat
		PB15/I2S2_SD
		PC6/I2S2_MCK
	*/
	GPIO_InitTypeDef GPIO_InitStructure;
	
	/* Enable GPIOB, GPIOC and AFIO clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC |  
	                     RCC_APB2Periph_AFIO, ENABLE);
	
	/* I2S2 SD, CK and WS pins configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	/* I2S2 MCK pin configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	函 数 名: I2S_Config
*	功能说明: 配置STM32的I2S外设工作模式
*	形    参:  _usStandard : 接口标准，I2S_Standard_Phillips, I2S_Standard_MSB 或 I2S_Standard_LSB
*			  _usMCLKOutput : 主时钟输出，I2S_MCLKOutput_Enable or I2S_MCLKOutput_Disable
*			  _usAudioFreq : 采样频率，I2S_AudioFreq_8K、I2S_AudioFreq_16K、I2S_AudioFreq_22K、
*							I2S_AudioFreq_44K、I2S_AudioFreq_48
*			  _usMode : CPU I2S的工作模式，I2S_Mode_MasterTx、I2S_Mode_MasterRx、
*						安富莱开发板硬件不支持 I2S_Mode_SlaveTx、I2S_Mode_SlaveRx 模式，这需要WM8978连接
*						外部振荡器
*	返 回 值: 无
*********************************************************************************************************
*/
static void I2S_Mode_Config(uint16_t _usStandard, uint16_t _usWordLen, uint32_t _uiAudioFreq, uint16_t _usMode)
{
	I2S_InitTypeDef I2S_InitStructure;

	if ((_usMode == I2S_Mode_SlaveTx) && (_usMode == I2S_Mode_SlaveRx))
	{
		/* 安富莱开发板不支持这2种模式 */
		return;
	}

	/* 打开 I2S2 APB1 时钟 */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

	/* 复位 SPI2 外设到缺省状态 */
	SPI_I2S_DeInit(SPI2); 

	/* I2S2 外设配置 */
	if (_usMode == I2S_Mode_MasterTx)
	{
		I2S_InitStructure.I2S_Mode = I2S_Mode_MasterTx;			/* 配置I2S工作模式 */
		I2S_InitStructure.I2S_Standard = _usStandard;			/* 接口标准 */
		I2S_InitStructure.I2S_DataFormat = _usWordLen;			/* 数据格式，16bit */
		I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Enable;	/* 主时钟模式 */
		I2S_InitStructure.I2S_AudioFreq = _uiAudioFreq;			/* 音频采样频率 */
		I2S_InitStructure.I2S_CPOL = I2S_CPOL_Low;  			
		I2S_Init(SPI2, &I2S_InitStructure);
	}
	else if (_usMode == I2S_Mode_MasterRx)
	{
		I2S_InitStructure.I2S_Mode = I2S_Mode_MasterRx;			/* 配置I2S工作模式 */
		I2S_InitStructure.I2S_Standard = _usStandard;			/* 接口标准 */
		I2S_InitStructure.I2S_DataFormat = _usWordLen;			/* 数据格式，16bit */
		I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Enable;	/* 主时钟模式 */
		I2S_InitStructure.I2S_AudioFreq = _uiAudioFreq;			/* 音频采样频率 */
		I2S_InitStructure.I2S_CPOL = I2S_CPOL_Low;  			
		I2S_Init(SPI2, &I2S_InitStructure);
	}

	/* 禁止I2S2 TXE中断(发送缓冲区空)，需要时再打开 */
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, DISABLE);

	/* 禁止I2S2 RXNE中断(接收不空)，需要时再打开 */
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, DISABLE);

	/* 使能 SPI2/I2S2 外设 */
	I2S_Cmd(SPI2, ENABLE);	
}

/*
*********************************************************************************************************
*	函 数 名: I2S_NVIC_Config
*	功能说明: 配置I2S NVIC通道(中断模式)。中断服务函数void SPI2_IRQHandler(void) 在stm32f10x_it.c
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void I2S_NVIC_Config(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	/* SPI2 IRQ 通道配置 */
	NVIC_InitStructure.NVIC_IRQChannel = SPI2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
