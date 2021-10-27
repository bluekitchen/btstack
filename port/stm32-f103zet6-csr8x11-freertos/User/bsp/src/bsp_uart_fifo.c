/*
*********************************************************************************************************
*
*	濡繝鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� : 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔峰建鐠佽瀚�+FIFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚瑰Ο锟犳晸閺傘倖瀚�
*	闁跨喍鑼庣涵閿嬪闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� : bsp_uart_fifo.c
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟� : V1.0
*	鐠囷拷    闁跨喐鏋婚幏锟� : 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚归柨鐔峰建鐠佽瀚�+FIFO濡€崇础鐎圭偤鏁撶悰妤勵啇閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐤樁闁跨喖鍙洪銉﹀敾閹风兘鏁撻弬銈嗗闁跨噦鎷�
*	闁跨喓娼鹃弨鍦€嬮幏宄扮秿 :
*		闁跨喕濮ら張顒勬晸閺傘倖瀚�  闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�       闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�    鐠囨挳鏁撻弬銈嗗
*		V1.0    2013-02-01 armfly  闁跨喐鏋婚幏宄扮础闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�
*		V1.1    2013-06-09 armfly  FiFo闁跨喓绮ㄩ弸鍕晸閺傘倖瀚归柨鐔告灮閹风ǖxCount闁跨喐鏋婚幏宄版喅闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閸欘偅鏌囨导娆愬闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷; 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏绋Fo闁跨喍鑼庨悮瀛樺闁跨喐鏋婚幏锟�
*		V1.2	2014-09-29 armfly  闁跨喐鏋婚幏鐑芥晸閺傘倖瀚筊S485 MODBUS闁跨喐甯撮崣锝傛閹风兘鏁撻弬銈嗗闁跨喓笑绾板瀚归柨鐔告灮閹风兘鏁撶悰妤勫Ν閻氬瓨瀚归惄鎾晸閺傘倖瀚归幍褔鏁撻崣顐㈡礀绾板瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏锟�
*
*	闁跨喓娼鹃弨纭咁嚋閹风兘鏁撻弬銈嗗 : 
*		闁跨喕濮ら張顒勬晸閺傘倖瀚�    闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�       闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�                  鐠囨挳鏁撻弬銈嗗
*		V1.0    2015-08-19  Eric2013       闁跨喎濮弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏绋eeRTOS闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�
*
*	Copyright (C), 2013-2014, 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏锟� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"


/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚瑰В蹇涙晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻懞鍌滅波閺嬪嫰鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閿燂拷 */
#if UART1_FIFO_EN == 1
	static UART_T g_tUart1;
	static uint8_t g_TxBuf1[UART1_TX_BUF_SIZE];		/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	static uint8_t g_RxBuf1[UART1_RX_BUF_SIZE];		/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
#endif

#if UART2_FIFO_EN == 1
	static UART_T g_tUart2;
	static uint8_t g_TxBuf2[UART2_TX_BUF_SIZE];		/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	static uint8_t g_RxBuf2[UART2_RX_BUF_SIZE];		/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
#endif

#if UART3_FIFO_EN == 1
	static UART_T g_tUart3;
	static uint8_t g_TxBuf3[UART3_TX_BUF_SIZE];		/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	static uint8_t g_RxBuf3[UART3_RX_BUF_SIZE];		/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
#endif

#if UART4_FIFO_EN == 1
	static UART_T g_tUart4;
	static uint8_t g_TxBuf4[UART4_TX_BUF_SIZE];		/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	static uint8_t g_RxBuf4[UART4_RX_BUF_SIZE];		/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
#endif

#if UART5_FIFO_EN == 1
	static UART_T g_tUart5;
	static uint8_t g_TxBuf5[UART5_TX_BUF_SIZE];		/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	static uint8_t g_RxBuf5[UART5_RX_BUF_SIZE];		/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
#endif

static void UartVarInit(void);

static void InitHardUart(void);
static void UartSend(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen);
static uint8_t UartGetChar(UART_T *_pUart, uint8_t *_pByte);
static void UartIRQ(UART_T *_pUart);
static void ConfigUartNVIC(void);

void RS485_InitTXE(void);

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: bsp_InitUart
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏宄邦潗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽鈥栭柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归崗銊╂晸鐞涙鍞婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸婏拷.
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�:  闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void bsp_InitUart(void)
{
	UartVarInit();		/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔笺偤缁涜瀚规慨瀣晸閺傘倖瀚归崗銊╂晸鐞涙鍞婚幏鐑芥晸閺傘倖瀚�,闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽鈥栭柨鐔告灮閹凤拷 */

	InitHardUart();		/* 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閼哄倻顣幏椋庘€栭柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏锟�(闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔虹哺绾板瀚�) */
	
#if RS485_ENABLE
	RS485_InitTXE();	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚筊S485閼侯垳澧栭柨鐔惰寧閸戙倖瀚归柨鐔告灮閹疯渹濞囬柨鐔告灮閹烽鈥栭柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨噦鎷� */
#endif
	
	ConfigUartNVIC();	/* 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚归柨鐔峰建鐠佽瀚� */
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: ComToUart
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏绋M闁跨喎澹欓崣锝囧皑閹风柉娴嗛柨鐔告灮閹疯渹璐烾ART閹稿洭鏁撻弬銈嗗
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _ucPort: 闁跨喎澹欓崣锝囧皑閹凤拷(COM1 - COM6)
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: uart閹稿洭鏁撻弬銈嗗
*********************************************************************************************************
*/
UART_T *ComToUart(COM_PORT_E _ucPort)
{
	if (_ucPort == COM1)
	{
		#if UART1_FIFO_EN == 1
			return &g_tUart1;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM2)
	{
		#if UART2_FIFO_EN == 1
			return &g_tUart2;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM3)
	{
		#if UART3_FIFO_EN == 1
			return &g_tUart3;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM4)
	{
		#if UART4_FIFO_EN == 1
			return &g_tUart4;
		#else
			return 0;
		#endif
	}
	else if (_ucPort == COM5)
	{
		#if UART5_FIFO_EN == 1
			return &g_tUart5;
		#else
			return 0;
		#endif
	}
	else
	{
		/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔诲▏娴ｆ洝鎻幏鐑芥晸閺傘倖瀚� */
		return 0;
	}
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: comSendBuf
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐瑙︽稉鎻掑經閸戙倖瀚归柨鐔告灮閹疯渹绔撮柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐宓庨埥鍛闁跨喐鏋婚幏鐑芥晸閹归攱鏂佺喊澶嬪闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幋顏庣秶閹风兘鏁撻弬銈嗗闁跨喎褰ㄩ弬顓炲殩閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠銈夋晸閺佹瑯鐓夐幏鐑芥晸缂傛潙鍤栭幏鐑芥晸閺傘倖瀚�
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _ucPort: 闁跨喎澹欓崣锝囧皑閹凤拷(COM1 - COM6)
*			  _ucaBuf: 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔煎徍绾板瀚归柨鐔告灮閹风兘鏁撻幑铚傜串閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏锟�
*			  _usLen : 闁跨喐鏋婚幏鐑芥晸閹归顒查幏鐑芥晸閺傘倖瀚�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void comSendBuf(COM_PORT_E _ucPort, uint8_t *_ucaBuf, uint16_t _usLen)
{
	UART_T *pUart;

	pUart = ComToUart(_ucPort);
	if (pUart == 0)
	{
		return;
	}

	if (pUart->SendBefor != 0)
	{
		pUart->SendBefor();		/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔虹ゲS485闁岸鏁撻懘姘剧秶閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归挅顏堟晸缁插485闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔告灮閹烽攱膩瀵拷 */
	}

	UartSend(pUart, _ucaBuf, _usLen);
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: comSendChar
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐瑙︽稉鎻掑經閸戙倖瀚归柨鐔告灮閹凤拷1闁跨喐鏋婚幏鐑芥晸鐞涙濡埥鍛闁跨喐鏋婚幏鐑芥晸閹归攱鏂佺喊澶嬪闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幋顏庣秶閹风兘鏁撻弬銈嗗闁跨喎褰ㄩ弬顓炲殩閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠銈夋晸閺佹瑯鐓夐幏鐑芥晸缂傛潙鍤栭幏鐑芥晸閺傘倖瀚�
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _ucPort: 闁跨喎澹欓崣锝囧皑閹凤拷(COM1 - COM6)
*			  _ucByte: 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔煎徍绾板瀚归柨鐔告灮閹风兘鏁撻弬銈嗗
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void comSendChar(COM_PORT_E _ucPort, uint8_t _ucByte)
{
	comSendBuf(_ucPort, &_ucByte, 1);
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: comGetChar
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐甯存潏鐐闁跨喕濡导娆愬闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲褰�1闁跨喕顢滈懞鍌︾秶閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐宓庨幘鍛闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _ucPort: 闁跨喎澹欓崣锝囧皑閹凤拷(COM1 - COM6)
*			  _pByte: 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幑鐤彧閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲娼�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 0 闁跨喐鏋婚幏椋庛仛闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷, 1 闁跨喐鏋婚幏椋庛仛闁跨喐鏋婚幏宄板絿闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归弫鍫ユ晸鐞涙鏋婚幏锟�
*********************************************************************************************************
*/
uint8_t comGetChar(COM_PORT_E _ucPort, uint8_t *_pByte)
{
	UART_T *pUart;

	pUart = ComToUart(_ucPort);
	if (pUart == 0)
	{
		return 0;
	}

	return UartGetChar(pUart, _pByte);
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: comGetChar
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐甯存潏鐐闁跨喕濡导娆愬闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲褰噉闁跨喕顢滈懞鍌︾秶閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐宓庨幘鍛闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _ucPort: 闁跨喎澹欓崣锝囧皑閹凤拷(COM1 - COM6)
*			  _pBuf: 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幑鐤彧閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲娼�
*			  _usLen: 閸戝棝鏁撻弬銈嗗闁跨喐鏋婚幏宄板絿闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔稿祹缁涜瀚归柨鐔告灮閹凤拷
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 鐎圭偤鏁撶紒鐐额啇閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔稿祹缁涜瀚归柨鐔笺偤閿濆繑瀚�0 闁跨喐鏋婚幏椋庛仛闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
*********************************************************************************************************
*/
uint16_t comGetBuf(COM_PORT_E _ucPort, uint8_t *_pBuf, const uint16_t _usLenToRead)
{
	UART_T *pUart;
	uint16_t usCount;
	uint16_t usRightCount;
	uint16_t usLeftCount;
	pUart = ComToUart(_ucPort);
	if (pUart == 0)
	{
		return 0;
	}
	/* usRxWrite 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崣顐ｆ焽閻氬瓨瀚归柨鐔告灮閹风兘鏁撻崣顐ｅ敾閹风兘鏁撻弬銈嗗閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撴琛℃閹峰嘲闄勯柨鐔告灮閹风兘鏁撶紒鐐村敾閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崝顐ｆ灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
	DISABLE_INT();
	usCount = pUart->usRxCount;
	ENABLE_INT();

	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崣顐ユ彧閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸闁版鍓ㄩ幏鐑芥晸閺傘倖瀚归摀宥囥偡闁跨噦鎷�0 */
	if (usCount == 0)	/* 闁跨喓鐛ら幘鍛濞岋繝鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
	{
		return 0;
	}
	else if (usCount < _usLenToRead) 
	{
		DISABLE_INT();
		if (pUart->usRxRead > pUart->usRxWrite) {
			usRightCount = pUart->usRxBufSize - pUart->usRxRead;
			usLeftCount = usCount - usRightCount;
			memcpy(_pBuf, &pUart->pRxBuf[pUart->usRxRead], usRightCount);
			memcpy(_pBuf + usRightCount, pUart->pRxBuf, usLeftCount);
		}
		else 
		{
			memcpy(_pBuf, &pUart->pRxBuf[pUart->usRxRead], usCount);
		}
		pUart->usRxRead = pUart->usRxWrite; /*闁跨喐鏋婚幏铚傜秴FIFO wptr * rptr閹稿洭鏁撻弬銈嗗*/
		pUart->usRxCount = 0;
		ENABLE_INT();
		return usCount;
	}
	else
	{
		DISABLE_INT();
		if (pUart->usRxRead > pUart->usRxWrite) {
			usRightCount = pUart->usRxBufSize - pUart->usRxRead;
			usLeftCount = _usLenToRead - usRightCount;
			memcpy(_pBuf, &pUart->pRxBuf[pUart->usRxRead], usRightCount);
			memcpy(_pBuf + usRightCount, pUart->pRxBuf, usLeftCount);
			pUart->usRxRead = usLeftCount; /*闁跨喐鏋婚幏铚傜秴FIFO wptr * rptr閹稿洭鏁撻弬銈嗗*/
		}
		else 
		{
			memcpy(_pBuf, &pUart->pRxBuf[pUart->usRxRead], _usLenToRead);
			pUart->usRxRead += _usLenToRead; /*闁跨喐鏋婚幏铚傜秴FIFO wptr * rptr閹稿洭鏁撻弬銈嗗*/
		}
		pUart->usRxCount -= _usLenToRead;
		ENABLE_INT();
		return _usLenToRead;
	}
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: comClearTxFifo
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏宄板絿闁跨喐鏋婚幏鐑芥晸閼哄倷绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风〾vailable buffer length
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _ucPort: 闁跨喎澹欓崣锝囧皑閹凤拷(COM1 - COM6)
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
uint16_t comGetRxFifoAvailableBufferLength(COM_PORT_E _ucPort)
{
	UART_T *pUart;

	pUart = ComToUart(_ucPort);
	if (pUart == 0)
	{
		return 0;
	}
	return pUart->usRxCount;
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: comClearTxFifo
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏鐑芥晸婵劒瑕嗛柨鐔诲Ν閸戙倖瀚归柨鐔煎徍娴兼瑦瀚归柨鐔告灮閹风兘鏁撻弬銈嗗
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _ucPort: 闁跨喎澹欓崣锝囧皑閹凤拷(COM1 - COM6)
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void comClearTxFifo(COM_PORT_E _ucPort)
{
	UART_T *pUart;

	pUart = ComToUart(_ucPort);
	if (pUart == 0)
	{
		return;
	}

	pUart->usTxWrite = 0;
	pUart->usTxRead = 0;
	pUart->usTxCount = 0;
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: comClearRxFifo
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏鐑芥晸婵劒瑕嗛柨鐔诲Ν閺傘倖瀚归柨鐔盒︽导娆愬闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _ucPort: 闁跨喎澹欓崣锝囧皑閹凤拷(COM1 - COM6)
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void comClearRxFifo(COM_PORT_E _ucPort)
{
	UART_T *pUart;

	pUart = ComToUart(_ucPort);
	if (pUart == 0)
	{
		return;
	}

	pUart->usRxWrite = 0;
	pUart->usRxRead = 0;
	pUart->usRxCount = 0;
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: bsp_SetUart1Baud
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喓娼鹃棃鈺傚UART1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void bsp_SetUart1Baud(uint32_t _baud)
{
	USART_InitTypeDef USART_InitStructure;

	/* 闁跨喐鏋婚幏锟�2闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚圭涵顒勬晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	USART_InitStructure.USART_BaudRate = _baud;	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: bsp_SetUart2Baud
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喓娼鹃棃鈺傚UART2闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void bsp_SetUart2Baud(uint32_t _baud)
{
	USART_InitTypeDef USART_InitStructure;

	/* 闁跨喐鏋婚幏锟�2闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚圭涵顒勬晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	USART_InitStructure.USART_BaudRate = _baud;	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);
}


/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔虹ゲS485闁岸鏁撻懘姘剧秶閹风兘鏁撶拠顐ｅ瘻闁跨喐鏋婚幏鐑芥晸闁句即娼婚幏宄扮础闁跨喐鏋婚幏宄板晸闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 闁跨喐鏋婚幏鐑芥晸鐟欐帗鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 USART3闁跨喐鏋婚幏铚傝礋RS485闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: RS485_InitTXE
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚筊S485闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规担鍧楁晸閺夋澘灏呴幏鐑芥晸閺傘倖瀚� TXE
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void RS485_InitTXE(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_RS485_TXEN, ENABLE);	/* 闁跨喐鏋婚幏绋IO閺冨爼鏁撻弬銈嗗 */

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撴笟銉悏閿燂拷 */
	GPIO_InitStructure.GPIO_Pin = PIN_RS485_TXEN;
	GPIO_Init(PORT_RS485_TXEN, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: bsp_Set485Baud
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喓娼鹃棃鈺傚UART3闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void bsp_Set485Baud(uint32_t _baud)
{
	USART_InitTypeDef USART_InitStructure;

	/* 闁跨喐鏋婚幏锟�2闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚圭涵顒勬晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	USART_InitStructure.USART_BaudRate = _baud;	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: RS485_SendBefor
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸撳秹鏁撻弬銈嗗閸戝棝鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏绋睸485闁岸鏁撻懘姘剧秶閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚筊S485閼侯垳澧栨稉娲晸閺傘倖瀚归柨鐔告灮閹烽濮搁幀渚€鏁撻弬銈嗗
*			  闁跨喐鏋婚幏鐑芥晸閻偊娼婚幏锟� UartVarInit()闁跨喎褰ㄩ惃鍕皑閹风兘鏁撻弬銈嗗閹稿洭鏁撻弬銈嗗闁跨喐鏋婚幏鐤槰闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐕傛嫹 g_tUart2.SendBefor = RS485_SendBefor
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void RS485_SendBefor(void)
{
	RS485_TX_EN();	/* 闁跨喎褰ㄦ导娆愬RS485闁跨喓笑閸戙倖瀚归懞顖滃娑撴椽鏁撻弬銈嗗闁跨喐鏋婚幏閿嬆佸锟� */
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: RS485_SendOver
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉鈧柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐宓庨弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲娼介柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔虹ゲS485闁岸鏁撻懘姘剧秶閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚筊S485閼侯垳澧栨稉娲晸閺傘倖瀚归柨鐔告灮閹烽濮搁幀渚€鏁撻弬銈嗗
*			  闁跨喐鏋婚幏鐑芥晸閻偊娼婚幏锟� UartVarInit()闁跨喎褰ㄩ惃鍕皑閹风兘鏁撻弬銈嗗閹稿洭鏁撻弬銈嗗闁跨喐鏋婚幏鐤槰闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐕傛嫹 g_tUart2.SendOver = RS485_SendOver
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void RS485_SendOver(void)
{
	RS485_RX_EN();	/* 闁跨喎褰ㄦ导娆愬RS485闁跨喓笑閸戙倖瀚归懞顖滃娑撴椽鏁撻弬銈嗗闁跨喐鏋婚幏閿嬆佸锟� */
}


/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: RS485_SendBuf
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁岸鏁撻弬銈嗗RS485閼侯垳澧栭柨鐔告灮閹风兘鏁撻弬銈嗗娑撯偓闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔稿祹閳藉懏瀚瑰▔銊╂晸鐟欙綇绱濋柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔笺偤鏉堢偓瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏椋庘枅闁跨噦鎷�
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void RS485_SendBuf(uint8_t *_ucaBuf, uint16_t _usLen)
{
	comSendBuf(COM3, _ucaBuf, _usLen);
}


/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: RS485_SendStr
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏锟�485闁跨喐鏋婚幏鐑芥晸缁旑厼鍤栭幏鐑芥晸閺傘倖瀚规稉鈧柨鐔告灮閹风兘鏁撶悰妤€鍤栭幏鐑芥晸閺傘倖瀚�
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _pBuf 闁跨喐鏋婚幏鐑芥晸閹硅渹绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
*			 _ucLen 闁跨喐鏋婚幏鐑芥晸閹归顒查幏鐑芥晸閺傘倖瀚�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
void RS485_SendStr(char *_pBuf)
{
	RS485_SendBuf((uint8_t *)_pBuf, strlen(_pBuf));
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: RS485_ReciveNew
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸闁炬壆顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _byte 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
//extern void MODBUS_ReciveNew(uint8_t _byte);
void RS485_ReciveNew(uint8_t _byte)
{
//	MODBUS_ReciveNew(_byte);
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: UartVarInit
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏宄邦潗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗濮樻劘鐨ㄩ柨鐔告灮閹风兘鏁撻敓锟�
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
extern void bt_driver_recieve_data_from_controller(uint8_t data);
extern void at_command_uart_rx_isr_handler(uint8_t data);
static void UartVarInit(void)
{
#if UART1_FIFO_EN == 1
	g_tUart1.uart = USART1;						/* STM32 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔封偓鐔奉槵 */
	g_tUart1.pTxBuf = g_TxBuf1;					/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart1.pRxBuf = g_RxBuf1;					/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart1.usTxBufSize = UART1_TX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart1.usRxBufSize = UART1_RX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart1.usTxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart1.usTxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart1.usRxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart1.usRxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart1.usRxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐宓庨棃鈺傚闁跨喐鏋婚幏锟� */
	g_tUart1.usTxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔煎徍绾板瀚归柨鐔告灮閹风兘鏁撻幑鐑芥交閹风兘鏁撻弬銈嗗 */
	g_tUart1.SendBefor = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸撳秹鏁撴笟銉ユ礀绾板瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	g_tUart1.SendOver = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉娅ㄩ柨鐔惰寧閸ョ偟顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart1.ReciveNew = at_command_uart_rx_isr_handler;	/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幑椋庡皑閹峰嘲绠峰鎰版晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻敓锟� */
#endif

#if UART2_FIFO_EN == 1
	g_tUart2.uart = USART2;						/* STM32 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔封偓鐔奉槵 */
	g_tUart2.pTxBuf = g_TxBuf2;					/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart2.pRxBuf = g_RxBuf2;					/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart2.usTxBufSize = UART2_TX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart2.usRxBufSize = UART2_RX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart2.usTxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart2.usTxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart2.usRxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart2.usRxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart2.usRxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐宓庨棃鈺傚闁跨喐鏋婚幏锟� */
	g_tUart2.usTxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔煎徍绾板瀚归柨鐔告灮閹风兘鏁撻幑鐑芥交閹风兘鏁撻弬銈嗗 */
	g_tUart2.SendBefor = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸撳秹鏁撴笟銉ユ礀绾板瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	g_tUart2.SendOver = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉娅ㄩ柨鐔惰寧閸ョ偟顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart2.ReciveNew = 0; //bt_driver_recieve_data_from_controller;/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幑椋庡皑閹峰嘲绠峰鎰版晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻敓锟� */
#endif

#if UART3_FIFO_EN == 1
	g_tUart3.uart = USART3;						/* STM32 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔封偓鐔奉槵 */
	g_tUart3.pTxBuf = g_TxBuf3;					/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart3.pRxBuf = g_RxBuf3;					/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart3.usTxBufSize = UART3_TX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart3.usRxBufSize = UART3_RX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart3.usTxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart3.usTxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart3.usRxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart3.usRxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart3.usRxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐宓庨棃鈺傚闁跨喐鏋婚幏锟� */
	g_tUart3.usTxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔煎徍绾板瀚归柨鐔告灮閹风兘鏁撻幑鐑芥交閹风兘鏁撻弬銈嗗 */
	g_tUart3.SendBefor = 0;//RS485_SendBefor;		/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸撳秹鏁撴笟銉ユ礀绾板瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	g_tUart3.SendOver = 0;//RS485_SendOver;			/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉娅ㄩ柨鐔惰寧閸ョ偟顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart3.ReciveNew = 0;//RS485_ReciveNew;		/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幑椋庡皑閹峰嘲绠峰鎰版晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻敓锟� */
#endif

#if UART4_FIFO_EN == 1
	g_tUart4.uart = UART4;						/* STM32 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔封偓鐔奉槵 */
	g_tUart4.pTxBuf = g_TxBuf4;					/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart4.pRxBuf = g_RxBuf4;					/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart4.usTxBufSize = UART4_TX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart4.usRxBufSize = UART4_RX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart4.usTxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart4.usTxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart4.usRxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart4.usRxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart4.usRxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐宓庨棃鈺傚闁跨喐鏋婚幏锟� */
	g_tUart4.usTxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔煎徍绾板瀚归柨鐔告灮閹风兘鏁撻幑鐑芥交閹风兘鏁撻弬銈嗗 */
	g_tUart4.SendBefor = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸撳秹鏁撴笟銉ユ礀绾板瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	g_tUart4.SendOver = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉娅ㄩ柨鐔惰寧閸ョ偟顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart4.ReciveNew = 0;						/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幑椋庡皑閹峰嘲绠峰鎰版晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻敓锟� */
#endif

#if UART5_FIFO_EN == 1
	g_tUart5.uart = UART5;						/* STM32 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔封偓鐔奉槵 */
	g_tUart5.pTxBuf = g_TxBuf5;					/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart5.pRxBuf = g_RxBuf5;					/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart5.usTxBufSize = UART5_TX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart5.usRxBufSize = UART5_RX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart5.usTxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart5.usTxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart5.usRxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart5.usRxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart5.usRxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐宓庨棃鈺傚闁跨喐鏋婚幏锟� */
	g_tUart5.usTxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔煎徍绾板瀚归柨鐔告灮閹风兘鏁撻幑鐑芥交閹风兘鏁撻弬銈嗗 */
	g_tUart5.SendBefor = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸撳秹鏁撴笟銉ユ礀绾板瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	g_tUart5.SendOver = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉娅ㄩ柨鐔惰寧閸ョ偟顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart5.ReciveNew = 0;						/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幑椋庡皑閹峰嘲绠峰鎰版晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻敓锟� */
#endif


#if UART6_FIFO_EN == 1
	g_tUart6.uart = USART6;						/* STM32 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔封偓鐔奉槵 */
	g_tUart6.pTxBuf = g_TxBuf6;					/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart6.pRxBuf = g_RxBuf6;					/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱瀵氶柨鐔告灮閹凤拷 */
	g_tUart6.usTxBufSize = UART6_TX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart6.usRxBufSize = UART6_RX_BUF_SIZE;	/* 闁跨喐鏋婚幏鐑芥晸缁夐晲绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗鐏忥拷 */
	g_tUart6.usTxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart6.usTxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart6.usRxWrite = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏锟� */
	g_tUart6.usRxRead = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笷IFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart6.usRxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐宓庨棃鈺傚闁跨喐鏋婚幏锟� */
	g_tUart6.usTxCount = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔煎徍绾板瀚归柨鐔告灮閹风兘鏁撻幑鐑芥交閹风兘鏁撻弬銈嗗 */
	g_tUart6.SendBefor = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸撳秹鏁撴笟銉ユ礀绾板瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	g_tUart6.SendOver = 0;						/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉娅ㄩ柨鐔惰寧閸ョ偟顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	g_tUart6.ReciveNew = 0;						/* 闁跨喐鏋婚幏鐑芥晸缁夊摜顣幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幑椋庡皑閹峰嘲绠峰鎰版晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻敓锟� */
#endif
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: InitHardUart
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閼哄倻顣幏椋庘€栭柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喓绮搁敐蹇斿闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规担宥夋晸閺傘倖瀚归崑婊勵剾娴ｅ秹鏁撻弬銈嗗闁跨喐鏋婚幏宄邦潗娴ｅ秹鏁撻弬銈嗗閺嶏繝鏁撻弬銈嗗娴ｅ秹鏁撻弬銈嗗闁跨喎褰ㄧ拋瑙勫娴ｅ潡鏁撻弶甯秶閹风兘鏁撶紒鐐靛皑閹风兘鏁撻弬銈嗗STM32-F4闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
static void InitHardUart(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

#if UART1_FIFO_EN == 1		/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�1 TX = PA9   RX = PA10 闁跨喐鏋婚幏锟� TX = PB6   RX = PB7*/

	/* 闁跨喐鏋婚幏锟�1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ěPIO闁跨喐鏋婚幏绋禨ART闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱妞傞柨鐔告灮閹凤拷 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	/* 闁跨喐鏋婚幏锟�2闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ǘSART Tx闁跨喐鏋婚幏绋IO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔风溇婢跺秹鏁撻弬銈嗗濡€崇础 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 闁跨喐鏋婚幏锟�3闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ǘSART Rx闁跨喐鏋婚幏绋IO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏閿嬆佸锟�
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笴PU闁跨喐鏋婚幏铚傜秴闁跨喐鏋婚幏绋IO缂傝櫣娓烽柨鐔告灮閹风兘鏁撶憴鎺楁交閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚瑰Ο鈥崇础闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崐鐔剁瑝闁跨喕顫楅幉瀣闁跨喐鏋婚幏鐑芥晸閿燂拷
		闁跨喐鏋婚幏鐑芥晸鐟欐帪缍囬幏鐑芥晸閹活厺绱幏鐑芥晸鐟欐帗鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲顎掗柨鐔告灮閹风兘鏁撻弬銈嗗娴溾晠鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归弴鎶芥晸鐞涙娅㈤幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉鍎伴柨鐔告灮閹风柉鏌楅柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔侯仾绾板瀚归柨鐔告灮閹风兘鏁撻惌顐ヮ嚋閹风兘鏁撻弬銈嗗
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/* 闁跨喐鏋婚幏锟�4闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚圭涵顒勬晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	USART_InitStructure.USART_BaudRate = UART1_BAUD;	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);	/* 娴ｅ潡鏁撻弶鐗堟灮閹风兘鏁撻弬銈嗗闁跨喎褰ㄧ拋瑙勫 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		濞夈劑鏁撻弬銈嗗: 闁跨喐鏋婚幏鐤洣闁跨喕濡銈堟彧閹风兘鏁撳ú銉ョ磻閸戙倖瀚归柨鐔告灮閹风兘鏁撻崣顐ヮ啇閹凤拷
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔峰建鐠佽瀚规担鍧楁晸閺傘倖瀚归柨鐔告灮閹风ùendUart()闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
	*/
	USART_Cmd(USART1, ENABLE);		/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗 */

	/* CPU闁跨喐鏋婚幏宄扮毈缂傛椽鏁撻幑鍑ょ秶閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔虹叓婵傛枻缍囬幏鐑芥晸閺傘倖瀚归柨鐔活敎閹插瀚归柨鐔虹コend闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐕傛嫹1闁跨喐鏋婚幏鐑芥晸鐞涙濡崙銈嗗闁跨喖鍙虹拠褎瀚归柨鐔告灮閹峰嘲骞�
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閿燂拷1闁跨喐鏋婚幏鐑芥晸鐞涙鏋婚幏鐑芥晸閻偄鍤栭幏鐑芥晸閺傘倖瀚圭涵顕€鏁撻弬銈嗗闁跨喖鍙虹粵瑙勫閸樺鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
	USART_ClearFlag(USART1, USART_FLAG_TC);     /* 闁跨喕妫勯崣鎴︽晸閺傘倖瀚归柨鐔告灮閹烽攱娼冮柨鐔活敎閹惧懏瀚归柨鐔虹ゴransmission Complete flag */
#endif

#if UART2_FIFO_EN == 1		/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�2 TX = PA2闁跨喐鏋婚幏锟� RX = PA3  */
/******************************************************************************
 * description : Initialization of USART2.PA0->CTS PA1->RTS PA2->TX PA3->RX
******************************************************************************/
	/* 闁跨喐鏋婚幏锟�1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ěPIO闁跨喐鏋婚幏绋禨ART闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱妞傞柨鐔告灮閹凤拷 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	/* 闁跨喐鏋婚幏锟�2闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ǘSART Tx闁跨喐鏋婚幏绋IO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔风溇婢跺秹鏁撻弬銈嗗濡€崇础 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1|GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 闁跨喐鏋婚幏锟�3闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ǘSART Rx闁跨喐鏋婚幏绋IO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏閿嬆佸锟�
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笴PU闁跨喐鏋婚幏铚傜秴闁跨喐鏋婚幏绋IO缂傝櫣娓烽柨鐔告灮閹风兘鏁撶憴鎺楁交閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚瑰Ο鈥崇础闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崐鐔剁瑝闁跨喕顫楅幉瀣闁跨喐鏋婚幏鐑芥晸閿燂拷
		闁跨喐鏋婚幏鐑芥晸鐟欐帪缍囬幏鐑芥晸閹活厺绱幏鐑芥晸鐟欐帗鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲顎掗柨鐔告灮閹风兘鏁撻弬銈嗗娴溾晠鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归弴鎶芥晸鐞涙娅㈤幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉鍎伴柨鐔告灮閹风柉鏌楅柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔侯仾绾板瀚归柨鐔告灮閹风兘鏁撻惌顐ヮ嚋閹风兘鏁撻弬銈嗗
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	/*  闁跨喐鏋婚幏锟�3闁跨喐鏋婚幏鐑芥晸缁愭牗鎷濋幏鐑芥晸閺傘倖瀚归柨鐔峰閿濆繑瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閻涚姵鏋婚幏鐑芥晸閺傘倖瀚归崷鍡涙晸閺傘倖瀚归柨鐕傛嫹
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	*/
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 闁跨喐鏋婚幏锟�4闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚圭涵顒勬晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	USART_InitStructure.USART_BaudRate = UART2_BAUD;	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS_CTS;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;		/* 闁跨喐鏋婚幏鐑解偓澶愭晸閺傘倖瀚归柨鐔告灮閹风兘鏁撴笟銉悏閿燂拷 */
	USART_Init(USART2, &USART_InitStructure);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);	/* 娴ｅ潡鏁撻弶鐗堟灮閹风兘鏁撻弬銈嗗闁跨喎褰ㄧ拋瑙勫 */
	//USART_ITConfig(USART2, USART_IT_TXE, ENABLE);	/* 娴ｅ潡鏁撻弶鏉垮殩閹风兘鏁撻弬銈嗗闁跨喎褰ㄧ拋瑙勫 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		濞夈劑鏁撻弬銈嗗: 闁跨喐鏋婚幏鐤洣闁跨喕濡銈堟彧閹风兘鏁撳ú銉ョ磻閸戙倖瀚归柨鐔告灮閹风兘鏁撻崣顐ヮ啇閹凤拷
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔峰建鐠佽瀚规担鍧楁晸閺傘倖瀚归柨鐔告灮閹风ùendUart()闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
	*/
	USART_Cmd(USART2, ENABLE);		/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗 */

	/* CPU闁跨喐鏋婚幏宄扮毈缂傛椽鏁撻幑鍑ょ秶閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔虹叓婵傛枻缍囬幏鐑芥晸閺傘倖瀚归柨鐔活敎閹插瀚归柨鐔虹コend闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐕傛嫹1闁跨喐鏋婚幏鐑芥晸鐞涙濡崙銈嗗闁跨喖鍙虹拠褎瀚归柨鐔告灮閹峰嘲骞�
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閿燂拷1闁跨喐鏋婚幏鐑芥晸鐞涙鏋婚幏鐑芥晸閻偄鍤栭幏鐑芥晸閺傘倖瀚圭涵顕€鏁撻弬銈嗗闁跨喖鍙虹粵瑙勫閸樺鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
	USART_ClearFlag(USART2, USART_FLAG_TC);     /* 闁跨喕妫勯崣鎴︽晸閺傘倖瀚归柨鐔告灮閹烽攱娼冮柨鐔活敎閹惧懏瀚归柨鐔虹ゴransmission Complete flag */
#endif

#if UART3_FIFO_EN == 1			/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�3 TX = PB10   RX = PB11 */

	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� PB2娑撴椽鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拫銏ゆ晸閿燂拷 RS485閼侯垳澧栭柨鐔告灮閹风兘鏁撶粔绋垮殩閹烽濮搁幀锟� */
#if RS485_ENABLE
	{
		RCC_APB2PeriphClockCmd(RCC_RS485_TXEN, ENABLE);

		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Pin = PIN_RS485_TXEN;
		GPIO_Init(PORT_RS485_TXEN, &GPIO_InitStructure);
	}
#endif
	/* 闁跨喐鏋婚幏锟�1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笹PIO闁跨喐鏋婚幏绋禔RT閺冨爼鏁撻弬銈嗗 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

	/* 闁跨喐鏋婚幏锟�2闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ǘSART Tx闁跨喐鏋婚幏绋IO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔风溇婢跺秹鏁撻弬銈嗗濡€崇础 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 闁跨喐鏋婚幏锟�3闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ǘSART Rx闁跨喐鏋婚幏绋IO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏閿嬆佸锟�
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笴PU闁跨喐鏋婚幏铚傜秴闁跨喐鏋婚幏绋IO缂傝櫣娓烽柨鐔告灮閹风兘鏁撶憴鎺楁交閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚瑰Ο鈥崇础闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崐鐔剁瑝闁跨喕顫楅幉瀣闁跨喐鏋婚幏鐑芥晸閿燂拷
		闁跨喐鏋婚幏鐑芥晸鐟欐帪缍囬幏鐑芥晸閹活厺绱幏鐑芥晸鐟欐帗鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲顎掗柨鐔告灮閹风兘鏁撻弬銈嗗娴溾晠鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归弴鎶芥晸鐞涙娅㈤幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉鍎伴柨鐔告灮閹风柉鏌楅柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔侯仾绾板瀚归柨鐔告灮閹风兘鏁撻惌顐ヮ嚋閹风兘鏁撻弬銈嗗
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	/*  闁跨喐鏋婚幏锟�3闁跨喐鏋婚幏鐑芥晸缁愭牗鎷濋幏鐑芥晸閺傘倖瀚归柨鐔峰閿濆繑瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閻涚姵鏋婚幏鐑芥晸閺傘倖瀚归崷鍡涙晸閺傘倖瀚归柨鐕傛嫹
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	*/
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 闁跨喐鏋婚幏锟�4闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚圭涵顒勬晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	USART_InitStructure.USART_BaudRate = UART3_BAUD;	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);	/* 娴ｅ潡鏁撻弶鐗堟灮閹风兘鏁撻弬銈嗗闁跨喎褰ㄧ拋瑙勫 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		濞夈劑鏁撻弬銈嗗: 闁跨喐鏋婚幏鐤洣闁跨喕濡銈堟彧閹风兘鏁撳ú銉ョ磻閸戙倖瀚归柨鐔告灮閹风兘鏁撻崣顐ヮ啇閹凤拷
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔峰建鐠佽瀚规担鍧楁晸閺傘倖瀚归柨鐔告灮閹风ùendUart()闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
	*/
	USART_Cmd(USART3, ENABLE);		/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗 */

	/* CPU闁跨喐鏋婚幏宄扮毈缂傛椽鏁撻幑鍑ょ秶閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔虹叓婵傛枻缍囬幏鐑芥晸閺傘倖瀚归柨鐔活敎閹插瀚归柨鐔虹コend闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐕傛嫹1闁跨喐鏋婚幏鐑芥晸鐞涙濡崙銈嗗闁跨喖鍙虹拠褎瀚归柨鐔告灮閹峰嘲骞�
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閿燂拷1闁跨喐鏋婚幏鐑芥晸鐞涙鏋婚幏鐑芥晸閻偄鍤栭幏鐑芥晸閺傘倖瀚圭涵顕€鏁撻弬銈嗗闁跨喖鍙虹粵瑙勫閸樺鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
	USART_ClearFlag(USART3, USART_FLAG_TC);     /* 闁跨喕妫勯崣鎴︽晸閺傘倖瀚归柨鐔告灮閹烽攱娼冮柨鐔活敎閹惧懏瀚归柨鐔虹ゴransmission Complete flag */
#endif

#if UART4_FIFO_EN == 1			/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�4 TX = PC10   RX = PC11 */
	/* 闁跨喐鏋婚幏锟�1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笹PIO闁跨喐鏋婚幏绋禔RT閺冨爼鏁撻弬銈嗗 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

	/* 闁跨喐鏋婚幏锟�2闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ǘSART Tx闁跨喐鏋婚幏绋IO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔风溇婢跺秹鏁撻弬銈嗗濡€崇础 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* 闁跨喐鏋婚幏锟�3闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ǘSART Rx闁跨喐鏋婚幏绋IO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏閿嬆佸锟�
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笴PU闁跨喐鏋婚幏铚傜秴闁跨喐鏋婚幏绋IO缂傝櫣娓烽柨鐔告灮閹风兘鏁撶憴鎺楁交閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚瑰Ο鈥崇础闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崐鐔剁瑝闁跨喕顫楅幉瀣闁跨喐鏋婚幏鐑芥晸閿燂拷
		闁跨喐鏋婚幏鐑芥晸鐟欐帪缍囬幏鐑芥晸閹活厺绱幏鐑芥晸鐟欐帗鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲顎掗柨鐔告灮閹风兘鏁撻弬銈嗗娴溾晠鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归弴鎶芥晸鐞涙娅㈤幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉鍎伴柨鐔告灮閹风柉鏌楅柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔侯仾绾板瀚归柨鐔告灮閹风兘鏁撻惌顐ヮ嚋閹风兘鏁撻弬銈嗗
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* 闁跨喐鏋婚幏锟�4闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚圭涵顒勬晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	USART_InitStructure.USART_BaudRate = UART4_BAUD;	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(UART4, &USART_InitStructure);

	USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);	/* 娴ｅ潡鏁撻弶鐗堟灮閹风兘鏁撻弬銈嗗闁跨喎褰ㄧ拋瑙勫 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		濞夈劑鏁撻弬銈嗗: 闁跨喐鏋婚幏鐤洣闁跨喕濡銈堟彧閹风兘鏁撳ú銉ョ磻閸戙倖瀚归柨鐔告灮閹风兘鏁撻崣顐ヮ啇閹凤拷
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔峰建鐠佽瀚规担鍧楁晸閺傘倖瀚归柨鐔告灮閹风ùendUart()闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
	*/
	USART_Cmd(UART4, ENABLE);		/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗 */

	/* CPU闁跨喐鏋婚幏宄扮毈缂傛椽鏁撻幑鍑ょ秶閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔虹叓婵傛枻缍囬幏鐑芥晸閺傘倖瀚归柨鐔活敎閹插瀚归柨鐔虹コend闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐕傛嫹1闁跨喐鏋婚幏鐑芥晸鐞涙濡崙銈嗗闁跨喖鍙虹拠褎瀚归柨鐔告灮閹峰嘲骞�
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閿燂拷1闁跨喐鏋婚幏鐑芥晸鐞涙鏋婚幏鐑芥晸閻偄鍤栭幏鐑芥晸閺傘倖瀚圭涵顕€鏁撻弬銈嗗闁跨喖鍙虹粵瑙勫閸樺鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
	USART_ClearFlag(UART4, USART_FLAG_TC);     /* 闁跨喕妫勯崣鎴︽晸閺傘倖瀚归柨鐔告灮閹烽攱娼冮柨鐔活敎閹惧懏瀚归柨鐔虹ゴransmission Complete flag */
#endif

#if UART5_FIFO_EN == 1			/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�5 TX = PC12   RX = PD2 */
	/* 闁跨喐鏋婚幏锟�1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笹PIO闁跨喐鏋婚幏绋禔RT閺冨爼鏁撻弬銈嗗 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5, ENABLE);

	/* 闁跨喐鏋婚幏锟�2闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ǘSART Tx闁跨喐鏋婚幏绋IO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔风溇婢跺秹鏁撻弬銈嗗濡€崇础 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* 闁跨喐鏋婚幏锟�3闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风ǘSART Rx闁跨喐鏋婚幏绋IO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚规稉娲晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏閿嬆佸锟�
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚笴PU闁跨喐鏋婚幏铚傜秴闁跨喐鏋婚幏绋IO缂傝櫣娓烽柨鐔告灮閹风兘鏁撶憴鎺楁交閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚瑰Ο鈥崇础闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崐鐔剁瑝闁跨喕顫楅幉瀣闁跨喐鏋婚幏鐑芥晸閿燂拷
		闁跨喐鏋婚幏鐑芥晸鐟欐帪缍囬幏鐑芥晸閹活厺绱幏鐑芥晸鐟欐帗鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲顎掗柨鐔告灮閹风兘鏁撻弬銈嗗娴溾晠鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归弴鎶芥晸鐞涙娅㈤幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉鍎伴柨鐔告灮閹风柉鏌楅柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔侯仾绾板瀚归柨鐔告灮閹风兘鏁撻惌顐ヮ嚋閹风兘鏁撻弬銈嗗
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOD, &GPIO_InitStructure);


	/* 闁跨喐鏋婚幏锟�4闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚圭涵顒勬晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	USART_InitStructure.USART_BaudRate = UART5_BAUD;	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(UART5, &USART_InitStructure);

	USART_ITConfig(UART5, USART_IT_RXNE, ENABLE);	/* 娴ｅ潡鏁撻弶鐗堟灮閹风兘鏁撻弬銈嗗闁跨喎褰ㄧ拋瑙勫 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		濞夈劑鏁撻弬銈嗗: 闁跨喐鏋婚幏鐤洣闁跨喕濡銈堟彧閹风兘鏁撳ú銉ョ磻閸戙倖瀚归柨鐔告灮閹风兘鏁撻崣顐ヮ啇閹凤拷
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔峰建鐠佽瀚规担鍧楁晸閺傘倖瀚归柨鐔告灮閹风ùendUart()闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
	*/
	USART_Cmd(UART5, ENABLE);		/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗 */

	/* CPU闁跨喐鏋婚幏宄扮毈缂傛椽鏁撻幑鍑ょ秶閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔虹叓婵傛枻缍囬幏鐑芥晸閺傘倖瀚归柨鐔活敎閹插瀚归柨鐔虹コend闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐕傛嫹1闁跨喐鏋婚幏鐑芥晸鐞涙濡崙銈嗗闁跨喖鍙虹拠褎瀚归柨鐔告灮閹峰嘲骞�
		闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閿燂拷1闁跨喐鏋婚幏鐑芥晸鐞涙鏋婚幏鐑芥晸閻偄鍤栭幏鐑芥晸閺傘倖瀚圭涵顕€鏁撻弬銈嗗闁跨喖鍙虹粵瑙勫閸樺鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
	USART_ClearFlag(UART5, USART_FLAG_TC);     /* 闁跨喕妫勯崣鎴︽晸閺傘倖瀚归柨鐔告灮閹烽攱娼冮柨鐔活敎閹惧懏瀚归柨鐔虹ゴransmission Complete flag */
#endif
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: ConfigUartNVIC
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏鐑芥晸閻偉鎻幏鐑芥晸閺傘倖瀚圭涵顒勬晸閺傘倖瀚归柨鐔峰建鐠佽瀚�.
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�:  闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
static void ConfigUartNVIC(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	/* Configure the NVIC Preemption Priority Bits */
	/*	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);  --- 闁跨喐鏋婚幏锟� bsp.c 闁跨喐鏋婚幏锟� bsp_Init() 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崣顐ヮ啇閹风兘鏁撻弬銈嗗闁跨喖銈虹涵閿嬪闁跨喐鏋婚幏锟� */

#if UART1_FIFO_EN == 1
	/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗1闁跨喎褰ㄧ拋瑙勫 */
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART2_FIFO_EN == 1
	/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗2闁跨喎褰ㄧ拋瑙勫 */
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART3_FIFO_EN == 1
	/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗3闁跨喎褰ㄧ拋瑙勫t */
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART4_FIFO_EN == 1
	/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗4闁跨喎褰ㄧ拋瑙勫t */
	NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART5_FIFO_EN == 1
	/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗5闁跨喎褰ㄧ拋瑙勫t */
	NVIC_InitStructure.NVIC_IRQChannel = UART5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART6_FIFO_EN == 1
	/* 娴ｅ潡鏁撻弶鎷屾彧閹风兘鏁撻弬銈嗗6闁跨喎褰ㄧ拋瑙勫t */
	NVIC_InitStructure.NVIC_IRQChannel = USART6_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: UartSend
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏宄板晸闁跨喐鏋婚幏鐑芥晸閹归顣幏绋禔RT闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷,闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閸欘偅鏌囬埥鍛闁跨喎褰ㄩ弬顓℃彧閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐤闁跨喐鏋婚幏鐤箼闁跨喐鏋婚幏铚傜疀鏉堟鏁撻弬銈嗗闁跨喐鏋婚幏宄板祻闁跨噦鎷�
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�:  闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
static void UartSend(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen)
{
	uint16_t i;

	for (i = 0; i < _usLen; i++)
	{
		/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽鐛婇柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏铚傛枾闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归懓宥夋晸閺傘倖瀚归柨鐔笺偤鏉堢偓瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
	#if 0
		/*
			闁跨喕濡喊澶嬪闁跨喐鏋婚幏绋RS闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归弮鍫曟晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閹峰洭鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏绌ile 闁跨喐鏋婚幏宄版儕闁跨喐鏋婚幏锟�
			閸樼喖鏁撻弬銈嗗 闁跨喐鏋婚幏鐑芥晸闁扮數顣幏锟�1闁跨喐鏋婚幏鐑芥晸鐞涙鏋婚幏閿嬫 _pUart->usTxWrite = 1闁跨喐鏋婚幏绌噋Uart->usTxRead = 0;
			闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风hile(1) 闁跨喓娼鹃崙銈嗗闁跨喎澹欑粵瑙勫
		*/
		while (1)
		{
			uint16_t usRead;

			DISABLE_INT();
			usRead = _pUart->usTxRead;
			ENABLE_INT();

			if (++usRead >= _pUart->usTxBufSize)
			{
				usRead = 0;
			}

			if (usRead != _pUart->usTxWrite)
			{
				break;
			}
		}
	#else
		/* 闁跨喐鏋婚幏锟� _pUart->usTxBufSize == 1 閺冿拷, 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归弳顕€鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻敓锟�(闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷) */
		while (1)
		{
			__IO uint16_t usCount;

			DISABLE_INT();
			usCount = _pUart->usTxCount;
			ENABLE_INT();

			if (usCount < _pUart->usTxBufSize)
			{
				break;
			}
		}
	#endif

		/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸鐠囶偄褰傞柨鐔煎徍娴兼瑦瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
		_pUart->pTxBuf[_pUart->usTxWrite] = _ucaBuf[i];

		DISABLE_INT();
		if (++_pUart->usTxWrite >= _pUart->usTxBufSize)
		{
			_pUart->usTxWrite = 0;
		}
		_pUart->usTxCount++;
		ENABLE_INT();
	}

	USART_ITConfig(_pUart->uart, USART_IT_TXE, ENABLE);
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: UartGetChar
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐甯存潏鐐闁跨喕濡弬銈嗗闁跨喓笑娴兼瑦瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏宄板絿1闁跨喕顢滈弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风柉灏楅柨鐕傛嫹
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _pUart : 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔封偓鐔奉槵
*			  _pByte : 闁跨喐鏋婚幏宄版憹闁跨喖銈洪埥鍛闁跨喐鏋婚幏鐤箰闁跨喕顢滈棃鈺傚闁跨噦鎷�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 0 闁跨喐鏋婚幏椋庛仛闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷  1闁跨喐鏋婚幏椋庛仛闁跨喐鏋婚幏宄板絿闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷
*********************************************************************************************************
*/
static uint8_t UartGetChar(UART_T *_pUart, uint8_t *_pByte)
{
	uint16_t usCount;

	/* usRxWrite 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崣顐ｆ焽閻氬瓨瀚归柨鐔告灮閹风兘鏁撻崣顐ｅ敾閹风兘鏁撻弬銈嗗閸愭瑩鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撴琛℃閹峰嘲闄勯柨鐔告灮閹风兘鏁撶紒鐐村敾閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崝顐ｆ灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
	DISABLE_INT();
	usCount = _pUart->usRxCount;
	ENABLE_INT();

	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻崣顐ユ彧閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸闁版鍓ㄩ幏鐑芥晸閺傘倖瀚归摀宥囥偡闁跨噦鎷�0 */
	//if (_pUart->usRxRead == usRxWrite)
	if (usCount == 0)	/* 闁跨喓鐛ら幘鍛濞岋繝鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
	{
		return 0;
	}
	else
	{
		*_pByte = _pUart->pRxBuf[_pUart->usRxRead];		/* 闁跨喐甯存潏鐐闁跨喕濡弬銈嗗闁跨喐鏋婚幏绋FO閸欙拷1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */

		/* 闁跨喐鏋婚幏宄板晸FIFO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
		DISABLE_INT();
		if (++_pUart->usRxRead >= _pUart->usRxBufSize)
		{
			_pUart->usRxRead = 0;
		}
		_pUart->usRxCount--;
		ENABLE_INT();
		return 1;
	}
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: UartIRQ
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鏋婚幏鐑芥晸閸欘偅鏌囬崙銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻惌顐秶閹风兘鈧岸鏁撻惌顐ユ彧閹风兘鏁撻弬銈嗗闁跨喎褰ㄩ弬顓℃彧閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: _pUart : 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔封偓鐔奉槵
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
static void UartIRQ(UART_T *_pUart)
{
	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喎褰ㄧ拋瑙勫  */
	if (USART_GetITStatus(_pUart->uart, USART_IT_RXNE) != RESET)
	{
		/* 闁跨喐甯存潏鐐闁跨喕濡弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔稿祹鐎靛嫯鎻幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲褰囬柨鐔告灮閹风兘鏁撻幑鐤彧閹风柉顕柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喓鍞嘔FO */
		uint8_t ch;

		ch = USART_ReceiveData(_pUart->uart);
		_pUart->pRxBuf[_pUart->usRxWrite] = ch;
		if (++_pUart->usRxWrite >= _pUart->usRxBufSize)
		{
			_pUart->usRxWrite = 0;
		}
		if (_pUart->usRxCount < _pUart->usRxBufSize)
		{
			_pUart->usRxCount++;
		}

		/* 闁跨喐鍩呯喊澶嬪闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�,闁氨鐓℃惔鏃堟晸閻偆顒查幏鐑芥晸閺傘倖瀚归柨鐔盒︾喊澶嬪闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷,娑撯偓闁跨喐鏋婚幏鐑芥晸鐟欐帒鍤栭幏鐑芥晸閺傘倖瀚�1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归幁顖炴晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏铚傜闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐕傛嫹 */
		//if (_pUart->usRxWrite == _pUart->usRxRead)
		//if (_pUart->usRxCount == 1)
		{
			if (_pUart->ReciveNew)
			{
				_pUart->ReciveNew(ch);
			}
		}
	}

	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻柊鍏哥串閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔峰建鐠佽瀚� */
	if (USART_GetITStatus(_pUart->uart, USART_IT_TXE) != RESET)
	{
		//if (_pUart->usTxRead == _pUart->usTxWrite)
		if (_pUart->usTxCount == 0)
		{
			/* 闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲褰囬柨鐔告灮閹烽攱妞傞柨鐔告灮閹凤拷 闁跨喐鏋婚幏閿嬵剾闁跨喐鏋婚幏鐑芥晸闁板吀绱幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喎褰ㄧ拋瑙勫 闁跨喐鏋婚幏閿嬫暈闁跨喕袙閿涙岸鏁撻弬銈嗗閺冨爼鏁撻弬銈嗗闁跨噦鎷�1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔稿祹娴兼瑦瀚归張顏堟晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭敮宀勬晸閿燂拷*/
			USART_ITConfig(_pUart->uart, USART_IT_TXE, DISABLE);

			/* 娴ｅ潡鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閹瑰嘲鍤栭幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閸楁悂鏁撻敓锟� */
			USART_ITConfig(_pUart->uart, USART_IT_TC, ENABLE);
		}
		else
		{
			/* 闁跨喐甯撮崙銈嗗闁跨喐鏋婚幏绋FO閸欙拷1闁跨喐鏋婚幏鐑芥晸鐞涙鏋婚幏宄板晸闁跨喕顕犳稉鏌ユ晸閼哄倸鍤栭幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻幑宄扮槑鏉堢偓瀚归柨鐔告灮閹凤拷 */
			USART_SendData(_pUart->uart, _pUart->pTxBuf[_pUart->usTxRead]);
			if (++_pUart->usTxRead >= _pUart->usTxBufSize)
			{
				_pUart->usTxRead = 0;
			}
			_pUart->usTxCount--;
		}

	}
	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚筨it娴ｅ秴鍙忛柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭化濠氭晸閺傘倖瀚归崡鎼佹晸閿燂拷 */
	else if (USART_GetITStatus(_pUart->uart, USART_IT_TC) != RESET)
	{
		//if (_pUart->usTxRead == _pUart->usTxWrite)
		if (_pUart->usTxCount == 0)
		{
			/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撶槐绲€FO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲鍙忛柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭敮宀勬晸閺傘倖瀚归柨鐔活敎閻у憡瀚归柨鐔告灮閹风柉甯㈤柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閸欘偉顔愰幏锟� */
			USART_ITConfig(_pUart->uart, USART_IT_TC, DISABLE);

			/* 闁跨喐鍩呯喊澶嬪闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�, 娑撯偓闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏绋睸485闁岸鏁撻懘姘剧秶閹风兘鏁撻弬銈嗗RS485閼侯垳澧栭柨鐔告灮閹风兘鏁撻弬銈嗗娑撴椽鏁撻弬銈嗗闁跨喐鏋婚幏閿嬆佸蹇涙晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏宄板窗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚� */
			if (_pUart->SendOver)
			{
				_pUart->SendOver();
			}
		}
		else
		{
			/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗閹枫垽鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹烽攱婀愰柨鐔活敎閿燂拷 */

			/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撶槐绲€FO闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔稿祹娴兼瑦瀚归張顏堟晸閺傘倖瀚圭敮宀勬晸閺傘倖瀚归柨鐔稿复閸戙倖瀚归柨鐔告灮閹风éIFO閸欙拷1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹峰嘲鍟撻柨鐔活嚑閸欐垿鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閹瑰嘲鐦庢潏鐐闁跨喐鏋婚幏锟� */
			USART_SendData(_pUart->uart, _pUart->pTxBuf[_pUart->usTxRead]);
			if (++_pUart->usTxRead >= _pUart->usTxBufSize)
			{
				_pUart->usTxRead = 0;
			}
			_pUart->usTxCount--;
		}
	}
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: USART1_IRQHandler  USART2_IRQHandler USART3_IRQHandler UART4_IRQHandler UART5_IRQHandler
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: USART闁跨喎褰ㄩ弬顓炲殩閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閿燂拷
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
#if UART1_FIFO_EN == 1
void USART1_IRQHandler(void)
{
	UartIRQ(&g_tUart1);
}
#endif

#if UART2_FIFO_EN == 1
void USART2_IRQHandler(void)
{
	UartIRQ(&g_tUart2);
}
#endif

#if UART3_FIFO_EN == 1
void USART3_IRQHandler(void)
{
	UartIRQ(&g_tUart3);
}
#endif

#if UART4_FIFO_EN == 1
void UART4_IRQHandler(void)
{
	UartIRQ(&g_tUart4);
}
#endif

#if UART5_FIFO_EN == 1
void UART5_IRQHandler(void)
{
	UartIRQ(&g_tUart5);
}
#endif

#if UART6_FIFO_EN == 1
void USART6_IRQHandler(void)
{
	UartIRQ(&g_tUart6);
}
#endif

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: fputc
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鍩呯拋瑙勫闁跨喐鏋婚幏绌歶tc闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹疯渹濞囬柨鐔告灮閹风rintf闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔稿复鏉堢偓瀚归柨鐔告灮閹凤拷1闁跨喐鏋婚幏宄板祪闁跨喐鏋婚幏鐑芥晸閿燂拷
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
int fputc(int ch, FILE *f)
{
#if 1	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭憰涔竢intf闁跨喐鏋婚幏鐑芥晸鐞涙鍤栭幏鐑解偓姘舵晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喎褰ㄧ拋瑙勫FIFO闁跨喐鏋婚幏鐑芥晸闁扮數顒查幏宄板箵闁跨喐鏋婚幏绌歳intf闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷 */
	comSendChar(COM1, ch);

	return ch;
#else	/* 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏宄扮础闁跨喐鏋婚幏鐑芥晸閺傘倖瀚瑰В蹇涙晸閺傘倖瀚归柨鐔活敎閸戙倖瀚�,闁跨喖銈烘潏鐐闁跨喐鏋婚幏鐑芥晸閹瑰嘲鍤栭幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻敓锟� */
	/* 閸愭瑤绔撮柨鐔告灮閹风兘鏁撶悰妤勫Ν绾板瀚筓SART1 */
	USART_SendData(USART1, (uint8_t) ch);

	/* 闁跨喖銈烘潏鐐闁跨喐鏋婚幏鐑芥晸闁板灚鏋婚幏鐑芥晸閺傘倖瀚� */
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
	{}

	return ch;
#endif
}

/*
*********************************************************************************************************
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟�: fgetc
*	闁跨喐鏋婚幏鐑芥晸閺傘倖瀚圭拠鎾晸閺傘倖瀚�: 闁跨喐鍩呯拋瑙勫闁跨喐鏋婚幏绌廵tc闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹疯渹濞囬柨鐔告灮閹风⿵etchar闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔稿复鏉堢偓瀚归柨鐔告灮閹凤拷1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗
*	闁跨喐鏋婚幏锟�    闁跨喐鏋婚幏锟�: 闁跨喐鏋婚幏锟�
*	闁跨喐鏋婚幏锟� 闁跨喐鏋婚幏锟� 閸婏拷: 闁跨喐鏋婚幏锟�
*********************************************************************************************************
*/
int fgetc(FILE *f)
{

#if 1	/* 闁跨喐甯存潏鐐闁跨喕濡弬銈嗗闁跨喐鏋婚幏绋FO闁跨喐鏋婚幏宄板絿1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹凤拷, 閸欘亪鏁撻弬銈嗗閸欐牠鏁撻弬銈嗗闁跨喐鏋婚幏鐑芥晸閹归攱澧犻崙銈嗗闁跨喐鏋婚幏锟� */
	uint8_t ucData;

	while(comGetChar(COM1, &ucData) == 0);

	return ucData;
#else
	/* 闁跨喖銈烘潏鐐闁跨喐鏋婚幏鐑芥晸閺傘倖瀚�1闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗 */
	while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);

	return (int)USART_ReceiveData(USART1);
#endif
}

int _write(int fd, char *pBuffer, int size)
{
	comSendBuf(COM1, pBuffer, size);
	return 0;
}



/***************************** 闁跨喐鏋婚幏鐑芥晸閺傘倖瀚归柨鐔告灮閹风兘鏁撻弬銈嗗闁跨喐鏋婚幏锟� www.armfly.com (END OF FILE) *********************************/
