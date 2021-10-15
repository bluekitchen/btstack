/*
*********************************************************************************************************
*
*	ģ������ : STM32�ڲ�RTCģ��
*	�ļ����� : bsp_cpu_rtc.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		v1.0    2015-08-08 armfly  �װ�.����������ԭ��
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

#define RTC_Debug   /* ����ѡ�����ģʽ */

/* ѡ��RTC��ʱ��Դ */
#define RTC_CLOCK_SOURCE_LSE       /* LSE */
//#define RTC_CLOCK_SOURCE_LSI     /* LSI */ 

RTC_T g_tRTC;

/* ƽ���ÿ�������� */
const uint8_t mon_table[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitRTC
*	����˵��: ��ʼ��CPU�ڲ�RTC
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitRTC(void)
{
     uint16_t u16_WaitForOscSource;

	/*
		������BKP�ĺ󱸼Ĵ���1�У�����һ�������ַ�0xA5A5, ��һ���ϵ��󱸵�Դ����󣬸üĴ������ݶ�ʧ��
		����RTC���ݶ�ʧ����Ҫ��������
	*/
    if (BKP_ReadBackupRegister(BKP_DR1) != 0xA5A5)
    {
		//��������RTC
		/* Enable PWR and BKP clocks */  /* PWRʱ�ӣ���Դ���ƣ���BKPʱ�ӣ�RTC�󱸼Ĵ�����ʹ�� */  
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
		/* Allow access to BKP Domain */ /*ʹ��RTC�ͺ󱸼Ĵ������� */  
		PWR_BackupAccessCmd(ENABLE);
		
		/* Reset Backup Domain */  /* ������BKP��ȫ���Ĵ�������Ϊȱʡֵ */   
		BKP_DeInit();
		/* Enable LSE */		
		RCC_LSEConfig(RCC_LSE_ON);
		
		#if 0
		for(u16_WaitForOscSource=0; u16_WaitForOscSource < 5000; u16_WaitForOscSource++)
		{
			;
		}
		#endif
		
		/* Wait till LSE is ready */ /* �ȴ��ⲿ�������ȶ���� */  
		while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
		
		/* Select LSE as RTC Clock Source */ /*ʹ���ⲿ32.768KHz������ΪRTCʱ�� */ 
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
		
		/* Enable RTC Clock */
		RCC_RTCCLKCmd(ENABLE);
		/* Wait for RTC registers synchronization */
		RTC_WaitForSynchro();
		/* Wait until last write operation on RTC registers has finished */
		RTC_WaitForLastTask();
		/* Enable the RTC Second */
		RTC_ITConfig(RTC_IT_SEC, ENABLE);
		/* Wait until last write operation on RTC registers has finished */
		RTC_WaitForLastTask();
		
	//	RTC_EnterConfigMode();
		
		/* Set RTC prescaler: set RTC period to 1sec */   
		/* 32.768KHz����Ԥ��Ƶֵ��32767,����Ծ���Ҫ��ܸ߿����޸Ĵ˷�Ƶֵ��У׼���� */ 
		RTC_SetPrescaler(32767); /* RTC period = RTCCLK/RTC_PR = (32.768 KHz)/(32767+1) */
		
		/* Wait until last write operation on RTC registers has finished */
		RTC_WaitForLastTask();
		
		RTC_WriteClock(2015, 8, 8, 0, 0, 0);//Ĭ��ʱ��
		
        /* ������ɺ���󱸼Ĵ�����д�����ַ�0xA5A5 */
        BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);
    }
    else
	{
		/* ���󱸼Ĵ���û�е��磬��������������RTC */
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
		
		for (u16_WaitForOscSource = 0; u16_WaitForOscSource < 5000; u16_WaitForOscSource++)
		{
			;
		}
        if (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
        {
			/* �ϵ縴λ */
        }
        else if (RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
        {
            /* �ⲿRST�ܽŸ�λ */
        }
        /* ���RCC�и�λ��־ */
        RCC_ClearFlag();
        
		//��ȻRTCģ�鲻��Ҫ�������ã��ҵ���������󱸵����Ȼ����
        //����ÿ���ϵ�󣬻���Ҫʹ��RTCCLK???????
        //RCC_RTCCLKCmd(ENABLE);
        //�ȴ�RTCʱ����APB1ʱ��ͬ��
        //RTC_WaitForSynchro();
        
        //ʹ�����ж�
        //RTC_ITConfig(RTC_IT_SEC, ENABLE);
        
        //�ȴ��������
        RTC_WaitForLastTask();
    }
	return;
}

/*
*********************************************************************************************************
*	�� �� ��: Is_Leap_Year
*	����˵��: �ж��Ƿ�Ϊ����
*	��    �Σ���
*	�� �� ֵ: 1,��.0,����
*********************************************************************************************************
*/
static uint8_t Is_Leap_Year(uint16_t _year)
{                     
	if (_year % 4 == 0) /* �����ܱ�4���� */
	{ 
		if (_year % 100 == 0) 
		{ 
			if (_year % 400 == 0)
			{
				return 1;	/* �����00��β,��Ҫ�ܱ�400���� */
			}
			else 
			{
				return 0;   
			}

		}
		else 
		{
			return 1;   
		}
	}
	else 
	{
		return 0; 
	}
}      

/*
*********************************************************************************************************
*	�� �� ��: RTC_WriteClock
*	����˵��: ����RTCʱ��
*	��    �Σ���
*	�� �� ֵ: 1��ʾ�ɹ� 0��ʾ����
*********************************************************************************************************
*/
uint8_t RTC_WriteClock(uint16_t _year, uint8_t _mon, uint8_t _day, uint8_t _hour, uint8_t _min, uint8_t _sec)
{
	uint16_t t;
	uint32_t seccount=0;

	if (_year < 2000 || _year > 2099)
	{
		return 0;	/* _year��Χ1970-2099���˴����÷�ΧΪ2000-2099 */   
	}		
	
	for (t = 1970; t < _year; t++) 	/* ��������ݵ�������� */
	{
		if (Is_Leap_Year(t))		/* �ж��Ƿ�Ϊ���� */
		{
			seccount += 31622400;	/* ����������� */
		}
		else
		{
			seccount += 31536000; 	/* ƽ��������� */
		}
	}

	_mon -= 1;

	for (t = 0; t < _mon; t++)         /* ��ǰ���·ݵ���������� */
	{
		seccount += (uint32_t)mon_table[t] * 86400;	/* �·���������� */

		if (Is_Leap_Year(_year) && t == 1)
		{
			seccount += 86400;	/* ����2�·�����һ��������� */
		}			
	}

	seccount += (uint32_t)(_day - 1) * 86400;	/* ��ǰ�����ڵ���������� */

	seccount += (uint32_t)_hour * 3600;		/* Сʱ������ */

	seccount += (uint32_t)_min * 60;	/* ���������� */

	seccount += _sec;	/* �������Ӽ���ȥ */
																	
	PWR_BackupAccessCmd(ENABLE);	/* ����Ҫ */

	RTC_WaitForLastTask();

	RTC_SetCounter(seccount);

	RTC_WaitForLastTask();			/* ����� PWR_BackupAccessCmd(ENABLE); ��Ȼ������ѭ�� */

	return 1;      
}

/*
*********************************************************************************************************
*	�� �� ��: RTC_ReadClock
*	����˵��: �õ���ǰʱ�ӡ��������� g_tRTC��
*	��    �Σ���
*	�� �� ֵ: 1��ʾ�ɹ� 0��ʾ����
*********************************************************************************************************
*/
void RTC_ReadClock(void)
{
	static uint16_t daycnt = 0;
	uint32_t timecount = 0; 
	uint32_t temp = 0;
	uint16_t temp1 = 0;

	timecount = RTC_GetCounter();     

	temp = timecount / 86400;   /* �õ����� */

	if (daycnt != temp)	/* ����һ���� */
	{       
		daycnt = temp;
		temp1 = 1970;  /* ��1970�꿪ʼ */

		while (temp >= 365)
		{                          
			if (Is_Leap_Year(temp1))	/* ������ */
			{
				if (temp >= 366)
				{
					temp -= 366;		/* ����������� */
				}
				else
				{
					//temp1++;		/* armfly: �������괦����󣬲��ܼ�1 */
					break;
				}  
            }
			else 
			{
				temp -= 365;       /* ƽ�� */
			}
			temp1++;  
		}   
		g_tRTC.Year = temp1;	/* �õ���� */
		
		temp1 = 0;
		while (temp >= 28)	/* ������һ���� */
		{
			if(Is_Leap_Year(g_tRTC.Year) && temp1 == 1)	/* �����ǲ�������/2�·� */
			{
				if (temp >= 29)
				{
					temp -= 29;	/* ����������� */
				}
				else
				{
					break; 
				}
            }
            else 
			{
				if (temp >= mon_table[temp1])
				{
					temp -= mon_table[temp1];	/* ƽ�� */
				}
				else 
				{
					break;
				}
			}
			temp1++;  
		}
		g_tRTC.Mon = temp1 + 1;	/* �õ��·� */
		g_tRTC.Day = temp + 1;  /* �õ����� */
	}

	temp = timecount % 86400;    /* �õ������� */

	g_tRTC.Hour = temp / 3600;	/* Сʱ */

	g_tRTC.Min = (temp % 3600) / 60; /* ���� */

	g_tRTC.Sec = (temp % 3600) % 60; /* ���� */

	g_tRTC.Week = RTC_CalcWeek(g_tRTC.Year, g_tRTC.Mon, g_tRTC.Day);	/* �������� */
}   


/*
*********************************************************************************************************
*	�� �� ��: bsp_CalcWeek
*	����˵��: �������ڼ������ڼ�
*	��    ��: _year _mon _day  ������  (����2�ֽ��������º������ֽ�������
*	�� �� ֵ: �ܼ� ��1-7�� 7��ʾ����
*********************************************************************************************************
*/
uint8_t RTC_CalcWeek(uint16_t _year, uint8_t _mon, uint8_t _day)
{
	/*
	���գ�Zeller����ʽ
		��ʷ�ϵ�ĳһ�������ڼ���δ����ĳһ�������ڼ�������������⣬�кܶ���㹫ʽ������ͨ�ü��㹫ʽ��
	һЩ�ֶμ��㹫ʽ�����������������ǲ��գ�Zeller����ʽ��
	    ��w=y+[y/4]+[c/4]-2c+[26(m+1)/10]+d-1

		��ʽ�еķ��ź������£�
	     w�����ڣ�
	     c����ĸ�2λ��������-1
	     y���꣨��λ������
	     m���£�m���ڵ���3��С�ڵ���14�����ڲ��չ�ʽ�У�ĳ���1��2��Ҫ������һ���13��14�������㣬
	  	    ����2003��1��1��Ҫ����2002���13��1�������㣩��
	     d���գ�
	     [ ]����ȡ������ֻҪ�������֡�

	    �������W����7�������Ǽ��������ڼ������������0����Ϊ�����ա�
        �������Ǹ�������������������Ҫ���⴦��
            �������ܰ�ϰ�ߵ������ĸ�����������ֻ�ܰ������е������Ķ������ࡣΪ�˷���
        ���㣬���ǿ��Ը�������һ��7����������ʹ����Ϊһ��������Ȼ����������

		��2049��10��1�գ�100������죩Ϊ�����ò��գ�Zeller����ʽ���м��㣬�������£�
		���գ�Zeller����ʽ��w=y+[y/4]+[c/4]-2c+[26(m+1)/10]+d-1
		=49+[49/4]+[20/4]-2��20+[26�� (10+1)/10]+1-1
		=49+[12.25]+5-40+[28.6]
		=49+12+5-40+28
		=54 (����7��5)
		��2049��10��1�գ�100������죩������5��
	*/
	uint8_t y, c, m, d;
	int16_t w;

	if (_mon >= 3)
	{
		m = _mon;
		y = _year % 100;
		c = _year / 100;
		d = _day;
	}
	else	/* ĳ���1��2��Ҫ������һ���13��14�������� */
	{
		m = _mon + 12;
		y = (_year - 1) % 100;
		c = (_year - 1) / 100;
		d = _day;
	}

	w = y + y / 4 +  c / 4 - 2 * c + ((uint16_t)26*(m+1))/10 + d - 1;
	if (w == 0)
	{
		w = 7;	/* ��ʾ���� */
	}
	else if (w < 0)	/* ���w�Ǹ����������������ʽ��ͬ */
	{
		w = 7 - (-w) % 7;
	}
	else
	{
		w = w % 7;
	}
	return w;
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
