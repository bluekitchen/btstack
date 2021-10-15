/*
*********************************************************************************************************
*
*	ģ������ : RA8875оƬ����ģ��
*	�ļ����� : LCD_RA8875.c
*	��    �� : V2.9
*	˵    �� : RA8875�ײ�������������
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		V1.0    2012-08-07 armfly  �����װ�
*		V1.1    2012-10-23 armfly  ������ƾ��κ�����BUG���߶ȿ�ȼ�����󣩣�����SPI-4�ӿ�ģʽ��
*								   �����������2Dͼ�εĺ�����æ��������ʵ�����⡣
*								   ����У׼����4��У׼
*		V1.2    2012-11-07 armfly  ���4��У׼��BUG
*		V1.3    2012-12-12 armfly  ���7���������߳�����������
*		V1.4    2013-05-04 armfly  ���ӽ����ͼģʽ�ĺ�������������ͷֱ��д�� RA8875_StartDirectDraw
*									RA8875_GetDispMemAddr, RA8875_QuitDirectDraw
*       V1.5    2013-06-12 armfly  Ϊ����IAR�澯��ȥ�� s_WinX��s_WinY, s_WinHeight,s_WinWidthǰ��__IO����
*		V1.6    2013-06-26 armfly  ���Ӻ���  RA8875_CtrlGPO() ����KOUT0-KOUT3 ���ߵ�״̬
*		V1.7    2013-06-26 armfly  ���Ӻ���  RA8875_CtrlGPO() ����͹���
*		V1.8    2013-06-29 armfly  �޸� RA8875_GetPixel() ����BUG����Ҫ��2�Ρ���������0x40�Ĵ���Ϊ����겻����
*		V1.9	2013-07-24 armfly  �޸� RA8875_DrawLine() ����, ����ʼ�����ֹ����ͬʱ����������.�������⴦��
*		V2.0	2013-09-22 armfly  ��� RA8875Ӳ��2D��ͼ��BUG���޸����ɺ��������������ͻ���⡣
*		V2.1    2013-09-26 armfly  ��MCU�ӿ���صĺ������뵽 bsp_ra8875_port.c �ļ���
*		V2.2    2014-01-31 armfly  ���� RA8875_DispBmpInFlash() ����,��ʾRA8875��Ҵ���Flash�е�BMPλͼ
*		V2.3    2014-06-27 armfly  �޸� RA8875_DispBmpInFlash() , ���Ӷ� W25Q128 (16MB) ��֧�֡�
*								   ��ʼ��7�����ķ�֧�����У�����RA8875����Ƶ��75M������ȶ��ԡ�
*		V2.4    2014-07-05 armfly  ������Ļ��ת�;���ĺ���
*		V2.5 	2014-09-06 armfly  
*					(1) RA8875_FillRect() ���������Ⱥ͸߶�Ϊ0�����.
*					(2) RA8875_WriteReg(); RA8875_ReadReg()����ȡ��static���ƣ��ɹ�����ģ��ʹ��
*					(3) ���Ӻ����������л�����. g_LcdDirection �������淽�����0-3��
*					(4) SPI ģʽ��RA8875_DispStr() ���� while(RA8875_ReadBusy()); 8080ģʽ���������ַ�ʽ
*					(5) bsp_touch.c ����Ҫͬ�����ķ�����param.c�в����ṹ�����ӷ��������
*					(6) ��� RA8875_DispBmpInFlash(), s_ucRA8875Busy ��־�����ͻ�����⡣
*		V2.6 	2014-10-21 armfly  
*					(1) g_LcdDirection �����Ƶ� bsp_tft_lcd.c ����Ϊ�����3.5��������
*		V2.7	2014-11-09
*					(1) �Ż� RA8875_PutPixel����
*		V2.8	2015-04-02 armfly  ��� RA8875_DrawBMP()����ȷ�����⣬��ͼ����Ĵ�������ȷ��
*		V2.9	2015-04-15 armfly  RA8875_InitHard() ���7����ˢͼ������⡣
*					  (ĳ�����εĴ���FLASH�ٶȵ�), ����PLL�ٶȣ��� 81.25M�޸�Ϊ75MHz��
*		V3.0	2015-04-21 armfly  �޸� RA8875_SetDirection(uint8_t _ucDir)��reg_40H��Ϊ01 ���emwin�µ���ʾ���⡣
*
*	Copyright (C), 2011-2012, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

#include "bsp_ra8875_port.h"

#define FLASH_DUAL_SPEED_EN			/* ˫��ģʽ�� MOSI��MISOͬʱ��ȡ�ֿ��ͼ���е�����. */

#define WRTIE_SF_EN					/* ��ʾʹ��д����FLASH���ܡ���ʼ��ʱ��������MCU��SPI�ӿ��Լ�PWM���� */

static __IO uint16_t s_usTouchX, s_usTouchY;	/* ������ADCֵ */
static __IO uint8_t s_ucRA8875Busy = 0;			/* æ��־, ����ģ��ͨ�� RA8875_IsBusy()�����ж�æ */
static __IO uint8_t s_reg_40H;		/* 0X40�Ĵ�����ֵ */
static __IO uint8_t s_reg_22H;		/* 0X22�Ĵ�����ֵ */

/* ���浱ǰ��ʾ���ڵ�λ�úʹ�С���⼸�������� RA8875_SetDispWin() �������� */
static uint16_t s_WinX = 0;
static uint16_t s_WinY = 0;
static uint16_t s_WinHeight = 272;
static uint16_t s_WinWidth = 480;

static void RA8875_SetCursor(uint16_t _usX, uint16_t _usY);
static void RA8875_SetReadCursor(uint16_t _usX, uint16_t _usY);
static void RA8875_SetTextCursor(uint16_t _usX, uint16_t _usY);

static void BTE_SetTarBlock(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint8_t _ucLayer);
static void BTE_SetOperateCode(uint8_t _ucOperate);
static void BTE_Start(void);

/*
*********************************************************************************************************
*	�� �� ��: RA8875_ReadID
*	����˵��: ��ȡLCD����оƬID
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint16_t RA8875_ReadID(void)
{
	RA8875_ConfigGPIO();
	
	return RA8875_ReadReg(0x00);
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_WriteReg
*	����˵��: дRA8875�Ĵ���. RA8875�ļĴ�����ַ�����ݶ���8bit��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_WriteReg(uint8_t _ucRegAddr, uint8_t _ucRegValue)
{
	s_ucRA8875Busy = 1;				/* ����æ��־�����ⴥ���ж��еĳ�����ʳ�ͻ */

	RA8875_WriteCmd(_ucRegAddr);
	RA8875_WriteData(_ucRegValue);

	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_WriteReg
*	����˵��: ��RA8875�Ĵ���. ���������� RA8875_ReadReg_Int(�� �����ж��е���
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint8_t RA8875_ReadReg(uint8_t _ucRegAddr)
{
	uint8_t value;
	
	s_ucRA8875Busy = 1;
	
	RA8875_WriteCmd(_ucRegAddr);
	value = RA8875_ReadData();
	
	s_ucRA8875Busy = 0;
	return value;	
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_ReadReg_Int
*	����˵��: ��RA8875�Ĵ���. �����жϺ����е��á�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint8_t RA8875_ReadReg_Int(uint8_t _ucRegAddr)
{
	RA8875_WriteCmd(_ucRegAddr);
	return RA8875_ReadData();
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_InitHard
*	����˵��: ��ʼ��RA8875�����������������⣬������Ӧ���ڳ�ʼ����Ϻ��ٵ��� RA8875_SetBackLight()����������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_InitHard(void)
{
	uint8_t ucGPIX;
	
	RA8875_ConfigGPIO();	/* ����GPIO FSMC�� */

	/* ��ȡ RA8875 оƬ��GPIX���ŵĵ�ƽ״̬��1��ʾ4.3������0��ʾ7����
	����ע�����ǰ���������Ϊ�˱���ͳһ�������̣���LCDģ��������Ӳ����ʶ��������Ʒʱ�������Զ�ʶ��
	*/
	ucGPIX = RA8875_ReadReg(0xC7);

	if (ucGPIX == 1)	/* 	GPIX = 1 ��ʾ 4.3 ���� 480x272 */
	{
		/* ��ʼ��PLL.  ����Ƶ��Ϊ25M */
		RA8875_WriteCmd(0x88);
		RA8875_Delaly1us();		/* �ӳ�1us */
		RA8875_WriteData(10);	/* PLLDIVM [7] = 0 ;  PLLDIVN [4:0] = 10 */

	    RA8875_Delaly1ms();

		RA8875_WriteCmd(0x89);
		RA8875_Delaly1us();		/* �ӳ�1us */
		RA8875_WriteData(2);		/* PLLDIVK[2:0] = 2, ����4 */

		/* RA8875 ���ڲ�ϵͳƵ�� (SYS_CLK) �ǽ���񵴵�·��PLL ��·��������Ƶ�ʼ��㹫ʽ���� :
		  SYS_CLK = FIN * ( PLLDIVN [4:0] +1 ) / ((PLLDIVM+1 ) * ( 2^PLLDIVK [2:0] ))
		          = 25M * (10 + 1) / ((0 + 1) * (2 ^ 2))
				  = 68.75MHz
		*/

		/* REG[88h]��REG[89h]���趨��Ϊ��֤PLL ����ȶ�����ȴ�һ�Ρ���Ƶʱ�䡹(< 100us)��*/
	    RA8875_Delaly1ms();
		RA8875_Delaly1ms();

		/*
			����ϵͳ���ƼĴ����� ����pdf ��18ҳ:

			bit3:2 ɫ������趨 (Color Depth Setting)
				00b : 8-bpp ��ͨ��TFT �ӿڣ� i.e. 256 ɫ��
				1xb : 16-bpp ��ͨ��TFT �ӿڣ� i.e. 65K ɫ��	 ��ѡ�����

			bit1:0 MCUIF ѡ��
				00b : 8-λMCU �ӿڡ�
				1xb : 16-λMCU �ӿڡ� ��ѡ�����
		*/
		if (g_RA8875_IF == RA_HARD_8080_16)
		{
			RA8875_WriteReg(0x10, (1 <<3 ) | (1 << 1));	/* ����16λMCU���ڣ�65Kɫ */
		}
		else
		{
			RA8875_WriteReg(0x10, (1 <<3 ) | (0 << 1));	/* SPI�ӿ�ʱ������8λ��65Kɫ */
		}

		/* REG[04h] Pixel Clock Setting Register   PCSR
			bit7  PCLK Inversion
				0 : PDAT ����PCLK ��Ե���� (Rising Edge) ʱ��ȡ����
				1 : PDAT ����PCLK ��Ե�½� (Falling Edge) ʱ��ȡ����
			bit1:0 PCLK Ƶ�������趨
				Pixel Clock  PCLK Ƶ�������趨��
				00b: PCLK Ƶ������= ϵͳƵ�����ڡ�
				01b: PCLK Ƶ������= 2 ����ϵͳƵ�����ڡ�
				10b: PCLK Ƶ������= 4 ����ϵͳƵ�����ڡ�
				11b: PCLK Ƶ������= 8 ����ϵͳƵ�����ڡ�
		*/
	    RA8875_WriteReg(0x04, 0x82);    /* ����PCLK���� */
	    RA8875_Delaly1ms();

	    //Horizontal set
	    //HDWR//Horizontal Display Width Setting Bit[6:0]
	 	//Horizontal display width(pixels) = (HDWR + 1)*8
	    RA8875_WriteReg(0x14, 0x3B);
	    RA8875_WriteReg(0x15, 0x00);

	    //HNDR//Horizontal Non-Display Period Bit[4:0]
	    //Horizontal Non-Display Period (pixels) = (HNDR + 1)*8
		RA8875_WriteReg(0x16, 0x01);

	    //HSTR//HSYNC Start Position[4:0]
	    //HSYNC Start Position(PCLK) = (HSTR + 1)*8
		RA8875_WriteReg(0x17, 0x00);

	    //HPWR//HSYNC Polarity ,The period width of HSYNC.
	    //HSYNC Width [4:0]   HSYNC Pulse width(PCLK) = (HPWR + 1)*8
		RA8875_WriteReg(0x18, 0x05);

	    //Vertical set
	    //VDHR0 //Vertical Display Height Bit [7:0]
	    //Vertical pixels = VDHR + 1
		RA8875_WriteReg(0x19, 0x0F);

	    //VDHR1 //Vertical Display Height Bit [8]
	    //Vertical pixels = VDHR + 1
		RA8875_WriteReg(0x1A, 0x01);

	    //VNDR0 //Vertical Non-Display Period Bit [7:0]
	    //Vertical Non-Display area = (VNDR + 1)
		RA8875_WriteReg(0x1B, 0x02);

	    //VNDR1 //Vertical Non-Display Period Bit [8]
	    //Vertical Non-Display area = (VNDR + 1)
		RA8875_WriteReg(0x1C, 0x00);

	    //VSTR0 //VSYNC Start Position[7:0]
	    //VSYNC Start Position(PCLK) = (VSTR + 1)
		RA8875_WriteReg(0x1D, 0x07);

	    //VSTR1 //VSYNC Start Position[8]
	    //VSYNC Start Position(PCLK) = (VSTR + 1)
		RA8875_WriteReg(0x1E, 0x00);

	    //VPWR //VSYNC Polarity ,VSYNC Pulse Width[6:0]
	    //VSYNC Pulse Width(PCLK) = (VPWR + 1)
	    RA8875_WriteReg(0x1F, 0x09);


	    g_LcdHeight = LCD_43_HEIGHT;
		g_LcdWidth = LCD_43_WIDTH;
	}
	else	/* GPIX = 0 ��ʾ7����(800x480) */
	{
	    g_LcdHeight = LCD_70_HEIGHT;
		g_LcdWidth = LCD_70_WIDTH;

		#if 1	/* 75MHz */
			RA8875_WriteCmd(0x88);
			RA8875_Delaly1us();		/* �ӳ�1us */
			RA8875_WriteData(11);	/* PLLDIVM [7] = 0 ;  PLLDIVN [4:0] = 11 */

			RA8875_Delaly1ms();

			RA8875_WriteCmd(0x89);
			RA8875_Delaly1us();		/* �ӳ�1us */
			RA8875_WriteData(2);		/* PLLDIVK[2:0] = 2, ����4 */
		#else	/* 81.25M */
			/* ��ʼ��PLL.  ����Ƶ��Ϊ25M */
			RA8875_WriteCmd(0x88);
			RA8875_Delaly1us();		/* �ӳ�1us */
			RA8875_WriteData(12);	/* PLLDIVM [7] = 0 ;  PLLDIVN [4:0] = 12 */

			RA8875_Delaly1ms();

			RA8875_WriteCmd(0x89);
			RA8875_Delaly1us();		/* �ӳ�1us */
			RA8875_WriteData(2);	/* PLLDIVK[2:0] = 2, ����4 */

			/* RA8875 ���ڲ�ϵͳƵ�� (SYS_CLK) �ǽ���񵴵�·��PLL ��·��������Ƶ�ʼ��㹫ʽ���� :
			  SYS_CLK = FIN * ( PLLDIVN [4:0] +1 ) / ((PLLDIVM+1 ) * ( 2^PLLDIVK [2:0] ))
					  = 25M * (12 + 1) / ((0 + 1) * (2 ^ 2))
					  = 81.25MHz
			*/
		#endif
		

		/* REG[88h]��REG[89h]���趨��Ϊ��֤PLL ����ȶ�����ȴ�һ�Ρ���Ƶʱ�䡹(< 100us)��*/
	    RA8875_Delaly1ms();

		/*
			����ϵͳ���ƼĴ����� ����pdf ��18ҳ:

			bit3:2 ɫ������趨 (Color Depth Setting)
				00b : 8-bpp ��ͨ��TFT �ӿڣ� i.e. 256 ɫ��
				1xb : 16-bpp ��ͨ��TFT �ӿڣ� i.e. 65K ɫ��	 ��ѡ�����

			bit1:0 MCUIF ѡ��
				00b : 8-λMCU �ӿڡ�
				1xb : 16-λMCU �ӿڡ� ��ѡ�����
		*/
		if (g_RA8875_IF == RA_HARD_8080_16)
		{
			RA8875_WriteReg(0x10, (1 <<3 ) | (1 << 1));	/* ����16λMCU���ڣ�65Kɫ */
		}
		else
		{
			RA8875_WriteReg(0x10, (1 <<3 ) | (0 << 1));	/* SPI�ӿ�ʱ������8λ��65Kɫ */
		}

		/* REG[04h] Pixel Clock Setting Register (PCSR)
			bit7  PCLK Inversion
				0 : PDAT ����PCLK ��Ե���� (Rising Edge) ʱ��ȡ����
				1 : PDAT ����PCLK ��Ե�½� (Falling Edge) ʱ��ȡ����
			bit1:0 PCLK Ƶ�������趨
				Pixel Clock ,PCLK Ƶ�������趨��
				00b: PCLK Ƶ������= ϵͳƵ�����ڡ�
				01b: PCLK Ƶ������= 2 ����ϵͳƵ�����ڡ�
				10b: PCLK Ƶ������= 4 ����ϵͳƵ�����ڡ�
				11b: PCLK Ƶ������= 8 ����ϵͳƵ�����ڡ�
		*/
	    RA8875_WriteReg(0x04, 0x81);
	    RA8875_Delaly1ms();

	#if 1
		/* OTD9960 & OTA7001 ���� */
		RA8875_WriteReg(0x14, 0x63);
		RA8875_WriteReg(0x15, 0x00);
		RA8875_WriteReg(0x16, 0x03);
		RA8875_WriteReg(0x17, 0x03);
		RA8875_WriteReg(0x18, 0x0B);
		RA8875_WriteReg(0x19, 0xDF);
		RA8875_WriteReg(0x1A, 0x01);
		RA8875_WriteReg(0x1B, 0x1F);
		RA8875_WriteReg(0x1C, 0x00);
		RA8875_WriteReg(0x1D, 0x16);
		RA8875_WriteReg(0x1E, 0x00);
		RA8875_WriteReg(0x1F, 0x01);

	#else	/* AT070TN92  setting */
	    //Horizontal set
	    //HDWR//Horizontal Display Width Setting Bit[6:0]
	 	//Horizontal display width(pixels) = (HDWR + 1)*8
	    RA8875_WriteReg(0x14, 0x4F);
	    RA8875_WriteReg(0x15, 0x05);

	    //HNDR//Horizontal Non-Display Period Bit[4:0]
	    //Horizontal Non-Display Period (pixels) = (HNDR + 1)*8
		RA8875_WriteReg(0x16, 0x0F);

	    //HSTR//HSYNC Start Position[4:0]
	    //HSYNC Start Position(PCLK) = (HSTR + 1)*8
		RA8875_WriteReg(0x17, 0x01);

	    //HPWR//HSYNC Polarity ,The period width of HSYNC.
	    //HSYNC Width [4:0]   HSYNC Pulse width(PCLK) = (HPWR + 1)*8
		RA8875_WriteReg(0x18, 0x00);

	    //Vertical set
	    //VDHR0 //Vertical Display Height Bit [7:0]
	    //Vertical pixels = VDHR + 1
		RA8875_WriteReg(0x19, 0xDF);

	    //VDHR1 //Vertical Display Height Bit [8]
	    //Vertical pixels = VDHR + 1
		RA8875_WriteReg(0x1A, 0x01);

	    //VNDR0 //Vertical Non-Display Period Bit [7:0]
	    //Vertical Non-Display area = (VNDR + 1)
		RA8875_WriteReg(0x1B, 0x0A);

	    //VNDR1 //Vertical Non-Display Period Bit [8]
	    //Vertical Non-Display area = (VNDR + 1)
		RA8875_WriteReg(0x1C, 0x00);

	    //VSTR0 //VSYNC Start Position[7:0]
	    //VSYNC Start Position(PCLK) = (VSTR + 1)
		RA8875_WriteReg(0x1D, 0x0E);

	    //VSTR1 //VSYNC Start Position[8]
	    //VSYNC Start Position(PCLK) = (VSTR + 1)
		RA8875_WriteReg(0x1E, 0x00);

	    //VPWR //VSYNC Polarity ,VSYNC Pulse Width[6:0]
	    //VSYNC Pulse Width(PCLK) = (VPWR + 1)
	    RA8875_WriteReg(0x1F, 0x01);
	#endif
	}

	/* ����TFT���� DISP  ����Ϊ�ߣ�ʹ�����. ������TFTģ���DISP�������ӵ�RA8875оƬ��GP0X�� */
	RA8875_WriteReg(0xC7, 0x01);	/* DISP = 1 */

	/* LCD ��ʾ/�ر�Ѷ�� (LCD Display on) */
	RA8875_WriteReg(0x01, 0x80);

	/* 	REG[40h] Memory Write Control Register 0 (MWCR0)

		Bit 7	��ʾģʽ�趨
			0 : ��ͼģʽ��
			1 : ����ģʽ��

		Bit 6	����д����/�ڴ�д�����趨
			0 : �趨����/�ڴ�д����Ϊ����ʾ��
			1 : �趨����/�ڴ�д����Ϊ��ʾ��

		Bit 5	����д����/�ڴ�д������˸�趨
			0 : �α겻��˸��
			1 : �α���˸��

		Bit 4   NA

		Bit 3-2  ��ͼģʽʱ���ڴ�д�뷽��
			00b : �� -> �ң�Ȼ���� -> �¡�
			01b : �� -> ��Ȼ���� -> �¡�
			10b : �� -> �£�Ȼ���� -> �ҡ�
			11b : �� -> �ϣ�Ȼ���� -> �ҡ�

		Bit 1 	�ڴ�д�����Զ����ӹ����趨
			0 : ���ڴ�д��ʱ���λ���Զ���һ��
			1 : ���ڴ�д��ʱ���λ�ò����Զ���һ��

		Bit 0 �ڴ��ȡ����Զ����ӹ����趨
			0 : ���ڴ��ȡʱ���λ���Զ���һ��
			1 : ���ڴ��ȡʱ���λ�ò����Զ���һ��
	*/
	//RA8875_WriteReg(0x40, 0x00);	/* ѡ���ͼģʽ */	
	RA8875_SetDirection(0);

	/* 	REG[41h] Memory Write Control Register1 (MWCR1)
		д��Ŀ��λ�ã�ѡ��ͼ��1
	*/
	RA8875_WriteReg(0x41, 0x00);	/* ѡ���ͼģʽ, Ŀ��ΪCGRAM */

	RA8875_SetDispWin(0, 0, g_LcdHeight, g_LcdWidth);

	if ((g_RA8875_IF == RA_HARD_SPI) || (g_RA8875_IF == RA_SOFT_SPI))
	{
		RA8875_HighSpeedSPI();		/* ������Ϻ��л�SPI������ģʽ */
	}
	else
	{
		#ifdef WRTIE_SF_EN		/* ʹ�����ֿ�ͼƬоƬд���ܣ�����CPU��SPI�ӿں�PWM-GPIO */
			bsp_InitRA8875Flash();		
		#endif	
	}
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_SetDirection
*	����˵��: ������ʾ����
*	��    ��:  _ucDir : ��ʾ������� 0 ��������, 1=����180�ȷ�ת, 2=����, 3=����180�ȷ�ת
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_SetDirection(uint8_t _ucDir)
{
	uint8_t reg20H = 0;
	
	s_reg_40H = 0x01;
	
	if (_ucDir == 0)
	{
		s_reg_40H = 0x01;
		reg20H = (0 << 3) | (0 << 2);
	}
	else if (_ucDir == 1)
	{
		s_reg_40H = 0x01;	/* 2015-04-21 ��05�޸�Ϊ 01. ���emwin�µ����� */
		reg20H = (1 << 3) | (1 << 2);
	}
	else if (_ucDir == 2)
	{
		s_reg_40H = 0x09;
		reg20H = (0 << 3) | (1 << 2);
	}
	else if (_ucDir == 3)
	{
		s_reg_40H = 0x09;
		reg20H = (1 << 3) | (0 << 2);
	}
	RA8875_WriteReg(0x40, s_reg_40H);	

	/* 
		REG[20h] Display Configuration Register (DPCR) 
		Bit  ˵  ��  ��ʼֵ  Access
		Bit7 
			ͼ���趨(Layer Control) 
			0 : ��ͼ�㡣
			1 : ˫ͼ�㡣
		Bit3  HDIR ˮƽɨ�跽���趨(n = SEG number) 
			0 : ��SEG0 ��SEG(n-1)��
			1 : ��SEG(n-1) ��SEG0��
		Bit2 VDIR ��ֱɨ�跽���趨(n = COM number) 
			0 : ��COM0 ��COM(n-1)��
			1 : ��COM(n-1) ��COM0��
	*/
	RA8875_WriteReg(0x20, reg20H);	

	if (_ucDir > 1)	/* ����  */
	{
		uint16_t temp;
		
		if (g_LcdHeight < g_LcdWidth)
		{
			temp = g_LcdHeight;
			g_LcdHeight = g_LcdWidth;
			g_LcdWidth = temp;
		}
	}		
	else	/* ���� */
	{
		uint16_t temp;
		
		if (g_LcdHeight > g_LcdWidth)
		{
			temp = g_LcdHeight;
			g_LcdHeight = g_LcdWidth;
			g_LcdWidth = temp;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: BTE_SetTarBlock
*	����˵��: ����RA8875 BTEĿ�������Լ�Ŀ��ͼ��
*	��    ��:
*			uint16_t _usX : ˮƽ�������
*			uint16_t _usY : ��ֱ�������
*			uint16_t _usHeight : ����߶�
*			uint16_t _usWidth : ������
*			uint8_t _ucLayer : 0 ͼ��1�� 1 ͼ��2
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void BTE_SetTarBlock(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint8_t _ucLayer)
{
	/* ����������� */
	RA8875_WriteReg(0x58, _usX);
	RA8875_WriteReg(0x59, _usX >> 8);

	RA8875_WriteReg(0x5A, _usY);
	if (_ucLayer == 0)	/* ͼ��2 */
	{
		RA8875_WriteReg(0x5B, _usY >> 8);
	}
	else
	{
		RA8875_WriteReg(0x5B, (1 << 7) | (_usY >> 8));	/* Bit7 ��ʾͼ�㣬 0 ͼ��1�� 1 ͼ��2*/
	}

	/* ���������� */
	RA8875_WriteReg(0x5C, _usWidth);
	RA8875_WriteReg(0x5D, _usWidth >> 8);

	/* ��������߶� */
	RA8875_WriteReg(0x5E, _usHeight);
	RA8875_WriteReg(0x5F, _usHeight >> 8);
}

/*
*********************************************************************************************************
*	�� �� ��: BTE_SetOperateCode
*	����˵��: �趨BTE ������͹�դ������
*	��    ��: _ucOperate : ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void BTE_SetOperateCode(uint8_t _ucOperate)
{
	/*  �趨BTE ������͹�դ������  */
	RA8875_WriteReg(0x51, _ucOperate);
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_SetFrontColor
*	����˵��: �趨ǰ��ɫ
*	��    ��: ��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_SetFrontColor(uint16_t _usColor)
{
	RA8875_WriteReg(0x63, (_usColor & 0xF800) >> 11);	/* R5  */
	RA8875_WriteReg(0x64, (_usColor & 0x07E0) >> 5);	/* G6 */
	RA8875_WriteReg(0x65, (_usColor & 0x001F));			/* B5 */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_SetBackColor
*	����˵��: �趨����ɫ
*	��    ��: ��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_SetBackColor(uint16_t _usColor)
{
	RA8875_WriteReg(0x60, (_usColor & 0xF800) >> 11);	/* R5  */
	RA8875_WriteReg(0x61, (_usColor & 0x07E0) >> 5);	/* G6 */
	RA8875_WriteReg(0x62, (_usColor & 0x001F));			/* B5 */
}


/*
*********************************************************************************************************
*	�� �� ��: BTE_Start
*	����˵��: ����BTE����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void BTE_Start(void)
{
	s_ucRA8875Busy = 1;
	/* RA8875_WriteReg(0x50, 0x80);  ����ʹ�������������Ϊ�ڲ��Ѿ������� s_ucRA8875Busy ��־ */
	RA8875_WriteCmd(0x50);	/* ���üĴ�����ַ */
	RA8875_WriteData(0x80);	/* д��Ĵ���ֵ */
}

/*
*********************************************************************************************************
*	�� �� ��: BTE_Wait
*	����˵��: �ȴ�BTE��������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void BTE_Wait(void)
{
	uint32_t i;
	
	//while ((RA8875_ReadStatus() & 0x40) == 0x40);
	for (i = 0; i < 100000; i++)
	{
		if ((RA8875_ReadStatus() & 0x40) == 0)
		{
			break;
		}
	}
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_IsBusy
*	����˵��: RA8875�Ƿ�æ
*	��    ��: ��
*	�� �� ֵ: 1 ��ʾæ�� 0 ��ʾ����
*********************************************************************************************************
*/
uint8_t RA8875_IsBusy(void)
{
	if (s_ucRA8875Busy == 0)
	{
		return 0;
	}
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_WaitBusy
*	����˵��: �ȴ�RA8875����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_WaitBusy(void)
{
	while ((RA8875_ReadStatus() & 0x80) == 0x80);
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_Layer1Visable
*	����˵��: RA8875 ͼ��1��ʾ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_Layer1Visable(void)
{
	/* 0x52 �Ĵ����� Bit2:0
		ͼ����ʾģʽ
		000b : ֻ��ͼ��1 ��ʾ��
		001b : ֻ��ͼ��2 ��ʾ��
		010b : ��ʾͼ��1 ��ͼ��2 �Ľ���/����ģʽ��
		011b : ��ʾͼ��1 ��ͼ��2 ��ͨ͸ģʽ��
		100b : Boolean OR��
		101b : Boolean AND��
		110b : ��������ģʽ (Floating window mode)��
		111b :������
	*/
	RA8875_WriteReg(0x52, RA8875_ReadReg(0x52) & 0xF8);	/* ֻ��ͼ��1 ��ʾ */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_Layer2Visable
*	����˵��: ֻ��ʾͼ��2
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_Layer2Visable(void)
{
	/* 0x52 �Ĵ����� Bit2:0
		ͼ����ʾģʽ
		000b : ֻ��ͼ��1 ��ʾ��
		001b : ֻ��ͼ��2 ��ʾ��
		010b : ��ʾͼ��1 ��ͼ��2 �Ľ���/����ģʽ��
		011b : ��ʾͼ��1 ��ͼ��2 ��ͨ͸ģʽ��
		100b : Boolean OR��
		101b : Boolean AND��
		110b : ��������ģʽ (Floating window mode)��
		111b :������
	*/
	RA8875_WriteReg(0x52, (RA8875_ReadReg(0x52) & 0xF8) | 0x01);	/* ֻ��ͼ��2 ��ʾ */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_TouchInit
*	����˵��: ��ʼ��������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_TouchInit(void)
{
	RA8875_WriteReg(0xF0, (1 << 2));	/* ������������ж�λ */

	/*
		REG[71h] Touch Panel Control Register 1 (TPCR1)

		Bit7 N/A
		Bit6 �������ģʽ�趨
			0 : �Զ�ģʽ��
			1 : �ֶ�ģʽ��
		Bit5 ������������ADC �ο���ѹ(Vref)��Դ�趨
			0 : �ڲ������ο���ѹ��
			1 : �ⲿ����ο���ѹ��ADC �ο���ѹ׼λ= 1/2 VDD��
		Bit4-3 N/A
		Bit2 �����ж�Ѷ�ŵ�����������·ѡ��
			0: �ر�����������·��
			1: ��������������·��
		Bit1-0 ��������ֶ�ģʽ֮ѡ��λ
			00b : ����ģʽ�����ؿ��Ƶ�Ԫ��������ģʽ��
			01b : ��ⴥ���¼��������ڴ�ģʽ����������ⴥ���¼��ķ�
				�����¼��������������жϻ����ɻ�������֪(REG[F1h]
				Bit2)��
			10b : X ������ߢȡģʽ���ڴ�ģʽ����λ�õ�X �����ݻᱻ��
				���� REG[72h] ��REG[74h]��
			11b : Y ������ߢȡģʽ���ڴ�ģʽ����λ�õ�Y �����ݻᱻ��
				����REG[73h] and REG[74h]��
	*/
	RA8875_WriteReg(0x71, (0 << 6) | (0 << 5) | (1 << 2));	/* ѡ���Զ�ģʽ */

	/*
		REG[70h] Touch Panel Control Register 0 (TPCR0)

		Bit7 ������幦���趨
			0 : �رա�
			1 : ������
		Bit6-4 ������������ȡ��ʱ���趨
			000b : ADC ȡ��ʱ��Ϊ512 ��ϵͳƵ�����ڡ�
			001b : ADC ȡ��ʱ��Ϊ 1024 ��ϵͳƵ�����ڡ�
			010b : ADC ȡ��ʱ��Ϊ 2048 ��ϵͳƵ�����ڡ�
			011b : ADC ȡ��ʱ��Ϊ 4096 ��ϵͳƵ�����ڡ�
			100b : ADC ȡ��ʱ��Ϊ 8192 ��ϵͳƵ�����ڡ�
			101b : ADC ȡ��ʱ��Ϊ 16384 ��ϵͳƵ�����ڡ�
			110b : ADC ȡ��ʱ��Ϊ 32768 ��ϵͳƵ�����ڡ�
			111b : ADC ȡ��ʱ��Ϊ65536 ��ϵͳƵ�����ڡ�
		Bit3 ������廽��ģʽ
			0 : �رմ����¼�����ģʽ��
			1 : �����¼��ɻ���˯��ģʽ��
		Bit2-0 ������������ADC Ƶ���趨
			000b : ϵͳƵ�ʡ�
			001b : ϵͳƵ�� / 2��
			010b : ϵͳƵ�� / 4��
			011b : ϵͳƵ�� / 8��
			100b : ϵͳƵ�� / 16��
			101b : ϵͳƵ�� / 32��
			110b : ϵͳƵ�� / 64��
			111b : ϵͳƵ�� / 128��

		ע : ADC ������Ƶ���趨���ܳ���10MHz��
	*/
	RA8875_WriteReg(0x70, (1 << 7) | (3 << 4) | (0 << 3) | (2 << 0));	/* �����������ܣ� */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_TouchReadX
*	����˵��: ��ȡXλ��ADC   �������������ʱ�жϷ��������ã���Ҫ����Ĵ������ʳ�ͻ )
*	��    ��: ��
*	�� �� ֵ: ADCֵ��10Bit
*********************************************************************************************************
*/
uint16_t RA8875_TouchReadX(void)
{
	uint16_t usAdc;
	uint8_t ucRegValue;
	uint8_t ucReg74;

	/* �����ȡ�ж��¼���־ */
	ucRegValue = RA8875_ReadReg_Int(0xF1);
	if (ucRegValue & (1 << 2))
	{
		ucReg74 = RA8875_ReadReg_Int(0x74);
		usAdc = RA8875_ReadReg_Int(0x72);	/* Bit9-2 */
		usAdc <<= 2;
		usAdc += (ucReg74 & 0x03);

		s_usTouchX = usAdc;

		usAdc = RA8875_ReadReg_Int(0x73);	/* Bit9-2 */
		usAdc <<= 2;
		usAdc += ((ucReg74 & 0x0C) >> 2);

		s_usTouchY = usAdc;
	}
	else
	{
		s_usTouchX = 0;
		s_usTouchY = 0;
	}

	/*
		bit2 д�빦��?? ��������ж����λ
		0 : δ������
		1 : �����������жϡ�
		��ȡ����?? ��������ж�״̬
		0 : δ������������жϡ�
		1 : ������������жϡ�
	*/

	/*
		��Ҫ�����������д�Ĵ�������Ϊ�ú�����д��busy��־
		RA8875_WriteReg(0xF1, (1 << 2));	 ��������� �Ż��´β���.
	*/
	RA8875_WriteCmd(0xF1);
	RA8875_WriteData(1 << 2);

	return s_usTouchX;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_TouchReadY
*	����˵��: ��ȡYλ��ADC �� �����ȵ��� RA8875_TouchReadX()
*	��    ��: ��
*	�� �� ֵ: ADCֵ��10Bit
*********************************************************************************************************
*/
uint16_t RA8875_TouchReadY(void)
{
	return s_usTouchY;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_DispOn
*	����˵��: ����ʾ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_DispOn(void)
{
	RA8875_WriteReg(0x01, 0x80);
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_DispOff
*	����˵��: �ر���ʾ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_DispOff(void)
{
	RA8875_WriteReg(0x01, 0x00);
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_Sleep
*	����˵��: RA8875����˯��ģʽ��ͬʱ�ر���ʾ����
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_Sleep(void)
{
	RA8875_SetBackLight(0);		/* �رձ��� */

	/* ����TFT���� DISP  ����Ϊ�ߣ�ʹ�����. ������TFTģ���DISP�������ӵ�RA8875оƬ��GP0X�� */
	RA8875_WriteReg(0xC7, 0x00);	/* DISP = 0  */

	RA8875_WriteReg(0x01, 0x01);
	RA8875_WriteReg(0x01, 0x00);

	RA8875_WriteReg(0x01, 0x02);	/* RA8875 Sleep */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_PutPixel
*	����˵��: ��1������
*	��    ��:
*			_usX,_usY : ��������
*			_usColor  :������ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor)
{	
	if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080Ӳ������16bit */
	{
		// �Ż� RA8875_SetCursor(_usX, _usY);
		s_ucRA8875Busy = 1;
		{	
			if (g_LcdDirection > 1)	/* ����  */
			{
				RA8875_REG = 0x46; RA8875_RAM = _usY;
				RA8875_REG = 0x47; RA8875_RAM = _usY >> 8;
				RA8875_REG = 0x48; RA8875_RAM = _usX;
				RA8875_REG = 0x49; RA8875_RAM = _usX >> 8;
			}
			else	/* ���� */
			{
				RA8875_REG = 0x46; RA8875_RAM = _usX;
				RA8875_REG = 0x47; RA8875_RAM = _usX >> 8;
				RA8875_REG = 0x48; RA8875_RAM = _usY;
				RA8875_REG = 0x49; RA8875_RAM = _usY >> 8;	
			}
		}
		
		// �Ż� RA8875_WriteCmd(0x02); 		/* �����趨RA8875 �����ڴ�(DDRAM��CGRAM)��ȡ/д��ģʽ */
		// �Ż� RA8875_WriteData16(_usColor);
		
		RA8875_REG = 0x02;
		RA8875_RAM = _usColor;

		s_ucRA8875Busy = 0;
	}
	else	/* SPI�ӿڷ�֧ */
	{
		RA8875_SetCursor(_usX, _usY);
		s_ucRA8875Busy = 1;

		RA8875_WriteCmd(0x02); 		/* �����趨RA8875 �����ڴ�(DDRAM��CGRAM)��ȡ/д��ģʽ */
		RA8875_WriteData16(_usColor);

		s_ucRA8875Busy = 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_GetPixel
*	����˵��: ��ȡ1������
*	��    ��:
*			_usX,_usY : ��������
*			_usColor  :������ɫ
*	�� �� ֵ: RGB��ɫֵ
*********************************************************************************************************
*/
uint16_t RA8875_GetPixel(uint16_t _usX, uint16_t _usY)
{
	uint16_t usRGB;

//	RA8875_WriteReg(0x40, (1 << 0));	/* ����Ϊ��ͼģʽ����ȡ��겻�Զ���1 */

	RA8875_SetReadCursor(_usX, _usY);	/* ���ö�ȡ���λ�� */

	s_ucRA8875Busy = 1;

	RA8875_WriteCmd(0x02);
	usRGB = RA8875_ReadData16();	/* ��1�ζ�ȡ���ݶ��� */
	usRGB = RA8875_ReadData16();
	usRGB = RA8875_ReadData16();

	s_ucRA8875Busy = 0;

	return usRGB;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_ClrScr
*	����˵��: �����������ɫֵ����.RA8875֧��Ӳ����ɫ��䡣�ú������Ե�ǰ�������ʾ���ڽ�������. ��ʾ
*			 ���ڵ�λ�úʹ�С�� RA8875_SetDispWin() ������������
*	��    ��:  _usColor : ����ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_ClrScr(uint16_t _usColor)
{
	/* Ҳ����ͨ��0x30-0x37�Ĵ�����û�õ�ǰ�������ʾ���� */

	/* ��ɫ��������, ����pdf ��162ҳ
	�˹���ʹ���ڽ�ѡ���ض�������������ǉ\�����ĳ��ǰ��ɫ��R8875 ����ĵ�ɫ�趨ΪBTE ǰ��ɫ��

	��������:
		1. �趨Ŀ��ͼ���λ�� REG[58h], [59h], [5Ah], [5Bh]
		2. �趨BTE ��Ⱥ͸߶� REG[5Ch], [5Dh], [5Eh], [5Fh]
		3. �趨BTE ������͹�դ������  REG[51h] Bit[3:0] = 0Ch
		4. �趨ǰ��ɫ  REG[63h], [64h], [65h]
		5. ����BTE ����  REG[50h] Bit7 = 1
		6. ���״̬������ STSR Bit6��ȷ��BTE �Ƿ����
	*/
	BTE_SetTarBlock(s_WinX, s_WinY, s_WinHeight, s_WinWidth, 0);	/* ����BTEλ�úͿ�ȸ߶��Լ�Ŀ��ͼ�㣨0��1�� */
	BTE_SetOperateCode(0x0C);		/* �趨BTE ������͹�դ������  REG[51h] Bit[3:0] = 0Ch */
	RA8875_SetFrontColor(_usColor);	/* ����BTEǰ��ɫ */
	BTE_Start();					/* ����BTE ���� */
	BTE_Wait();						/* �ȴ��������� */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_DispBmpInFlash
*	����˵��: ��ʾRA8875��Ҵ���Flash�е�BMPλͼ.
*			    λͼ��ʽ:������ ���ϵ���ɨ��, ÿ������2�ֽ�, RGB = 565 ��ʽ, ��λ��ǰ.
*	��    ��:
*			_usX, _usY : ͼƬ������
*			_usHeight  :ͼƬ�߶�
*			_usWidth   :ͼƬ���
*			_uiFlashAddr       :����Flash��ַ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_DispBmpInFlash(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth,
	uint32_t _uiFlashAddr)
{
	/* pdf 179ҳ 7-10-1 �����ڴ�ֱ�Ӵ�ȡģʽ
		1. �趨�������ڷ�Χ (REG[30h] ~REG[37h])���ڴ�д��λ�� (REG[46h] ~REG[49h])
		2. �趨Serial Flash/ROM ��̬ (REG[05h])
		3. �趨 �ڴ�ֱ�Ӵ�ȡ������Դ��ʼλ�� (REG[B0h] ~REG[B2h])
		4. �趨 �ڴ�ֱ�Ӵ�ȡ������ (REG[B4h] ��REG[B5h])
		5. �趨 �ڴ�ֱ�Ӵ�ȡ����߶� (REG[B6h] �� REG[B7h])
		6. �趨�ڴ�ֱ�Ӵ�ȡ��ԴͼƬ��� (REG[B8h] �� REG[B9h])
		7. �����ڴ�ֱ�Ӵ�ȡΪ�������ģʽ (REG[BFh] bit 1)
		8. �����ڴ�ֱ�Ӵ�ȡ��ʼѶ���Ҽ���ڴ�ֱ�Ӵ�ȡæµѶ�� (REG[BFh] bit 0)
	*/
	RA8875_SetDispWin(_usX, _usY, _usHeight, _usWidth);	/* REG[30h] ~REG[37h] �� REG[46h] ~REG[49h] */

	/* 05H  REG[05h] Serial Flash/ROM Configuration Register (SROC)
		7	Serial Flash/ROM I/F # ѡ��
				0:ѡ��Serial Flash/ROM 0 �ӿڡ�[������RA8875������ͼ��оƬ���� 1 �ӿ�]
				1:ѡ��Serial Flash/ROM 1 �ӿڡ�
		6	Serial Flash/ROM Ѱַģʽ
				0: 24 λѰַģʽ��
				��λ������Ϊ0��
		5	Serial Flash/ROM ����ģʽ
				0: ����ģʽ 0��
				1: ����ģʽ 3��
		4-3	Serial Flash /ROM ��ȡ���� (Read Cycle)
			00b: 4 bus ?? �޿����� (No Dummy Cycle)��
			01b: 5 bus ??1 byte �����ڡ�
			1Xb: 6 bus ??2 byte �����ڡ�
		2	Serial Flash /ROM ��ȡģʽ (Access Mode)
			0: ����ģʽ ��
			1: DMA ģʽ��
		1-0	Serial Flash /ROM I/F Data Latch ѡ��ģʽ
			0Xb: ��һģʽ��
			10b: ˫��ģʽ0��
			11b: ˫��ģʽ1��
	*/
	
	/* ������������һƬ W25Q128��ǰ��2MB���ֿ⡣����14MB��ͼƬ */
	if (g_tW25.ChipID == W25Q128)
	{
		_uiFlashAddr += PIC_OFFSET;	/* ǰ��2MB������ֿ� */	
		#ifdef FLASH_DUAL_SPEED_EN			/* ˫��ģʽ�� MOSI��MISOͬʱ��ȡ���� */
			RA8875_WriteReg(0x05, (0 << 7) | (0 << 6) | (1 << 5) | (0 << 3) | (1 << 2) | (2 << 0));
		#else				/* ��ͨģʽ���� MISO ��ȡ���� */
			RA8875_WriteReg(0x05, (0 << 7) | (0 << 6) | (1 << 5) | (0 << 3) | (1 << 2) | (0 << 0));
		#endif		
	}	
	else	/* ��ǿ������1ƬW25Q64(8MB)���ֿ⣬1ƬW25Q64(8MB)��ͼ�� */
	{
		#ifdef FLASH_DUAL_SPEED_EN			/* ˫��ģʽ�� MOSI��MISOͬʱ��ȡ���� */
			RA8875_WriteReg(0x05, (1 << 7) | (0 << 6) | (1 << 5) | (0 << 3) | (1 << 2) | (2 << 0));
		#else				/* ��ͨģʽ���� MISO ��ȡ���� */
			RA8875_WriteReg(0x05, (1 << 7) | (0 << 6) | (1 << 5) | (0 << 3) | (1 << 2) | (0 << 0));
		#endif	
	}

	/*
		Serial Flash/ROM Ƶ��Ƶ���趨
			0xb: SFCL Ƶ�� = ϵͳƵ��Ƶ��(��DMA Ϊʹ��״̬������ɫ�����Ϊ256 ɫ����SFCL Ƶ��
				�̶�Ϊ=ϵͳƵ��Ƶ��/ 2)
			10b: SFCL Ƶ�� =ϵͳƵ��Ƶ��/ 2
			11b: SFCL Ƶ�� =ϵͳƵ��Ƶ��/ 4

		������TFT�����塣4.3��ϵͳƵ��Ϊ 68MHz�� 7��ϵͳƵ��Ϊ 81.25MHz��

		8M����Flash�ķ����ٶ�:SPI ʱ��Ƶ��:80MHz(max.)
		��˿�������Ϊ 1 ��Ƶ
	*/
	RA8875_WriteReg(0x06, (0 << 0));	/* ����SCLʱ��Ƶ�� */

	/* �趨Դ��ַ
		B0H   DMA ��Դ��ʼλ��[7:0]
		B1H   DMA ��Դ��ʼλ��[15:8]
		B2H   DMA ��Դ��ʼλ��[23:16]
	*/
	RA8875_WriteReg(0xB0, (uint8_t)_uiFlashAddr);
	RA8875_WriteReg(0xB1, (uint8_t)(_uiFlashAddr >> 8));
	RA8875_WriteReg(0xB2, (uint8_t)(_uiFlashAddr >> 16));

	/*
		���������С
		B4   DMA ������[7:0]
		B5   DMA ������[9:8]
		B6   DMA ����߶�[7:0]
	    B7   DMA ����߶�[9:8]
	    B8   DMA ��ԴͼƬ���[7:0]
	    B9   DMA ��ԴͼƬ���[9:8]
	*/
	RA8875_WriteReg(0xB4, _usWidth);		/* DMA ������ */
	RA8875_WriteReg(0xB5, _usWidth >> 8);

	RA8875_WriteReg(0xB6, _usHeight);		/* DMA ����߶� */
	RA8875_WriteReg(0xB7, _usHeight >> 8);

	RA8875_WriteReg(0xB8, _usWidth);		/* DMA ��ԴͼƬ��� */
	RA8875_WriteReg(0xB9, _usWidth >> 8);

	RA8875_WriteReg(0xBF, 1 << 1);			/* ѡ������ģʽ */
	RA8875_WriteReg(0xBF, 1 << 1);			/* ѡ������ģʽ */

	s_ucRA8875Busy = 1;
	RA8875_WriteCmd(0xBF);
	RA8875_WriteData((1 << 1) | (1 << 0));
	while (RA8875_ReadReg_Int(0xBF) & (1 << 0));	/* �ȴ����� */
	s_ucRA8875Busy = 0;

	RA8875_QuitWinMode();					/* �˳�С���ڻ�ͼģʽ */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_DrawBMP
*	����˵��: ��LCD����ʾһ��BMPλͼ��λͼ����ɨ�����:�����ң����ϵ���
*	��    ��:
*			_usX, _usY : ͼƬ������
*			_usHeight  :ͼƬ�߶�
*			_usWidth   :ͼƬ���
*			_ptr       :ͼƬ����ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr)
{
	uint32_t index = 0;
	const uint16_t *p;

	/* ����ͼƬ��λ�úʹ�С�� ��������ʾ���� */
	RA8875_SetDispWin(_usX, _usY, _usHeight, _usWidth);

	s_ucRA8875Busy = 1;

	RA8875_WriteCmd(0x02); 		/* ׼����д�Դ� */

	p = _ptr;
	for (index = 0; index < _usHeight * _usWidth; index++)
	{
		/*
			armfly : �����Ż�, �����͵�չ��
			RA8875_WriteRAM(_ptr[index]);

			�˴��ɿ�����DMA����
		*/
		RA8875_WriteData16(*p++);
	}
	s_ucRA8875Busy = 0;

	/* �˳����ڻ�ͼģʽ */
	RA8875_QuitWinMode();
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_StartDirectDraw
*	����˵��: ��ʼֱ�ӻ�ͼģʽ.  ��֧��SPIģʽ����֧��8080ģʽ
*	��    ��:
*			_usX, _usY : ��ʾ���ڵ�����
*			_usHeight  : ��ʾ���ڵĸ߶�
*			_usWidth   : ��ʾ���ڵĿ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_StartDirectDraw(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth)
{
	/* ����ͼƬ��λ�úʹ�С�� ��������ʾ���� */
	RA8875_SetDispWin(_usX, _usY, _usHeight, _usWidth);

	s_ucRA8875Busy = 1;

	RA8875_WriteCmd(0x02); 		/* ׼����д�Դ� */

	/* ֮��Ӧ�ó������ֱ�ӻ�ͼ */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_QuitDirectDraw
*	����˵��: �˳���ͼģʽ.  ��֧��SPIģʽ����֧��8080ģʽ
*	��    ��:
*			_usX, _usY : ��ʾ���ڵ�����
*			_usHeight  : ��ʾ���ڵĸ߶�
*			_usWidth   : ��ʾ���ڵĿ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_QuitDirectDraw(void)
{
	s_ucRA8875Busy = 0;

	/* �˳����ڻ�ͼģʽ */
	RA8875_QuitWinMode();
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_DrawLine
*	����˵��: ����RA8875��Ӳ�����ܣ���2��仭һ��ֱ�ߡ�
*	��    ��:
*			_usX1, _usY1 :��ʼ������
*			_usX2, _usY2 :��ֹ��Y����
*			_usColor     :��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor)
{
	/* pdf ��131ҳ
		RA8875 ֧��ֱ�߻�ͼ���ܣ���ʹ�����Լ��׻���ٵ�MCU �Ϳ�����TFT ģ���ϻ�ֱ�ߡ�����
		��ֱ�ߵ���ʼ��REG[91h~94h] �������REG[95h~98h]��ֱ�ߵ���ɫREG[63h~65h]��Ȼ����
		����ͼ�趨REG[90h] Bit4 = 0, Bit0=0 ��REG[90h] Bit7 = 1��RA8875 �ͻὫֱ�ߵ�ͼ��д��
		DDRAM����Ե���TFT ģ���ϾͿ�����ʾ������ֱ�ߡ�
	*/

	if ((_usX1 == _usX2) && (_usY1 == _usY2))
	{
		RA8875_PutPixel(_usX1, _usY1, _usColor);
		return;
	}

	if (g_LcdDirection > 1)	/* ����  */
	{
		uint16_t temp;
		
		temp = _usX1;
		_usX1 = _usY1;
		_usY1 = temp;

		temp = _usX2;
		_usX2 = _usY2;
		_usY2 = temp;		
	}
	
	/* ����������� */
	RA8875_WriteReg(0x91, _usX1);
	RA8875_WriteReg(0x92, _usX1 >> 8);
	RA8875_WriteReg(0x93, _usY1);
	RA8875_WriteReg(0x94, _usY1 >> 8);

	/* �����յ����� */
	RA8875_WriteReg(0x95, _usX2);
	RA8875_WriteReg(0x96, _usX2 >> 8);
	RA8875_WriteReg(0x97, _usY2);
	RA8875_WriteReg(0x98, _usY2 >> 8);

	RA8875_SetFrontColor(_usColor);	/* ����ֱ�ߵ���ɫ */

	s_ucRA8875Busy = 1;
//	RA8875_WriteReg(0x90, (1 << 7) | (0 << 4) | (0 << 0));/* ��ʼ��ֱ�� */
	RA8875_WriteCmd(0x90);
	RA8875_WriteData((1 << 7) | (0 << 4) | (0 << 0));
//	while (RA8875_ReadReg(0x90) & (1 << 7));				/* �ȴ����� */
	RA8875_WaitBusy();
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_DrawRect
*	����˵��: ����RA8875Ӳ�����ܻ��ƾ���
*	��    ��:
*			_usX,_usY:�������Ͻǵ�����
*			_usHeight :���εĸ߶�
*			_usWidth  :���εĿ��
*			_usColor  :��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	/*
		RA8875 ֧�ַ��λ�ͼ���ܣ���ʹ�����Լ��׻���ٵ�MCU �Ϳ�����TFT ģ���ϻ����Ρ�����
	�����ε���ʼ��REG[91h~94h]�������REG[95h~98h]�����ε���ɫREG[63h~65h]��Ȼ����
	����ͼ�趨REG[90h] Bit4=1, Bit0=0 ��REG[90h] Bit7 = 1��RA8875 �ͻὫ���ε�ͼ��д��
	DDRAM����Ե���TFT ģ���ϾͿ�����ʾ�����ķ��Ρ����趨REG[90h] Bit5 = 1����ɻ���һ
	ʵ�ķ��� (Fill)

	 ---------------->---
	|(_usX��_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/

	if (g_LcdDirection > 1)	/* ����  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;

		temp = _usHeight;
		_usHeight = _usWidth;
		_usWidth = temp;		
	}

	/* ����������� */
	RA8875_WriteReg(0x91, _usX);
	RA8875_WriteReg(0x92, _usX >> 8);
	RA8875_WriteReg(0x93, _usY);
	RA8875_WriteReg(0x94, _usY >> 8);

	/* �����յ����� */
	RA8875_WriteReg(0x95, _usX + _usWidth - 1);
	RA8875_WriteReg(0x96, (_usX + _usWidth - 1) >> 8);
	RA8875_WriteReg(0x97, _usY + _usHeight - 1);
	RA8875_WriteReg(0x98, (_usY + _usHeight - 1) >> 8);

	RA8875_SetFrontColor(_usColor);	/* ������ɫ */

	s_ucRA8875Busy = 1;
//	RA8875_WriteReg(0x90, (1 << 7) | (0 << 5) | (1 << 4) | (0 << 0));	/* ��ʼ������  */
	RA8875_WriteCmd(0x90);
	RA8875_WriteData((1 << 7) | (0 << 5) | (1 << 4) | (0 << 0));
//	while (RA8875_ReadReg(0x90) & (1 << 7));							/* �ȴ����� */
	RA8875_WaitBusy();
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_FillRect
*	����˵��: ����RA8875Ӳ���������һ������Ϊ��ɫ
*	��    ��:
*			_usX,_usY:�������Ͻǵ�����
*			_usHeight :���εĸ߶�
*			_usWidth  :���εĿ��
*			_usColor  :��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_FillRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	/*
		RA8875 ֧�ַ��λ�ͼ���ܣ���ʹ�����Լ��׻���ٵ�MCU �Ϳ�����TFT ģ���ϻ����Ρ�����
	�����ε���ʼ��REG[91h~94h]�������REG[95h~98h]�����ε���ɫREG[63h~65h]��Ȼ����
	����ͼ�趨REG[90h] Bit4=1, Bit0=0 ��REG[90h] Bit7 = 1��RA8875 �ͻὫ���ε�ͼ��д��
	DDRAM����Ե���TFT ģ���ϾͿ�����ʾ�����ķ��Ρ����趨REG[90h] Bit5 = 1����ɻ���һ
	ʵ�ķ��� (Fill)

	 ---------------->---
	|(_usX��_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/

	if (_usWidth == 0 || _usHeight == 0)
	{
		return;
	}

	if (g_LcdDirection > 1)	/* ����  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;
		
		temp = _usHeight;
		_usHeight = _usWidth;
		_usWidth = temp;
	}
	
	/* ����������� */
	RA8875_WriteReg(0x91, _usX);
	RA8875_WriteReg(0x92, _usX >> 8);
	RA8875_WriteReg(0x93, _usY);
	RA8875_WriteReg(0x94, _usY >> 8);

	/* �����յ����� */
	RA8875_WriteReg(0x95, _usX + _usWidth - 1);
	RA8875_WriteReg(0x96, (_usX + _usWidth - 1) >> 8);
	RA8875_WriteReg(0x97, _usY + _usHeight - 1);
	RA8875_WriteReg(0x98, (_usY + _usHeight - 1) >> 8);

	RA8875_SetFrontColor(_usColor);	/* ������ɫ */

	s_ucRA8875Busy = 1;
//	RA8875_WriteReg(0x90, (1 << 7) | (1 << 5) | (1 << 4) | (0 << 0));	/* ��ʼ������  */
	RA8875_WriteCmd(0x90);
	RA8875_WriteData((1 << 7) | (1 << 5) | (1 << 4) | (0 << 0));
//	while (RA8875_ReadReg(0x90) & (1 << 7));							/* �ȴ����� */
	RA8875_WaitBusy();
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_DrawCircle
*	����˵��: ����һ��Բ���ʿ�Ϊ1������
*	��    ��:
*			_usX,_usY  :Բ�ĵ�����
*			_usRadius  :Բ�İ뾶
*			_usColor  :��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	/*
		RA8875 ֧��Բ�λ�ͼ���ܣ���ʹ�����Լ��׻���ٵ�MCU �Ϳ�����TFT ģ���ϻ�Բ�����趨
		Բ�����ĵ�REG[99h~9Ch]��Բ�İ뾶REG[9Dh]��Բ����ɫREG[63h~65h]��Ȼ��������ͼ
		REG[90h] Bit6 = 1��RA8875 �ͻὫԲ��ͼ��д��DDRAM����Ե���TFT ģ���ϾͿ�����ʾ��
		����Բ�����趨REG[90h] Bit5 = 1����ɻ���һʵ��Բ (Fill)�����趨REG[90h] Bit5 = 0�����
		��������Բ (Not Fill
	*/
	/* ����Բ�İ뾶 */
	if (_usRadius > 255)
	{
		return;
	}

	if (g_LcdDirection > 1)	/* ����  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;
	}
	
	/* ����Բ������ */
	RA8875_WriteReg(0x99, _usX);
	RA8875_WriteReg(0x9A, _usX >> 8);
	RA8875_WriteReg(0x9B, _usY);
	RA8875_WriteReg(0x9C, _usY >> 8);

	RA8875_WriteReg(0x9D, _usRadius);	/* ����Բ�İ뾶 */

	RA8875_SetFrontColor(_usColor);	/* ������ɫ */

	s_ucRA8875Busy = 1;
//	RA8875_WriteReg(0x90, (1 << 6) | (0 << 5));				/* ��ʼ��Բ, �����  */
	RA8875_WriteCmd(0x90);
	RA8875_WriteData( (1 << 6) | (0 << 5));
//	while (RA8875_ReadReg(0x90) & (1 << 6));				/* �ȴ����� */
	RA8875_WaitBusy();
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_FillCircle
*	����˵��: ���һ��Բ
*	��    ��:
*			_usX,_usY  :Բ�ĵ�����
*			_usRadius  :Բ�İ뾶
*			_usColor   :��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_FillCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	/*
		RA8875 ֧��Բ�λ�ͼ���ܣ���ʹ�����Լ��׻���ٵ�MCU �Ϳ�����TFT ģ���ϻ�Բ�����趨
		Բ�����ĵ�REG[99h~9Ch]��Բ�İ뾶REG[9Dh]��Բ����ɫREG[63h~65h]��Ȼ��������ͼ
		REG[90h] Bit6 = 1��RA8875 �ͻὫԲ��ͼ��д��DDRAM����Ե���TFT ģ���ϾͿ�����ʾ��
		����Բ�����趨REG[90h] Bit5 = 1����ɻ���һʵ��Բ (Fill)�����趨REG[90h] Bit5 = 0�����
		��������Բ (Not Fill
	*/
	/* ����Բ�İ뾶 */
	if (_usRadius > 255)
	{
		return;
	}

	if (g_LcdDirection > 1)	/* ����  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;
	}
	
	/* ����Բ������ */
	RA8875_WriteReg(0x99, _usX);
	RA8875_WriteReg(0x9A, _usX >> 8);
	RA8875_WriteReg(0x9B, _usY);
	RA8875_WriteReg(0x9C, _usY >> 8);

	RA8875_WriteReg(0x9D, _usRadius);	/* ����Բ�İ뾶 */

	RA8875_SetFrontColor(_usColor);	/* ������ɫ */

	s_ucRA8875Busy = 1;
//	RA8875_WriteReg(0x90, (1 << 6) | (1 << 5));				/* ��ʼ��Բ, ���  */
	RA8875_WriteCmd(0x90);
	RA8875_WriteData((1 << 6) | (1 << 5));
//	while (RA8875_ReadReg(0x90) & (1 << 6));				/* �ȴ����� */
	RA8875_WaitBusy();
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_SetFont
*	����˵��: �ı�ģʽ�������������塢�о���־�
*	��    ��:
*			_ucFontType : ��������: RA_FONT_16, RA_FONT_24, RA_FONT_32
*			_ucLineSpace : �о࣬���ص�λ (0-31)
*			_ucCharSpace : �־࣬���ص�λ (0-63)
*
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_SetFont(uint8_t _ucFontType, uint8_t _ucLineSpace, uint8_t _ucCharSpace)
{
	/*
		[29H]������ģʽ�£������趨���ּ���о� (��λ: ����) ��
		ֻ�е�5��bit��Ч��0-31
	*/
	if (_ucLineSpace >31)
	{
		_ucLineSpace = 31;
	}
	RA8875_WriteReg(0x29, _ucLineSpace);

	/*
		[2EH] �����ַ���ࣨ���ص�λ��0-63���������壨16*16��24*24��32*32��
	*/
	if (_ucCharSpace > 63)
	{
		_ucCharSpace = 63;
	}
	if (_ucFontType > RA_FONT_32)
	{
		_ucFontType = RA_FONT_16;
	}
	RA8875_WriteReg(0x2E, (_ucCharSpace & 0x3F) | (_ucFontType << 6));
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_SetFontZoom
*	����˵��: �ı�ģʽ���������ֵķŴ�ģʽ��1X,2X,3X, 4X
*	��    ��:
*			_ucHSize : ����ˮƽ�Ŵ�����0-3 �ֱ��Ӧ X1��X2��X3��X4
*			_ucVSize : ���ִ��÷Ŵ�����0-3 �ֱ��Ӧ X1��X2��X3��X4
*		�������������ö��:RA_SIZE_X1��RA_SIZE_X2��RA_SIZE_X3��RA_SIZE_X4
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_SetTextZoom(uint8_t _ucHSize, uint8_t _ucVSize)
{
	/*
		pdf ��22ҳ		[22H]
		bit7 ���ڶ��룬һ�㲻�ã�ȱʡ��0
		bit6 ����ͨ͸��һ�㲻�ã�ȱʡ��0
		bit4 ������ת90����һ�㲻�ã�ȱʡ��0
		bit3-2 ˮƽ�Ŵ���
		bit1-0 ��ֱ�Ŵ���
	*/
	if (g_LcdDirection > 1)	/* ����  */
	{
		RA8875_WriteReg(0x22, (1 << 4) | ((_ucHSize & 0x03) << 2) | ( _ucVSize & 0x03));
	}
	else	/* ���� */
	{
		RA8875_WriteReg(0x22, (0 << 4) | ((_ucHSize & 0x03) << 2) | ( _ucVSize & 0x03));
	}
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_DispAscii
*	����˵��: ��ʾASCII�ַ������ַ�����������RA8875�ڲ�ROM�е�ASCII�ַ�����8*16������֧�ֺ���.
*			������ɫ��������ɫ���Ƿ�ͨ͸�������ĺ�����������
*			void RA8875_SetFrontColor(uint16_t _usColor);
*			void RA8875_SetBackColor(uint16_t _usColor);
*	��    ��:  _usX, _usY : ��ʾλ�ã����꣩
*			 _ptr : AascII�ַ�������0����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_DispAscii(uint16_t _usX, uint16_t _usY, char *_ptr)
{
	/*
		RA8875 �ڽ�8x16 ���ASCII ����ROM���ṩʹ���߸�����ķ�ʽ���ض����� (Code) ������
		�֡��ڽ����ּ�֧��ISO/IEC 8859-1~4 �����׼�����⣬ʹ���߿���͸��REG[60h~62h] ѡ��
		����ǰ����ɫ���Լ�͸��REG[63h~65h] ѡ�񱳾���ɫ.

		ISO/IEC 8859-1���ֳ�Latin-1����ŷ���ԡ����ǹ��ʱ�׼����֯��ISO/IEC 8859�ĵ�һ��8λ�ַ�����
		����ASCIIΪ�������ڿ��õ�0xA0-0xFF�ķ�Χ�ڣ�����192����ĸ�����ţ����Թ�ʹ�ñ������ŵ�������ĸ����ʹ�á�

		ISO/IEC 8859-2 Latin-2����ŷ���ԡ����ǹ��ʱ�׼����֯��ISO/IEC 8859������һ��8λ�ַ��� .
		ISO/IEC 8859-3 ��ŷ�����ַ���
		ISO/IEC 8859-4 ��ŷ�����ַ���
	*/

	/*
	(1) Text mode  REG[40h] bit7=1
	(2) Internal Font ROM Select   REG[21h] bit7=0, bit5=0
	(3) Font foreground and background color Select  REG[63h~65h], REG[60h~62h]
	(4) Write the font Code  CMD_WR[02h]    DATA_WR[font_code]
	*/
	
	RA8875_SetTextCursor(_usX, _usY);

	s_reg_40H |= (1 << 7);
	RA8875_WriteReg(0x40, s_reg_40H);	/* ����Ϊ�ı�ģʽ */

	/* ѡ��CGROM font; ѡ���ڲ�CGROM; �ڲ�CGROM ����ѡ��ISO/IEC 8859-1. */
	RA8875_WriteReg(0x2F, 0x00);
	RA8875_WriteReg(0x21, (0 << 7) | (0 << 5) | (0 << 1) | (0 << 0));

	s_ucRA8875Busy = 1;

	RA8875_WriteCmd(0x02); 			/* �����趨RA8875 �����ڴ�(DDRAM��CGRAM)��ȡ/д��ģʽ */

	/* ��ʼѭ�������ַ� */
	while (*_ptr != 0)
	{
		RA8875_WriteData(*_ptr);
		while ((RA8875_ReadStatus() & 0x80) == 0x80);	/* ����ȴ��ڲ�д��������� */
		_ptr++;
	}
	s_ucRA8875Busy = 0;

	s_reg_40H &= 0x7F;
	RA8875_WriteReg(0x40, s_reg_40H);	/* ��ԭΪͼ��ģʽ */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_DispStr
*	����˵��: ��ʾ�ַ������ַ�����������RA8875��ӵ��ֿ�оƬ��֧�ֺ���.
*			������ɫ��������ɫ���Ƿ�ͨ͸�������ĺ�����������
*			void RA8875_SetFrontColor(uint16_t _usColor);
*			void RA8875_SetBackColor(uint16_t _usColor);
*	��    ��:  _usX, _usY : ��ʾλ�ã����꣩
*			 _ptr : AascII�ַ�������0����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_DispStr(uint16_t _usX, uint16_t _usY, char *_ptr)
{
	/*
		RA8875 ͸��ʹ�� �� �Ϻ���ͨ��˾ (Genitop Inc) �ⲿ����ʽ�����ڴ� (Font ROM)����֧�ָ���
		������д�뵽DDRAM �С�RA8875 ���Ϻ���ͨ��˾ (Genitop Inc) ���ݵĲ�Ʒ���� :
		GT21L16TW/GT21H16T1W ��GT23L16U2W ��GT23L24T3Y/GT23H24T3Y ��GT23L24M1Z ��
		�� GT23L32S4W/GT23H32S4W����Щ�������16x16, 24x24, 32x32 �� (Dot) �벻ͬ����

		������RA8875�����弯�ɵ��ֿ�оƬΪ ��ͨ�ֿ�оƬ_GT23l32S4W

		GT23L32S4W��һ���ں�11X12����15X16�㡢24X24����32X32����ĺ��ֿ�оƬ��֧��GB2312
		���꺺�֣����й����ű�ί�Ϸ���Ȩ����SCII�ַ������и�ʽΪ���ú��š��û�ͨ���ַ����룬���ñ���
		���ṩ�ķ�����������ַ�������оƬ�еĵ�ַ���ɴӸõ�ַ���������ַ�������Ϣ��
	*/

	/* �����ı���ʾλ�ã�ע���ı�ģʽ��д�����ͼ��ģʽ��д�����ǲ�ͬ�ļĴ��� */
	RA8875_SetTextCursor(_usX, _usY);

	//RA8875_SetTextZoom(0, 0);	/* for test */
	
	s_reg_40H |= (1 << 7);
	RA8875_WriteReg(0x40, s_reg_40H);	/* ����Ϊ�ı�ģʽ */

	/*
		Serial Flash/ROM Ƶ��Ƶ���趨
			0xb: SFCL Ƶ�� = ϵͳƵ��Ƶ��(��DMA Ϊ����״̬������ɫ�����Ϊ256 ɫ����SFCL Ƶ��
				�̶�Ϊ=ϵͳƵ��Ƶ��/ 2)
			10b: SFCL Ƶ�� =ϵͳƵ��Ƶ��/ 2
			11b: SFCL Ƶ�� =ϵͳƵ��Ƶ��/ 4

		������������ϵͳƵ��Ϊ 68MHz

		GT23L32S4W�ķ����ٶ�:SPI ʱ��Ƶ��:20MHz(max.)

		�����Ҫ���� 4 ��Ƶ, 17MHz
	*/
	RA8875_WriteReg(0x06, (3 << 0));	/* ����Ϊ�ı�ģʽ */

	/* ѡ���ⲿ����ROM */
	RA8875_WriteReg(0x21, (0 << 7) | (1 << 5));

	/* 05H  REG[05h] Serial Flash/ROM Configuration Register (SROC)
		7	Serial Flash/ROM I/F # ѡ��
				0:ѡ��Serial Flash/ROM 0 �ӿڡ�[������RA8875�������ֿ�оƬ���� 0 �ӿ�]
				1:ѡ��Serial Flash/ROM 1 �ӿڡ�
		6	Serial Flash/ROM Ѱַģʽ
				0: 24 λѰַģʽ��
				��λ������Ϊ0��
		5	Serial Flash/ROM ����ģʽ
				0: ����ģʽ 0��
				1: ����ģʽ 3��
		4-3	Serial Flash /ROM ��ȡ���� (Read Cycle)
			00b: 4 bus ?? �޿����� (No Dummy Cycle)��
			01b: 5 bus ??1 byte �����ڡ�
			1Xb: 6 bus ??2 byte �����ڡ�
		2	Serial Flash /ROM ��ȡģʽ (Access Mode)
			0: ����ģʽ ��
			1: DMA ģʽ��
		1-0	Serial Flash /ROM I/F Data Latch ѡ��ģʽ
			0Xb: ��һģʽ��
			10b: ˫��ģʽ0��
			11b: ˫��ģʽ1��
	*/
	RA8875_WriteReg(0x05, (0 << 7) | (0 << 6) | (1 << 5) | (1 << 3) | (0 << 2) | (0 << 1));

	/*
		�����ⲿ����оƬ�ͺ�Ϊ GT23L32S4W, ����ΪGB2312,

		Bit1:0 ����ASCII�ַ��ĸ�ʽ:
			0 = NORMAL		 [�ʻ�ϸ, �ͺ��ֶ�������]
			1 = Arial		 [�ʻ��֣��ͺ��ֵײ�����]
			2 = Roman		 [�ʻ�ϸ, �ͺ��ֵײ�����]
			3 = Bold		 [����,������]
	 */
	RA8875_WriteReg(0x2F, (4 << 5) | (0 << 2) | (1 << 0));

	s_ucRA8875Busy = 1;
	RA8875_WriteCmd(0x02); 			/* �����趨RA8875 �����ڴ�(DDRAM��CGRAM)��ȡ/д��ģʽ */

	/* ��ʼѭ�������ַ� */
	while (*_ptr != 0)
	{
		RA8875_WriteData(*_ptr);
		while ((RA8875_ReadStatus() & 0x80) == 0x80);
		//while ((RA8875_ReadStatus() & 0x01) != 0);	
		//while(RA8875_ReadBusy());
		_ptr++;
	}
	s_ucRA8875Busy = 0;

	s_reg_40H &= 0x7F;
	RA8875_WriteReg(0x40, s_reg_40H);	/* ��ԭΪͼ��ģʽ */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_SetDispWin
*	����˵��: ������ʾ���ڣ����봰����ʾģʽ���ڴ�����ʾģʽ������д�Դ棬�����Զ����趨�����ڽ��е���
*	��    ��:
*		_usX : ˮƽ����
*		_usY : ��ֱ����
*		_usHeight: ���ڸ߶�
*		_usWidth : ���ڿ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_SetDispWin(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth)
{

	uint16_t usTemp;

	/* ����ϵͳʾ��ͼ: ��������
			 -----------------------------
			|(0,0)                        |
			|     --------->              |
			|         |                   |
			|         |                   |
			|         |                   |
			|         V                   |
			|     --------->              |
			|                    (479,271)|
			 -----------------------------

		���Ͻ����������, ɨ�跽���ȴ����ң��ٴ��ϵ��¡�

		�����Ҫ��������ʽ������Ҫ��������������߼������ת��
	*/
	RA8875_WriteReg(0x30, _usX);
    RA8875_WriteReg(0x31, _usX >> 8);

	RA8875_WriteReg(0x32, _usY);
    RA8875_WriteReg(0x33, _usY >> 8);

	usTemp = _usWidth + _usX - 1;
	RA8875_WriteReg(0x34, usTemp);
    RA8875_WriteReg(0x35, usTemp >> 8);

	usTemp = _usHeight + _usY - 1;
	RA8875_WriteReg(0x36, usTemp);
    RA8875_WriteReg(0x37, usTemp >> 8);

	RA8875_SetCursor(_usX, _usY);

	/* ���浱ǰ������Ϣ������Ժ�ɫ��������Ч��.
	����һ��������ͨ����ȡ0x30-0x37�Ĵ�����õ�ǰ���ڣ�����Ч�ʽϵ� */
	s_WinX = _usX;
	s_WinY = _usY;
	s_WinHeight = _usHeight;
	s_WinWidth = _usWidth;
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_SetCursor
*	����˵��: ����д�Դ�Ĺ��λ�ã�ͼ��ģʽ��
*	��    ��:  _usX : X����; _usY: Y����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void RA8875_SetCursor(uint16_t _usX, uint16_t _usY)
{
	if (g_LcdDirection > 1)	/* ����  */
	{
		RA8875_WriteReg(0x46, _usY);
		RA8875_WriteReg(0x47, _usY >> 8);
		RA8875_WriteReg(0x48, _usX);
		RA8875_WriteReg(0x49, _usX >> 8);
	}
	else	/* ���� */
	{
		RA8875_WriteReg(0x46, _usX);
		RA8875_WriteReg(0x47, _usX >> 8);
		RA8875_WriteReg(0x48, _usY);
		RA8875_WriteReg(0x49, _usY >> 8);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_SetReadCursor
*	����˵��: ���ö��Դ�Ĺ��λ�á� �ܶ������Ŀ�����д���Ͷ��������ͬ�ļĴ���������RA8875�ǲ�ͬ�ġ�
*	��    ��:  _usX : X����; _usY: Y����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void RA8875_SetReadCursor(uint16_t _usX, uint16_t _usY)
{
	if (g_LcdDirection > 1)	/* ����  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;
	}
		
	/* �����ڴ���������� */
	RA8875_WriteReg(0x4A, _usX);
	RA8875_WriteReg(0x4B, _usX >> 8);
	RA8875_WriteReg(0x4C, _usY);
	RA8875_WriteReg(0x4D, _usY >> 8);
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_SetTextCursor
*	����˵��: �����ı�д����λ��
*	��    ��:  _usX : X����; _usY: Y����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void RA8875_SetTextCursor(uint16_t _usX, uint16_t _usY)
{
	if (g_LcdDirection > 1)	/* ����  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;
	}
			
	/* �����ڴ���������� */
	RA8875_WriteReg(0x2A, _usX);
	RA8875_WriteReg(0x2B, _usX >> 8);
	RA8875_WriteReg(0x2C, _usY);
	RA8875_WriteReg(0x2D, _usY >> 8);
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_QuitWinMode
*	����˵��: �˳�������ʾģʽ����Ϊȫ����ʾģʽ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_QuitWinMode(void)
{
	RA8875_SetDispWin(0, 0, g_LcdHeight, g_LcdWidth);
}


/*
*********************************************************************************************************
*	�� �� ��: RA8875_CtrlGPO
*	����˵��: ����GPO���״̬��Ҳ����KOUT0-KOUT3��״̬
*	��    ��: _pin : 0-3, �ֱ��Ӧ GPO0 GPO1 GPO2 GPO3
*			  _value : 0 �� 1
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_CtrlGPO(uint8_t _pin, uint8_t _value)
{
	RA8875_WriteReg(0x13, _value << _pin);
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_SetBackLight
*	����˵��: ����RA8875оƬ��PWM1��ؼĴ���������LCD����
*	��    ��:  _bright ���ȣ�0����255������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_SetBackLight(uint8_t _bright)
{
	if (_bright == 0)
	{
		/* �ر�PWM, PWM1����ȱʡ���LOW  */
		RA8875_WriteReg(0x8A, 0 << 6);
	}
	else if (_bright == BRIGHT_MAX)	/* ������� */
	{
		/* �ر�PWM, PWM1����ȱʡ���HIGH */
		RA8875_WriteReg(0x8A, 1 << 6);
	}
	else
	{
		/* ʹ��PWM1, ����ռ�ձȵ��� */

		/* 	REG[8Ah] PWM1 Control Register (P1CR)

			Bit7 ������ȵ��� (PWM1) �趨
				0 : �رգ���״̬�£�PWM1 ���׼λ���մ˻�����Bit6 ������
				1 : ������

			Bit6 PWM1 �ر�ʱ��׼λ
				0 : ��PWM �رջ���˯��ģʽʱ��PWM1 ���Ϊ��Low�� ״̬��
				1 : ��PWM �رջ���˯��ģʽʱ��PWM1 ���Ϊ��High�� ״̬��

			Bit5 ����

			Bit4 PWM1 ����ѡ��
				0 : PWM1 ���ܡ�
				1 : PWM1 �̶����һƵ��Ϊ�ⲿ��������Clock (Fin) Ƶ�� 1 /16 ��Clock

			Bit3-0  PWM1 ��·��Ƶ����Դѡ��PWM_CLK	������PWM���Ƶ�ʡ�
				0000b : SYS_CLK / 1   , 1000b : SYS_CLK / 256
				0001b : SYS_CLK / 2   , 1001b : SYS_CLK / 512
				0010b : SYS_CLK / 4   , 1010b : SYS_CLK / 1024
				0011b : SYS_CLK / 8   , 1011b : SYS_CLK / 2048
				0100b : SYS_CLK / 16  , 1100b : SYS_CLK / 4096
				0101b : SYS_CLK / 32  , 1101b : SYS_CLK / 8192
				0110b : SYS_CLK / 64  , 1110b : SYS_CLK / 16384
				0111b : SYS_CLK / 128 , 1111b : SYS_CLK / 32768

				��SYS_CLK�� ����ϵͳƵ�ʣ� ����SYS_CLK Ϊ20MHz�� ��Bit[3:0] =0001b ʱ��PWM1 Ƶ����ԴΪ10MHz��
				ʵ�������PWMƵ�ʻ���Ҫ���� 256��֧��8λ�ķֱ��ʡ�

				��������4.3��(480*272)ģ�飬SYS_CLK =  68.75MHz
				��������7.0��(800*480)ģ�飬SYS_CLK =  81.25MHz

				Ϊ�˱�����Ƶ������PWMƵ�ʿ���ѡ��
				��1�� ��Ƶ100Hz
				��2�� ���� 20KHz

				���磬Bit3-0Ϊ0011bʱ SYS_CLK / 8��
					4.3�� �����PWMƵ��Ϊ (68.75MHz / 8) / 256 = 33.56KHz
					7�� �����PWMƵ��Ϊ (75 MHz / 8) / 256 = 36.62KHz
		*/

		// 5���7������������ 3 ����ƵPWM, 4.3�粻��
		RA8875_WriteReg(0x8A, (1 << 7) | 3);   
		// RA8875_WriteReg(0x8A, (1 << 7) | 12);

		/* REG[8Bh] PWM1 Duty Cycle Register (P1DCR) */
		RA8875_WriteReg(0x8B, _bright);
	}
}


/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
