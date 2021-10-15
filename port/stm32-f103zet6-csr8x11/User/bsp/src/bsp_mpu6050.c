/*
*********************************************************************************************************
*
*	模块名称 : 三轴陀螺仪MPU-6050驱动模块
*	文件名称 : bsp_mpu6050.c
*	版    本 : V1.0
*	说    明 : 实现MPU-6050的读写操作。
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
	应用说明：访问MPU-6050前，请先调用一次 bsp_InitI2C()函数配置好I2C相关的GPIO.
*/

#include "bsp.h"

MPU6050_T g_tMPU6050;		/* 定义一个全局变量，保存实时数据 */

/*
*********************************************************************************************************
*	函 数 名: bsp_InitMPU6050
*	功能说明: 初始化MPU-6050
*	形    参:  无
*	返 回 值: 1 表示正常， 0 表示不正常
*********************************************************************************************************
*/
void bsp_InitMPU6050(void)
{
	MPU6050_WriteByte(PWR_MGMT_1, 0x00);	//解除休眠状态
	MPU6050_WriteByte(SMPLRT_DIV, 0x07);
	MPU6050_WriteByte(CONFIG, 0x06);
	MPU6050_WriteByte(GYRO_CONFIG, 0xE8);
	MPU6050_WriteByte(ACCEL_CONFIG, 0x01);
}

/*
*********************************************************************************************************
*	函 数 名: MPU6050_WriteByte
*	功能说明: 向 MPU-6050 寄存器写入一个数据
*	形    参: _ucRegAddr : 寄存器地址
*			  _ucRegData : 寄存器数据
*	返 回 值: 无
*********************************************************************************************************
*/
void MPU6050_WriteByte(uint8_t _ucRegAddr, uint8_t _ucRegData)
{
    i2c_Start();							/* 总线开始信号 */

    i2c_SendByte(MPU6050_SLAVE_ADDRESS);	/* 发送设备地址+写信号 */
	i2c_WaitAck();

    i2c_SendByte(_ucRegAddr);				/* 内部寄存器地址 */
	i2c_WaitAck();

    i2c_SendByte(_ucRegData);				/* 内部寄存器数据 */
	i2c_WaitAck();

    i2c_Stop();                   			/* 总线停止信号 */
}

/*
*********************************************************************************************************
*	函 数 名: MPU6050_ReadByte
*	功能说明: 读取 MPU-6050 寄存器的数据
*	形    参: _ucRegAddr : 寄存器地址
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t MPU6050_ReadByte(uint8_t _ucRegAddr)
{
	uint8_t ucData;

	i2c_Start();                  			/* 总线开始信号 */
	i2c_SendByte(MPU6050_SLAVE_ADDRESS);	/* 发送设备地址+写信号 */
	i2c_WaitAck();
	i2c_SendByte(_ucRegAddr);     			/* 发送存储单元地址 */
	i2c_WaitAck();

	i2c_Start();                  			/* 总线开始信号 */

	i2c_SendByte(MPU6050_SLAVE_ADDRESS+1); 	/* 发送设备地址+读信号 */
	i2c_WaitAck();

	ucData = i2c_ReadByte();       			/* 读出寄存器数据 */
	i2c_NAck();
	i2c_Stop();                  			/* 总线停止信号 */
	return ucData;
}


/*
*********************************************************************************************************
*	函 数 名: MPU6050_ReadData
*	功能说明: 读取 MPU-6050 数据寄存器， 结果保存在全局变量 g_tMPU6050.  主程序可以定时调用该程序刷新数据
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void MPU6050_ReadData(void)
{
	uint8_t ucReadBuf[14];
	uint8_t i;
	uint8_t ack;

#if 1 /* 连续读 */
	i2c_Start();                  			/* 总线开始信号 */
	i2c_SendByte(MPU6050_SLAVE_ADDRESS);	/* 发送设备地址+写信号 */
	ack = i2c_WaitAck();
	if (ack != 0)
	{
		i2c_Stop(); 
		return;
	}
	i2c_SendByte(ACCEL_XOUT_H);     		/* 发送存储单元地址  */
	ack = i2c_WaitAck();
	if (ack != 0)
	{
		i2c_Stop(); 
		return;
	}

	i2c_Start();                  			/* 总线开始信号 */

	i2c_SendByte(MPU6050_SLAVE_ADDRESS + 1); /* 发送设备地址+读信号 */
	ack = i2c_WaitAck();
	if (ack != 0)
	{
		i2c_Stop(); 
		return;
	}

	for (i = 0; i < 13; i++)
	{
		ucReadBuf[i] = i2c_ReadByte();       			/* 读出寄存器数据 */
		i2c_Ack();
	}

	/* 读最后一个字节时给 NAck */
	ucReadBuf[13] = i2c_ReadByte();
	i2c_NAck();

	i2c_Stop();                  			/* 总线停止信号 */

#else	/* 单字节读 */
	for (i = 0 ; i < 14; i++)
	{
		ucReadBuf[i] = MPU6050_ReadByte(ACCEL_XOUT_H + i);
	}
#endif

	/* 将读出的数据保存到全局结构体变量 */
	g_tMPU6050.Accel_X = (ucReadBuf[0] << 8) + ucReadBuf[1];
	g_tMPU6050.Accel_Y = (ucReadBuf[2] << 8) + ucReadBuf[3];
	g_tMPU6050.Accel_Z = (ucReadBuf[4] << 8) + ucReadBuf[5];

	g_tMPU6050.Temp = (int16_t)((ucReadBuf[6] << 8) + ucReadBuf[7]);

	g_tMPU6050.GYRO_X = (ucReadBuf[8] << 8) + ucReadBuf[9];
	g_tMPU6050.GYRO_Y = (ucReadBuf[10] << 8) + ucReadBuf[11];
	g_tMPU6050.GYRO_Z = (ucReadBuf[12] << 8) + ucReadBuf[13];
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
