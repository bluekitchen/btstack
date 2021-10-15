/*
*********************************************************************************************************
*
*	ģ������ : TFTҺ����ʾ������ģ��
*	�ļ����� : LCD_tft_lcd.h
*	��    �� : V2.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2010-2011, ���������� www.armfly.com
*
*********************************************************************************************************
*/


#ifndef _BSP_TFT_LCD_H
#define _BSP_TFT_LCD_H

#define BUTTON_BEEP()	BEEP_KeyTone();	/* ������ʾ�� */
//#define BUTTON_BEEP()	/* �ް�����ʾ�� */


/* ����LCD��ʾ����ķֱ��� */
#define LCD_30_HEIGHT	240		/* 3.0����� �߶ȣ���λ������ */
#define LCD_30_WIDTH	400		/* 3.0����� ��ȣ���λ������ */

#define LCD_43_HEIGHT	272		/* 4.3����� �߶ȣ���λ������ */
#define LCD_43_WIDTH	480		/* 4.3����� ��ȣ���λ������ */

#define LCD_70_HEIGHT	480		/* 7.0����� �߶ȣ���λ������ */
#define LCD_70_WIDTH	800		/* 7.0����� ��ȣ���λ������ */


/* ֧�ֵ�����оƬID */
enum
{
	IC_5420		= 0x5420,
	IC_4001		= 0x4001,
	IC_61509 	= 0xB509,
	IC_8875 	= 0x0075,
	
	IC_9488 	= 0x9488
};

#define CHIP_STR_5420	"SPFD5420A"
#define CHIP_STR_4001	"OTM4001A"
#define CHIP_STR_61509	"R61509V"
#define CHIP_STR_8875	"RA8875"
#define CHIP_STR_9488	"ILI9488"

/*
	LCD ��ɫ���룬CL_��Color�ļ�д
	16Bit�ɸ�λ����λ�� RRRR RGGG GGGB BBBB

	�����RGB �꽫24λ��RGBֵת��Ϊ16λ��ʽ��
	����windows�Ļ��ʳ��򣬵���༭��ɫ��ѡ���Զ�����ɫ�����Ի�õ�RGBֵ��

	�Ƽ�ʹ������ȡɫ���������㿴���Ľ�����ɫ��
*/
#define RGB(R,G,B)	(((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3))	/* ��8λR,G,Bת��Ϊ 16λRGB565��ʽ */
#define RGB565_R(x)  ((x >> 8) & 0xF8)
#define RGB565_G(x)  ((x >> 3) & 0xFC)
#define RGB565_B(x)  ((x << 3) & 0xF8)
enum
{
	CL_WHITE        = RGB(255,255,255),	/* ��ɫ */
	CL_BLACK        = RGB(  0,  0,  0),	/* ��ɫ */
	CL_RED          = RGB(255,	0,  0),	/* ��ɫ */
	CL_GREEN        = RGB(  0,255,  0),	/* ��ɫ */
	CL_BLUE         = RGB(  0,	0,255),	/* ��ɫ */
	CL_YELLOW       = RGB(255,255,  0),	/* ��ɫ */

	CL_GREY			= RGB( 98, 98, 98), 	/* ���ɫ */
	CL_GREY1		= RGB( 150, 150, 150), 	/* ǳ��ɫ */
	CL_GREY2		= RGB( 180, 180, 180), 	/* ǳ��ɫ */
	CL_GREY3		= RGB( 200, 200, 200), 	/* ��ǳ��ɫ */
	CL_GREY4		= RGB( 230, 230, 230), 	/* ��ǳ��ɫ */

	CL_BUTTON_GREY	= RGB( 220, 220, 220), /* WINDOWS ��ť�����ɫ */

	CL_MAGENTA      = 0xF81F,	/* ����ɫ�����ɫ */
	CL_CYAN         = 0x7FFF,	/* ����ɫ����ɫ */

	CL_BLUE1        = RGB(  0,  0, 240),		/* ����ɫ */
	CL_BLUE2        = RGB(  0,  0, 128),		/* ����ɫ */
	CL_BLUE3        = RGB(  68, 68, 255),		/* ǳ��ɫ1 */
	CL_BLUE4        = RGB(  0, 64, 128),		/* ǳ��ɫ1 */

	/* UI ���� Windows�ؼ�����ɫ */
	CL_BTN_FACE		= RGB(236, 233, 216),	/* ��ť������ɫ(��) */
	
	CL_BTN_FONT		= CL_BLACK,				/* ��ť������ɫ���ڣ� */
	
	CL_BOX_BORDER1	= RGB(172, 168,153),	/* �����������ɫ */
	CL_BOX_BORDER2	= RGB(255, 255,255),	/* �������Ӱ����ɫ */


	CL_MASK			= 0x9999	/* ��ɫ���룬�������ֱ���͸�� */
};

/* ���ֶ��뷽ʽ */
enum
{
	ALIGN_LEFT = 0,
	ALIGN_CENTER = 1,
	ALIGN_RIGHT = 2
};

/* �༭���� */
enum
{
	EDIT_BORDER_COLOR		= CL_BLUE2,		/* �༭���ĸ��ߵ���ɫ */
	EDIT_BACK_COLOR			= CL_WHITE,			/* �༭�򱳾� */
};

/* ��ť��� */
enum
{
	BUTTON_BORDER_COLOR		= CL_BLUE2,			/* ��ť�ĸ��ߵ���ɫ */
	BUTTON_BORDER1_COLOR	= CL_WHITE,			/* ��ť�ĸ����ڲ�2�е���ɫ */
	BUTTON_BORDER2_COLOR	= CL_GREY1,			/* ��ť�ĸ����ڲ�2�е���ɫ */
	BUTTON_BACK_COLOR		= CL_GREY3,			/* ��ť���� */
	BUTTON_ACTIVE_COLOR		= CL_CYAN,			/* ����İ�ť���� */
};

/* ���ڷ�� */
enum
{
	WIN_BORDER_COLOR	= CL_BLUE4,		/* ���ڱ߿� */
	WIN_TITLE_COLOR		= CL_BLUE3,		/* ���ڱ�����������ɫ */
	WIN_CAPTION_COLOR	= CL_WHITE,		/* ���ڱ�����������ɫ */
	WIN_BODY_COLOR		= CL_GREY2,		/* ������ɫ */
};

/* ������ */
enum
{
	CHECK_BOX_BORDER_COLOR	= CL_BLUE2,		/* �����ĸ��ߵ���ɫ */
	CHECK_BOX_BACK_COLOR	= CL_GREY3,		/* ���򱳾� */
	CHECK_BOX_CHECKED_COLOR	= CL_RED,		/* ����򹴵���ɫ */

	CHECK_BOX_H			= 24,				/* ����߶� */
	CHECK_BOX_W			= 24,				/* ����߶� */
};

/* ������� */
typedef enum
{
	FC_ST_12 = 0,		/* ����12x12���� ����x�ߣ� */
	FC_ST_16,			/* ����15x16���� ����x�ߣ� */
	FC_ST_24,			/* ����24x24���� ����x�ߣ� -- ��ʱδ֧�� */
	FC_ST_32,			/* ����32x32���� ����x�ߣ� -- ��ʱδ֧�� */	
	
	FC_RA8875_16,		/* RA8875 �������� 16���� */
	FC_RA8875_24,		/* RA8875 �������� 24���� */
	FC_RA8875_32		/* RA8875 �������� 32���� */	
}FONT_CODE_E;

/* �������Խṹ, ����LCD_DispStr() */
typedef struct
{
	FONT_CODE_E FontCode;	/* ������� FONT_CODE_E  */
	uint16_t FrontColor;/* ������ɫ */
	uint16_t BackColor;	/* ���ֱ�����ɫ��͸�� */
	uint16_t Space;		/* ���ּ�࣬��λ = ���� */
}FONT_T;

/* �ؼ�ID */
typedef enum
{
	ID_ICON		= 1,
	ID_WIN		= 2,
	ID_LABEL	= 3,
	ID_BUTTON	= 4,
	ID_CHECK 	= 5,
	ID_EDIT 	= 6,
	ID_GROUP 	= 7,
}CONTROL_ID_T;

/* ͼ��ṹ */
typedef struct
{
	uint8_t id;
	uint16_t Left;		/* ���Ͻ�X���� */
	uint16_t Top;		/* ���Ͻ�Y���� */
	uint16_t Height;	/* ͼ��߶� */
	uint16_t Width;		/* ͼ���� */
	uint16_t *pBmp;		/* ָ��ͼ��ͼƬ���� */
	char  Text[16];	/* ͼ���ı�, �����ʾ5������16���� */
}ICON_T;

/* ����ṹ */
typedef struct
{
	uint8_t id;
	uint16_t Left;
	uint16_t Top;
	uint16_t Height;
	uint16_t Width;
	uint16_t Color;
	FONT_T *Font;
	char *pCaption;
}WIN_T;

/* �ı���ǩ�ṹ */
typedef struct
{
	uint8_t id;
	uint16_t Left;			/* ���Ͻ�X���� */
	uint16_t Top;			/* ���Ͻ�Y���� */
	uint16_t Height;		/* �߶� */
	uint16_t Width;			/* ��� */
	uint16_t MaxLen;		/* �ַ������� */
	FONT_T *Font;			/* ���� */
	char  *pCaption;
}LABEL_T;

/* ��ť�ṹ */
typedef struct
{
	uint8_t id;
	uint16_t Left;
	uint16_t Top;
	uint16_t Height;
	uint16_t Width;
	/* ��ť����ɫ���ɵײ��Զ����� */
	FONT_T *Font;			/* ���� */
	char *pCaption;
	uint8_t Focus;			/* ���� */
}BUTTON_T;

/* �༭��ṹ */
typedef struct
{
	uint8_t id;
	uint16_t Left;
	uint16_t Top;
	uint16_t Height;
	uint16_t Width;
	uint16_t Color;
	FONT_T *Font;			/* ���� */
	char   *pCaption;
	char Text[32];			/* ���ڴ�ű༭���� */
}EDIT_T;

/* ���� CHECK BOX �ṹ */
typedef struct
{
	uint8_t id;
	uint16_t Left;			/* ���Ͻ�X���� */
	uint16_t Top;			/* ���Ͻ�Y���� */
	uint16_t Height;		/* �߶� */
	uint16_t Width;			/* ��� */
	uint16_t Color;			/* ��ɫ */
	FONT_T *Font;			/* ���� */
	char  *pCaption;
	uint8_t Checked;		/* 1��ʾ�� */
}CHECK_T;

/* �����GROUP BOX �ṹ */
typedef struct
{
	uint8_t id;
	uint16_t Left;			/* ���Ͻ�X���� */
	uint16_t Top;			/* ���Ͻ�Y���� */
	uint16_t Height;		/* �߶� */
	uint16_t Width;			/* ��� */
	FONT_T *Font;			/* ���� */
	char  *pCaption;
}GROUP_T;

/* ��������� */
#define BRIGHT_MAX		255
#define BRIGHT_MIN		0
#define BRIGHT_DEFAULT	200
#define BRIGHT_STEP		5

/* �ɹ��ⲿģ����õĺ��� */
void LCD_InitHard(void);
void LCD_GetChipDescribe(char *_str);
uint16_t LCD_GetHeight(void);
uint16_t LCD_GetWidth(void);
void LCD_DispOn(void);
void LCD_DispOff(void);
void LCD_ClrScr(uint16_t _usColor);
void LCD_DispStr(uint16_t _usX, uint16_t _usY, char *_ptr, FONT_T *_tFont);
void LCD_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor);
uint16_t LCD_GetPixel(uint16_t _usX, uint16_t _usY);
void LCD_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor);
void LCD_DrawPoints(uint16_t *x, uint16_t *y, uint16_t _usSize, uint16_t _usColor);
void LCD_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor);
void LCD_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor);
void LCD_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr);
void LCD_SetBackLight(uint8_t _bright);
uint8_t LCD_GetBackLight(void);

void LCD_Fill_Rect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor);

void LCD_DrawWin(WIN_T *_pWin);
void LCD_DrawIcon(const ICON_T *_tIcon, FONT_T *_tFont, uint8_t _ucFocusMode);
void LCD_DrawEdit(EDIT_T *_pEdit);
void LCD_DrawButton(BUTTON_T *_pBtn);
void LCD_DrawLabel(LABEL_T *_pLabel);
void LCD_DrawCheckBox(CHECK_T *_pCheckBox);
void LCD_DrawGroupBox(GROUP_T *_pBox);

void LCD_DispControl(void *_pControl);

void LCD_DrawIcon32(const ICON_T *_tIcon, FONT_T *_tFont, uint8_t _ucFocusMode);
void LCD_DrawBmp32(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint8_t *_pBmp);

uint16_t LCD_GetFontWidth(FONT_T *_tFont);
uint16_t LCD_GetFontHeight(FONT_T *_tFont);
uint16_t LCD_GetStrWidth(char *_ptr, FONT_T *_tFont);
void LCD_DispStrEx(uint16_t _usX, uint16_t _usY, char *_ptr, FONT_T *_tFont, uint16_t _Width,
	uint8_t _Align);

void LCD_SetDirection(uint8_t _dir);
uint8_t LCD_ButtonTouchDown(BUTTON_T *_btn, uint16_t _usX, uint16_t _usY);
uint8_t LCD_ButtonTouchRelease(BUTTON_T *_btn, uint16_t _usX, uint16_t _usY);
void LCD_InitButton(BUTTON_T *_btn, uint16_t _x, uint16_t _y, uint16_t _h, uint16_t _w, char *_pCaption, FONT_T *_pFont);

/* ����3����������Ҫ����ʹ����ͬʱ֧�ֲ�ͬ���� */
extern uint16_t g_ChipID;			/* ����оƬID */
extern uint16_t g_LcdHeight;		/* ��ʾ���ֱ���-�߶� */
extern uint16_t g_LcdWidth;			/* ��ʾ���ֱ���-��� */
extern uint8_t g_LcdDirection;		/* ��ʾ����.0��1��2��3 */

#endif


