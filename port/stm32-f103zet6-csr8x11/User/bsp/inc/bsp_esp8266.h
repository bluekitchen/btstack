/*
*********************************************************************************************************
*
*	ģ������ : ESP8266 ����WIFIģ����������
*	�ļ����� : bsp_esp8266.c
*	��    �� : V1.3
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_ESP8266_H
#define __BSP_ESP8266_H

#define COM_ESP8266	COM2		/* ѡ�񴮿� */

/* ����������仰, �����յ����ַ����͵����Դ���1 */
#define ESP8266_TO_COM1_EN

/* ��ģ�鲿�ֺ����õ��������ʱ�����1��ID�� �����������ñ�ģ��ĺ���ʱ����ע��رܶ�ʱ�� TMR_COUNT - 1��
  bsp_StartTimer(3, _usTimeOut);

  TMR_COUNT �� bsp_timer.h �ļ�����
*/
#define ESP8266_TMR_ID	(TMR_COUNT - 1)

/* �����豸�ṹ�� */
typedef struct
{
	char ssid[33];	/* SSID��һ�����߾������磨WLAN�������ơ�SSID�����ִ�Сд���ı��ַ�������󳤶�32���ַ� */
	uint8_t ecn;	/* ���ܷ�ʽ 
						0   OPEN
						1   WEP
						2   WPA_PSK
						3   WPA2_PSK
						4   WPA_WPA2_PSK
					*/
	int32_t rssi;		/* �ź�ǿ�� */
	uint8_t mac[20];	/* MAC��ַ�ַ���*/
	uint8_t ch;			/* �ŵ� */
}WIFI_AP_T;

/* ���ܷ�ʽ */
enum
{
	ECN_OPEN = 0,
	ECN_WEP = 1,
	ECN_WPA_PSK = 2,
	ECN_WPA2_PSK = 3,
	ECN_WPA_WPA2_PSK = 4,
};

/* ���ⲿ���õĺ������� */
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

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
