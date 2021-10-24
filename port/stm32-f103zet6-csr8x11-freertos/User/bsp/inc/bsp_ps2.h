/*
*********************************************************************************************************
*
*	ģ������ : PS/2���̺��������ģ��
*	�ļ����� : bsp_ps2.h
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_PS2_H
#define _BSP_PS2_H

/* �����Զ�ʶ���豸���͵ĺ��� */
enum
{
	PS2_UNKNOW_DEVICE	= 0,	/* δ֪�豸 */
	PS2_MOUSE			= 1,	/* ����豸 */
	PS2_KEYBOARD		= 2,	/* �����豸 */
};

/* ָʾ��������ڵ�������LED�Ƶĺ��� */
enum
{
	LED_CapsLock = 0,
	LED_NumLock,
	LED_ScrollLock
};

#define PS2_NONE     0
#define KB_PAUSE     0xE11477E1
#define KB_PRNT_SCRN 0xE012E07C
enum
{
    KB_A         = 0x00001C,
    KB_B         = 0x000032,
    KB_C         = 0x000021,
    KB_D         = 0x000023,
    KB_E         = 0x000024,
    KB_F         = 0x00002B,
    KB_G         = 0x000034,
    KB_H         = 0x000033,
    KB_I         = 0x000043,
    KB_J         = 0x00003B,
    KB_K         = 0x000042,
    KB_L         = 0x00004B,
    KB_M         = 0x00003A,
    KB_N         = 0x000031,
    KB_O         = 0x000044,
    KB_P         = 0x00004D,
    KB_Q         = 0x000015,
    KB_R         = 0x00002D,
    KB_S         = 0x00001B,
    KB_T         = 0x00002C,
    KB_U         = 0x00003C,
    KB_V         = 0x00002A,
    KB_W         = 0x00001D,
    KB_X         = 0x000022,
    KB_Y         = 0x000035,
    KB_Z         = 0x00001A,
    KB_0         = 0x000045,
    KB_1         = 0x000016,
    KB_2         = 0x00001E,
    KB_3         = 0x000026,
    KB_4         = 0x000025,
    KB_5         = 0x00002E,
    KB_6         = 0x000036,
    KB_7         = 0x00003D,
    KB_8         = 0x00003E,
    KB_9         = 0x000046,
    KB_PIE       = 0x00000E,    /* Ʋ���������Ͻ� */
    KB_SUB       = 0x00004E,    /* �иܣ����� */
    KB_EQU       = 0x000055,    /* �Ⱥ� */
    KB_FXG       = 0x00005D,    /* ��б�� */
    KB_BKSP      = 0x000066,
    KB_SPACE     = 0x000029,
    KB_TAB       = 0x00000D,
    KB_CAPS      = 0x000058,
    KB_L_SHFT    = 0x000012,
    KB_L_CTRL    = 0x000014,
    KB_L_GUI     = 0x00E01F,
    KB_L_ALT     = 0x000011,
    KB_R_SHFT    = 0x000059,
    KB_R_CTRL    = 0x00E014,
    KB_R_GUI     = 0x00E027,
    KB_R_ALT     = 0x00E011,
    KB_APPS      = 0x00E02F,
    KB_ENTER     = 0x00005A,
    KB_ESC       = 0x000076,
    KB_F1        = 0x000005,
    KB_F2        = 0x000006,
    KB_F3        = 0x000004,
    KB_F4        = 0x00000C,
    KB_F5        = 0x000003,
    KB_F6        = 0x00000B,
    KB_F7        = 0x000083,
    KB_F8        = 0x00000A,
    KB_F9        = 0x000001,
    KB_F10       = 0x000009,
    KB_F11       = 0x000078,
    KB_F12       = 0x000007,
    //KB_PRNT_SCRN = 0xE012E07C,    /* E0 12 E0 7C */
    KB_SCROLL    = 0x00007E,
    //KB_PAUSE     = 0xE11477E1,    /* E1,14,77,    E1,F0,14, F0,77  �����������int���޾��� */
    KB_ZZKH      = 0x000054,    /* [ �������� */
    KB_INSERT    = 0x00E070,
    KB_HOME      = 0x00E06C,
    KB_PGUP      = 0x00E07D,
    KB_DELETE    = 0x00E071,
    KB_END       = 0x00E069,
    KB_PGDN      = 0x00E07A,
    KB_U_ARROW   = 0x00E075,
    KB_L_ARROW   = 0x00E06B,
    KB_D_ARROW   = 0x00E072,
    KB_R_ARROW   = 0x00E074,
    KB_NUM       = 0x000077,
    KB_KP_DIV    = 0x00E04A,    /* С���̳���  KP ��ʾС���� */
    KB_KP_MULT   = 0x00007C,    /* С���̳˺� */
    KB_KP_SUB    = 0x00007B,    /* - */
    KB_KP_ADD    = 0x000079,    /* + */
    KB_KP_ENTER  = 0x00E05A,
    KB_KP_DOT    = 0x000071,    /* С���� */
    KB_KP_0      = 0x000070,
    KB_KP_1      = 0x000069,
    KB_KP_2      = 0x000072,
    KB_KP_3      = 0x00007A,
    KB_KP_4      = 0x00006B,
    KB_KP_5      = 0x000073,
    KB_KP_6      = 0x000074,
    KB_KP_7      = 0x00006C,
    KB_KP_8      = 0x000075,
    KB_KP_9      = 0x00007D,
    KB_YZKH      = 0x00005B,    /* ] �������� */
    KB_SEMICOLON = 0x00004C,    /* ; �ֺ� */
    KB_QUOTES    = 0x000052,    /* ������ */
    KB_COMMA     = 0x000041,    /* ���� */
    KB_DOT       = 0x000049,    /* С���� */
    KB_DIV       = 0x00004A,    /* ���� */

    /* �����Ǽ����ͷŵĴ��� */
    BREAK_L_SHFT    = 0x00F012,
    BREAK_L_CTRL    = 0x00F014,
    BREAK_L_ALT     = 0x00F011,
    BREAK_R_SHFT    = 0x00F059,
    BREAK_R_CTRL    = 0xE0F014,
    BREAK_R_ALT     = 0xE0F011,
};

/*
	Key        Make Code    (Set 2) Break Code
	"A"           1C              F0,1C
	"5"           2E              F0,2E
	"F10"         09              F0,09
	Right Arrow   E0, 74       E0, F0, 74
	Right "Ctrl"  E0, 14       E0, F0, 14
*/

/* ����FIFO�õ����� */
#define PS2_FIFO_SIZE	10

#define PS2_MAX_LEN	10
typedef struct
{
	uint32_t Buf[PS2_FIFO_SIZE];		/* ��ֵ������ */
	uint8_t Read;					/* ��������ָ��1 */
	uint8_t Write;					/* ������дָ�� */

	uint8_t Status;
	uint8_t CodeBuf[PS2_MAX_LEN];	/* ԭʼ���ݻ����� */
	uint8_t Len;					/* ���ݳ��� */
	uint8_t TxTimeOut;				/* �����������ʱ */
	uint8_t RxTimeOut;				/* ��������PS2�豸���ݰ���ʱ */	

	uint8_t Sending;				/* 1��ʾ ���������ͼ��̷�������״̬ */
	uint8_t Cmd;
	uint8_t Ack;					/* �������豸����1�ֽں��豸��������Ӧ���ź�,����ACK = 0 */

	/* ����״̬λ */
	uint8_t ksShift;				/* 1��ʾ Shift �������� */
	uint8_t ksCtrl;					/* 1��ʾ Ctrl �������� */
	uint8_t ksAlt;					/* 1��ʾ Alt �������� */
	uint8_t ksCapsLock;				/* 1��ʾ CapsLock ״ָ̬ʾ���� */
	uint8_t KsNumLock;				/* 1��ʾ NumLock ״ָ̬ʾ���� */
	uint8_t KsScrollLock;			/* 1��ʾ ScrollLock ״ָ̬ʾ���� */
	
	/* �޸ļ��̵�״̬�õ��ı��� */
	uint8_t LedReq;					/* �������⵽ 1 ʱ, ����������Ƽ��̵�״̬ */
	uint8_t LedData;				
}PS2_T;

/* ������ݽṹ�� */
typedef struct
{
	uint8_t Intelli;	/* 1��ʾ������� */
	uint8_t Xoverflow;
	uint8_t Yoverflow;
	int16_t Xmove;
	int16_t Ymove;
	int16_t Zmove;
	uint8_t BtnLeft;
	uint8_t BtnMid;
	uint8_t BtnRight;
	uint8_t Btn4;
	uint8_t Btn5;
}MOUSE_PACKET_T;

void bsp_InitPS2(void);
void PS2_StartWork(void);
void PS2_StopWork(void);

void PS2_ClearBuf(void);
void PS2_PutMsg(uint32_t _KeyCode);
uint32_t PS2_GetMsg(void);
void PS2_Timer(void);
uint8_t PS2_GetDevceType(void);
uint8_t PS2_InitMouse(void);
void PS2_DecodeMouse(uint32_t _input, MOUSE_PACKET_T *_res);
uint8_t PS2_IsMousePacket(uint32_t _input);

uint8_t PS2_InitKeyboard(void);
void PS2_SetKeyboardLed(uint8_t _id, uint8_t _on);
const char * GetNameOfKey(uint32_t _code);

extern PS2_T g_tPS2;

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
