/*
*********************************************************************************************************
*
*	ģ������ : ��ΪGPRSģ��MG323��������
*	�ļ����� : bsp_mg323.c
*	��    �� : V1.0
*	˵    �� : ��װMG323ģ����ص�AT����
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	������STM32-V4 ��������߷��䣺
	GPRS_TERM_ON   :  PB4/TRST/GPRS_TERM_ON
	GPRS_RESET     :  PC2/ADC123_IN12/GPRS_RESET
	
	GPRS_TX        :  PA2/USART2_TX
	GPRS_RX        :  PA3/USART2_RX

	GPRS_CTS       :  ������
	GPRS_RTS       :  ������

*/

/*
	AT+CIND=<mode>[,<mode>[,<mode>...]] ����ָʾ�¼��Ƿ��ϱ�
		+CIND: 5,99,1,0,1,0,0,0,4

	AT+CREG?  ��ѯ��ǰ����״̬

	AT+CSQ ��ѯ�ź���������

	AT+CIMI ��ѯSIM ����IMSI �š�

	AT+CIND? ��ȡ��ǰ��ָʾ״̬

	ATA ��������
	ATH �Ҷ���������

	AT^SWSPATH=<n>  �л���Ƶͨ��
*/

/* �����ڶ�Ӧ��RCCʱ�� */
#define RCC_TERM_ON 	RCC_APB2Periph_GPIOB
#define PORT_TERM_ON	GPIOB
#define PIN_TERM_ON		GPIO_Pin_4

#define RCC_RESET 		RCC_APB2Periph_GPIOC
#define PORT_RESET		GPIOC
#define PIN_RESET		GPIO_Pin_2

/* STM32��MG323��TERM_ON���ż���1��NPN�����ܣ������Ҫ���� */
#define TERM_ON_1()		GPIO_ResetBits(PORT_TERM_ON, PIN_TERM_ON);
#define TERM_ON_0()		GPIO_SetBits(PORT_TERM_ON, PIN_TERM_ON);

/* STM32��MG323��RESET���ż���1��NPN�����ܣ������Ҫ����. MG323��RESET���ǵ����帴λ */
#define MG_RESET_1()	GPIO_ResetBits(PORT_RESET, PIN_RESET);
#define MG_RESET_0()	GPIO_SetBits(PORT_RESET, PIN_RESET);

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitMG323
*	����˵��: ��������ģ����ص�GPIO,  �ú����� bsp_Init() ���á�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitMG323(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ��GPIOʱ�� */
	RCC_APB2PeriphClockCmd(RCC_TERM_ON | RCC_RESET, ENABLE);


	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* �������ģʽ */

	GPIO_InitStructure.GPIO_Pin = PIN_TERM_ON;
	GPIO_Init(PORT_TERM_ON, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_RESET;
	GPIO_Init(PORT_RESET, &GPIO_InitStructure);

	/* CPU�Ĵ��������Ѿ��� bsp_uart_fifo.c �е� bsp_InitUart() ���� */
	
	TERM_ON_0();
	MG_RESET_1();
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_PrintRxData
*	����˵��: ��ӡSTM32��MG323�յ������ݵ�COM1���ڣ���Ҫ���ڸ��ٵ���
*	��    ��: _ch : �յ�������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_PrintRxData(uint8_t _ch)
{
	#ifdef MG323_TO_COM1_EN
		comSendChar(COM1, _ch);		/* �����յ����ݴ�ӡ�����Դ���1 */
	#endif
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_PowerOn
*	����˵��: ��MG323ģ���ϵ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_PowerOn(void)
{
	comClearRxFifo(COM_MG323);	/* ���㴮�ڽ��ջ����� */
	
	MG323_Reset();

	/*
		����MG323�ֲᣬģ���ϵ���ӳ�250ms��Ȼ������ TERM_ON����Ϊ�͵�ƽ 750ms ֮������Ϊ�ߣ���ɿ���ʱ��
	*/
	TERM_ON_1();
	bsp_DelayMS(250);

	TERM_ON_0();
	bsp_DelayMS(750);
	TERM_ON_1();	

	/* �ȴ�ģ������ϵ磬�ж��Ƿ���յ� ^SYSSTART */
	MG323_WaitResponse("^SYSSTART", 5000);
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_PowerOn
*	����˵��: ��MG323ģ���ϵ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_Reset(void)
{
	/*
		����MG323�ֲᣬ
		RESET �ܽ�����ʵ��ģ��Ӳ����λ����ģ�����������������ʱ��ͨ������
		RESET �ܽ� �� 10 ms ��ģ�����Ӳ����λ��
	*/
	MG_RESET_0();
	bsp_DelayMS(20);
	MG_RESET_1();
	
	bsp_DelayMS(10);
}


/*
*********************************************************************************************************
*	�� �� ��: MG323_PowerOff
*	����˵��: ����MG323ģ��ػ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_PowerOff(void)
{
	/* Ӳ���ػ� */
	TERM_ON_0();

	/* Ҳ��������ػ� */
	//MG323_SendAT("AT^SMSO");
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_WaitResponse
*	����˵��: �ȴ�MG323����ָ����Ӧ���ַ���. ����ȴ� OK
*	��    ��: _pAckStr : Ӧ����ַ����� ���Ȳ��ó���255
*			 _usTimeOut : ����ִ�г�ʱ��0��ʾһֱ�ȴ�. >����ʾ��ʱʱ�䣬��λ1ms
*	�� �� ֵ: 1 ��ʾ�ɹ�  0 ��ʾʧ��
*********************************************************************************************************
*/
uint8_t MG323_WaitResponse(char *_pAckStr, uint16_t _usTimeOut)
{
	uint8_t ucData;
	uint8_t ucRxBuf[256];
	uint16_t pos = 0;
	uint32_t len;
	uint8_t ret;

	len = strlen(_pAckStr);
	if (len > 255)
	{
		return 0;
	}

	/* _usTimeOut == 0 ��ʾ���޵ȴ� */
	if (_usTimeOut > 0)
	{
		bsp_StartTimer(MG323_TMR_ID, _usTimeOut);		/* ʹ�������ʱ��3����Ϊ��ʱ���� */
	}
	while (1)
	{
		bsp_Idle();				/* CPU����ִ�еĲ����� �� bsp.c �� bsp.h �ļ� */

		if (_usTimeOut > 0)
		{
			if (bsp_CheckTimer(MG323_TMR_ID))
			{
				ret = 0;	/* ��ʱ */
				break;
			}
		}

		if (comGetChar(COM_MG323, &ucData))
		{
			MG323_PrintRxData(ucData);		/* �����յ����ݴ�ӡ�����Դ���1 */

			if (ucData == '\n')
			{
				if (pos > 0)	/* ��2���յ��س����� */
				{
					if (memcmp(ucRxBuf, _pAckStr,  len) == 0)
					{
						ret = 1;	/* �յ�ָ����Ӧ�����ݣ����سɹ� */
						break;
					}
					else
					{
						pos = 0;
					}
				}
				else
				{
					pos = 0;
				}
			}
			else
			{
				if (pos < sizeof(ucRxBuf))
				{
					/* ֻ����ɼ��ַ� */
					if (ucData >= ' ')
					{
						ucRxBuf[pos++] = ucData;
					}
				}
			}
		}
	}
	return ret;
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_ReadResponse
*	����˵��: ��ȡMG323����Ӧ���ַ������ú��������ַ��䳬ʱ�жϽ����� ��������Ҫ����AT����ͺ�����
*	��    ��: _pBuf : ���ģ�鷵�ص������ַ���
*			  _usBufSize : ��������󳤶�
*			 _usTimeOut : ����ִ�г�ʱ��0��ʾһֱ�ȴ�. >0 ��ʾ��ʱʱ�䣬��λ1ms
*	�� �� ֵ: 0 ��ʾ���󣨳�ʱ��  > 0 ��ʾӦ������ݳ���
*********************************************************************************************************
*/
uint16_t MG323_ReadResponse(char *_pBuf, uint16_t _usBufSize, uint16_t _usTimeOut)
{
	uint8_t ucData;
	uint16_t pos = 0;
	uint8_t ret;
	uint8_t status = 0;		/* ����״̬ */

	/* _usTimeOut == 0 ��ʾ���޵ȴ� */
	if (_usTimeOut > 0)
	{
		bsp_StartTimer(MG323_TMR_ID, _usTimeOut);		/* ʹ�������ʱ����Ϊ��ʱ���� */
	}
	while (1)
	{
		bsp_Idle();				/* CPU����ִ�еĲ����� �� bsp.c �� bsp.h �ļ� */

		if (status == 2)		/* ���ڽ�����ЧӦ��׶Σ�ͨ���ַ��䳬ʱ�ж����ݽ������ */
		{
			if (bsp_CheckTimer(MG323_TMR_ID))
			{
				_pBuf[pos]	 = 0;	/* ��β��0�� ���ں���������ʶ���ַ������� */
				ret = pos;		/* �ɹ��� �������ݳ��� */
				break;
			}
		}
		else
		{
			if (_usTimeOut > 0)
			{
				if (bsp_CheckTimer(MG323_TMR_ID))
				{
					ret = 0;	/* ��ʱ */
					break;
				}
			}
		}
		
		if (comGetChar(COM_MG323, &ucData))
		{			
			MG323_PrintRxData(ucData);		/* �����յ����ݴ�ӡ�����Դ���1 */

			switch (status)
			{
				case 0:			/* ���ַ� */
					if (ucData == AT_CR)		/* ������ַ��ǻس�����ʾ AT������� */
					{
						_pBuf[pos++] = ucData;		/* ������յ������� */
						status = 2;	 /* ��Ϊ�յ�ģ��Ӧ���� */
					}
					else	/* ���ַ��� A ��ʾ AT������� */
					{
						status = 1;	 /* �����������͵�AT�����ַ�����������Ӧ�����ݣ�ֱ������ CR�ַ� */
					}
					break;
					
				case 1:			/* AT������Խ׶�, ����������. �����ȴ� */
					if (ucData == AT_CR)
					{
						status = 2;
					}
					break;
					
				case 2:			/* ��ʼ����ģ��Ӧ���� */
					/* ֻҪ�յ�ģ���Ӧ���ַ���������ַ��䳬ʱ�жϽ�������ʱ�����ܳ�ʱ�������� */
					bsp_StartTimer(MG323_TMR_ID, 5);
					if (pos < _usBufSize - 1)
					{
						_pBuf[pos++] = ucData;		/* ������յ������� */
					}
					break;
			}
		}
	}
	return ret;
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_SendAT
*	����˵��: ��GSMģ�鷢��AT��� �������Զ���AT�ַ���������<CR>�ַ�
*	��    ��: _Str : AT�����ַ�����������ĩβ�Ļس�<CR>. ���ַ�0����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_SendAT(char *_Cmd)
{
	comSendBuf(COM_MG323, (uint8_t *)_Cmd, strlen(_Cmd));
	comSendBuf(COM_MG323, "\r", 1);
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_SetAutoReport
*	����˵��: �����¼��Զ��ϱ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_SetAutoReport(void)
{
	MG323_SendAT("AT+CIND=1,1,1,1,1,1,1,1,1");
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_SwitchPath
*	����˵��: �л���Ƶͨ��
*	��    ��: ch �� 0��ʾ��1·��Ƶ��INTMIC, INTEAR)  1��ʾ��2·��Ƶ��EXTMIC,EXTEAR)
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_SwitchPath(uint8_t ch)
{
	if (ch == 0)
	{
		MG323_SendAT("AT^SWSPATH=0");
	}
	else
	{
		MG323_SendAT("AT^SWSPATH=1");
	}
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_SetEarVolume
*	����˵��: ���ö�������
*	��    ��: _ucVolume : ������ 0 ��ʾ������1-5 ��ʾ������С��5���4��ȱʡ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_SetEarVolume(uint8_t _ucVolume)
{
	char CmdBuf[32];

	sprintf(CmdBuf, "AT+CLVL=%d", _ucVolume);
	MG323_SendAT(CmdBuf);
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_SetMicGain
*	����˵��: ����MIC ���� .    ���ú����·ͨ���������ã���ֻ�����м���绰ǰʹ�á�
*	��    ��: _iGain : ���档 -12 ��С��12���13��ʾ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_SetMicGain(int16_t _iGain)
{
	char CmdBuf[32];

	sprintf(CmdBuf, "AT+CMIC=%d", _iGain);
	MG323_SendAT(CmdBuf);
}


/*
*********************************************************************************************************
*	�� �� ��: MG323_DialTel
*	����˵��: ����绰
*	��    ��: _pTel �绰�����ַ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_DialTel(char *_pTel)
{
	char CmdBuf[64];

	sprintf(CmdBuf, "ATD%s;", _pTel);
	MG323_SendAT(CmdBuf);
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_Hangup
*	����˵��: �Ҷϵ绰
*	��    ��: _pTel �绰�����ַ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MG323_Hangup(void)
{
	MG323_SendAT("ATH");
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_GetHardInfo
*	����˵��: ��ȡģ���Ӳ����Ϣ. ���� ATI ����Ӧ������
*	��    ��: ��Ž���Ľṹ��ָ��
*	�� �� ֵ: 1 ��ʾ�ɹ��� 0 ��ʾʧ��
*********************************************************************************************************
*/
uint8_t MG323_GetHardInfo(MG_HARD_INFO_T *_pInfo)
{
	/*
		ATI
		Manufacture: HUAWEI
		Model: MG323
		Revision: 12.210.10.05.00
		IMEI: 351869045435933
		+GCAP: +CGSM
		
		OK	
	*/	
	char buf[255];
	uint16_t len, i, begin = 0, num;
	uint8_t status = 0;	
	
	comClearRxFifo(COM_MG323);	/* ���㴮�ڽ��ջ����� */	
	
	MG323_SendAT("ATI");		/* ���� ATI ���� */
	
	len = MG323_ReadResponse(buf, sizeof(buf), 300);	/* ��ʱ 300ms */
	if (len == 0)
	{
		return 0;		
	}
	
	_pInfo->Manfacture[0] = 0;
	_pInfo->Model[0] = 0;
	_pInfo->Revision[0] = 0;
	_pInfo->IMEI[0] = 0;
	
	for (i = 2; i < len; i++)
	{
		if (buf[i] == ':')
		{
			i += 2;
			begin = i;
		}
		if (buf[i] == AT_CR)
		{
			num = i - begin;
			
			if (status == 0)
			{
				if (num >= sizeof(_pInfo->Manfacture))
				{
					num = sizeof(_pInfo->Manfacture) - 1;
				}
				memcpy(_pInfo->Manfacture, &buf[begin], num);
				_pInfo->Manfacture[num] = 0;
			}
			else if (status == 1)
			{
				if (num >= sizeof(_pInfo->Model))
				{
					num = sizeof(_pInfo->Model) - 1;
				}				
				memcpy(_pInfo->Model, &buf[begin], num);
				_pInfo->Model[num] = 0;
			}
			else if (status == 2)
			{
				if (num >= sizeof(_pInfo->Revision))
				{
					num = sizeof(_pInfo->Revision) - 1;
				}				
				memcpy(_pInfo->Revision, &buf[begin], num);
				_pInfo->Revision[num] = 0;
			}
			else if (status == 3)
			{
				if (num >= sizeof(_pInfo->IMEI))
				{
					num = sizeof(_pInfo->IMEI) - 1;
				}					
				memcpy(_pInfo->IMEI, &buf[begin], num);
				_pInfo->IMEI[num] = 0;
				break;
			}
			status++;	
		}
	}
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: MG323_GetNetStatus
*	����˵��: ��ѯ��ǰ����״̬
*	��    ��: ��
*	�� �� ֵ: ����״̬, CREG_NO_REG, CREG_LOCAL_OK �ȡ�
*********************************************************************************************************
*/
uint8_t MG323_GetNetStatus(void)
{
	/*
		AT+CREG?
		+CREG: 0,1
		
		OK				
	*/	
	char buf[128];
	uint16_t len, i;
	uint8_t status = 0;
	
	comClearRxFifo(COM_MG323);	/* ���㴮�ڽ��ջ����� */	
	
	MG323_SendAT("AT+CREG?");	/* ���� AT ���� */
	
	len = MG323_ReadResponse(buf, sizeof(buf), 200);	/* ��ʱ 200ms */
	if (len == 0)
	{
		return 0;		
	}
	
	for (i = 2; i < len; i++)
	{
		if (buf[i] == ',')
		{
			i++;
			
			status = buf[i] - '0';
		}
	}
	return status;
}


/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
