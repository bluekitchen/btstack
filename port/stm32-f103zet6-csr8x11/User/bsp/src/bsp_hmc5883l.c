/*
*********************************************************************************************************
*
*	模块名称 : 三轴磁力计HMC5883L驱动模块
*	文件名称 : bsp_HMC5883L.c
*	版    本 : V1.0
*	说    明 : 实现HMC5883L的读写操作。
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-02-01 armfly  正式发布
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

/*
	应用说明:访问HMC5883L前，请先调用一次 bsp_InitI2C()函数配置好I2C相关的GPIO.
*/

#include "bsp.h"

HMC5883L_T g_tMag;		/* 定义一个全局变量，保存实时数据 */

/*
*********************************************************************************************************
*	函 数 名: bsp_InitHMC5883L
*	功能说明: 初始化HMC5883L
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitHMC5883L(void)
{


	
	/* 设置Mode寄存器 */
	#if 1
		HMC5883L_WriteByte(0x00, 0x70);	
		HMC5883L_WriteByte(0x01, 0x20);	
		HMC5883L_WriteByte(0x02, 0x00);	
	#else	/* 自校准模式 */
		HMC5883L_WriteByte(0x00, 0x70 + 2);	
		HMC5883L_WriteByte(0x01, 0x20);	
		HMC5883L_WriteByte(0x02, 0x00);		
	#endif

	g_tMag.CfgRegA = HMC5883L_ReadByte(0);
	g_tMag.CfgRegB = HMC5883L_ReadByte(1);
	g_tMag.ModeReg = HMC5883L_ReadByte(2);

	g_tMag.IDReg[0] = HMC5883L_ReadByte(10);
	g_tMag.IDReg[1] = HMC5883L_ReadByte(11);
	g_tMag.IDReg[2] = HMC5883L_ReadByte(12);
	g_tMag.IDReg[3] = 0;
		

	/* 设置最小最大值初值 */
	g_tMag.X_Min = 4096;
	g_tMag.X_Max = -4096;
	
	g_tMag.Y_Min = 4096;
	g_tMag.Y_Max = -4096;

	g_tMag.Z_Min = 4096;
	g_tMag.Z_Max = -4096;
}

/*
*********************************************************************************************************
*	函 数 名: HMC5883L_WriteByte
*	功能说明: 向 HMC5883L 寄存器写入一个数据
*	形    参: _ucRegAddr : 寄存器地址
*			  _ucRegData : 寄存器数据
*	返 回 值: 无
*********************************************************************************************************
*/
void HMC5883L_WriteByte(uint8_t _ucRegAddr, uint8_t _ucRegData)
{
    i2c_Start();							/* 总线开始信号 */

    i2c_SendByte(HMC5883L_SLAVE_ADDRESS);	/* 发送设备地址+写信号 */
	i2c_WaitAck();

    i2c_SendByte(_ucRegAddr);				/* 内部寄存器地址 */
	i2c_WaitAck();

    i2c_SendByte(_ucRegData);				/* 内部寄存器数据 */
	i2c_WaitAck();

    i2c_Stop();                   			/* 总线停止信号 */
}

/*
*********************************************************************************************************
*	函 数 名: HMC5883L_ReadByte
*	功能说明: 读取 MPU-6050 寄存器的数据
*	形    参: _ucRegAddr : 寄存器地址
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t HMC5883L_ReadByte(uint8_t _ucRegAddr)
{
	uint8_t ucData;

	i2c_Start();                  			/* 总线开始信号 */
	i2c_SendByte(HMC5883L_SLAVE_ADDRESS);	/* 发送设备地址+写信号 */
	i2c_WaitAck();
	i2c_SendByte(_ucRegAddr);     			/* 发送存储单元地址 */
	i2c_WaitAck();

	i2c_Start();                  			/* 总线开始信号 */

	i2c_SendByte(HMC5883L_SLAVE_ADDRESS+1); 	/* 发送设备地址+读信号 */
	i2c_WaitAck();

	ucData = i2c_ReadByte();       			/* 读出寄存器数据 */
	i2c_NAck();
	i2c_Stop();                  			/* 总线停止信号 */
	return ucData;
}


/*
*********************************************************************************************************
*	函 数 名: HMC5883L_ReadData
*	功能说明: 读取 MPU-6050 数据寄存器， 结果保存在全局变量 g_tMag.  主程序可以定时调用该程序刷新数据
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void HMC5883L_ReadData(void)
{
	uint8_t ucReadBuf[7];
	uint8_t i;

#if 1 /* 连续读 */
	i2c_Start();                  			/* 总线开始信号 */
	i2c_SendByte(HMC5883L_SLAVE_ADDRESS);	/* 发送设备地址+写信号 */
	i2c_WaitAck();
	i2c_SendByte(DATA_OUT_X);     		/* 发送存储单元地址  */
	i2c_WaitAck();

	i2c_Start();                  			/* 总线开始信号 */

	i2c_SendByte(HMC5883L_SLAVE_ADDRESS + 1); /* 发送设备地址+读信号 */
	i2c_WaitAck();

	for (i = 0; i < 6; i++)
	{
		ucReadBuf[i] = i2c_ReadByte();       			/* 读出寄存器数据 */
		i2c_Ack();
	}

	/* 读最后一个字节，时给 NAck */
	ucReadBuf[6] = i2c_ReadByte();
	i2c_NAck();

	i2c_Stop();                  			/* 总线停止信号 */

#else	/* 单字节读 */
	for (i = 0 ; i < 7; i++)
	{
		ucReadBuf[i] = HMC5883L_ReadByte(DATA_OUT_X + i);
	}
#endif

	/* 将读出的数据保存到全局结构体变量 */
	g_tMag.X = (int16_t)((ucReadBuf[0] << 8) + ucReadBuf[1]);
	g_tMag.Z = (int16_t)((ucReadBuf[2] << 8) + ucReadBuf[3]);
	g_tMag.Y = (int16_t)((ucReadBuf[4] << 8) + ucReadBuf[5]);
	
	g_tMag.Status = ucReadBuf[6];
	
	/* 统计最大值和最小值 */
	if ((g_tMag.X > - 2048) && (g_tMag.X < 2048))
	{
		if (g_tMag.X > g_tMag.X_Max)
		{
			g_tMag.X_Max = g_tMag.X;
		}
		if (g_tMag.X < g_tMag.X_Min)
		{
			g_tMag.X_Min = g_tMag.X;
		}	
	}

	if ((g_tMag.Y > - 2048) && (g_tMag.Y < 2048))
	{
		if (g_tMag.Y > g_tMag.Y_Max)
		{
			g_tMag.Y_Max = g_tMag.Y;
		}
		if (g_tMag.Y < g_tMag.Y_Min)
		{
			g_tMag.Y_Min = g_tMag.Y;
		}	
	}
	
	if ((g_tMag.Z > - 2048) && (g_tMag.Z < 2048))
	{
		if (g_tMag.Z > g_tMag.Z_Max)
		{
			g_tMag.Z_Max = g_tMag.Z;
		}
		if (g_tMag.Z < g_tMag.Z_Min)
		{
			g_tMag.Z_Min = g_tMag.Z;
		}	
	}
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/

