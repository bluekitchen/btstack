/*
*********************************************************************************************************
*
*	妯￠敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 : 閿熸枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹+FIFO閿熸枻鎷烽敓鏂ゆ嫹妯￠敓鏂ゆ嫹
*	閿熶茎纭锋嫹閿熸枻鎷烽敓鏂ゆ嫹 : bsp_uart_fifo.c
*	閿熸枻鎷�    閿熸枻鎷� : V1.0
*	璇�    閿熸枻鎷� : 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹閿熷彨璁规嫹+FIFO妯″紡瀹為敓琛楄鎷烽敓鏂ゆ嫹閿熸枻鎷疯瘶閿熼叺顑ユ唻鎷烽敓鏂ゆ嫹閿燂拷
*	閿熺潾鏀圭》鎷峰綍 :
*		閿熻姤鏈敓鏂ゆ嫹  閿熸枻鎷烽敓鏂ゆ嫹       閿熸枻鎷烽敓鏂ゆ嫹    璇撮敓鏂ゆ嫹
*		V1.0    2013-02-01 armfly  閿熸枻鎷峰紡閿熸枻鎷烽敓鏂ゆ嫹
*		V1.1    2013-06-09 armfly  FiFo閿熺粨鏋勯敓鏂ゆ嫹閿熸枻鎷稵xCount閿熸枻鎷峰憳閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍙柇浼欐嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�; 閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷稦iFo閿熶茎鐚存嫹閿熸枻鎷�
*		V1.2	2014-09-29 armfly  閿熸枻鎷烽敓鏂ゆ嫹RS485 MODBUS閿熸帴鍙ｂ槄鎷烽敓鏂ゆ嫹閿熺Ц纰夋嫹閿熸枻鎷烽敓琛楄妭鐚存嫹鐩撮敓鏂ゆ嫹鎵ч敓鍙洖纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
*
*	閿熺潾鏀硅鎷烽敓鏂ゆ嫹 : 
*		閿熻姤鏈敓鏂ゆ嫹    閿熸枻鎷烽敓鏂ゆ嫹       閿熸枻鎷烽敓鏂ゆ嫹                  璇撮敓鏂ゆ嫹
*		V1.0    2015-08-19  Eric2013       閿熷姭鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稦reeRTOS閿熸枻鎷烽敓鏂ゆ嫹
*
*	Copyright (C), 2013-2014, 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"


/* 閿熸枻鎷烽敓鏂ゆ嫹姣忛敓鏂ゆ嫹閿熸枻鎷烽敓鑺傜粨鏋勯敓鏂ゆ嫹閿熸枻鎷烽敓锟� */
#if UART1_FIFO_EN == 1
	static UART_T g_tUart1;
	static uint8_t g_TxBuf1[UART1_TX_BUF_SIZE];		/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	static uint8_t g_RxBuf1[UART1_RX_BUF_SIZE];		/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷� */
#endif

#if UART2_FIFO_EN == 1
	static UART_T g_tUart2;
	static uint8_t g_TxBuf2[UART2_TX_BUF_SIZE];		/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	static uint8_t g_RxBuf2[UART2_RX_BUF_SIZE];		/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷� */
#endif

#if UART3_FIFO_EN == 1
	static UART_T g_tUart3;
	static uint8_t g_TxBuf3[UART3_TX_BUF_SIZE];		/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	static uint8_t g_RxBuf3[UART3_RX_BUF_SIZE];		/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷� */
#endif

#if UART4_FIFO_EN == 1
	static UART_T g_tUart4;
	static uint8_t g_TxBuf4[UART4_TX_BUF_SIZE];		/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	static uint8_t g_RxBuf4[UART4_RX_BUF_SIZE];		/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷� */
#endif

#if UART5_FIFO_EN == 1
	static UART_T g_tUart5;
	static uint8_t g_TxBuf5[UART5_TX_BUF_SIZE];		/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	static uint8_t g_RxBuf5[UART5_RX_BUF_SIZE];		/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷� */
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
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: bsp_InitUart
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷峰閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷风‖閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鍏ㄩ敓琛楁唻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鍊�.
*	閿熸枻鎷�    閿熸枻鎷�:  閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
void bsp_InitUart(void)
{
	UartVarInit();		/* 閿熸枻鎷烽敓鏂ゆ嫹閿熼ズ绛规嫹濮嬮敓鏂ゆ嫹鍏ㄩ敓琛楁唻鎷烽敓鏂ゆ嫹,閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷风‖閿熸枻鎷� */

	InitHardUart();		/* 閿熸枻鎷烽敓鐭揪鎷烽敓鑺傜鎷风‖閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�(閿熸枻鎷烽敓鏂ゆ嫹閿熺粸纰夋嫹) */
	
#if RS485_ENABLE
	RS485_InitTXE();	/* 閿熸枻鎷烽敓鏂ゆ嫹RS485鑺墖閿熶茎鍑ゆ嫹閿熸枻鎷蜂娇閿熸枻鎷风‖閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿燂拷 */
#endif
	
	ConfigUartNVIC();	/* 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹閿熷彨璁规嫹 */
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: ComToUart
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷稢OM閿熷壙鍙ｇ尨鎷疯浆閿熸枻鎷蜂负UART鎸囬敓鏂ゆ嫹
*	閿熸枻鎷�    閿熸枻鎷�: _ucPort: 閿熷壙鍙ｇ尨鎷�(COM1 - COM6)
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: uart鎸囬敓鏂ゆ嫹
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
		/* 閿熸枻鎷烽敓鏂ゆ嫹閿熻娇浣曡揪鎷烽敓鏂ゆ嫹 */
		return 0;
	}
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: comSendBuf
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸触涓插彛鍑ゆ嫹閿熸枻鎷蜂竴閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎鈽呮嫹閿熸枻鎷烽敓鎹锋斁纰夋嫹閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎴綇鎷烽敓鏂ゆ嫹閿熷彨鏂嚖鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹璇ら敓鏁欘煉鎷烽敓缂村嚖鎷烽敓鏂ゆ嫹
*	閿熸枻鎷�    閿熸枻鎷�: _ucPort: 閿熷壙鍙ｇ尨鎷�(COM1 - COM6)
*			  _ucaBuf: 閿熸枻鎷烽敓鏂ゆ嫹閿熼叺纰夋嫹閿熸枻鎷烽敓鎹蜂紮鎷烽敓鏂ゆ嫹閿熸枻鎷�
*			  _usLen : 閿熸枻鎷烽敓鎹风鎷烽敓鏂ゆ嫹
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
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
		pUart->SendBefor();		/* 閿熸枻鎷烽敓鏂ゆ嫹閿熺祲S485閫氶敓鑴氾綇鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹钖敓绲塖485閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熸枻鎷锋ā寮� */
	}

	UartSend(pUart, _ucaBuf, _usLen);
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: comSendChar
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸触涓插彛鍑ゆ嫹閿熸枻鎷�1閿熸枻鎷烽敓琛楄妭鈽呮嫹閿熸枻鎷烽敓鎹锋斁纰夋嫹閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎴綇鎷烽敓鏂ゆ嫹閿熷彨鏂嚖鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹璇ら敓鏁欘煉鎷烽敓缂村嚖鎷烽敓鏂ゆ嫹
*	閿熸枻鎷�    閿熸枻鎷�: _ucPort: 閿熷壙鍙ｇ尨鎷�(COM1 - COM6)
*			  _ucByte: 閿熸枻鎷烽敓鏂ゆ嫹閿熼叺纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
void comSendChar(COM_PORT_E _ucPort, uint8_t _ucByte)
{
	comSendBuf(_ucPort, &_ucByte, 1);
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: comGetChar
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸帴杈炬嫹閿熻妭浼欐嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰彇1閿熻鑺傦綇鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎鎾呮嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹
*	閿熸枻鎷�    閿熸枻鎷�: _ucPort: 閿熷壙鍙ｇ尨鎷�(COM1 - COM6)
*			  _pByte: 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎹疯揪鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰潃
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 0 閿熸枻鎷风ず閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�, 1 閿熸枻鎷风ず閿熸枻鎷峰彇閿熸枻鎷烽敓鏂ゆ嫹鏁堥敓琛楁枻鎷�
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
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: comGetChar
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸帴杈炬嫹閿熻妭浼欐嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰彇n閿熻鑺傦綇鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎鎾呮嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹
*	閿熸枻鎷�    閿熸枻鎷�: _ucPort: 閿熷壙鍙ｇ尨鎷�(COM1 - COM6)
*			  _pBuf: 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎹疯揪鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰潃
*			  _usLen: 鍑嗛敓鏂ゆ嫹閿熸枻鎷峰彇閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎绛规嫹閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 瀹為敓缁炶鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎绛规嫹閿熼ズ锝忔嫹0 閿熸枻鎷风ず閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
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
	/* usRxWrite 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍙柇鐚存嫹閿熸枻鎷烽敓鍙唻鎷烽敓鏂ゆ嫹鍐欓敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓楗衡槄鎷峰附閿熸枻鎷烽敓缁炴唻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍔枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	DISABLE_INT();
	usCount = pUart->usRxCount;
	ENABLE_INT();

	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍙揪鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓閰殿剨鎷烽敓鏂ゆ嫹铓嶇シ閿燂拷0 */
	if (usCount == 0)	/* 閿熺獤鎾呮嫹娌￠敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
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
		pUart->usRxRead = pUart->usRxWrite; /*閿熸枻鎷蜂綅FIFO wptr * rptr鎸囬敓鏂ゆ嫹*/
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
			pUart->usRxRead = usLeftCount; /*閿熸枻鎷蜂綅FIFO wptr * rptr鎸囬敓鏂ゆ嫹*/
		}
		else 
		{
			memcpy(_pBuf, &pUart->pRxBuf[pUart->usRxRead], _usLenToRead);
			pUart->usRxRead += _usLenToRead; /*閿熸枻鎷蜂綅FIFO wptr * rptr鎸囬敓鏂ゆ嫹*/
		}
		pUart->usRxCount -= _usLenToRead;
		ENABLE_INT();
		return _usLenToRead;
	}
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: comClearTxFifo
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷峰彇閿熸枻鎷烽敓鑺備紮鎷烽敓鏂ゆ嫹閿熸枻鎷穉vailable buffer length
*	閿熸枻鎷�    閿熸枻鎷�: _ucPort: 閿熷壙鍙ｇ尨鎷�(COM1 - COM6)
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
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
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: comClearTxFifo
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷烽敓濮愪覆閿熻妭鍑ゆ嫹閿熼叺浼欐嫹閿熸枻鎷烽敓鏂ゆ嫹
*	閿熸枻鎷�    閿熸枻鎷�: _ucPort: 閿熷壙鍙ｇ尨鎷�(COM1 - COM6)
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
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
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: comClearRxFifo
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷烽敓濮愪覆閿熻妭鏂ゆ嫹閿熺Ц浼欐嫹閿熸枻鎷烽敓鏂ゆ嫹
*	閿熸枻鎷�    閿熸枻鎷�: _ucPort: 閿熷壙鍙ｇ尨鎷�(COM1 - COM6)
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
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
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: bsp_SetUart1Baud
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熺潾闈╂嫹UART1閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
void bsp_SetUart1Baud(uint32_t _baud)
{
	USART_InitTypeDef USART_InitStructure;

	/* 閿熸枻鎷�2閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_InitStructure.USART_BaudRate = _baud;	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: bsp_SetUart2Baud
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熺潾闈╂嫹UART2閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
void bsp_SetUart2Baud(uint32_t _baud)
{
	USART_InitTypeDef USART_InitStructure;

	/* 閿熸枻鎷�2閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_InitStructure.USART_BaudRate = _baud;	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);
}


/* 閿熸枻鎷烽敓鏂ゆ嫹閿熺祲S485閫氶敓鑴氾綇鎷烽敓璇寜閿熸枻鎷烽敓閾伴潻鎷峰紡閿熸枻鎷峰啓閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� 閿熸枻鎷烽敓瑙掓枻鎷烽敓鏂ゆ嫹閿熸枻鎷� USART3閿熸枻鎷蜂负RS485閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: RS485_InitTXE
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷烽敓鏂ゆ嫹RS485閿熸枻鎷烽敓鏂ゆ嫹浣块敓鏉板尅鎷烽敓鏂ゆ嫹 TXE
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
void RS485_InitTXE(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_RS485_TXEN, ENABLE);	/* 閿熸枻鎷稧PIO鏃堕敓鏂ゆ嫹 */

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓渚ワ吉锟� */
	GPIO_InitStructure.GPIO_Pin = PIN_RS485_TXEN;
	GPIO_Init(PORT_RS485_TXEN, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: bsp_Set485Baud
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熺潾闈╂嫹UART3閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
void bsp_Set485Baud(uint32_t _baud)
{
	USART_InitTypeDef USART_InitStructure;

	/* 閿熸枻鎷�2閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_InitStructure.USART_BaudRate = _baud;	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: RS485_SendBefor
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鍓嶉敓鏂ゆ嫹鍑嗛敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稲S485閫氶敓鑴氾綇鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹RS485鑺墖涓洪敓鏂ゆ嫹閿熸枻鎷风姸鎬侀敓鏂ゆ嫹
*			  閿熸枻鎷烽敓鐫潻鎷� UartVarInit()閿熷彨鐨勭尨鎷烽敓鏂ゆ嫹鎸囬敓鏂ゆ嫹閿熸枻鎷疯瘨閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿燂拷 g_tUart2.SendBefor = RS485_SendBefor
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
void RS485_SendBefor(void)
{
	RS485_TX_EN();	/* 閿熷彨浼欐嫹RS485閿熺Ц鍑ゆ嫹鑺墖涓洪敓鏂ゆ嫹閿熸枻鎷锋ā寮� */
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: RS485_SendOver
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷烽敓鏂ゆ嫹涓€閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰潽閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺祲S485閫氶敓鑴氾綇鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹RS485鑺墖涓洪敓鏂ゆ嫹閿熸枻鎷风姸鎬侀敓鏂ゆ嫹
*			  閿熸枻鎷烽敓鐫潻鎷� UartVarInit()閿熷彨鐨勭尨鎷烽敓鏂ゆ嫹鎸囬敓鏂ゆ嫹閿熸枻鎷疯瘨閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿燂拷 g_tUart2.SendOver = RS485_SendOver
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
void RS485_SendOver(void)
{
	RS485_RX_EN();	/* 閿熷彨浼欐嫹RS485閿熺Ц鍑ゆ嫹鑺墖涓洪敓鏂ゆ嫹閿熸枻鎷锋ā寮� */
}


/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: RS485_SendBuf
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閫氶敓鏂ゆ嫹RS485鑺墖閿熸枻鎷烽敓鏂ゆ嫹涓€閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎鈽呮嫹娉ㄩ敓瑙ｏ紝閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熼ズ杈炬嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷风█閿燂拷
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
void RS485_SendBuf(uint8_t *_ucaBuf, uint16_t _usLen)
{
	comSendBuf(COM3, _ucaBuf, _usLen);
}


/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: RS485_SendStr
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷�485閿熸枻鎷烽敓绔嚖鎷烽敓鏂ゆ嫹涓€閿熸枻鎷烽敓琛楀嚖鎷烽敓鏂ゆ嫹
*	閿熸枻鎷�    閿熸枻鎷�: _pBuf 閿熸枻鎷烽敓鎹蜂紮鎷烽敓鏂ゆ嫹閿熸枻鎷�
*			 _ucLen 閿熸枻鎷烽敓鎹风鎷烽敓鏂ゆ嫹
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
void RS485_SendStr(char *_pBuf)
{
	RS485_SendBuf((uint8_t *)_pBuf, strlen(_pBuf));
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: RS485_ReciveNew
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷烽敓绉哥鎷烽敓閾扮鎷烽敓鏂ゆ嫹閿熸枻鎷�
*	閿熸枻鎷�    閿熸枻鎷�: _byte 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
//extern void MODBUS_ReciveNew(uint8_t _byte);
void RS485_ReciveNew(uint8_t _byte)
{
//	MODBUS_ReciveNew(_byte);
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: UartVarInit
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷峰閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹姘愯皨閿熸枻鎷烽敓锟�
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
extern void bt_driver_recieve_data_from_controller(uint8_t data);
extern void at_command_uart_rx_isr_handler(uint8_t data);
static void UartVarInit(void)
{
#if UART1_FIFO_EN == 1
	g_tUart1.uart = USART1;						/* STM32 閿熸枻鎷烽敓鏂ゆ嫹閿熷€熷 */
	g_tUart1.pTxBuf = g_TxBuf1;					/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart1.pRxBuf = g_RxBuf1;					/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart1.usTxBufSize = UART1_TX_BUF_SIZE;	/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart1.usRxBufSize = UART1_RX_BUF_SIZE;	/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart1.usTxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart1.usTxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart1.usRxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart1.usRxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart1.usRxCount = 0;						/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎闈╂嫹閿熸枻鎷� */
	g_tUart1.usTxCount = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熼叺纰夋嫹閿熸枻鎷烽敓鎹烽潻鎷烽敓鏂ゆ嫹 */
	g_tUart1.SendBefor = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鍓嶉敓渚ュ洖纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	g_tUart1.SendOver = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷疯櫨閿熶茎鍥炵鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart1.ReciveNew = 0;//at_command_uart_rx_isr_handler;	/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎹风尨鎷峰箷姘愰敓鏂ゆ嫹閿熸枻鎷烽敓锟� */
#endif

#if UART2_FIFO_EN == 1
	g_tUart2.uart = USART2;						/* STM32 閿熸枻鎷烽敓鏂ゆ嫹閿熷€熷 */
	g_tUart2.pTxBuf = g_TxBuf2;					/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart2.pRxBuf = g_RxBuf2;					/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart2.usTxBufSize = UART2_TX_BUF_SIZE;	/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart2.usRxBufSize = UART2_RX_BUF_SIZE;	/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart2.usTxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart2.usTxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart2.usRxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart2.usRxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart2.usRxCount = 0;						/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎闈╂嫹閿熸枻鎷� */
	g_tUart2.usTxCount = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熼叺纰夋嫹閿熸枻鎷烽敓鎹烽潻鎷烽敓鏂ゆ嫹 */
	g_tUart2.SendBefor = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鍓嶉敓渚ュ洖纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	g_tUart2.SendOver = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷疯櫨閿熶茎鍥炵鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart2.ReciveNew = 0; //bt_driver_recieve_data_from_controller;/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎹风尨鎷峰箷姘愰敓鏂ゆ嫹閿熸枻鎷烽敓锟� */
#endif

#if UART3_FIFO_EN == 1
	g_tUart3.uart = USART3;						/* STM32 閿熸枻鎷烽敓鏂ゆ嫹閿熷€熷 */
	g_tUart3.pTxBuf = g_TxBuf3;					/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart3.pRxBuf = g_RxBuf3;					/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart3.usTxBufSize = UART3_TX_BUF_SIZE;	/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart3.usRxBufSize = UART3_RX_BUF_SIZE;	/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart3.usTxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart3.usTxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart3.usRxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart3.usRxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart3.usRxCount = 0;						/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎闈╂嫹閿熸枻鎷� */
	g_tUart3.usTxCount = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熼叺纰夋嫹閿熸枻鎷烽敓鎹烽潻鎷烽敓鏂ゆ嫹 */
	g_tUart3.SendBefor = 0;//RS485_SendBefor;		/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鍓嶉敓渚ュ洖纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	g_tUart3.SendOver = 0;//RS485_SendOver;			/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷疯櫨閿熶茎鍥炵鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart3.ReciveNew = 0;//RS485_ReciveNew;		/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎹风尨鎷峰箷姘愰敓鏂ゆ嫹閿熸枻鎷烽敓锟� */
#endif

#if UART4_FIFO_EN == 1
	g_tUart4.uart = UART4;						/* STM32 閿熸枻鎷烽敓鏂ゆ嫹閿熷€熷 */
	g_tUart4.pTxBuf = g_TxBuf4;					/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart4.pRxBuf = g_RxBuf4;					/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart4.usTxBufSize = UART4_TX_BUF_SIZE;	/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart4.usRxBufSize = UART4_RX_BUF_SIZE;	/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart4.usTxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart4.usTxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart4.usRxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart4.usRxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart4.usRxCount = 0;						/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎闈╂嫹閿熸枻鎷� */
	g_tUart4.usTxCount = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熼叺纰夋嫹閿熸枻鎷烽敓鎹烽潻鎷烽敓鏂ゆ嫹 */
	g_tUart4.SendBefor = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鍓嶉敓渚ュ洖纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	g_tUart4.SendOver = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷疯櫨閿熶茎鍥炵鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart4.ReciveNew = 0;						/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎹风尨鎷峰箷姘愰敓鏂ゆ嫹閿熸枻鎷烽敓锟� */
#endif

#if UART5_FIFO_EN == 1
	g_tUart5.uart = UART5;						/* STM32 閿熸枻鎷烽敓鏂ゆ嫹閿熷€熷 */
	g_tUart5.pTxBuf = g_TxBuf5;					/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart5.pRxBuf = g_RxBuf5;					/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart5.usTxBufSize = UART5_TX_BUF_SIZE;	/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart5.usRxBufSize = UART5_RX_BUF_SIZE;	/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart5.usTxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart5.usTxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart5.usRxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart5.usRxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart5.usRxCount = 0;						/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎闈╂嫹閿熸枻鎷� */
	g_tUart5.usTxCount = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熼叺纰夋嫹閿熸枻鎷烽敓鎹烽潻鎷烽敓鏂ゆ嫹 */
	g_tUart5.SendBefor = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鍓嶉敓渚ュ洖纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	g_tUart5.SendOver = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷疯櫨閿熶茎鍥炵鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart5.ReciveNew = 0;						/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎹风尨鎷峰箷姘愰敓鏂ゆ嫹閿熸枻鎷烽敓锟� */
#endif


#if UART6_FIFO_EN == 1
	g_tUart6.uart = USART6;						/* STM32 閿熸枻鎷烽敓鏂ゆ嫹閿熷€熷 */
	g_tUart6.pTxBuf = g_TxBuf6;					/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart6.pRxBuf = g_RxBuf6;					/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷锋寚閿熸枻鎷� */
	g_tUart6.usTxBufSize = UART6_TX_BUF_SIZE;	/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart6.usRxBufSize = UART6_RX_BUF_SIZE;	/* 閿熸枻鎷烽敓绉镐紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹灏� */
	g_tUart6.usTxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart6.usTxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart6.usRxWrite = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO鍐欓敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart6.usRxRead = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart6.usRxCount = 0;						/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎闈╂嫹閿熸枻鎷� */
	g_tUart6.usTxCount = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熼叺纰夋嫹閿熸枻鎷烽敓鎹烽潻鎷烽敓鏂ゆ嫹 */
	g_tUart6.SendBefor = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鍓嶉敓渚ュ洖纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	g_tUart6.SendOver = 0;						/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷疯櫨閿熶茎鍥炵鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	g_tUart6.ReciveNew = 0;						/* 閿熸枻鎷烽敓绉哥鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎹风尨鎷峰箷姘愰敓鏂ゆ嫹閿熸枻鎷烽敓锟� */
#endif
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: InitHardUart
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷烽敓鐭揪鎷烽敓鑺傜鎷风‖閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺粸锝忔嫹閿熸枻鎷烽敓鏂ゆ嫹浣嶉敓鏂ゆ嫹鍋滄浣嶉敓鏂ゆ嫹閿熸枻鎷峰浣嶉敓鏂ゆ嫹鏍￠敓鏂ゆ嫹浣嶉敓鏂ゆ嫹閿熷彨璁规嫹浣块敓鏉帮綇鎷烽敓缁炵尨鎷烽敓鏂ゆ嫹STM32-F4閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
static void InitHardUart(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

#if UART1_FIFO_EN == 1		/* 閿熸枻鎷烽敓鏂ゆ嫹1 TX = PA9   RX = PA10 閿熸枻鎷� TX = PB6   RX = PB7*/

	/* 閿熸枻鎷�1閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稧PIO閿熸枻鎷稶SART閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷锋椂閿熸枻鎷� */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	/* 閿熸枻鎷�2閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稶SART Tx閿熸枻鎷稧PIO閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熷眾澶嶉敓鏂ゆ嫹妯″紡 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 閿熸枻鎷�3閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稶SART Rx閿熸枻鎷稧PIO閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷锋ā寮�
		閿熸枻鎷烽敓鏂ゆ嫹CPU閿熸枻鎷蜂綅閿熸枻鎷稧PIO缂虹渷閿熸枻鎷烽敓瑙掗潻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹妯″紡閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍊熶笉閿熻鎲嬫嫹閿熸枻鎷烽敓锟�
		閿熸枻鎷烽敓瑙掞綇鎷烽敓鎻紮鎷烽敓瑙掓枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰閿熸枻鎷烽敓鏂ゆ嫹浜╅敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鏇抽敓琛楃櫢鎷烽敓鏂ゆ嫹閿熸枻鎷疯儰閿熸枻鎷疯柗閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺纰夋嫹閿熸枻鎷烽敓鐭鎷烽敓鏂ゆ嫹
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/* 閿熸枻鎷�4閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_InitStructure.USART_BaudRate = UART1_BAUD;	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);	/* 浣块敓鏉版枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		娉ㄩ敓鏂ゆ嫹: 閿熸枻鎷疯閿熻妭姝よ揪鎷烽敓娲ュ紑鍑ゆ嫹閿熸枻鎷烽敓鍙鎷�
		閿熸枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹浣块敓鏂ゆ嫹閿熸枻鎷稴endUart()閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
	*/
	USART_Cmd(USART1, ENABLE);		/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹 */

	/* CPU閿熸枻鎷峰皬缂洪敓鎹凤綇鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺煫濂斤綇鎷烽敓鏂ゆ嫹閿熻鎲嬫嫹閿熺祳end閿熸枻鎷烽敓鏂ゆ嫹閿燂拷1閿熸枻鎷烽敓琛楄妭鍑ゆ嫹閿熼叺璇ф嫹閿熸枻鎷峰幓
		閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓锟�1閿熸枻鎷烽敓琛楁枻鎷烽敓鐫嚖鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熼叺绛规嫹鍘婚敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_ClearFlag(USART1, USART_FLAG_TC);     /* 閿熻棄鍙戦敓鏂ゆ嫹閿熸枻鎷锋潃閿熻鎾呮嫹閿熺祴ransmission Complete flag */
#endif

#if UART2_FIFO_EN == 1		/* 閿熸枻鎷烽敓鏂ゆ嫹2 TX = PA2閿熸枻鎷� RX = PA3  */
/******************************************************************************
 * description : Initialization of USART2.PA0->CTS PA1->RTS PA2->TX PA3->RX
******************************************************************************/
	/* 閿熸枻鎷�1閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稧PIO閿熸枻鎷稶SART閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷锋椂閿熸枻鎷� */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	/* 閿熸枻鎷�2閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稶SART Tx閿熸枻鎷稧PIO閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熷眾澶嶉敓鏂ゆ嫹妯″紡 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1|GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 閿熸枻鎷�3閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稶SART Rx閿熸枻鎷稧PIO閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷锋ā寮�
		閿熸枻鎷烽敓鏂ゆ嫹CPU閿熸枻鎷蜂綅閿熸枻鎷稧PIO缂虹渷閿熸枻鎷烽敓瑙掗潻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹妯″紡閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍊熶笉閿熻鎲嬫嫹閿熸枻鎷烽敓锟�
		閿熸枻鎷烽敓瑙掞綇鎷烽敓鎻紮鎷烽敓瑙掓枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰閿熸枻鎷烽敓鏂ゆ嫹浜╅敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鏇抽敓琛楃櫢鎷烽敓鏂ゆ嫹閿熸枻鎷疯儰閿熸枻鎷疯柗閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺纰夋嫹閿熸枻鎷烽敓鐭鎷烽敓鏂ゆ嫹
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	/*  閿熸枻鎷�3閿熸枻鎷烽敓绐栨拝鎷烽敓鏂ゆ嫹閿熷壙锝忔嫹閿熸枻鎷烽敓鏂ゆ嫹鐛犳枻鎷烽敓鏂ゆ嫹鍦嗛敓鏂ゆ嫹閿燂拷
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	*/
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 閿熸枻鎷�4閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_InitStructure.USART_BaudRate = UART2_BAUD;	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS_CTS;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;		/* 閿熸枻鎷烽€夐敓鏂ゆ嫹閿熸枻鎷烽敓渚ワ吉锟� */
	USART_Init(USART2, &USART_InitStructure);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);	/* 浣块敓鏉版枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹 */
	//USART_ITConfig(USART2, USART_IT_TXE, ENABLE);	/* 浣块敓鏉板嚖鎷烽敓鏂ゆ嫹閿熷彨璁规嫹 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		娉ㄩ敓鏂ゆ嫹: 閿熸枻鎷疯閿熻妭姝よ揪鎷烽敓娲ュ紑鍑ゆ嫹閿熸枻鎷烽敓鍙鎷�
		閿熸枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹浣块敓鏂ゆ嫹閿熸枻鎷稴endUart()閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
	*/
	USART_Cmd(USART2, ENABLE);		/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹 */

	/* CPU閿熸枻鎷峰皬缂洪敓鎹凤綇鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺煫濂斤綇鎷烽敓鏂ゆ嫹閿熻鎲嬫嫹閿熺祳end閿熸枻鎷烽敓鏂ゆ嫹閿燂拷1閿熸枻鎷烽敓琛楄妭鍑ゆ嫹閿熼叺璇ф嫹閿熸枻鎷峰幓
		閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓锟�1閿熸枻鎷烽敓琛楁枻鎷烽敓鐫嚖鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熼叺绛规嫹鍘婚敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_ClearFlag(USART2, USART_FLAG_TC);     /* 閿熻棄鍙戦敓鏂ゆ嫹閿熸枻鎷锋潃閿熻鎾呮嫹閿熺祴ransmission Complete flag */
#endif

#if UART3_FIFO_EN == 1			/* 閿熸枻鎷烽敓鏂ゆ嫹3 TX = PB10   RX = PB11 */

	/* 閿熸枻鎷烽敓鏂ゆ嫹 PB2涓洪敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹璋㈤敓锟� RS485鑺墖閿熸枻鎷烽敓绉稿嚖鎷风姸鎬� */
#if RS485_ENABLE
	{
		RCC_APB2PeriphClockCmd(RCC_RS485_TXEN, ENABLE);

		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Pin = PIN_RS485_TXEN;
		GPIO_Init(PORT_RS485_TXEN, &GPIO_InitStructure);
	}
#endif
	/* 閿熸枻鎷�1閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鏂ゆ嫹GPIO閿熸枻鎷稶ART鏃堕敓鏂ゆ嫹 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

	/* 閿熸枻鎷�2閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稶SART Tx閿熸枻鎷稧PIO閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熷眾澶嶉敓鏂ゆ嫹妯″紡 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 閿熸枻鎷�3閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稶SART Rx閿熸枻鎷稧PIO閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷锋ā寮�
		閿熸枻鎷烽敓鏂ゆ嫹CPU閿熸枻鎷蜂綅閿熸枻鎷稧PIO缂虹渷閿熸枻鎷烽敓瑙掗潻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹妯″紡閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍊熶笉閿熻鎲嬫嫹閿熸枻鎷烽敓锟�
		閿熸枻鎷烽敓瑙掞綇鎷烽敓鎻紮鎷烽敓瑙掓枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰閿熸枻鎷烽敓鏂ゆ嫹浜╅敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鏇抽敓琛楃櫢鎷烽敓鏂ゆ嫹閿熸枻鎷疯儰閿熸枻鎷疯柗閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺纰夋嫹閿熸枻鎷烽敓鐭鎷烽敓鏂ゆ嫹
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	/*  閿熸枻鎷�3閿熸枻鎷烽敓绐栨拝鎷烽敓鏂ゆ嫹閿熷壙锝忔嫹閿熸枻鎷烽敓鏂ゆ嫹鐛犳枻鎷烽敓鏂ゆ嫹鍦嗛敓鏂ゆ嫹閿燂拷
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	*/
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 閿熸枻鎷�4閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_InitStructure.USART_BaudRate = UART3_BAUD;	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);	/* 浣块敓鏉版枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		娉ㄩ敓鏂ゆ嫹: 閿熸枻鎷疯閿熻妭姝よ揪鎷烽敓娲ュ紑鍑ゆ嫹閿熸枻鎷烽敓鍙鎷�
		閿熸枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹浣块敓鏂ゆ嫹閿熸枻鎷稴endUart()閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
	*/
	USART_Cmd(USART3, ENABLE);		/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹 */

	/* CPU閿熸枻鎷峰皬缂洪敓鎹凤綇鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺煫濂斤綇鎷烽敓鏂ゆ嫹閿熻鎲嬫嫹閿熺祳end閿熸枻鎷烽敓鏂ゆ嫹閿燂拷1閿熸枻鎷烽敓琛楄妭鍑ゆ嫹閿熼叺璇ф嫹閿熸枻鎷峰幓
		閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓锟�1閿熸枻鎷烽敓琛楁枻鎷烽敓鐫嚖鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熼叺绛规嫹鍘婚敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_ClearFlag(USART3, USART_FLAG_TC);     /* 閿熻棄鍙戦敓鏂ゆ嫹閿熸枻鎷锋潃閿熻鎾呮嫹閿熺祴ransmission Complete flag */
#endif

#if UART4_FIFO_EN == 1			/* 閿熸枻鎷烽敓鏂ゆ嫹4 TX = PC10   RX = PC11 */
	/* 閿熸枻鎷�1閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鏂ゆ嫹GPIO閿熸枻鎷稶ART鏃堕敓鏂ゆ嫹 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

	/* 閿熸枻鎷�2閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稶SART Tx閿熸枻鎷稧PIO閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熷眾澶嶉敓鏂ゆ嫹妯″紡 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* 閿熸枻鎷�3閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稶SART Rx閿熸枻鎷稧PIO閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷锋ā寮�
		閿熸枻鎷烽敓鏂ゆ嫹CPU閿熸枻鎷蜂綅閿熸枻鎷稧PIO缂虹渷閿熸枻鎷烽敓瑙掗潻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹妯″紡閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍊熶笉閿熻鎲嬫嫹閿熸枻鎷烽敓锟�
		閿熸枻鎷烽敓瑙掞綇鎷烽敓鎻紮鎷烽敓瑙掓枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰閿熸枻鎷烽敓鏂ゆ嫹浜╅敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鏇抽敓琛楃櫢鎷烽敓鏂ゆ嫹閿熸枻鎷疯儰閿熸枻鎷疯柗閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺纰夋嫹閿熸枻鎷烽敓鐭鎷烽敓鏂ゆ嫹
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* 閿熸枻鎷�4閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_InitStructure.USART_BaudRate = UART4_BAUD;	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(UART4, &USART_InitStructure);

	USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);	/* 浣块敓鏉版枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		娉ㄩ敓鏂ゆ嫹: 閿熸枻鎷疯閿熻妭姝よ揪鎷烽敓娲ュ紑鍑ゆ嫹閿熸枻鎷烽敓鍙鎷�
		閿熸枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹浣块敓鏂ゆ嫹閿熸枻鎷稴endUart()閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
	*/
	USART_Cmd(UART4, ENABLE);		/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹 */

	/* CPU閿熸枻鎷峰皬缂洪敓鎹凤綇鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺煫濂斤綇鎷烽敓鏂ゆ嫹閿熻鎲嬫嫹閿熺祳end閿熸枻鎷烽敓鏂ゆ嫹閿燂拷1閿熸枻鎷烽敓琛楄妭鍑ゆ嫹閿熼叺璇ф嫹閿熸枻鎷峰幓
		閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓锟�1閿熸枻鎷烽敓琛楁枻鎷烽敓鐫嚖鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熼叺绛规嫹鍘婚敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_ClearFlag(UART4, USART_FLAG_TC);     /* 閿熻棄鍙戦敓鏂ゆ嫹閿熸枻鎷锋潃閿熻鎾呮嫹閿熺祴ransmission Complete flag */
#endif

#if UART5_FIFO_EN == 1			/* 閿熸枻鎷烽敓鏂ゆ嫹5 TX = PC12   RX = PD2 */
	/* 閿熸枻鎷�1閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鏂ゆ嫹GPIO閿熸枻鎷稶ART鏃堕敓鏂ゆ嫹 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5, ENABLE);

	/* 閿熸枻鎷�2閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稶SART Tx閿熸枻鎷稧PIO閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熷眾澶嶉敓鏂ゆ嫹妯″紡 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* 閿熸枻鎷�3閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稶SART Rx閿熸枻鎷稧PIO閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷锋ā寮�
		閿熸枻鎷烽敓鏂ゆ嫹CPU閿熸枻鎷蜂綅閿熸枻鎷稧PIO缂虹渷閿熸枻鎷烽敓瑙掗潻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹妯″紡閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍊熶笉閿熻鎲嬫嫹閿熸枻鎷烽敓锟�
		閿熸枻鎷烽敓瑙掞綇鎷烽敓鎻紮鎷烽敓瑙掓枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰閿熸枻鎷烽敓鏂ゆ嫹浜╅敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鏇抽敓琛楃櫢鎷烽敓鏂ゆ嫹閿熸枻鎷疯儰閿熸枻鎷疯柗閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺纰夋嫹閿熸枻鎷烽敓鐭鎷烽敓鏂ゆ嫹
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOD, &GPIO_InitStructure);


	/* 閿熸枻鎷�4閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_InitStructure.USART_BaudRate = UART5_BAUD;	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(UART5, &USART_InitStructure);

	USART_ITConfig(UART5, USART_IT_RXNE, ENABLE);	/* 浣块敓鏉版枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		娉ㄩ敓鏂ゆ嫹: 閿熸枻鎷疯閿熻妭姝よ揪鎷烽敓娲ュ紑鍑ゆ嫹閿熸枻鎷烽敓鍙鎷�
		閿熸枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹浣块敓鏂ゆ嫹閿熸枻鎷稴endUart()閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
	*/
	USART_Cmd(UART5, ENABLE);		/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹 */

	/* CPU閿熸枻鎷峰皬缂洪敓鎹凤綇鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熺煫濂斤綇鎷烽敓鏂ゆ嫹閿熻鎲嬫嫹閿熺祳end閿熸枻鎷烽敓鏂ゆ嫹閿燂拷1閿熸枻鎷烽敓琛楄妭鍑ゆ嫹閿熼叺璇ф嫹閿熸枻鎷峰幓
		閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓锟�1閿熸枻鎷烽敓琛楁枻鎷烽敓鐫嚖鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熼叺绛规嫹鍘婚敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	USART_ClearFlag(UART5, USART_FLAG_TC);     /* 閿熻棄鍙戦敓鏂ゆ嫹閿熸枻鎷锋潃閿熻鎾呮嫹閿熺祴ransmission Complete flag */
#endif
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: ConfigUartNVIC
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷烽敓鐭揪鎷烽敓鏂ゆ嫹纭敓鏂ゆ嫹閿熷彨璁规嫹.
*	閿熸枻鎷�    閿熸枻鎷�:  閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
static void ConfigUartNVIC(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	/* Configure the NVIC Preemption Priority Bits */
	/*	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);  --- 閿熸枻鎷� bsp.c 閿熸枻鎷� bsp_Init() 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍙鎷烽敓鏂ゆ嫹閿熼ズ纭锋嫹閿熸枻鎷� */

#if UART1_FIFO_EN == 1
	/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹1閿熷彨璁规嫹 */
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART2_FIFO_EN == 1
	/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹2閿熷彨璁规嫹 */
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART3_FIFO_EN == 1
	/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹3閿熷彨璁规嫹t */
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART4_FIFO_EN == 1
	/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹4閿熷彨璁规嫹t */
	NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART5_FIFO_EN == 1
	/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹5閿熷彨璁规嫹t */
	NVIC_InitStructure.NVIC_IRQChannel = UART5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART6_FIFO_EN == 1
	/* 浣块敓鏉拌揪鎷烽敓鏂ゆ嫹6閿熷彨璁规嫹t */
	NVIC_InitStructure.NVIC_IRQChannel = USART6_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: UartSend
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷峰啓閿熸枻鎷烽敓鎹风鎷稶ART閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷�,閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍙柇鈽呮嫹閿熷彨鏂揪鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷疯櫨閿熸枻鎷疯繙閿熸枻鎷蜂箛杈楅敓鏂ゆ嫹閿熸枻鎷峰嵏閿燂拷
*	閿熸枻鎷�    閿熸枻鎷�:  閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
static void UartSend(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen)
{
	uint16_t i;

	for (i = 0; i < _usLen; i++)
	{
		/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷风獊閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷蜂斧閿熸枻鎷烽敓鏂ゆ嫹鑰嶉敓鏂ゆ嫹閿熼ズ杈炬嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	#if 0
		/*
			閿熻妭纰夋嫹閿熸枻鎷稧PRS閿熸枻鎷烽敓鏂ゆ嫹鏃堕敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鎷囬敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷穡hile 閿熸枻鎷峰惊閿熸枻鎷�
			鍘熼敓鏂ゆ嫹 閿熸枻鎷烽敓閰电鎷�1閿熸枻鎷烽敓琛楁枻鎷锋椂 _pUart->usTxWrite = 1閿熸枻鎷穇pUart->usTxRead = 0;
			閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷穡hile(1) 閿熺潾鍑ゆ嫹閿熷壙绛规嫹
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
		/* 閿熸枻鎷� _pUart->usTxBufSize == 1 鏃�, 閿熸枻鎷烽敓鏂ゆ嫹鏆敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓锟�(閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�) */
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

		/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓璇彂閿熼叺浼欐嫹閿熸枻鎷烽敓鏂ゆ嫹 */
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
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: UartGetChar
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸帴杈炬嫹閿熻妭鏂ゆ嫹閿熺Ц浼欐嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰彇1閿熻鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷疯尗閿燂拷
*	閿熸枻鎷�    閿熸枻鎷�: _pUart : 閿熸枻鎷烽敓鏂ゆ嫹閿熷€熷
*			  _pByte : 閿熸枻鎷峰摝閿熼ズ鈽呮嫹閿熸枻鎷疯幐閿熻闈╂嫹閿燂拷
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 0 閿熸枻鎷风ず閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�  1閿熸枻鎷风ず閿熸枻鎷峰彇閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�
*********************************************************************************************************
*/
static uint8_t UartGetChar(UART_T *_pUart, uint8_t *_pByte)
{
	uint16_t usCount;

	/* usRxWrite 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍙柇鐚存嫹閿熸枻鎷烽敓鍙唻鎷烽敓鏂ゆ嫹鍐欓敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓楗衡槄鎷峰附閿熸枻鎷烽敓缁炴唻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍔枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	DISABLE_INT();
	usCount = _pUart->usRxCount;
	ENABLE_INT();

	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍙揪鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓閰殿剨鎷烽敓鏂ゆ嫹铓嶇シ閿燂拷0 */
	//if (_pUart->usRxRead == usRxWrite)
	if (usCount == 0)	/* 閿熺獤鎾呮嫹娌￠敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	{
		return 0;
	}
	else
	{
		*_pByte = _pUart->pRxBuf[_pUart->usRxRead];		/* 閿熸帴杈炬嫹閿熻妭鏂ゆ嫹閿熸枻鎷稦IFO鍙�1閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */

		/* 閿熸枻鎷峰啓FIFO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
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
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: UartIRQ
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸枻鎷烽敓鍙柇鍑ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鐭綇鎷烽€氶敓鐭揪鎷烽敓鏂ゆ嫹閿熷彨鏂揪鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹
*	閿熸枻鎷�    閿熸枻鎷�: _pUart : 閿熸枻鎷烽敓鏂ゆ嫹閿熷€熷
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
static void UartIRQ(UART_T *_pUart)
{
	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹  */
	if (USART_GetITStatus(_pUart->uart, USART_IT_RXNE) != RESET)
	{
		/* 閿熸帴杈炬嫹閿熻妭鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎瀵勮揪鎷烽敓鏂ゆ嫹閿熸枻鎷峰彇閿熸枻鎷烽敓鎹疯揪鎷疯閿熸枻鎷烽敓鏂ゆ嫹閿熺唇IFO */
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

		/* 閿熸埅纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹,閫氱煡搴旈敓鐭鎷烽敓鏂ゆ嫹閿熺Ц纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�,涓€閿熸枻鎷烽敓瑙掑嚖鎷烽敓鏂ゆ嫹1閿熸枻鎷烽敓鏂ゆ嫹鎭敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷蜂竴閿熸枻鎷烽敓鏂ゆ嫹閿燂拷 */
		//if (_pUart->usRxWrite == _pUart->usRxRead)
		//if (_pUart->usRxCount == 1)
		{
			if (_pUart->ReciveNew)
			{
				_pUart->ReciveNew(ch);
			}
		}
	}

	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹 */
	if (USART_GetITStatus(_pUart->uart, USART_IT_TXE) != RESET)
	{
		//if (_pUart->usTxRead == _pUart->usTxWrite)
		if (_pUart->usTxCount == 0)
		{
			/* 閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰彇閿熸枻鎷锋椂閿熸枻鎷� 閿熸枻鎷锋閿熸枻鎷烽敓閰典紮鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹 閿熸枻鎷锋敞閿熻В锛氶敓鏂ゆ嫹鏃堕敓鏂ゆ嫹閿燂拷1閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎浼欐嫹鏈敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹甯岄敓锟�*/
			USART_ITConfig(_pUart->uart, USART_IT_TXE, DISABLE);

			/* 浣块敓鏂ゆ嫹閿熸枻鎷烽敓鎹峰嚖鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鍗搁敓锟� */
			USART_ITConfig(_pUart->uart, USART_IT_TC, ENABLE);
		}
		else
		{
			/* 閿熸帴鍑ゆ嫹閿熸枻鎷稦IFO鍙�1閿熸枻鎷烽敓琛楁枻鎷峰啓閿熻涓查敓鑺傚嚖鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鎹峰瘎杈炬嫹閿熸枻鎷� */
			USART_SendData(_pUart->uart, _pUart->pTxBuf[_pUart->usTxRead]);
			if (++_pUart->usTxRead >= _pUart->usTxBufSize)
			{
				_pUart->usTxRead = 0;
			}
			_pUart->usTxCount--;
		}

	}
	/* 閿熸枻鎷烽敓鏂ゆ嫹bit浣嶅叏閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹绯婚敓鏂ゆ嫹鍗搁敓锟� */
	else if (USART_GetITStatus(_pUart->uart, USART_IT_TC) != RESET)
	{
		//if (_pUart->usTxRead == _pUart->usTxWrite)
		if (_pUart->usTxCount == 0)
		{
			/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓绱絀FO閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰叏閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹甯岄敓鏂ゆ嫹閿熻鐧告嫹閿熸枻鎷疯帢閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鍙鎷� */
			USART_ITConfig(_pUart->uart, USART_IT_TC, DISABLE);

			/* 閿熸埅纰夋嫹閿熸枻鎷烽敓鏂ゆ嫹, 涓€閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷稲S485閫氶敓鑴氾綇鎷烽敓鏂ゆ嫹RS485鑺墖閿熸枻鎷烽敓鏂ゆ嫹涓洪敓鏂ゆ嫹閿熸枻鎷锋ā寮忛敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰崰閿熸枻鎷烽敓鏂ゆ嫹 */
			if (_pUart->SendOver)
			{
				_pUart->SendOver();
			}
		}
		else
		{
			/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹鎷㈤敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷锋湐閿熻锟� */

			/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓绱絀FO閿熸枻鎷烽敓鏂ゆ嫹閿熸嵎浼欐嫹鏈敓鏂ゆ嫹甯岄敓鏂ゆ嫹閿熸帴鍑ゆ嫹閿熸枻鎷稦IFO鍙�1閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰啓閿熻鍙戦敓鏂ゆ嫹閿熸枻鎷烽敓鎹峰瘎杈炬嫹閿熸枻鎷� */
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
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: USART1_IRQHandler  USART2_IRQHandler USART3_IRQHandler UART4_IRQHandler UART5_IRQHandler
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: USART閿熷彨鏂嚖鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓锟�
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
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
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: fputc
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸埅璁规嫹閿熸枻鎷穚utc閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷蜂娇閿熸枻鎷穚rintf閿熸枻鎷烽敓鏂ゆ嫹閿熸帴杈炬嫹閿熸枻鎷�1閿熸枻鎷峰嵃閿熸枻鎷烽敓锟�
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
int fputc(int ch, FILE *f)
{
#if 1	/* 閿熸枻鎷烽敓鏂ゆ嫹瑕乸rintf閿熸枻鎷烽敓琛楀嚖鎷烽€氶敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熷彨璁规嫹FIFO閿熸枻鎷烽敓閰电鎷峰幓閿熸枻鎷穚rintf閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� */
	comSendChar(COM1, ch);

	return ch;
#else	/* 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷峰紡閿熸枻鎷烽敓鏂ゆ嫹姣忛敓鏂ゆ嫹閿熻鍑ゆ嫹,閿熼ズ杈炬嫹閿熸枻鎷烽敓鎹峰嚖鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓锟� */
	/* 鍐欎竴閿熸枻鎷烽敓琛楄妭纰夋嫹USART1 */
	USART_SendData(USART1, (uint8_t) ch);

	/* 閿熼ズ杈炬嫹閿熸枻鎷烽敓閰垫枻鎷烽敓鏂ゆ嫹 */
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
	{}

	return ch;
#endif
}

/*
*********************************************************************************************************
*	閿熸枻鎷� 閿熸枻鎷� 閿熸枻鎷�: fgetc
*	閿熸枻鎷烽敓鏂ゆ嫹璇撮敓鏂ゆ嫹: 閿熸埅璁规嫹閿熸枻鎷穏etc閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷蜂娇閿熸枻鎷穏etchar閿熸枻鎷烽敓鏂ゆ嫹閿熸帴杈炬嫹閿熸枻鎷�1閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹
*	閿熸枻鎷�    閿熸枻鎷�: 閿熸枻鎷�
*	閿熸枻鎷� 閿熸枻鎷� 鍊�: 閿熸枻鎷�
*********************************************************************************************************
*/
int fgetc(FILE *f)
{

#if 1	/* 閿熸帴杈炬嫹閿熻妭鏂ゆ嫹閿熸枻鎷稦IFO閿熸枻鎷峰彇1閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷�, 鍙敓鏂ゆ嫹鍙栭敓鏂ゆ嫹閿熸枻鎷烽敓鎹锋墠鍑ゆ嫹閿熸枻鎷� */
	uint8_t ucData;

	while(comGetChar(COM1, &ucData) == 0);

	return ucData;
#else
	/* 閿熼ズ杈炬嫹閿熸枻鎷烽敓鏂ゆ嫹1閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹 */
	while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);

	return (int)USART_ReceiveData(USART1);
#endif
}

int _write(int fd, char *pBuffer, int size)
{
	comSendBuf(COM1, pBuffer, size);
	return 0;
}



/***************************** 閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷� www.armfly.com (END OF FILE) *********************************/
