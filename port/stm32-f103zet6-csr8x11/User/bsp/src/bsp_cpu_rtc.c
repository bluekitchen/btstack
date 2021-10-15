/*
*********************************************************************************************************
*
*	模块名称 : STM32内部RTC模块
*	文件名称 : bsp_cpu_rtc.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2015-08-08 armfly  首版.安富莱电子原创
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

#define RTC_Debug   /* 用于选择调试模式 */

/* 选择RTC的时钟源 */
#define RTC_CLOCK_SOURCE_LSE       /* LSE */
//#define RTC_CLOCK_SOURCE_LSI     /* LSI */ 

RTC_T g_tRTC;

/* 平年的每月天数表 */
const uint8_t mon_table[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
*********************************************************************************************************
*	函 数 名: bsp_InitRTC
*	功能说明: 初始化CPU内部RTC
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitRTC(void)
{
     uint16_t u16_WaitForOscSource;

	/*
		我们在BKP的后备寄存器1中，存了一个特殊字符0xA5A5, 第一次上电或后备电源掉电后，该寄存器数据丢失，
		表明RTC数据丢失，需要重新配置
	*/
    if (BKP_ReadBackupRegister(BKP_DR1) != 0xA5A5)
    {
		//重新配置RTC
		/* Enable PWR and BKP clocks */  /* PWR时钟（电源控制）与BKP时钟（RTC后备寄存器）使能 */  
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
		/* Allow access to BKP Domain */ /*使能RTC和后备寄存器访问 */  
		PWR_BackupAccessCmd(ENABLE);
		
		/* Reset Backup Domain */  /* 将外设BKP的全部寄存器重设为缺省值 */   
		BKP_DeInit();
		/* Enable LSE */		
		RCC_LSEConfig(RCC_LSE_ON);
		
		#if 0
		for(u16_WaitForOscSource=0; u16_WaitForOscSource < 5000; u16_WaitForOscSource++)
		{
			;
		}
		#endif
		
		/* Wait till LSE is ready */ /* 等待外部晶振震荡稳定输出 */  
		while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
		
		/* Select LSE as RTC Clock Source */ /*使用外部32.768KHz晶振作为RTC时钟 */ 
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
		/* 32.768KHz晶振预分频值是32767,如果对精度要求很高可以修改此分频值来校准晶振 */ 
		RTC_SetPrescaler(32767); /* RTC period = RTCCLK/RTC_PR = (32.768 KHz)/(32767+1) */
		
		/* Wait until last write operation on RTC registers has finished */
		RTC_WaitForLastTask();
		
		RTC_WriteClock(2015, 8, 8, 0, 0, 0);//默认时间
		
        /* 配置完成后，向后备寄存器中写特殊字符0xA5A5 */
        BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);
    }
    else
	{
		/* 若后备寄存器没有掉电，则无需重新配置RTC */
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
		
		for (u16_WaitForOscSource = 0; u16_WaitForOscSource < 5000; u16_WaitForOscSource++)
		{
			;
		}
        if (RCC_GetFlagStatus(RCC_FLAG_PORRST) != RESET)
        {
			/* 上电复位 */
        }
        else if (RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET)
        {
            /* 外部RST管脚复位 */
        }
        /* 清除RCC中复位标志 */
        RCC_ClearFlag();
        
		//虽然RTC模块不需要重新配置，且掉电后依靠后备电池依然运行
        //但是每次上电后，还是要使能RTCCLK???????
        //RCC_RTCCLKCmd(ENABLE);
        //等待RTC时钟与APB1时钟同步
        //RTC_WaitForSynchro();
        
        //使能秒中断
        //RTC_ITConfig(RTC_IT_SEC, ENABLE);
        
        //等待操作完成
        RTC_WaitForLastTask();
    }
	return;
}

/*
*********************************************************************************************************
*	函 数 名: Is_Leap_Year
*	功能说明: 判断是否为闰年
*	形    参：无
*	返 回 值: 1,是.0,不是
*********************************************************************************************************
*/
static uint8_t Is_Leap_Year(uint16_t _year)
{                     
	if (_year % 4 == 0) /* 必须能被4整除 */
	{ 
		if (_year % 100 == 0) 
		{ 
			if (_year % 400 == 0)
			{
				return 1;	/* 如果以00结尾,还要能被400整除 */
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
*	函 数 名: RTC_WriteClock
*	功能说明: 设置RTC时钟
*	形    参：无
*	返 回 值: 1表示成功 0表示错误
*********************************************************************************************************
*/
uint8_t RTC_WriteClock(uint16_t _year, uint8_t _mon, uint8_t _day, uint8_t _hour, uint8_t _min, uint8_t _sec)
{
	uint16_t t;
	uint32_t seccount=0;

	if (_year < 2000 || _year > 2099)
	{
		return 0;	/* _year范围1970-2099，此处设置范围为2000-2099 */   
	}		
	
	for (t = 1970; t < _year; t++) 	/* 把所有年份的秒钟相加 */
	{
		if (Is_Leap_Year(t))		/* 判断是否为闰年 */
		{
			seccount += 31622400;	/* 闰年的秒钟数 */
		}
		else
		{
			seccount += 31536000; 	/* 平年的秒钟数 */
		}
	}

	_mon -= 1;

	for (t = 0; t < _mon; t++)         /* 把前面月份的秒钟数相加 */
	{
		seccount += (uint32_t)mon_table[t] * 86400;	/* 月份秒钟数相加 */

		if (Is_Leap_Year(_year) && t == 1)
		{
			seccount += 86400;	/* 闰年2月份增加一天的秒钟数 */
		}			
	}

	seccount += (uint32_t)(_day - 1) * 86400;	/* 把前面日期的秒钟数相加 */

	seccount += (uint32_t)_hour * 3600;		/* 小时秒钟数 */

	seccount += (uint32_t)_min * 60;	/* 分钟秒钟数 */

	seccount += _sec;	/* 最后的秒钟加上去 */
																	
	PWR_BackupAccessCmd(ENABLE);	/* 必须要 */

	RTC_WaitForLastTask();

	RTC_SetCounter(seccount);

	RTC_WaitForLastTask();			/* 必须加 PWR_BackupAccessCmd(ENABLE); 不然进入死循环 */

	return 1;      
}

/*
*********************************************************************************************************
*	函 数 名: RTC_ReadClock
*	功能说明: 得到当前时钟。结果存放在 g_tRTC。
*	形    参：无
*	返 回 值: 1表示成功 0表示错误
*********************************************************************************************************
*/
void RTC_ReadClock(void)
{
	static uint16_t daycnt = 0;
	uint32_t timecount = 0; 
	uint32_t temp = 0;
	uint16_t temp1 = 0;

	timecount = RTC_GetCounter();     

	temp = timecount / 86400;   /* 得到天数 */

	if (daycnt != temp)	/* 超过一天了 */
	{       
		daycnt = temp;
		temp1 = 1970;  /* 从1970年开始 */

		while (temp >= 365)
		{                          
			if (Is_Leap_Year(temp1))	/* 是闰年 */
			{
				if (temp >= 366)
				{
					temp -= 366;		/* 闰年的秒钟数 */
				}
				else
				{
					//temp1++;		/* armfly: 这里闰年处理错误，不能加1 */
					break;
				}  
            }
			else 
			{
				temp -= 365;       /* 平年 */
			}
			temp1++;  
		}   
		g_tRTC.Year = temp1;	/* 得到年份 */
		
		temp1 = 0;
		while (temp >= 28)	/* 超过了一个月 */
		{
			if(Is_Leap_Year(g_tRTC.Year) && temp1 == 1)	/* 当年是不是闰年/2月份 */
			{
				if (temp >= 29)
				{
					temp -= 29;	/* 闰年的秒钟数 */
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
					temp -= mon_table[temp1];	/* 平年 */
				}
				else 
				{
					break;
				}
			}
			temp1++;  
		}
		g_tRTC.Mon = temp1 + 1;	/* 得到月份 */
		g_tRTC.Day = temp + 1;  /* 得到日期 */
	}

	temp = timecount % 86400;    /* 得到秒钟数 */

	g_tRTC.Hour = temp / 3600;	/* 小时 */

	g_tRTC.Min = (temp % 3600) / 60; /* 分钟 */

	g_tRTC.Sec = (temp % 3600) % 60; /* 秒钟 */

	g_tRTC.Week = RTC_CalcWeek(g_tRTC.Year, g_tRTC.Mon, g_tRTC.Day);	/* 计算星期 */
}   


/*
*********************************************************************************************************
*	函 数 名: bsp_CalcWeek
*	功能说明: 根据日期计算星期几
*	形    参: _year _mon _day  年月日  (年是2字节整数，月和日是字节整数）
*	返 回 值: 周几 （1-7） 7表示周日
*********************************************************************************************************
*/
uint8_t RTC_CalcWeek(uint16_t _year, uint8_t _mon, uint8_t _day)
{
	/*
	蔡勒（Zeller）公式
		历史上的某一天是星期几？未来的某一天是星期几？关于这个问题，有很多计算公式（两个通用计算公式和
	一些分段计算公式），其中最著名的是蔡勒（Zeller）公式。
	    即w=y+[y/4]+[c/4]-2c+[26(m+1)/10]+d-1

		公式中的符号含义如下，
	     w：星期；
	     c：年的高2位，即世纪-1
	     y：年（两位数）；
	     m：月（m大于等于3，小于等于14，即在蔡勒公式中，某年的1、2月要看作上一年的13、14月来计算，
	  	    比如2003年1月1日要看作2002年的13月1日来计算）；
	     d：日；
	     [ ]代表取整，即只要整数部分。

	    算出来的W除以7，余数是几就是星期几。如果余数是0，则为星期日。
        如果结果是负数，负数求余数则需要特殊处理：
            负数不能按习惯的余数的概念求余数，只能按数论中的余数的定义求余。为了方便
        计算，我们可以给它加上一个7的整数倍，使它变为一个正数，然后再求余数

		以2049年10月1日（100周年国庆）为例，用蔡勒（Zeller）公式进行计算，过程如下：
		蔡勒（Zeller）公式：w=y+[y/4]+[c/4]-2c+[26(m+1)/10]+d-1
		=49+[49/4]+[20/4]-2×20+[26× (10+1)/10]+1-1
		=49+[12.25]+5-40+[28.6]
		=49+12+5-40+28
		=54 (除以7余5)
		即2049年10月1日（100周年国庆）是星期5。
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
	else	/* 某年的1、2月要看作上一年的13、14月来计算 */
	{
		m = _mon + 12;
		y = (_year - 1) % 100;
		c = (_year - 1) / 100;
		d = _day;
	}

	w = y + y / 4 +  c / 4 - 2 * c + ((uint16_t)26*(m+1))/10 + d - 1;
	if (w == 0)
	{
		w = 7;	/* 表示周日 */
	}
	else if (w < 0)	/* 如果w是负数，则计算余数方式不同 */
	{
		w = 7 - (-w) % 7;
	}
	else
	{
		w = w % 7;
	}
	return w;
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
