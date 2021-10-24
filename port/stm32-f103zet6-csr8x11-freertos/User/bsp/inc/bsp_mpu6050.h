/*
*********************************************************************************************************
*
*	模块名称 : 三轴陀螺仪MPU-6050驱动模块
*	文件名称 : bsp_mpu6050.c
*	版    本 : V1.0
*	说    明 : 头文件
*
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2012-10-12 armfly  ST固件库版本 V2.1.0
*
*	Copyright (C), 2012-2013, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_MPU6050_H
#define _BSP_MPU6050_H

#define MPU6050_SLAVE_ADDRESS    0xD0		/* I2C从机地址 */

//****************************************
// 定义MPU6050内部地址
//****************************************
#define	SMPLRT_DIV		0x19	//陀螺仪采样率，典型值：0x07(125Hz)
#define	CONFIG			0x1A	//低通滤波频率，典型值：0x06(5Hz)
#define	GYRO_CONFIG		0x1B	//陀螺仪自检及测量范围，典型值：0x18(不自检，2000deg/s)

#define	ACCEL_CONFIG	0x1C	//加速计自检、测量范围及高通滤波频率，典型值：0x01(不自检，2G，5Hz)

#define	ACCEL_XOUT_H	0x3B
#define	ACCEL_XOUT_L	0x3C
#define	ACCEL_YOUT_H	0x3D
#define	ACCEL_YOUT_L	0x3E
#define	ACCEL_ZOUT_H	0x3F
#define	ACCEL_ZOUT_L	0x40

#define	TEMP_OUT_H		0x41
#define	TEMP_OUT_L		0x42

#define	GYRO_XOUT_H		0x43
#define	GYRO_XOUT_L		0x44
#define	GYRO_YOUT_H		0x45
#define	GYRO_YOUT_L		0x46
#define	GYRO_ZOUT_H		0x47
#define	GYRO_ZOUT_L		0x48

#define	PWR_MGMT_1		0x6B	//电源管理，典型值：0x00(正常启用)
#define	WHO_AM_I		0x75	//IIC地址寄存器(默认数值0x68，只读)

typedef struct
{
	int16_t Accel_X;
	int16_t Accel_Y;
	int16_t Accel_Z;

	int16_t Temp;

	int16_t GYRO_X;
	int16_t GYRO_Y;
	int16_t GYRO_Z;
}MPU6050_T;

extern MPU6050_T g_tMPU6050;

void bsp_InitMPU6050(void);
void MPU6050_WriteByte(uint8_t _ucRegAddr, uint8_t _ucRegData);
uint8_t MPU6050_ReadByte(uint8_t _ucRegAddr);

void MPU6050_ReadData(void);


#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
