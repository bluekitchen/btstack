/*
*********************************************************************************************************
*
*	ģ������ : ��ΪGPRSģ��MG323��������
*	�ļ����� : bsp_mg323.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_MG323_H
#define __BSP_MG323_H

#define COM_MG323	COM2		/* ѡ�񴮿� */

/* ����������仰, �����յ����ַ����͵����Դ���1 */
#define MG323_TO_COM1_EN

/* ��ģ�鲿�ֺ����õ��������ʱ�����1��ID�� �����������ñ�ģ��ĺ���ʱ����ע��رܶ�ʱ�� TMR_COUNT - 1��
  bsp_StartTimer(3, _usTimeOut);
  
  TMR_COUNT �� bsp_timer.h �ļ�����
*/
#define MG323_TMR_ID	(TMR_COUNT - 1)

#define AT_CR		'\r'
#define AT_LF		'\n'

/* MIC���淶Χ */
#define	MIC_GAIN_MIN		-12
#define	MIC_GAIN_MAX		12
#define	MIC_GAIN_MUTE		13
#define	MIC_GAIN_DEFAULT	0

/* ����������Χ */
#define	EAR_VOL_MIN			0
#define	EAR_VOL_MAX			5
#define	EAR_VOL_DEFAULT		4

/* AT+CREG? ����Ӧ���е�����״̬���� 	��ǰ����ע��״̬  MG323_GetNetStatus() */
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
Model: MG323
Revision: 12.210.10.05.00
IMEI: 351869045435933
+GCAP: +CGSM

OK
*/
typedef struct
{
	char Manfacture[12];	/* ���� */
	char Model[12];			/* �ͺ� */
	char Revision[15 + 1];	/* �̼��汾 */
	char IMEI[15 + 1];		/* IMEI �� */
}MG_HARD_INFO_T;

/* ���ⲿ���õĺ������� */
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

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
