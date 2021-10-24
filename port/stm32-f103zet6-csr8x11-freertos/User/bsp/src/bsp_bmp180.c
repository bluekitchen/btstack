/*
*********************************************************************************************************
*
*	模块名称 : 气压强度BMP180驱动模块
*	文件名称 : bsp_bmp180.c
*	版    本 : V1.0
*	说    明 : 实现BMP180的底层控制。BMP180停产了，BMP180可以替代。封装变小了，软件一样，
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2015-07-11  armfly  正式发布
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

/*
	应用说明:访问BMP180前，请先调用一次 bsp_InitI2C()函数配置好I2C相关的GPIO.

	BMP180 是BOSCH公司生产的的一款高精度、超低能耗的压力传感器。绝对精度最低可以达到0.03hPa。
	BMP180 通过I2C总线直接与各种微处理器相连。
	压力范围：300 ... 1100hPa（海拔9000米...-500米）
			30kPa ... 110kPa

	压强单位：
		inHg 英寸汞柱
		mmHg 毫米汞柱
		mbar 毫巴(=百帕)
		hPa 百帕
		kPa 千帕, 1kPa = 1000Pa
		MPa 兆帕, 1MPa = 1000kPa = 1000000Pa

	1百帕=1毫巴=3/4毫米水银柱
	在海平面的平均气压 约为1013.25百帕斯卡（760毫米水银柱），这个值也被称为标准大气压

	度量衡换算：
		1标准大气压=101 325帕斯卡
		1帕斯卡=9.8692326671601*10-6标准大气压

	Doc文件夹下有《全国各地主要城市海拔高度及大气压参考数据.pdf》

	城市   海拔高度(m)  气压强度(kPa)
	北京     31.2         99.86
	天津     3.3          100.48
	石家庄   80.5         99.56
	太原     777.9        91.92
	呼和浩特 1063         88.94
	沈阳     41.6         100.07
	大连     92.8         99.47
	长春     236.8        97.79
	哈尔滨   171.7        98.51
	上海     4.5          100.53
	南京     8.9          100.4
	杭州     41.7         100.05
	合肥     29.8         100.09
	福州     84           99.64
	厦门     63.2         99.91
	南昌     46.7         99.91
	济南     51.6         99.85
	武汉     23.3         100.17
	郑州     110.4        99.17
	长沙     44.9         99.94
	广州     6.6          100.45
	南宁     72.2         99.6
	重庆     259.1        97.32
	贵阳     1071.2       88.79
	昆明     1891.4       80.8
	拉萨     3658.        65.23
	西安     396.9        95.92
	兰州     1517.2       84.31
	成都     505.9        94.77
	西宁     2261.2       77.35
	银川     1111.5       88.35
	乌鲁木齐 917.9        90.67

	香港     32           100.56
	台北     9            100.53

	汕头     1.2          100.55   【最低海拔】广东省
	那曲     4507         58.9     【最高海拔】西藏自治区

	珠峰高度为海拔 8848米    相当于0.3个大气压 ,即30.39KPa  【该数据存在争议】

	【根据气压，温度计算海拔高度，只是大致关系，仅供参考】
	H = (RT/gM)*ln(p0/p)
		R为常数8.51
		T为热力学温度（常温下）（摄氏度要转化成热力学温度）
		g为重力加速度10
		M为气体的分子量29
		P0为标准大气压
		P为要求的高度的气压
		这个公式的推导过程较复杂就不推导了，自己应该会转换的。


	可以总结出近似的计算公式；
		P=100KPa   H*10kPa/km   H在0,3km之间
		P=70kPa    H*8kPa/km    H在3km,5km之间
		P=54kPa    H*6.5kPa/km   H在5km,7km之间
		P=41kPa    H*5kPa/km   H在7km,10km之间
		P=25kPa    H*3.5kPa/km   H在10km,12km之间

*/

#include "bsp.h"

static void BMP180_WriteReg(uint8_t _ucRegAddr, uint8_t _ucRegValue);
static uint16_t BMP180_Read2Bytes(uint8_t _ucRegAddr);
static uint32_t BMP180_Read3Bytes(uint8_t _ucRegAddr);
static void BMP180_WaitConvert(void);

BMP180_T g_tBMP180;

/*
*********************************************************************************************************
*	函 数 名: bsp_InitBMP180
*	功能说明: 初始化BMP180
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitBMP180(void)
{
	/* 读出芯片内部的校准参数（每个芯片不同，这是BOSCH出厂前校准好的数据） */
	g_tBMP180.AC1 = (int16_t)BMP180_Read2Bytes(0xAA);
	g_tBMP180.AC2 = (int16_t)BMP180_Read2Bytes(0xAC);
	g_tBMP180.AC3 = (int16_t)BMP180_Read2Bytes(0xAE);
	g_tBMP180.AC4 = (uint16_t)BMP180_Read2Bytes(0xB0);
	g_tBMP180.AC5 = (uint16_t)BMP180_Read2Bytes(0xB2);
	g_tBMP180.AC6 = (uint16_t)BMP180_Read2Bytes(0xB4);
	g_tBMP180.B1 =  (int16_t)BMP180_Read2Bytes(0xB6);
	g_tBMP180.B2 =  (int16_t)BMP180_Read2Bytes(0xB8);
	g_tBMP180.MB =  (int16_t)BMP180_Read2Bytes(0xBA);
	g_tBMP180.MC =  (int16_t)BMP180_Read2Bytes(0xBC);
	g_tBMP180.MD =  (int16_t)BMP180_Read2Bytes(0xBE);

	g_tBMP180.OSS = 0;	/* 过采样参数，0-3 */
}

/*
*********************************************************************************************************
*	函 数 名: BMP180_WriteReg
*	功能说明: 写寄存器
*	形    参: _ucOpecode : 寄存器地址
*			  _ucRegData : 寄存器数据
*	返 回 值: 无
*********************************************************************************************************
*/
static void BMP180_WriteReg(uint8_t _ucRegAddr, uint8_t _ucRegValue)
{
    i2c_Start();							/* 总线开始信号 */

    i2c_SendByte(BMP180_SLAVE_ADDRESS);		/* 发送设备地址+写信号 */
	i2c_WaitAck();

    i2c_SendByte(_ucRegAddr);				/* 发送寄存器地址 */
	i2c_WaitAck();

    i2c_SendByte(_ucRegValue);				/* 发送寄存器数值 */
	i2c_WaitAck();

    i2c_Stop();                   			/* 总线停止信号 */
}

/*
*********************************************************************************************************
*	函 数 名: BMP180_Read2Bytes
*	功能说明: 读取BMP180寄存器数值，连续2字节。用于温度寄存器
*	形    参: _ucRegAddr 寄存器地址
*	返 回 值: 寄存器值
*********************************************************************************************************
*/
static uint16_t BMP180_Read2Bytes(uint8_t _ucRegAddr)
{
	uint8_t ucData1;
	uint8_t ucData2;
	uint16_t usRegValue;

	i2c_Start();                  			/* 总线开始信号 */
	i2c_SendByte(BMP180_SLAVE_ADDRESS);		/* 发送设备地址+写信号 */
	i2c_WaitAck();
	i2c_SendByte(_ucRegAddr);				/* 发送地址 */
	i2c_WaitAck();

	i2c_Start();                  			/* 总线开始信号 */
	i2c_SendByte(BMP180_SLAVE_ADDRESS + 1);/* 发送设备地址+读信号 */
	i2c_WaitAck();

	ucData1 = i2c_ReadByte();       		/* 读出高字节数据 */
	i2c_Ack();

	ucData2 = i2c_ReadByte();       		/* 读出低字节数据 */
	i2c_NAck();
	i2c_Stop();                  			/* 总线停止信号 */

	usRegValue = (ucData1 << 8) + ucData2;

	return usRegValue;
}

/*
*********************************************************************************************************
*	函 数 名: BMP180_Read3Bytes
*	功能说明: 读取BMP180寄存器数值，连续3字节  用于读压力寄存器
*	形    参: _ucRegAddr 寄存器地址
*	返 回 值: 寄存器值
*********************************************************************************************************
*/
static uint32_t BMP180_Read3Bytes(uint8_t _ucRegAddr)
{
	uint8_t ucData1;
	uint8_t ucData2;
	uint8_t ucData3;
	uint32_t uiRegValue;

	i2c_Start();                  			/* 总线开始信号 */
	i2c_SendByte(BMP180_SLAVE_ADDRESS);		/* 发送设备地址+写信号 */
	i2c_WaitAck();
	i2c_SendByte(_ucRegAddr);				/* 发送地址 */
	i2c_WaitAck();

	i2c_Start();                  			/* 总线开始信号 */
	i2c_SendByte(BMP180_SLAVE_ADDRESS + 1);/* 发送设备地址+读信号 */
	i2c_WaitAck();

	ucData1 = i2c_ReadByte();       		/* 读出高字节数据 */
	i2c_Ack();

	ucData2 = i2c_ReadByte();       		/* 读出中间字节数据 */
	i2c_Ack();

	ucData3 = i2c_ReadByte();       		/* 读出最低节数据 */
	i2c_NAck();
	i2c_Stop();                  			/* 总线停止信号 */

	uiRegValue = (ucData1 << 16) + (ucData2 << 8) + ucData3;

	return uiRegValue;
}

/*
*********************************************************************************************************
*	函 数 名: BMP180_WaitConvert
*	功能说明: 等待内部转换结束
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void BMP180_WaitConvert(void)
{
	if (g_tBMP180.OSS == 0)
	{
		bsp_DelayMS(6);		/* 4.5ms  7.5ms  13.5ms   25.5ms */
	}
	else if (g_tBMP180.OSS == 1)
	{
		bsp_DelayMS(9);		/* 4.5ms  7.5ms  13.5ms   25.5ms */
	}
	else if (g_tBMP180.OSS == 2)
	{
		bsp_DelayMS(15);	/* 4.5ms  7.5ms  13.5ms   25.5ms */
	}
	else if (g_tBMP180.OSS == 3)
	{
		bsp_DelayMS(27);	/* 4.5ms  7.5ms  13.5ms   25.5ms */
	}
}

/*
*********************************************************************************************************
*	函 数 名: BMP180_ReadTempPress
*	功能说明: 读取BMP180测量的温度值和压力值。结果存放在全局变量 g_tBMP180.Temp、g_tBMP180.Press
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void BMP180_ReadTempPress(void)
{
	long UT, X1, X2, B5, T;
	long UP, X3, B3, B6, B7, p;
	unsigned long B4;

	/* 流程见 pdf page 12 */

	/* 读温度原始值 */
	BMP180_WriteReg(0xF4, 0x2E);
	BMP180_WaitConvert();	/* 等待转换结束 */
	UT = BMP180_Read2Bytes(0xF6);

	/* 读压力原始值 */
	BMP180_WriteReg(0xF4, 0x34 + (g_tBMP180.OSS << 6));
	BMP180_WaitConvert();	/* 等待转换结束 */
	UP = BMP180_Read3Bytes(0xF6) >> (8 - g_tBMP180.OSS);

	/* 计算真实温度（单位 0.1摄氏度） */
	X1 = ((long)(UT - g_tBMP180.AC6) * g_tBMP180.AC5) >> 15;
	X2 = ((long)g_tBMP180.MC << 11) / (X1 + g_tBMP180.MD);
	B5 = X1 + X2;	/* 该系数将用于压力的温度补偿计算 */
	T = (B5 + 8) >> 4;
	g_tBMP180.Temp = T;		/* 将计算结果保存在全局变量 */

	/* 计算真实压力值（单位 Pa） */
	B6 = B5 - 4000;
	X1 = (g_tBMP180.B2 * (B6 * B6) >> 12) >> 11;
	X2 = (g_tBMP180.AC2 * B6) >> 11;
	X3 = X1 + X2;
	B3 = (((((long)g_tBMP180.AC1) * 4 + X3) << g_tBMP180.OSS) + 2) >> 2;

	X1 = (g_tBMP180.AC3 * B6) >> 13;
	X2 = (g_tBMP180.B1 * ((B6 * B6) >> 12)) >> 16;
	X3 = ((X1 + X2) + 2) >> 2;
	B4 = (g_tBMP180.AC4 * (unsigned long)(X3 + 32768)) >> 15;

	B7 = ((unsigned long)(UP - B3) * (50000 >> g_tBMP180.OSS));
	if (B7 < 0x80000000)
	{
		p = (B7 << 1) / B4;
	}
	else
	{
		p = (B7 / B4) << 1;
	}

	X1 = (p >> 8) * (p >> 8);
	X1 = (X1 * 3038) >> 16;
	X2 = (-7357 * p) >> 16;
 	p =  p + ((X1 + X2 + 3791) >> 4);

	g_tBMP180.Press = p;		/* 将计算结果保存在全局变量 */
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
