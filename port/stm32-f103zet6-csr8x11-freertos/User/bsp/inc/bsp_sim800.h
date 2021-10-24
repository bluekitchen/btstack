/*
*********************************************************************************************************
*
*	ģ������ : ��ΪGPRSģ��SIM800��������
*	�ļ����� : bsp_SIM800.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_SIM800_H
#define __BSP_SIM800_H

#define COM_SIM800	COM2		/* ѡ�񴮿� */

/* ����������仰, �����յ����ַ����͵����Դ���1 */
#define SIM800_TO_COM1_EN

/* ��ģ�鲿�ֺ����õ��������ʱ�����1��ID�� �����������ñ�ģ��ĺ���ʱ����ע��رܶ�ʱ�� TMR_COUNT - 1��
  bsp_StartTimer(3, _usTimeOut);
  
  TMR_COUNT �� bsp_timer.h �ļ�����
*/
#define SIM800_TMR_ID	(TMR_COUNT - 1)

#define AT_CR		'\r'
#define AT_LF		'\n'

/* MIC���淶Χ */
#define	MIC_GAIN_MIN		0
#define	MIC_GAIN_MAX		15
#define	MIC_GAIN_MUTE		0
#define	MIC_GAIN_DEFAULT	0

/* ����������Χ */
#define	EAR_VOL_MIN			0
#define	EAR_VOL_MAX			100
#define	EAR_VOL_DEFAULT		50
#define	MAIN_AUDIO_CHANNEL	0

/* AT+CREG? ����Ӧ���е�����״̬���� 	��ǰ����ע��״̬  SIM800_GetNetStatus() */
enum
{
	CREG_NO_REG = 0,  	/* 0��û��ע�ᣬME���ڲ�û������ѰҪע����µ���Ӫ�� */
	CREG_LOCAL_OK = 1,	/* 1��ע���˱������� */
	CREG_SEARCH = 2,	/* 2��û��ע�ᣬ��MS������ѰҪע����µ���Ӫ�� */
	CREG_REJECT = 3,	/* 3��ע�ᱻ�ܾ� */
	CREG_UNKNOW = 4,	/* 4��δ֪ԭ�� */
	CREG_REMOTE_OK = 5, /* 5��ע������������ */
};

/* ͨ�� ATI ָ����Բ�ѯģ���Ӳ����Ϣ 
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
	char Manfacture[12];	/* ���� SIMCOM_Ltd */
	char Model[12];			/* �ͺ� SIM800 */
	char Revision[15 + 1];	/* �̼��汾 R13.08 */
	//char IMEI[15 + 1];		/* IMEI �� ��Ҫͨ��AT+GSN��� */
}SIM800_INFO_T;

/* ���ⲿ���õĺ������� */
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

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
