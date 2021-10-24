/*
*********************************************************************************************************
*
*	模块名称 : 华为GPRS模块SIM800驱动程序
*	文件名称 : bsp_SIM800.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_SIM800_H
#define __BSP_SIM800_H

#define COM_SIM800	COM2		/* 选择串口 */

/* 定义下面这句话, 将把收到的字符发送到调试串口1 */
#define SIM800_TO_COM1_EN

/* 本模块部分函数用到了软件定时器最后1个ID。 因此主程序调用本模块的函数时，请注意回避定时器 TMR_COUNT - 1。
  bsp_StartTimer(3, _usTimeOut);
  
  TMR_COUNT 在 bsp_timer.h 文件定义
*/
#define SIM800_TMR_ID	(TMR_COUNT - 1)

#define AT_CR		'\r'
#define AT_LF		'\n'

/* MIC增益范围 */
#define	MIC_GAIN_MIN		0
#define	MIC_GAIN_MAX		15
#define	MIC_GAIN_MUTE		0
#define	MIC_GAIN_DEFAULT	0

/* 耳机音量范围 */
#define	EAR_VOL_MIN			0
#define	EAR_VOL_MAX			100
#define	EAR_VOL_DEFAULT		50
#define	MAIN_AUDIO_CHANNEL	0

/* AT+CREG? 命令应答中的网络状态定义 	当前网络注册状态  SIM800_GetNetStatus() */
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
Model: SIM800
Revision: 12.210.10.05.00
IMEI: 351869045435933
+GCAP: +CGSM

OK
*/
typedef struct
{
	char Manfacture[12];	/* 厂商 SIMCOM_Ltd */
	char Model[12];			/* 型号 SIM800 */
	char Revision[15 + 1];	/* 固件版本 R13.08 */
	//char IMEI[15 + 1];		/* IMEI 码 需要通过AT+GSN获得 */
}SIM800_INFO_T;

/* 供外部调用的函数声明 */
void bsp_InitSIM800(void);
void SIM800_Reset(void);
uint8_t SIM800_PowerOn(void);
void SIM800_PowerOff(void);
void SIM800_SendAT(char *_Cmd);
uint8_t SIM800_WaitResponse(char *_pAckStr, uint16_t _usTimeOut);
void SIM800_SetMicGain(uint16_t _Channel, uint16_t _iGain);
void SIM800_SetEarVolume(uint8_t _ucVolume);
void SIM800_PrintRxData(uint8_t _ch);
uint16_t SIM800_ReadResponse(char *_pBuf, uint16_t _usBufSize, uint16_t _usTimeOut);
uint8_t SIM800_GetHardInfo(SIM800_INFO_T *_pInfo);
uint8_t SIM800_GetNetStatus(void);
void SIM800_DialTel(char *_pTel);
void SIM800_Hangup(void);
void SIM800_AnswerIncall(void);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
