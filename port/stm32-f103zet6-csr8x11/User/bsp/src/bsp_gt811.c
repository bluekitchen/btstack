/*
*********************************************************************************************************
*
*	模块名称 : 电容触摸芯片GT811驱动程序
*	文件名称 : bsp_ct811.c
*	版    本 : V1.0
*	说    明 : GT811触摸芯片驱动程序。
*	修改记录 :
*		版本号   日期        作者     说明
*		V1.0    2014-12-25  armfly   正式发布
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*********************************************************************************************************
*/

#include "bsp.h"

#define GT811_READ_XY_REG 	0x721	/* 坐标寄存器 */
#define GT811_CONFIG_REG	0x6A2	/* 配置参数寄存器 */

/* GT811配置参数，一次性写入 */
//const uint8_t s_GT811_CfgParams[]=
uint8_t s_GT811_CfgParams[]=
{
	/*
		0x6A2  R/W  Sen_CH0    触摸屏 1 号感应线对应的 IC 感应线 
		0x6A3  R/W  Sen_CH1    触摸屏 2 号感应线对应的 IC 感应线
		0x6A4  R/W  Sen_CH2    触摸屏 3 号感应线对应的 IC 感应线
		0x6A5  R/W  Sen_CH3    触摸屏 4 号感应线对应的 IC 感应线
		0x6A6  R/W  Sen_CH4    触摸屏 5 号感应线对应的 IC 感应线
		0x6A7  R/W  Sen_CH5    触摸屏 6 号感应线对应的 IC 感应线 
		0x6A8  R/W  Sen_CH6    触摸屏 7 号感应线对应的 IC 感应线
		0x6A9  R/W  Sen_CH7    触摸屏 8 号感应线对应的 IC 感应线
		0x6AA  R/W  Sen_CH8    触摸屏 9 号感应线对应的 IC 感应线
		0x6AB  R/W  Sen_CH9    触摸屏 10 号感应线对应的 IC 感应线
	*/
    0x12,0x10,0x0E,0x0C,0x0A,0x08,0x06,0x04,0x02,0x00,

	/*
	0x6AC  R/W  Dr0_Con  CHSELEF0  F1DELAY0
	0x6AD  R/W  Dr0_Con  F2DELAY0  F3DELAY0
	
	0x6AE  R/W  Dr1_Con  CHSELEF1  F1DELAY1
	0x6AF  R/W  Dr1_Con  F2DELAY1  F3DELAY1
	
	0x6B0  R/W  Dr2_Con  CHSELEF2  F1DELAY2
	0x6B1  R/W  Dr2_Con  F2DELAY2  F3DELAY2
	
	0x6B2  R/W  Dr3_Con  CHSELEF3  F1DELAY3
	0x6B3  R/W  Dr3_Con  F2DELAY3  F3DELAY3
	
	0x6B4  R/W  Dr4_Con  CHSELEF4  F1DELAY4
	0x6B5  R/W  Dr4_Con  F2DELAY4  F3DELAY4
	
	0x6B6  R/W  Dr5_Con  CHSELEF5  F1DELAY5
	0x6B7  R/W  Dr5_Con  F2DELAY5  F3DELAY5
	
	0x6B8  R/W  Dr6_Con  CHSELEF6  F1DELAY6
	0x6B9  R/W  Dr6_Con  F2DELAY6  F3DELAY6
	
	0x6BA  R/W  Dr7_Con  CHSELEF7  F1DELAY7
	0x6BB  R/W  Dr7_Con  F2DELAY7  F3DELAY7
	
	0x6BC  R/W  Dr8_Con  CHSELEF8  F1DELAY8
	0x6BD  R/W  Dr8_Con  F2DELAY8  F3DELAY8
	
	0x6BE  R/W  Dr9_Con  CHSELEF9  F1DELAY9
	0x6BF  R/W  Dr9_Con  F2DELAY9  F3DELAY9
	
	0x6C0  R/W  Dr10_Con  CHSELEF10  F1DELAY10
	0x6C1  R/W  Dr10_Con  F2DELAY10  F3DELAY10
	
	0x6C2  R/W  Dr11_Con  CHSELEF11  F1DELAY11
	0x6C3  R/W  Dr11_Con  F2DELAY11  F3DELAY11
	
	0x6C4  R/W  Dr12_Con  CHSELEF12  F1DELAY12
	0x6C5  R/W  Dr12_Con  F2DELAY12  F3DELAY12
	
	0x6C6  R/W  Dr13_Con  CHSELEF13  F1DELAY13
	0x6C7  R/W  Dr13_Con  F2DELAY13  F3DELAY13
	
	0x6C8  R/W  Dr14_Con  CHSELEF14  F1DELAY14
	0x6C9  R/W  Dr14_Con  F2DELAY14  F3DELAY14
	
	0x6CA  R/W  Dr15_Con  CHSELEF15  F1DELAY15
	0x6CB  R/W  Dr15_Con  F2DELAY15  F3DELAY15
	*/
	0x05,0x55,0x15,0x55,0x25,0x55,0x35,0x55,0x45,0x55,0x55,0x55,0x65,0x55,0x75,0x55,
	0x85,0x55,0x95,0x55,0xA5,0x55,0xB5,0x55,0xC5,0x55,0xD5,0x55,0xE5,0x55,0xF5,0x55,	
	
	/*
	0x6CC  R/W  ADCCFG  芯片扫描控制参数
	0x6CD  R/W  SCAN    芯片扫描控制参数
	*/
	0x1B,0x03,
	
	/*
	0x6CE  R/W  F1SET  驱动脉冲 1 频率
	0x6CF  R/W  F2SET  驱动脉冲 2 频率
	0x6D0  R/W  F3SET  驱动脉冲 3 频率
	0x6D1  R/W  F1PNUM  驱动脉冲 1 个数
	0x6D2  R/W  F2PNUM  驱动脉冲 2 个数 
	0x6D3  R/W  F3PNUM  驱动脉冲 3 个数
	*/
	0x00,0x00,0x00,0x13,0x13,0x13,
	
	/* 0x6D4  R/W  TOTALROW  全部使用的驱动通道数(屏的驱动线+按键驱动线) */
	0x0F,
	
	/*
	0x6D5  R/W  TSROW  用在屏上的驱动线
	0x6D6  R/W  TOTALCOL  用在屏上的感应线
	*/
	0x0F,0x0A,
	
	/*
	0x6D7  R/W  Sc_Touch  屏幕按键阈值
	0x6D8  R/W  Sc_Leave  屏幕松键阈值
	*/
	0x50,0x30,
	
	/* 
	0x6D9  R/W  Md_Switch  保留  DD2    R1  R0  INT    SITO    RT    ST 
	0x6DA  R/W  LPower_C  保留  Auto 无按键进低功耗时间，0-63 有效，以 s 为单位
	*/
	0x05,0x03,
	
	/* 0x6DB  R/W  Refresh  触摸刷新速率控制参数（50Hz~100Hz）：0-100 有效 */	
	0x64,
	
	/* 0x6DC  R/W  Touch_N  保留  使能触摸点个数：1-5 有效 */
	0x05,
	
	/* 
	0x6DD  R/W  X_Ou_Max_L X 坐标输出最大值  480
	0x6DE  R/W  X_Ou_Max_H
	*/
	0xe0,0x01,
	
	/*
	0x6DF  R/W  Y_Ou_Max_L  Y 坐标输出最大值  800
	0x6E0  R/W  Y_Ou_Max_H
	*/
	0x20,0x03,
	
	/*
	0x6E1  R/W  X _Th  X 坐标输出门限：0-255，以 4 个原始坐标点为单位
	0x6E2  R/W  Y_Th  Y 坐标输出门限：0-255，以 4 个原始坐标点为单位
	*/ 
	0x00,  0x00,
	
	/*
	0x6E3  R/W  X_Co_Sm  X 方向平滑控制变量，0-255 可配置，0 表示关
	0x6E4  R/W  Y_Co_Sm  Y 方向平滑控制变量，0-255 可配置，0 表示关
	0x6E5  R/W  X_Sp_Lim  X 方向平滑上限速度：0-255 可配置，0 表示关
	0x6E6  R/W  Y_Sp_ Lim  Y 方向平滑上限速度：0-255 可配置，0 表示关
	*/
	0x32,0x2C,0x34,0x2E,
	
	/*
	0x6E7  R/W  X_Bor_Lim  Reserved  Reserved
	0x6E8  R/W  Y_Bor_Lim  Reserved  Reserved
	*/
	0x00,0x00,
	
	/* 0x6E9  R/W  Filter  丢弃数据帧数  坐标窗口滤波值，以 4 为基数 */
	0x04,
	
	/* 0x6EA  R/W  Large_Tc  0-255 有效：单一触摸区包含结点数大于此数会判为大面积触摸 */
	0x14,
	
	/* 0x6EB  R/W  Shake_Cu  Touch 事件建立去抖  手指个数从多到少去抖 */
	0x22,
	
	/* 0x6EC  R/W  Noise_R  保留  白噪声削减量（低 nibble）有效 */
	0x04,
	
	/* 0x6ED~0x6F1 R/W    保留 */
	0x00,0x00,0x00,0x00,0x00,
	
	
    0x20,0x14,0xEC,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x30,
    0x25,0x28,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
};

#if 0
uint8_t s_GT811_CfgParams[]=
{
    0x30,0x0f,0x05,0x06,0x28,0x02,0x14,0x14,0x10,0x28,0xb2,
	800>>8,
	800&0xFF,
	480>>8,
	480&0xFF,
	0xed,
    0xcb,0xa9,0x87,0x65,0x43,0x21,0x00,0x00,0x00,0x00,0x00,0x4d,0xc1,0x20,0x01,0x01,
    0x41,0x64,0x3c,0x1e,0x28,0x0e,0x00,0x00,0x00,0x00,0x50,0x3c,0x32,0x71,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x01
};  

uint8_t s_GT811_CfgParams[]=
{
        0x06,0xA2,  
        0x12,0x10,0x0E,0x0C,0x0A,0x08,0x06,0x04,0x02,0x00,0xE2,0x53,0xD2,0x53,0xC2,0x53,  
        0xB2,0x53,0xA2,0x53,0x92,0x53,0x82,0x53,0x72,0x53,0x62,0x53,0x52,0x53,0x42,0x53,  
        0x32,0x53,0x22,0x53,0x12,0x53,0x02,0x53,0xF2,0x53,0x0F,0x13,0x40,0x40,0x40,0x10,  
        0x10,0x10,0x0F,0x0F,0x0A,0x35,0x25,0x0C,0x03,0x00,0x05,0x20,0x03,0xE0,0x01,0x00,  
        0x00,0x34,0x2C,0x36,0x2E,0x00,0x00,0x03,0x19,0x03,0x08,0x00,0x00,0x00,0x00,0x00,  
        0x14,0x10,0xEC,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0D,0x40,  
        0x30,0x3C,0x28,0x00,0x00,0x00,0x00,0xC0,0x12,0x01     
};  
#endif



static void GT811_WriteReg(uint16_t _usRegAddr, uint8_t *_pRegBuf, uint8_t _ucLen);
static void GT811_ReadReg(uint16_t _usRegAddr, uint8_t *_pRegBuf, uint8_t _ucLen);

GT811_T g_GT811;

/*
*********************************************************************************************************
*	函 数 名: bsp_InitTouch
*	功能说明: 配置STM32和触摸相关的口线，片选由软件控制. SPI总线的GPIO在bsp_spi_bus.c 中统一配置。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void GT811_InitHard(void)
{
	uint16_t ver;

	g_GT811.TimerCount = 0;
	
	ver = GT811_ReadVersion();

	printf("GT811 Version : %04X\r\n", ver);

	/* I2C总线初始化在 bsp.c 中执行 */
	
	GT811_WriteReg(GT811_CONFIG_REG, (uint8_t *)s_GT811_CfgParams, sizeof(s_GT811_CfgParams));
	
	g_GT811.Enable = 1;
}

/*
*********************************************************************************************************
*	函 数 名: GT811_ReadVersion
*	功能说明: 获得GT811的芯片版本
*	形    参: 无
*	返 回 值: 16位版本
*********************************************************************************************************
*/
uint16_t GT811_ReadVersion(void)
{
	uint8_t buf[2];

	GT811_ReadReg(0x717, buf, 2);

	return ((uint16_t)buf[0] << 8) + buf[1];
}


/*
*********************************************************************************************************
*	函 数 名: GT811_WriteReg
*	功能说明: 写1个或连续的多个寄存器
*	形    参: _usRegAddr : 寄存器地址
*			  _pRegBuf : 寄存器数据缓冲区
*			 _ucLen : 数据长度
*	返 回 值: 无
*********************************************************************************************************
*/
static void GT811_WriteReg(uint16_t _usRegAddr, uint8_t *_pRegBuf, uint8_t _ucLen)
{
	uint8_t i;

    i2c_Start();					/* 总线开始信号 */

    i2c_SendByte(GT811_I2C_ADDR);	/* 发送设备地址+写信号 */
	i2c_WaitAck();

    i2c_SendByte(_usRegAddr >> 8);	/* 地址高8位 */
	i2c_WaitAck();

    i2c_SendByte(_usRegAddr);		/* 地址低8位 */
	i2c_WaitAck();

	for (i = 0; i < _ucLen; i++)
	{
	    i2c_SendByte(_pRegBuf[i]);		/* 寄存器数据 */
		i2c_WaitAck();
	}

    i2c_Stop();                   			/* 总线停止信号 */
}

/*
*********************************************************************************************************
*	函 数 名: GT811_ReadReg
*	功能说明: 写1个或连续的多个寄存器
*	形    参: _usRegAddr : 寄存器地址
*			  _pRegBuf : 寄存器数据缓冲区
*			 _ucLen : 数据长度
*	返 回 值: 无
*********************************************************************************************************
*/
static void GT811_ReadReg(uint16_t _usRegAddr, uint8_t *_pRegBuf, uint8_t _ucLen)
{
	uint8_t i;

    i2c_Start();					/* 总线开始信号 */

    i2c_SendByte(GT811_I2C_ADDR);	/* 发送设备地址+写信号 */
	i2c_WaitAck();

    i2c_SendByte(_usRegAddr >> 8);	/* 地址高8位 */
	i2c_WaitAck();

    i2c_SendByte(_usRegAddr);		/* 地址低8位 */
	i2c_WaitAck();

	i2c_Start();
    i2c_SendByte(GT811_I2C_ADDR + 0x01);	/* 发送设备地址+读信号 */
	i2c_WaitAck();

	for (i = 0; i < _ucLen - 1; i++)
	{
	    _pRegBuf[i] = i2c_ReadByte();	/* 读寄存器数据 */
		i2c_Ack();
	}

	/* 最后一个数据 */
	 _pRegBuf[i] = i2c_ReadByte();		/* 读寄存器数据 */
	i2c_NAck();

    i2c_Stop();							/* 总线停止信号 */
}

/*
*********************************************************************************************************
*	函 数 名: GT811_Timer1ms
*	功能说明: 每隔10ms调用1次
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void GT811_Timer1ms(void)
{
	g_GT811.TimerCount++;
}

/*
*********************************************************************************************************
*	函 数 名: GT811_Scan
*	功能说明: 读取GT811触摸数据。读取全部的数据，需要 720us左右。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void GT811_Scan(void)
{
	uint8_t buf[48];
	//uint8_t i;
	static uint8_t s_tp_down = 0;
	uint16_t x, y;
	static uint16_t x_save, y_save;

	if (g_GT811.Enable == 0)
	{
		return;
	}
	
	/* 20ms 执行一次 */
	if (g_GT811.TimerCount < 20)
	{
		return;
	}

	g_GT811.TimerCount = 0;
	
	GT811_ReadReg(GT811_READ_XY_REG, buf, 1);		
	if ((buf[0] & 0x01) == 0)
	{
		if (s_tp_down == 1)
		{
			s_tp_down = 0;
			TOUCH_PutKey(TOUCH_RELEASE, x_save, y_save);
		}
		return;
	}
					
	GT811_ReadReg(GT811_READ_XY_REG + 1, &buf[1], 33);
	
	/*
	0x721  R  TouchpointFlag  Sensor_ID  key  tp4  tp3  tp2  tp1  tp0
	0x722  R  Touchkeystate     0  0  0  0  key4  key3  key2  key1

	0x723  R  Point0Xh  触摸点 0，X 坐标高 8 位
	0x724  R  Point0Xl  触摸点 0，X 坐标低 8 位
	0x725  R  Point0Yh  触摸点 0，Y 坐标高 8 位
	0x726  R  Point0Yl  触摸点 0，Y 坐标低 8 位
	0x727  R  Point0Pressure  触摸点 0，触摸压力

	0x728  R  Point1Xh  触摸点 1，X 坐标高 8 位
	0x729  R  Point1Xl  触摸点 1，X 坐标低 8 位
	0x72A  R  Point1Yh  触摸点 1，Y 坐标高 8 位
	0x72B  R  Point1Yl  触摸点 1，Y 坐标低 8 位
	0x72C  R  Point1Pressure  触摸点 1，触摸压力

	0x72D  R  Point2Xh  触摸点 2，X 坐标高 8 位
	0x72E  R  Point2Xl  触摸点 2，X 坐标低 8 位
	0x72F  R  Point2Yh  触摸点 2，Y 坐标高 8 位
	0x730  R  Point2Yl  触摸点 2，Y 坐标低 8 位
	0x731  R  Point2Pressure  触摸点 2，触摸压力

	0x732  R  Point3Xh  触摸点 3，X 坐标高 8 位
	0x733-0x738  R    Reserve  none
	0x739  R  Point3Xl  触摸点 3，X 坐标低 8 位
	0x73A  R  Point3Yh  触摸点 3，Y 坐标高 8 位
	0x73B  R  Point3Yl  触摸点 3，Y 坐标低 8 位
	0x73C  R  Point3Pressure  触摸点 3，触摸压力

	0x73D  R  Point4Xh  触摸点 4，X 坐标高 8 位
	0x73E  R  Point4Xl  触摸点 4，X 坐标低 8 位
	0x73F  R  Point4Yh  触摸点 4，Y 坐标高 8 位
	0x740  R  Point4Yl  触摸点 4，Y 坐标低 8 位
	0x741  R  Point4Pressure  触摸点 4，触摸压力

	0x742  R  Data_check_sum  Data check Sum
	*/

	g_GT811.TouchpointFlag = buf[0];
	g_GT811.Touchkeystate = buf[1];

	g_GT811.X0 = ((uint16_t)buf[2] << 8) + buf[3];
	g_GT811.Y0 = ((uint16_t)buf[4] << 8) + buf[5];
	g_GT811.P0 = buf[6];

	g_GT811.X1 = ((uint16_t)buf[7] << 8) + buf[8];
	g_GT811.Y1 = ((uint16_t)buf[9] << 8) + buf[10];
	g_GT811.P1 = buf[11];

	g_GT811.X2 = ((uint16_t)buf[12] << 8) + buf[13];
	g_GT811.Y2 = ((uint16_t)buf[14] << 8) + buf[15];
	g_GT811.P2 = buf[16];

	/* 触摸点3的地址不连续 */
	g_GT811.X3 = ((uint16_t)buf[17] << 8) + buf[24];
	g_GT811.Y3 = ((uint16_t)buf[25] << 8) + buf[26];
	g_GT811.P3 = buf[27];

	g_GT811.X4 = ((uint16_t)buf[28] << 8) + buf[29];
	g_GT811.Y4 = ((uint16_t)buf[30] << 8) + buf[31];
	g_GT811.P4 = buf[32];

	/* 检测按下 */
	{
		/* 坐标转换 :
			电容触摸板左下角是 (0，0);  右上角是 (479，799)
			需要转到LCD的像素坐标 (左上角是 (0，0), 右下角是 (799，479)
		*/
		x = g_GT811.Y0;
		y = 479 - g_GT811.X0;
	}
	
	if (s_tp_down == 0)
	{
		s_tp_down = 1;
		
		TOUCH_PutKey(TOUCH_DOWN, x, y);
	}
	else
	{
		TOUCH_PutKey(TOUCH_MOVE, x, y);
	}
	x_save = x;	/* 保存坐标，用于释放事件 */
	y_save = y;

#if 0
	for (i = 0; i < 34; i++)
	{
		printf("%02X ", buf[i]);
	}
	printf("\r\n");

	printf("(%5d,%5d,%3d) ",  g_GT811.X0, g_GT811.Y0, g_GT811.P0);
	printf("(%5d,%5d,%3d) ",  g_GT811.X1, g_GT811.Y1, g_GT811.P1);
	printf("(%5d,%5d,%3d) ",  g_GT811.X2, g_GT811.Y2, g_GT811.P2);
	printf("(%5d,%5d,%3d) ",  g_GT811.X3, g_GT811.Y3, g_GT811.P3);
	printf("(%5d,%5d,%3d) ",  x, y, g_GT811.P4);
	printf("\r\n");
#endif	
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
