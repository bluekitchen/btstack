/*
*********************************************************************************************************
*
*	模块名称 : ESP8266 串口WIFI模块驱动程序
*	文件名称 : bsp_esp8266.c
*	版    本 : V1.3
*	说    明 : 头文件
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_ESP8266_H
#define __BSP_ESP8266_H

#define COM_ESP8266	COM2		/* 选择串口 */

/* 定义下面这句话, 将把收到的字符发送到调试串口1 */
#define ESP8266_TO_COM1_EN

/* 本模块部分函数用到了软件定时器最后1个ID。 因此主程序调用本模块的函数时，请注意回避定时器 TMR_COUNT - 1。
  bsp_StartTimer(3, _usTimeOut);

  TMR_COUNT 在 bsp_timer.h 文件定义
*/
#define ESP8266_TMR_ID	(TMR_COUNT - 1)

/* 串口设备结构体 */
typedef struct
{
	char ssid[33];	/* SSID是一个无线局域网络（WLAN）的名称。SSID是区分大小写的文本字符串，最大长度32个字符 */
	uint8_t ecn;	/* 加密方式 
						0   OPEN
						1   WEP
						2   WPA_PSK
						3   WPA2_PSK
						4   WPA_WPA2_PSK
					*/
	int32_t rssi;		/* 信号强度 */
	uint8_t mac[20];	/* MAC地址字符串*/
	uint8_t ch;			/* 信道 */
}WIFI_AP_T;

/* 加密方式 */
enum
{
	ECN_OPEN = 0,
	ECN_WEP = 1,
	ECN_WPA_PSK = 2,
	ECN_WPA2_PSK = 3,
	ECN_WPA_WPA2_PSK = 4,
};

/* 供外部调用的函数声明 */
void bsp_InitESP8266(void);
void ESP8266_Reset(void);
void ESP8266_PowerOn(void);
void ESP8266_PowerOff(void);
void ESP8266_EnterISP(void);
void ESP8266_ExitISP(void);
void ESP8266_SendAT(char *_Cmd);

uint8_t ESP8266_WaitResponse(char *_pAckStr, uint16_t _usTimeOut);
void ESP8266_PrintRxData(uint8_t _ch);
void ESP8266_QuitAP(void);
int16_t ESP8266_ScanAP(WIFI_AP_T *_pList, uint16_t _MaxNum);
uint16_t ESP8266_RxNew(uint8_t *_pRxBuf);

uint8_t ESP8266_CreateTCPServer(uint16_t _TcpPort);
void ESP8266_SendTcpUdp(uint8_t *_databuf, uint16_t _len);
void ESP8266_CloseTcpUdp(uint8_t _id);
uint8_t ESP8266_GetLocalIP(char *_ip, char *_mac);

uint8_t ESP8266_JoinAP(char *_ssid, char *_pwd, uint16_t _timeout);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
