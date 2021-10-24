/*
*********************************************************************************************************
*
*	模块名称 : 华为GPRS模块MG323驱动程序
*	文件名称 : bsp_mg323.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_MG323_H
#define __BSP_MG323_H

#define COM_MG323	COM2		/* 选择串口 */

/* 定义下面这句话, 将把收到的字符发送到调试串口1 */
#define MG323_TO_COM1_EN

/* 本模块部分函数用到了软件定时器最后1个ID。 因此主程序调用本模块的函数时，请注意回避定时器 TMR_COUNT - 1。
  bsp_StartTimer(3, _usTimeOut);
  
  TMR_COUNT 在 bsp_timer.h 文件定义
*/
#define MG323_TMR_ID	(TMR_COUNT - 1)

#define AT_CR		'\r'
#define AT_LF		'\n'

/* MIC增益范围 */
#define	MIC_GAIN_MIN		-12
#define	MIC_GAIN_MAX		12
#define	MIC_GAIN_MUTE		13
#define	MIC_GAIN_DEFAULT	0

/* 耳机音量范围 */
#define	EAR_VOL_MIN			0
#define	EAR_VOL_MAX			5
#define	EAR_VOL_DEFAULT		4

/* AT+CREG? 命令应答中的网络状态定义 	当前网络注册状态  MG323_GetNetStatus() */
enum
{
	CREG_NO_REG = 0,  	/* 0：没有注册，ME现在并没有在搜寻要注册的新的运营商 */
	CREG_LOCAL_OK = 1,	/* 1：注册了本地网络 */
	CREG_SEARCH = 2,	/* 2：没有注册，但MS正在搜寻要注册的新的运营商 */
	CREG_REJECT = 3,	/* 3：注册被拒绝 */
	CREG_UNKNOW = 4,	/* 4：未知原因 */
	CREG_REMOTE_OK = 5, /* 5：注册了漫游网络 */
};

/* 通过 ATI 指令，可以查询模块的硬件信息 
ATI
Manufacture: HUAWEI
Model: MG323
Revision: 12.210.10.05.00
IMEI: 351869045435933
+GCAP: +CGSM

OK
*/
typedef struct
{
	char Manfacture[12];	/* 厂商 */
	char Model[12];			/* 型号 */
	char Revision[15 + 1];	/* 固件版本 */
	char IMEI[15 + 1];		/* IMEI 码 */
}MG_HARD_INFO_T;

/* 供外部调用的函数声明 */
void bsp_InitMG323(void);
void MG323_Reset(void);
void MG323_PowerOn(void);
void MG323_PowerOff(void);
void MG323_SendAT(char *_Cmd);
void MG323_SetAutoReport(void);

uint8_t MG323_WaitResponse(char *_pAckStr, uint16_t _usTimeOut);

void MG323_SwitchPath(uint8_t ch);
void MG323_SetMicGain(int16_t _iGain);
void MG323_SetEarVolume(uint8_t _ucVolume);

void MG323_PrintRxData(uint8_t _ch);

uint16_t MG323_ReadResponse(char *_pBuf, uint16_t _usBufSize, uint16_t _usTimeOut);

uint8_t MG323_GetHardInfo(MG_HARD_INFO_T *_pInfo);
uint8_t MG323_GetNetStatus(void);
void MG323_DialTel(char *_pTel);
void MG323_Hangup(void);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
