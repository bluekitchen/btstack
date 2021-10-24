/*
*********************************************************************************************************
*
*	模块名称 : GPS定位模块驱动程序
*	文件名称 : bsp_g_tGPS.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_GPS_H
#define __BSP_GPS_H

typedef struct
{
	uint8_t UartOk;		/* 1表示串口数据接收正常，即可以收到GPS的命令字符串 */
	
	/* 从GGA 命令 : 时间、位置、定位类型  (位置信息可有 RMC命令获得) */

	/* 定位有效标志, 0:未定位 1:SPS模式，定位有效 2:差分，SPS模式，定位有效 3:PPS模式，定位有效 */
	uint8_t PositionOk;
	uint32_t Altitude;	/* 海拔高度，相对海平面多少米   x.x  */

	/* GLL：经度、纬度、UTC时间 */
	/* 样例数据：$GPGLL,3723.2475,N,12158.3416,W,161229.487,A*2C */


	/* GSA：GPS接收机操作模式，定位使用的卫星，DOP值 */
	/* 样例数据：$GPGSA,A,3,07,02,26,27,09,04,15, , , , , ,1.8,1.0,1.5*33 */
	uint8_t ModeAM;				/* M=手动（强制操作在2D或3D模式），A=自动 */
	uint8_t Mode2D3D;			/* 定位类型 1=未定位，2=2D定位，3=3D定位 */
	uint8_t SateID[12];			/* ID of 1st satellite used for fix */
	uint16_t PDOP;				/* 位置精度, 0.1米 */
	uint16_t HDOP;				/* 水平精度, 0.1米 */
	uint16_t VDOP;				/* 垂直精度, 0.1米 */

	/* GSV：可见GPS卫星信息、仰角、方位角、信噪比（SNR） */
	/*
		$GPGSV,2,1,07,07,79,048,42,02,51,062,43,26,36,256,42,27,27,138,42*71
		$GPGSV,2,2,07,09,23,313,42,04,19,159,41,15,12,041,42*41
	*/
	uint8_t ViewNumber;			/* 可见卫星个数 */
	uint8_t Elevation[12];		/* 仰角 elevation in degrees  度,最大90°*/
	uint16_t Azimuth[12];		/* 方位角 azimuth in degrees to true  度,0-359°*/
	uint8_t SNR[12];			/* 载噪比（C/No）  信噪比？ ， dBHz   范围0到99，没有跟踪时为空 */

	/* RMC：时间、日期、位置	、速度 */
	/* 样例数据：$GPRMC,161229.487,A,3723.2475,N,12158.3416,W,0.13,309.62,120598, ,*10 */
	uint16_t WeiDu_Du;			/* 纬度，度 */
	uint32_t WeiDu_Fen;			/* 纬度，分. 232475；  小数点后4位, 表示 23.2475 分 */
	char     NS;				/* 纬度半球N（北半球）或S（南半球） */

	uint16_t JingDu_Du;			/* 经度，单位度 */
	uint32_t JingDu_Fen;		/* 经度，单位度 */
	char     EW;				/* 经度半球E（东经）或W（西经） */

	uint16_t Year;				/* 日期 120598 ddmmyy */
	uint8_t  Month;
	uint8_t  Day;
	uint8_t  Hour;				/* UTC 时间。 hhmmss.sss */
	uint8_t  Min;				/* 分 */
	uint8_t  Sec;				/* 秒 */
	uint16_t mSec;				/* 毫秒 */
	char   TimeOk;				/* A=UTC时间数据有效；V=数据无效 */

	/* VTG：地面速度信息 */
	/* 样例数据：$GPVTG,309.62,T, ,M,0.13,N,0.2,K*6E */
	uint16_t TrackDegTrue;		/* 以真北为参考基准的地面航向（000~359度，前面的0也将被传输） */
	uint16_t TrackDegMag;		/* 以磁北为参考基准的地面航向（000~359度，前面的0也将被传输） */
	uint32_t SpeedKnots;		/* 地面速率（000.0~999.9节，前面的0也将被传输） */
	uint32_t SpeedKM;			/* 地面速率（000.0~999.9节，前面的0也将被传输） */

}GPS_T;

void bsp_InitGPS(void);
void gps_pro(void);
uint32_t gps_FenToDu(uint32_t _fen);
uint16_t gps_FenToMiao(uint32_t _fen);

extern GPS_T g_tGPS;

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
