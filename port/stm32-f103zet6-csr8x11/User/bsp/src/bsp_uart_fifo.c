/*
*********************************************************************************************************
*
*	模锟斤拷锟斤拷锟斤拷 : 锟斤拷锟斤拷锟叫讹拷+FIFO锟斤拷锟斤拷模锟斤拷
*	锟侥硷拷锟斤拷锟斤拷 : bsp_uart_fifo.c
*	锟斤拷    锟斤拷 : V1.0
*	说    锟斤拷 : 锟斤拷锟矫达拷锟斤拷锟叫讹拷+FIFO模式实锟街讹拷锟斤拷锟斤拷诘锟酵憋拷锟斤拷锟�
*	锟睫改硷拷录 :
*		锟芥本锟斤拷  锟斤拷锟斤拷       锟斤拷锟斤拷    说锟斤拷
*		V1.0    2013-02-01 armfly  锟斤拷式锟斤拷锟斤拷
*		V1.1    2013-06-09 armfly  FiFo锟结构锟斤拷锟斤拷TxCount锟斤拷员锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟叫断伙拷锟斤拷锟斤拷锟斤拷; 锟斤拷锟斤拷 锟斤拷FiFo锟侥猴拷锟斤拷
*		V1.2	2014-09-29 armfly  锟斤拷锟斤拷RS485 MODBUS锟接口★拷锟斤拷锟秸碉拷锟斤拷锟街节猴拷直锟斤拷执锟叫回碉拷锟斤拷锟斤拷锟斤拷
*
*	锟睫改诧拷锟斤拷 : 
*		锟芥本锟斤拷    锟斤拷锟斤拷       锟斤拷锟斤拷                  说锟斤拷
*		V1.0    2015-08-19  Eric2013       锟劫斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷FreeRTOS锟斤拷锟斤拷
*
*	Copyright (C), 2013-2014, 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"


/* 锟斤拷锟斤拷每锟斤拷锟斤拷锟节结构锟斤拷锟斤拷锟� */
#if UART1_FIFO_EN == 1
	static UART_T g_tUart1;
	static uint8_t g_TxBuf1[UART1_TX_BUF_SIZE];		/* 锟斤拷锟酵伙拷锟斤拷锟斤拷 */
	static uint8_t g_RxBuf1[UART1_RX_BUF_SIZE];		/* 锟斤拷锟秸伙拷锟斤拷锟斤拷 */
#endif

#if UART2_FIFO_EN == 1
	static UART_T g_tUart2;
	static uint8_t g_TxBuf2[UART2_TX_BUF_SIZE];		/* 锟斤拷锟酵伙拷锟斤拷锟斤拷 */
	static uint8_t g_RxBuf2[UART2_RX_BUF_SIZE];		/* 锟斤拷锟秸伙拷锟斤拷锟斤拷 */
#endif

#if UART3_FIFO_EN == 1
	static UART_T g_tUart3;
	static uint8_t g_TxBuf3[UART3_TX_BUF_SIZE];		/* 锟斤拷锟酵伙拷锟斤拷锟斤拷 */
	static uint8_t g_RxBuf3[UART3_RX_BUF_SIZE];		/* 锟斤拷锟秸伙拷锟斤拷锟斤拷 */
#endif

#if UART4_FIFO_EN == 1
	static UART_T g_tUart4;
	static uint8_t g_TxBuf4[UART4_TX_BUF_SIZE];		/* 锟斤拷锟酵伙拷锟斤拷锟斤拷 */
	static uint8_t g_RxBuf4[UART4_RX_BUF_SIZE];		/* 锟斤拷锟秸伙拷锟斤拷锟斤拷 */
#endif

#if UART5_FIFO_EN == 1
	static UART_T g_tUart5;
	static uint8_t g_TxBuf5[UART5_TX_BUF_SIZE];		/* 锟斤拷锟酵伙拷锟斤拷锟斤拷 */
	static uint8_t g_RxBuf5[UART5_RX_BUF_SIZE];		/* 锟斤拷锟秸伙拷锟斤拷锟斤拷 */
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
*	锟斤拷 锟斤拷 锟斤拷: bsp_InitUart
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷始锟斤拷锟斤拷锟斤拷硬锟斤拷锟斤拷锟斤拷锟斤拷全锟街憋拷锟斤拷锟斤拷锟斤拷值.
*	锟斤拷    锟斤拷:  锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
void bsp_InitUart(void)
{
	UartVarInit();		/* 锟斤拷锟斤拷锟饺筹拷始锟斤拷全锟街憋拷锟斤拷,锟斤拷锟斤拷锟斤拷硬锟斤拷 */

	InitHardUart();		/* 锟斤拷锟矫达拷锟节碉拷硬锟斤拷锟斤拷锟斤拷(锟斤拷锟斤拷锟绞碉拷) */
	
#if RS485_ENABLE
	RS485_InitTXE();	/* 锟斤拷锟斤拷RS485芯片锟侥凤拷锟斤拷使锟斤拷硬锟斤拷锟斤拷锟斤拷锟斤拷为锟斤拷锟斤拷锟斤拷锟� */
#endif
	
	ConfigUartNVIC();	/* 锟斤拷锟矫达拷锟斤拷锟叫讹拷 */
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: ComToUart
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷COM锟剿口猴拷转锟斤拷为UART指锟斤拷
*	锟斤拷    锟斤拷: _ucPort: 锟剿口猴拷(COM1 - COM6)
*	锟斤拷 锟斤拷 值: uart指锟斤拷
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
		/* 锟斤拷锟斤拷锟轿何达拷锟斤拷 */
		return 0;
	}
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: comSendBuf
*	锟斤拷锟斤拷说锟斤拷: 锟津串口凤拷锟斤拷一锟斤拷锟斤拷锟捷★拷锟斤拷锟捷放碉拷锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟截ｏ拷锟斤拷锟叫断凤拷锟斤拷锟斤拷锟斤拷诤锟教拷锟缴凤拷锟斤拷
*	锟斤拷    锟斤拷: _ucPort: 锟剿口猴拷(COM1 - COM6)
*			  _ucaBuf: 锟斤拷锟斤拷锟酵碉拷锟斤拷锟捷伙拷锟斤拷锟斤拷
*			  _usLen : 锟斤拷锟捷筹拷锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
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
		pUart->SendBefor();		/* 锟斤拷锟斤拷锟絉S485通锟脚ｏ拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷薪锟絉S485锟斤拷锟斤拷为锟斤拷锟斤拷模式 */
	}

	UartSend(pUart, _ucaBuf, _usLen);
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: comSendChar
*	锟斤拷锟斤拷说锟斤拷: 锟津串口凤拷锟斤拷1锟斤拷锟街节★拷锟斤拷锟捷放碉拷锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟截ｏ拷锟斤拷锟叫断凤拷锟斤拷锟斤拷锟斤拷诤锟教拷锟缴凤拷锟斤拷
*	锟斤拷    锟斤拷: _ucPort: 锟剿口猴拷(COM1 - COM6)
*			  _ucByte: 锟斤拷锟斤拷锟酵碉拷锟斤拷锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
void comSendChar(COM_PORT_E _ucPort, uint8_t _ucByte)
{
	comSendBuf(_ucPort, &_ucByte, 1);
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: comGetChar
*	锟斤拷锟斤拷说锟斤拷: 锟接达拷锟节伙拷锟斤拷锟斤拷锟斤拷取1锟街节ｏ拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟捷撅拷锟斤拷锟斤拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: _ucPort: 锟剿口猴拷(COM1 - COM6)
*			  _pByte: 锟斤拷锟秸碉拷锟斤拷锟斤拷锟捷达拷锟斤拷锟斤拷锟斤拷锟斤拷址
*	锟斤拷 锟斤拷 值: 0 锟斤拷示锟斤拷锟斤拷锟斤拷, 1 锟斤拷示锟斤拷取锟斤拷锟斤拷效锟街斤拷
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
*	锟斤拷 锟斤拷 锟斤拷: comGetChar
*	锟斤拷锟斤拷说锟斤拷: 锟接达拷锟节伙拷锟斤拷锟斤拷锟斤拷取n锟街节ｏ拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟捷撅拷锟斤拷锟斤拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: _ucPort: 锟剿口猴拷(COM1 - COM6)
*			  _pBuf: 锟斤拷锟秸碉拷锟斤拷锟斤拷锟捷达拷锟斤拷锟斤拷锟斤拷锟斤拷址
*			  _usLen: 准锟斤拷锟斤拷取锟斤拷锟斤拷锟捷筹拷锟斤拷
*	锟斤拷 锟斤拷 值: 实锟绞讹拷锟斤拷锟斤拷锟斤拷锟捷筹拷锟饺ｏ拷0 锟斤拷示锟斤拷锟斤拷锟斤拷
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
	/* usRxWrite 锟斤拷锟斤拷锟斤拷锟叫断猴拷锟斤拷锟叫憋拷锟斤拷写锟斤拷锟斤拷锟斤拷锟斤拷锟饺★拷帽锟斤拷锟绞憋拷锟斤拷锟斤拷锟斤拷锟斤拷锟劫斤拷锟斤拷锟斤拷锟斤拷 */
	DISABLE_INT();
	usCount = pUart->usRxCount;
	ENABLE_INT();

	/* 锟斤拷锟斤拷锟斤拷锟叫达拷锟斤拷锟斤拷锟酵拷锟斤拷蚍祷锟�0 */
	if (usCount == 0)	/* 锟窖撅拷没锟斤拷锟斤拷锟斤拷 */
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
		pUart->usRxRead = pUart->usRxWrite; /*锟斤拷位FIFO wptr * rptr指锟斤拷*/
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
			pUart->usRxRead = usLeftCount; /*锟斤拷位FIFO wptr * rptr指锟斤拷*/
		}
		else 
		{
			memcpy(_pBuf, &pUart->pRxBuf[pUart->usRxRead], _usLenToRead);
			pUart->usRxRead += _usLenToRead; /*锟斤拷位FIFO wptr * rptr指锟斤拷*/
		}
		pUart->usRxCount -= _usLenToRead;
		ENABLE_INT();
		return _usLenToRead;
	}
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: comClearTxFifo
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷取锟斤拷锟节伙拷锟斤拷锟斤拷available buffer length
*	锟斤拷    锟斤拷: _ucPort: 锟剿口猴拷(COM1 - COM6)
*	锟斤拷 锟斤拷 值: 锟斤拷
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
*	锟斤拷 锟斤拷 锟斤拷: comClearTxFifo
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷锟姐串锟节凤拷锟酵伙拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: _ucPort: 锟剿口猴拷(COM1 - COM6)
*	锟斤拷 锟斤拷 值: 锟斤拷
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
*	锟斤拷 锟斤拷 锟斤拷: comClearRxFifo
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷锟姐串锟节斤拷锟秸伙拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: _ucPort: 锟剿口猴拷(COM1 - COM6)
*	锟斤拷 锟斤拷 值: 锟斤拷
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
*	锟斤拷 锟斤拷 锟斤拷: bsp_SetUart1Baud
*	锟斤拷锟斤拷说锟斤拷: 锟睫革拷UART1锟斤拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
void bsp_SetUart1Baud(uint32_t _baud)
{
	USART_InitTypeDef USART_InitStructure;

	/* 锟斤拷2锟斤拷锟斤拷 锟斤拷锟矫达拷锟斤拷硬锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_BaudRate = _baud;	/* 锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: bsp_SetUart2Baud
*	锟斤拷锟斤拷说锟斤拷: 锟睫革拷UART2锟斤拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
void bsp_SetUart2Baud(uint32_t _baud)
{
	USART_InitTypeDef USART_InitStructure;

	/* 锟斤拷2锟斤拷锟斤拷 锟斤拷锟矫达拷锟斤拷硬锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_BaudRate = _baud;	/* 锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);
}


/* 锟斤拷锟斤拷锟絉S485通锟脚ｏ拷锟诫按锟斤拷锟铰革拷式锟斤拷写锟斤拷锟斤拷锟斤拷 锟斤拷锟角斤拷锟斤拷锟斤拷 USART3锟斤拷为RS485锟斤拷锟斤拷锟斤拷 */

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: RS485_InitTXE
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷锟斤拷RS485锟斤拷锟斤拷使锟杰匡拷锟斤拷 TXE
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
void RS485_InitTXE(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_RS485_TXEN, ENABLE);	/* 锟斤拷GPIO时锟斤拷 */

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* 锟斤拷锟斤拷锟斤拷锟侥Ｊ� */
	GPIO_InitStructure.GPIO_Pin = PIN_RS485_TXEN;
	GPIO_Init(PORT_RS485_TXEN, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: bsp_Set485Baud
*	锟斤拷锟斤拷说锟斤拷: 锟睫革拷UART3锟斤拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
void bsp_Set485Baud(uint32_t _baud)
{
	USART_InitTypeDef USART_InitStructure;

	/* 锟斤拷2锟斤拷锟斤拷 锟斤拷锟矫达拷锟斤拷硬锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_BaudRate = _baud;	/* 锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: RS485_SendBefor
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷锟斤拷锟斤拷锟斤拷前锟斤拷准锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷RS485通锟脚ｏ拷锟斤拷锟斤拷锟斤拷RS485芯片为锟斤拷锟斤拷状态锟斤拷
*			  锟斤拷锟睫革拷 UartVarInit()锟叫的猴拷锟斤拷指锟斤拷锟斤拷诒锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟� g_tUart2.SendBefor = RS485_SendBefor
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
void RS485_SendBefor(void)
{
	RS485_TX_EN();	/* 锟叫伙拷RS485锟秸凤拷芯片为锟斤拷锟斤拷模式 */
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: RS485_SendOver
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷锟斤拷一锟斤拷锟斤拷锟捷斤拷锟斤拷锟斤拷锟斤拷坪锟斤拷锟斤拷锟斤拷锟斤拷锟絉S485通锟脚ｏ拷锟斤拷锟斤拷锟斤拷RS485芯片为锟斤拷锟斤拷状态锟斤拷
*			  锟斤拷锟睫革拷 UartVarInit()锟叫的猴拷锟斤拷指锟斤拷锟斤拷诒锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟� g_tUart2.SendOver = RS485_SendOver
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
void RS485_SendOver(void)
{
	RS485_RX_EN();	/* 锟叫伙拷RS485锟秸凤拷芯片为锟斤拷锟斤拷模式 */
}


/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: RS485_SendBuf
*	锟斤拷锟斤拷说锟斤拷: 通锟斤拷RS485芯片锟斤拷锟斤拷一锟斤拷锟斤拷锟捷★拷注锟解，锟斤拷锟斤拷锟斤拷锟斤拷锟饺达拷锟斤拷锟斤拷锟斤拷稀锟�
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
void RS485_SendBuf(uint8_t *_ucaBuf, uint16_t _usLen)
{
	comSendBuf(COM3, _ucaBuf, _usLen);
}


/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: RS485_SendStr
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷485锟斤拷锟竭凤拷锟斤拷一锟斤拷锟街凤拷锟斤拷
*	锟斤拷    锟斤拷: _pBuf 锟斤拷锟捷伙拷锟斤拷锟斤拷
*			 _ucLen 锟斤拷锟捷筹拷锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
void RS485_SendStr(char *_pBuf)
{
	RS485_SendBuf((uint8_t *)_pBuf, strlen(_pBuf));
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: RS485_ReciveNew
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷锟秸碉拷锟铰碉拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: _byte 锟斤拷锟秸碉拷锟斤拷锟斤拷锟斤拷锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
//extern void MODBUS_ReciveNew(uint8_t _byte);
void RS485_ReciveNew(uint8_t _byte)
{
//	MODBUS_ReciveNew(_byte);
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: UartVarInit
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷始锟斤拷锟斤拷锟斤拷锟斤拷氐谋锟斤拷锟�
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
extern void bt_driver_recieve_data_from_controller(uint8_t data);
extern void at_command_uart_rx_isr_handler(uint8_t data);
static void UartVarInit(void)
{
#if UART1_FIFO_EN == 1
	g_tUart1.uart = USART1;						/* STM32 锟斤拷锟斤拷锟借备 */
	g_tUart1.pTxBuf = g_TxBuf1;					/* 锟斤拷锟酵伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart1.pRxBuf = g_RxBuf1;					/* 锟斤拷锟秸伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart1.usTxBufSize = UART1_TX_BUF_SIZE;	/* 锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart1.usRxBufSize = UART1_RX_BUF_SIZE;	/* 锟斤拷锟秸伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart1.usTxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart1.usTxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart1.usRxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart1.usRxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart1.usRxCount = 0;						/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart1.usTxCount = 0;						/* 锟斤拷锟斤拷锟酵碉拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart1.SendBefor = 0;						/* 锟斤拷锟斤拷锟斤拷锟斤拷前锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart1.SendOver = 0;						/* 锟斤拷锟斤拷锟斤拷虾锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart1.ReciveNew = 0;//at_command_uart_rx_isr_handler;	/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟捷猴拷幕氐锟斤拷锟斤拷锟� */
#endif

#if UART2_FIFO_EN == 1
	g_tUart2.uart = USART2;						/* STM32 锟斤拷锟斤拷锟借备 */
	g_tUart2.pTxBuf = g_TxBuf2;					/* 锟斤拷锟酵伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart2.pRxBuf = g_RxBuf2;					/* 锟斤拷锟秸伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart2.usTxBufSize = UART2_TX_BUF_SIZE;	/* 锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart2.usRxBufSize = UART2_RX_BUF_SIZE;	/* 锟斤拷锟秸伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart2.usTxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart2.usTxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart2.usRxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart2.usRxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart2.usRxCount = 0;						/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart2.usTxCount = 0;						/* 锟斤拷锟斤拷锟酵碉拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart2.SendBefor = 0;						/* 锟斤拷锟斤拷锟斤拷锟斤拷前锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart2.SendOver = 0;						/* 锟斤拷锟斤拷锟斤拷虾锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart2.ReciveNew = 0; //bt_driver_recieve_data_from_controller;/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟捷猴拷幕氐锟斤拷锟斤拷锟� */
#endif

#if UART3_FIFO_EN == 1
	g_tUart3.uart = USART3;						/* STM32 锟斤拷锟斤拷锟借备 */
	g_tUart3.pTxBuf = g_TxBuf3;					/* 锟斤拷锟酵伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart3.pRxBuf = g_RxBuf3;					/* 锟斤拷锟秸伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart3.usTxBufSize = UART3_TX_BUF_SIZE;	/* 锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart3.usRxBufSize = UART3_RX_BUF_SIZE;	/* 锟斤拷锟秸伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart3.usTxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart3.usTxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart3.usRxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart3.usRxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart3.usRxCount = 0;						/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart3.usTxCount = 0;						/* 锟斤拷锟斤拷锟酵碉拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart3.SendBefor = 0;//RS485_SendBefor;		/* 锟斤拷锟斤拷锟斤拷锟斤拷前锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart3.SendOver = 0;//RS485_SendOver;			/* 锟斤拷锟斤拷锟斤拷虾锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart3.ReciveNew = 0;//RS485_ReciveNew;		/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟捷猴拷幕氐锟斤拷锟斤拷锟� */
#endif

#if UART4_FIFO_EN == 1
	g_tUart4.uart = UART4;						/* STM32 锟斤拷锟斤拷锟借备 */
	g_tUart4.pTxBuf = g_TxBuf4;					/* 锟斤拷锟酵伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart4.pRxBuf = g_RxBuf4;					/* 锟斤拷锟秸伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart4.usTxBufSize = UART4_TX_BUF_SIZE;	/* 锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart4.usRxBufSize = UART4_RX_BUF_SIZE;	/* 锟斤拷锟秸伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart4.usTxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart4.usTxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart4.usRxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart4.usRxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart4.usRxCount = 0;						/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart4.usTxCount = 0;						/* 锟斤拷锟斤拷锟酵碉拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart4.SendBefor = 0;						/* 锟斤拷锟斤拷锟斤拷锟斤拷前锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart4.SendOver = 0;						/* 锟斤拷锟斤拷锟斤拷虾锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart4.ReciveNew = 0;						/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟捷猴拷幕氐锟斤拷锟斤拷锟� */
#endif

#if UART5_FIFO_EN == 1
	g_tUart5.uart = UART5;						/* STM32 锟斤拷锟斤拷锟借备 */
	g_tUart5.pTxBuf = g_TxBuf5;					/* 锟斤拷锟酵伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart5.pRxBuf = g_RxBuf5;					/* 锟斤拷锟秸伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart5.usTxBufSize = UART5_TX_BUF_SIZE;	/* 锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart5.usRxBufSize = UART5_RX_BUF_SIZE;	/* 锟斤拷锟秸伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart5.usTxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart5.usTxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart5.usRxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart5.usRxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart5.usRxCount = 0;						/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart5.usTxCount = 0;						/* 锟斤拷锟斤拷锟酵碉拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart5.SendBefor = 0;						/* 锟斤拷锟斤拷锟斤拷锟斤拷前锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart5.SendOver = 0;						/* 锟斤拷锟斤拷锟斤拷虾锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart5.ReciveNew = 0;						/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟捷猴拷幕氐锟斤拷锟斤拷锟� */
#endif


#if UART6_FIFO_EN == 1
	g_tUart6.uart = USART6;						/* STM32 锟斤拷锟斤拷锟借备 */
	g_tUart6.pTxBuf = g_TxBuf6;					/* 锟斤拷锟酵伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart6.pRxBuf = g_RxBuf6;					/* 锟斤拷锟秸伙拷锟斤拷锟斤拷指锟斤拷 */
	g_tUart6.usTxBufSize = UART6_TX_BUF_SIZE;	/* 锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart6.usRxBufSize = UART6_RX_BUF_SIZE;	/* 锟斤拷锟秸伙拷锟斤拷锟斤拷锟斤拷小 */
	g_tUart6.usTxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart6.usTxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart6.usRxWrite = 0;						/* 锟斤拷锟斤拷FIFO写锟斤拷锟斤拷 */
	g_tUart6.usRxRead = 0;						/* 锟斤拷锟斤拷FIFO锟斤拷锟斤拷锟斤拷 */
	g_tUart6.usRxCount = 0;						/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart6.usTxCount = 0;						/* 锟斤拷锟斤拷锟酵碉拷锟斤拷锟捷革拷锟斤拷 */
	g_tUart6.SendBefor = 0;						/* 锟斤拷锟斤拷锟斤拷锟斤拷前锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart6.SendOver = 0;						/* 锟斤拷锟斤拷锟斤拷虾锟侥回碉拷锟斤拷锟斤拷 */
	g_tUart6.ReciveNew = 0;						/* 锟斤拷锟秸碉拷锟斤拷锟斤拷锟捷猴拷幕氐锟斤拷锟斤拷锟� */
#endif
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: InitHardUart
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷锟矫达拷锟节碉拷硬锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟绞ｏ拷锟斤拷锟斤拷位锟斤拷停止位锟斤拷锟斤拷始位锟斤拷校锟斤拷位锟斤拷锟叫讹拷使锟杰ｏ拷锟绞猴拷锟斤拷STM32-F4锟斤拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
static void InitHardUart(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

#if UART1_FIFO_EN == 1		/* 锟斤拷锟斤拷1 TX = PA9   RX = PA10 锟斤拷 TX = PB6   RX = PB7*/

	/* 锟斤拷1锟斤拷锟斤拷锟斤拷GPIO锟斤拷USART锟斤拷锟斤拷锟斤拷时锟斤拷 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	/* 锟斤拷2锟斤拷锟斤拷锟斤拷USART Tx锟斤拷GPIO锟斤拷锟斤拷为锟斤拷锟届复锟斤拷模式 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 锟斤拷3锟斤拷锟斤拷锟斤拷USART Rx锟斤拷GPIO锟斤拷锟斤拷为锟斤拷锟斤拷锟斤拷锟斤拷模式
		锟斤拷锟斤拷CPU锟斤拷位锟斤拷GPIO缺省锟斤拷锟角革拷锟斤拷锟斤拷锟斤拷模式锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟借不锟角憋拷锟斤拷锟�
		锟斤拷锟角ｏ拷锟揭伙拷锟角斤拷锟斤拷锟斤拷媳锟斤拷锟斤拷亩锟斤拷锟斤拷锟斤拷曳锟街癸拷锟斤拷锟斤拷胤锟斤拷薷锟斤拷锟斤拷锟斤拷锟斤拷锟竭碉拷锟斤拷锟矫诧拷锟斤拷
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/* 锟斤拷4锟斤拷锟斤拷 锟斤拷锟矫达拷锟斤拷硬锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_BaudRate = UART1_BAUD;	/* 锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);	/* 使锟杰斤拷锟斤拷锟叫讹拷 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		注锟斤拷: 锟斤拷要锟节此达拷锟津开凤拷锟斤拷锟叫讹拷
		锟斤拷锟斤拷锟叫讹拷使锟斤拷锟斤拷SendUart()锟斤拷锟斤拷锟斤拷
	*/
	USART_Cmd(USART1, ENABLE);		/* 使锟杰达拷锟斤拷 */

	/* CPU锟斤拷小缺锟捷ｏ拷锟斤拷锟斤拷锟斤拷锟矫好ｏ拷锟斤拷锟街憋拷锟絊end锟斤拷锟斤拷锟�1锟斤拷锟街节凤拷锟酵诧拷锟斤拷去
		锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟�1锟斤拷锟街斤拷锟睫凤拷锟斤拷确锟斤拷锟酵筹拷去锟斤拷锟斤拷锟斤拷 */
	USART_ClearFlag(USART1, USART_FLAG_TC);     /* 锟藉发锟斤拷锟斤拷杀锟街撅拷锟絋ransmission Complete flag */
#endif

#if UART2_FIFO_EN == 1		/* 锟斤拷锟斤拷2 TX = PA2锟斤拷 RX = PA3  */
/******************************************************************************
 * description : Initialization of USART2.PA0->CTS PA1->RTS PA2->TX PA3->RX
******************************************************************************/
	/* 锟斤拷1锟斤拷锟斤拷锟斤拷GPIO锟斤拷USART锟斤拷锟斤拷锟斤拷时锟斤拷 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	/* 锟斤拷2锟斤拷锟斤拷锟斤拷USART Tx锟斤拷GPIO锟斤拷锟斤拷为锟斤拷锟届复锟斤拷模式 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1|GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 锟斤拷3锟斤拷锟斤拷锟斤拷USART Rx锟斤拷GPIO锟斤拷锟斤拷为锟斤拷锟斤拷锟斤拷锟斤拷模式
		锟斤拷锟斤拷CPU锟斤拷位锟斤拷GPIO缺省锟斤拷锟角革拷锟斤拷锟斤拷锟斤拷模式锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟借不锟角憋拷锟斤拷锟�
		锟斤拷锟角ｏ拷锟揭伙拷锟角斤拷锟斤拷锟斤拷媳锟斤拷锟斤拷亩锟斤拷锟斤拷锟斤拷曳锟街癸拷锟斤拷锟斤拷胤锟斤拷薷锟斤拷锟斤拷锟斤拷锟斤拷锟竭碉拷锟斤拷锟矫诧拷锟斤拷
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	/*  锟斤拷3锟斤拷锟窖撅拷锟斤拷锟剿ｏ拷锟斤拷锟斤拷獠斤拷锟斤拷圆锟斤拷锟�
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	*/
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 锟斤拷4锟斤拷锟斤拷 锟斤拷锟矫达拷锟斤拷硬锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_BaudRate = UART2_BAUD;	/* 锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS_CTS;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;		/* 锟斤拷选锟斤拷锟斤拷锟侥Ｊ� */
	USART_Init(USART2, &USART_InitStructure);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);	/* 使锟杰斤拷锟斤拷锟叫讹拷 */
	//USART_ITConfig(USART2, USART_IT_TXE, ENABLE);	/* 使锟杰凤拷锟斤拷锟叫讹拷 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		注锟斤拷: 锟斤拷要锟节此达拷锟津开凤拷锟斤拷锟叫讹拷
		锟斤拷锟斤拷锟叫讹拷使锟斤拷锟斤拷SendUart()锟斤拷锟斤拷锟斤拷
	*/
	USART_Cmd(USART2, ENABLE);		/* 使锟杰达拷锟斤拷 */

	/* CPU锟斤拷小缺锟捷ｏ拷锟斤拷锟斤拷锟斤拷锟矫好ｏ拷锟斤拷锟街憋拷锟絊end锟斤拷锟斤拷锟�1锟斤拷锟街节凤拷锟酵诧拷锟斤拷去
		锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟�1锟斤拷锟街斤拷锟睫凤拷锟斤拷确锟斤拷锟酵筹拷去锟斤拷锟斤拷锟斤拷 */
	USART_ClearFlag(USART2, USART_FLAG_TC);     /* 锟藉发锟斤拷锟斤拷杀锟街撅拷锟絋ransmission Complete flag */
#endif

#if UART3_FIFO_EN == 1			/* 锟斤拷锟斤拷3 TX = PB10   RX = PB11 */

	/* 锟斤拷锟斤拷 PB2为锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷谢锟� RS485芯片锟斤拷锟秸凤拷状态 */
#if RS485_ENABLE
	{
		RCC_APB2PeriphClockCmd(RCC_RS485_TXEN, ENABLE);

		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Pin = PIN_RS485_TXEN;
		GPIO_Init(PORT_RS485_TXEN, &GPIO_InitStructure);
	}
#endif
	/* 锟斤拷1锟斤拷锟斤拷 锟斤拷锟斤拷GPIO锟斤拷UART时锟斤拷 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

	/* 锟斤拷2锟斤拷锟斤拷锟斤拷USART Tx锟斤拷GPIO锟斤拷锟斤拷为锟斤拷锟届复锟斤拷模式 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 锟斤拷3锟斤拷锟斤拷锟斤拷USART Rx锟斤拷GPIO锟斤拷锟斤拷为锟斤拷锟斤拷锟斤拷锟斤拷模式
		锟斤拷锟斤拷CPU锟斤拷位锟斤拷GPIO缺省锟斤拷锟角革拷锟斤拷锟斤拷锟斤拷模式锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟借不锟角憋拷锟斤拷锟�
		锟斤拷锟角ｏ拷锟揭伙拷锟角斤拷锟斤拷锟斤拷媳锟斤拷锟斤拷亩锟斤拷锟斤拷锟斤拷曳锟街癸拷锟斤拷锟斤拷胤锟斤拷薷锟斤拷锟斤拷锟斤拷锟斤拷锟竭碉拷锟斤拷锟矫诧拷锟斤拷
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	/*  锟斤拷3锟斤拷锟窖撅拷锟斤拷锟剿ｏ拷锟斤拷锟斤拷獠斤拷锟斤拷圆锟斤拷锟�
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	*/
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* 锟斤拷4锟斤拷锟斤拷 锟斤拷锟矫达拷锟斤拷硬锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_BaudRate = UART3_BAUD;	/* 锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);	/* 使锟杰斤拷锟斤拷锟叫讹拷 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		注锟斤拷: 锟斤拷要锟节此达拷锟津开凤拷锟斤拷锟叫讹拷
		锟斤拷锟斤拷锟叫讹拷使锟斤拷锟斤拷SendUart()锟斤拷锟斤拷锟斤拷
	*/
	USART_Cmd(USART3, ENABLE);		/* 使锟杰达拷锟斤拷 */

	/* CPU锟斤拷小缺锟捷ｏ拷锟斤拷锟斤拷锟斤拷锟矫好ｏ拷锟斤拷锟街憋拷锟絊end锟斤拷锟斤拷锟�1锟斤拷锟街节凤拷锟酵诧拷锟斤拷去
		锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟�1锟斤拷锟街斤拷锟睫凤拷锟斤拷确锟斤拷锟酵筹拷去锟斤拷锟斤拷锟斤拷 */
	USART_ClearFlag(USART3, USART_FLAG_TC);     /* 锟藉发锟斤拷锟斤拷杀锟街撅拷锟絋ransmission Complete flag */
#endif

#if UART4_FIFO_EN == 1			/* 锟斤拷锟斤拷4 TX = PC10   RX = PC11 */
	/* 锟斤拷1锟斤拷锟斤拷 锟斤拷锟斤拷GPIO锟斤拷UART时锟斤拷 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

	/* 锟斤拷2锟斤拷锟斤拷锟斤拷USART Tx锟斤拷GPIO锟斤拷锟斤拷为锟斤拷锟届复锟斤拷模式 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* 锟斤拷3锟斤拷锟斤拷锟斤拷USART Rx锟斤拷GPIO锟斤拷锟斤拷为锟斤拷锟斤拷锟斤拷锟斤拷模式
		锟斤拷锟斤拷CPU锟斤拷位锟斤拷GPIO缺省锟斤拷锟角革拷锟斤拷锟斤拷锟斤拷模式锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟借不锟角憋拷锟斤拷锟�
		锟斤拷锟角ｏ拷锟揭伙拷锟角斤拷锟斤拷锟斤拷媳锟斤拷锟斤拷亩锟斤拷锟斤拷锟斤拷曳锟街癸拷锟斤拷锟斤拷胤锟斤拷薷锟斤拷锟斤拷锟斤拷锟斤拷锟竭碉拷锟斤拷锟矫诧拷锟斤拷
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* 锟斤拷4锟斤拷锟斤拷 锟斤拷锟矫达拷锟斤拷硬锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_BaudRate = UART4_BAUD;	/* 锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(UART4, &USART_InitStructure);

	USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);	/* 使锟杰斤拷锟斤拷锟叫讹拷 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		注锟斤拷: 锟斤拷要锟节此达拷锟津开凤拷锟斤拷锟叫讹拷
		锟斤拷锟斤拷锟叫讹拷使锟斤拷锟斤拷SendUart()锟斤拷锟斤拷锟斤拷
	*/
	USART_Cmd(UART4, ENABLE);		/* 使锟杰达拷锟斤拷 */

	/* CPU锟斤拷小缺锟捷ｏ拷锟斤拷锟斤拷锟斤拷锟矫好ｏ拷锟斤拷锟街憋拷锟絊end锟斤拷锟斤拷锟�1锟斤拷锟街节凤拷锟酵诧拷锟斤拷去
		锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟�1锟斤拷锟街斤拷锟睫凤拷锟斤拷确锟斤拷锟酵筹拷去锟斤拷锟斤拷锟斤拷 */
	USART_ClearFlag(UART4, USART_FLAG_TC);     /* 锟藉发锟斤拷锟斤拷杀锟街撅拷锟絋ransmission Complete flag */
#endif

#if UART5_FIFO_EN == 1			/* 锟斤拷锟斤拷5 TX = PC12   RX = PD2 */
	/* 锟斤拷1锟斤拷锟斤拷 锟斤拷锟斤拷GPIO锟斤拷UART时锟斤拷 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5, ENABLE);

	/* 锟斤拷2锟斤拷锟斤拷锟斤拷USART Tx锟斤拷GPIO锟斤拷锟斤拷为锟斤拷锟届复锟斤拷模式 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* 锟斤拷3锟斤拷锟斤拷锟斤拷USART Rx锟斤拷GPIO锟斤拷锟斤拷为锟斤拷锟斤拷锟斤拷锟斤拷模式
		锟斤拷锟斤拷CPU锟斤拷位锟斤拷GPIO缺省锟斤拷锟角革拷锟斤拷锟斤拷锟斤拷模式锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟借不锟角憋拷锟斤拷锟�
		锟斤拷锟角ｏ拷锟揭伙拷锟角斤拷锟斤拷锟斤拷媳锟斤拷锟斤拷亩锟斤拷锟斤拷锟斤拷曳锟街癸拷锟斤拷锟斤拷胤锟斤拷薷锟斤拷锟斤拷锟斤拷锟斤拷锟竭碉拷锟斤拷锟矫诧拷锟斤拷
	*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOD, &GPIO_InitStructure);


	/* 锟斤拷4锟斤拷锟斤拷 锟斤拷锟矫达拷锟斤拷硬锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_BaudRate = UART5_BAUD;	/* 锟斤拷锟斤拷锟斤拷 */
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(UART5, &USART_InitStructure);

	USART_ITConfig(UART5, USART_IT_RXNE, ENABLE);	/* 使锟杰斤拷锟斤拷锟叫讹拷 */
	/*
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
		注锟斤拷: 锟斤拷要锟节此达拷锟津开凤拷锟斤拷锟叫讹拷
		锟斤拷锟斤拷锟叫讹拷使锟斤拷锟斤拷SendUart()锟斤拷锟斤拷锟斤拷
	*/
	USART_Cmd(UART5, ENABLE);		/* 使锟杰达拷锟斤拷 */

	/* CPU锟斤拷小缺锟捷ｏ拷锟斤拷锟斤拷锟斤拷锟矫好ｏ拷锟斤拷锟街憋拷锟絊end锟斤拷锟斤拷锟�1锟斤拷锟街节凤拷锟酵诧拷锟斤拷去
		锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟�1锟斤拷锟街斤拷锟睫凤拷锟斤拷确锟斤拷锟酵筹拷去锟斤拷锟斤拷锟斤拷 */
	USART_ClearFlag(UART5, USART_FLAG_TC);     /* 锟藉发锟斤拷锟斤拷杀锟街撅拷锟絋ransmission Complete flag */
#endif
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: ConfigUartNVIC
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷锟矫达拷锟斤拷硬锟斤拷锟叫讹拷.
*	锟斤拷    锟斤拷:  锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
static void ConfigUartNVIC(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	/* Configure the NVIC Preemption Priority Bits */
	/*	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);  --- 锟斤拷 bsp.c 锟斤拷 bsp_Init() 锟斤拷锟斤拷锟斤拷锟叫讹拷锟斤拷锟饺硷拷锟斤拷 */

#if UART1_FIFO_EN == 1
	/* 使锟杰达拷锟斤拷1锟叫讹拷 */
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART2_FIFO_EN == 1
	/* 使锟杰达拷锟斤拷2锟叫讹拷 */
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART3_FIFO_EN == 1
	/* 使锟杰达拷锟斤拷3锟叫讹拷t */
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART4_FIFO_EN == 1
	/* 使锟杰达拷锟斤拷4锟叫讹拷t */
	NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART5_FIFO_EN == 1
	/* 使锟杰达拷锟斤拷5锟叫讹拷t */
	NVIC_InitStructure.NVIC_IRQChannel = UART5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif

#if UART6_FIFO_EN == 1
	/* 使锟杰达拷锟斤拷6锟叫讹拷t */
	NVIC_InitStructure.NVIC_IRQChannel = USART6_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
#endif
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: UartSend
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷写锟斤拷锟捷碉拷UART锟斤拷锟酵伙拷锟斤拷锟斤拷,锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟叫断★拷锟叫断达拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷虾锟斤拷远锟斤拷乇辗锟斤拷锟斤拷卸锟�
*	锟斤拷    锟斤拷:  锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
static void UartSend(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen)
{
	uint16_t i;

	for (i = 0; i < _usLen; i++)
	{
		/* 锟斤拷锟斤拷锟斤拷突锟斤拷锟斤拷锟斤拷丫锟斤拷锟斤拷耍锟斤拷锟饺达拷锟斤拷锟斤拷锟斤拷锟斤拷 */
	#if 0
		/*
			锟节碉拷锟斤拷GPRS锟斤拷锟斤拷时锟斤拷锟斤拷锟斤拷拇锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷while 锟斤拷循锟斤拷
			原锟斤拷 锟斤拷锟酵碉拷1锟斤拷锟街斤拷时 _pUart->usTxWrite = 1锟斤拷_pUart->usTxRead = 0;
			锟斤拷锟斤拷锟斤拷while(1) 锟睫凤拷锟剿筹拷
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
		/* 锟斤拷 _pUart->usTxBufSize == 1 时, 锟斤拷锟斤拷暮锟斤拷锟斤拷锟斤拷锟斤拷锟�(锟斤拷锟斤拷锟斤拷) */
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

		/* 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟诫发锟酵伙拷锟斤拷锟斤拷 */
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
*	锟斤拷 锟斤拷 锟斤拷: UartGetChar
*	锟斤拷锟斤拷说锟斤拷: 锟接达拷锟节斤拷锟秸伙拷锟斤拷锟斤拷锟斤拷取1锟街斤拷锟斤拷锟斤拷 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷茫锟�
*	锟斤拷    锟斤拷: _pUart : 锟斤拷锟斤拷锟借备
*			  _pByte : 锟斤拷哦锟饺★拷锟斤拷莸锟街革拷锟�
*	锟斤拷 锟斤拷 值: 0 锟斤拷示锟斤拷锟斤拷锟斤拷  1锟斤拷示锟斤拷取锟斤拷锟斤拷锟斤拷
*********************************************************************************************************
*/
static uint8_t UartGetChar(UART_T *_pUart, uint8_t *_pByte)
{
	uint16_t usCount;

	/* usRxWrite 锟斤拷锟斤拷锟斤拷锟叫断猴拷锟斤拷锟叫憋拷锟斤拷写锟斤拷锟斤拷锟斤拷锟斤拷锟饺★拷帽锟斤拷锟绞憋拷锟斤拷锟斤拷锟斤拷锟斤拷锟劫斤拷锟斤拷锟斤拷锟斤拷 */
	DISABLE_INT();
	usCount = _pUart->usRxCount;
	ENABLE_INT();

	/* 锟斤拷锟斤拷锟斤拷锟叫达拷锟斤拷锟斤拷锟酵拷锟斤拷蚍祷锟�0 */
	//if (_pUart->usRxRead == usRxWrite)
	if (usCount == 0)	/* 锟窖撅拷没锟斤拷锟斤拷锟斤拷 */
	{
		return 0;
	}
	else
	{
		*_pByte = _pUart->pRxBuf[_pUart->usRxRead];		/* 锟接达拷锟节斤拷锟斤拷FIFO取1锟斤拷锟斤拷锟斤拷 */

		/* 锟斤拷写FIFO锟斤拷锟斤拷锟斤拷 */
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
*	锟斤拷 锟斤拷 锟斤拷: UartIRQ
*	锟斤拷锟斤拷说锟斤拷: 锟斤拷锟叫断凤拷锟斤拷锟斤拷锟斤拷锟矫ｏ拷通锟矫达拷锟斤拷锟叫断达拷锟斤拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: _pUart : 锟斤拷锟斤拷锟借备
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
static void UartIRQ(UART_T *_pUart)
{
	/* 锟斤拷锟斤拷锟斤拷锟斤拷锟叫讹拷  */
	if (USART_GetITStatus(_pUart->uart, USART_IT_RXNE) != RESET)
	{
		/* 锟接达拷锟节斤拷锟斤拷锟斤拷锟捷寄达拷锟斤拷锟斤拷取锟斤拷锟捷达拷诺锟斤拷锟斤拷锟紽IFO */
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

		/* 锟截碉拷锟斤拷锟斤拷,通知应锟矫筹拷锟斤拷锟秸碉拷锟斤拷锟斤拷锟斤拷,一锟斤拷锟角凤拷锟斤拷1锟斤拷锟斤拷息锟斤拷锟斤拷锟斤拷锟斤拷一锟斤拷锟斤拷锟� */
		//if (_pUart->usRxWrite == _pUart->usRxRead)
		//if (_pUart->usRxCount == 1)
		{
			if (_pUart->ReciveNew)
			{
				_pUart->ReciveNew(ch);
			}
		}
	}

	/* 锟斤拷锟斤拷锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷锟叫讹拷 */
	if (USART_GetITStatus(_pUart->uart, USART_IT_TXE) != RESET)
	{
		//if (_pUart->usTxRead == _pUart->usTxWrite)
		if (_pUart->usTxCount == 0)
		{
			/* 锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷取锟斤拷时锟斤拷 锟斤拷止锟斤拷锟酵伙拷锟斤拷锟斤拷锟斤拷锟叫讹拷 锟斤拷注锟解：锟斤拷时锟斤拷锟�1锟斤拷锟斤拷锟捷伙拷未锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷希锟�*/
			USART_ITConfig(_pUart->uart, USART_IT_TXE, DISABLE);

			/* 使锟斤拷锟斤拷锟捷凤拷锟斤拷锟斤拷锟斤拷卸锟� */
			USART_ITConfig(_pUart->uart, USART_IT_TC, ENABLE);
		}
		else
		{
			/* 锟接凤拷锟斤拷FIFO取1锟斤拷锟街斤拷写锟诫串锟节凤拷锟斤拷锟斤拷锟捷寄达拷锟斤拷 */
			USART_SendData(_pUart->uart, _pUart->pTxBuf[_pUart->usTxRead]);
			if (++_pUart->usTxRead >= _pUart->usTxBufSize)
			{
				_pUart->usTxRead = 0;
			}
			_pUart->usTxCount--;
		}

	}
	/* 锟斤拷锟斤拷bit位全锟斤拷锟斤拷锟斤拷锟斤拷系锟斤拷卸锟� */
	else if (USART_GetITStatus(_pUart->uart, USART_IT_TC) != RESET)
	{
		//if (_pUart->usTxRead == _pUart->usTxWrite)
		if (_pUart->usTxCount == 0)
		{
			/* 锟斤拷锟斤拷锟斤拷锟紽IFO锟斤拷锟斤拷锟斤拷全锟斤拷锟斤拷锟斤拷锟斤拷希锟斤拷锟街癸拷锟斤拷莘锟斤拷锟斤拷锟斤拷锟叫讹拷 */
			USART_ITConfig(_pUart->uart, USART_IT_TC, DISABLE);

			/* 锟截碉拷锟斤拷锟斤拷, 一锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷RS485通锟脚ｏ拷锟斤拷RS485芯片锟斤拷锟斤拷为锟斤拷锟斤拷模式锟斤拷锟斤拷锟斤拷锟斤拷占锟斤拷锟斤拷 */
			if (_pUart->SendOver)
			{
				_pUart->SendOver();
			}
		}
		else
		{
			/* 锟斤拷锟斤拷锟斤拷锟斤拷拢锟斤拷锟斤拷锟斤拷锟斤拷朔锟街� */

			/* 锟斤拷锟斤拷锟斤拷锟紽IFO锟斤拷锟斤拷锟捷伙拷未锟斤拷希锟斤拷锟接凤拷锟斤拷FIFO取1锟斤拷锟斤拷锟斤拷写锟诫发锟斤拷锟斤拷锟捷寄达拷锟斤拷 */
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
*	锟斤拷 锟斤拷 锟斤拷: USART1_IRQHandler  USART2_IRQHandler USART3_IRQHandler UART4_IRQHandler UART5_IRQHandler
*	锟斤拷锟斤拷说锟斤拷: USART锟叫断凤拷锟斤拷锟斤拷锟�
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
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
*	锟斤拷 锟斤拷 锟斤拷: fputc
*	锟斤拷锟斤拷说锟斤拷: 锟截讹拷锟斤拷putc锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷使锟斤拷printf锟斤拷锟斤拷锟接达拷锟斤拷1锟斤拷印锟斤拷锟�
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
int fputc(int ch, FILE *f)
{
#if 1	/* 锟斤拷锟斤拷要printf锟斤拷锟街凤拷通锟斤拷锟斤拷锟斤拷锟叫讹拷FIFO锟斤拷锟酵筹拷去锟斤拷printf锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 */
	comSendChar(COM1, ch);

	return ch;
#else	/* 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷式锟斤拷锟斤拷每锟斤拷锟街凤拷,锟饺达拷锟斤拷锟捷凤拷锟斤拷锟斤拷锟� */
	/* 写一锟斤拷锟街节碉拷USART1 */
	USART_SendData(USART1, (uint8_t) ch);

	/* 锟饺达拷锟斤拷锟酵斤拷锟斤拷 */
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
	{}

	return ch;
#endif
}

/*
*********************************************************************************************************
*	锟斤拷 锟斤拷 锟斤拷: fgetc
*	锟斤拷锟斤拷说锟斤拷: 锟截讹拷锟斤拷getc锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷使锟斤拷getchar锟斤拷锟斤拷锟接达拷锟斤拷1锟斤拷锟斤拷锟斤拷锟斤拷
*	锟斤拷    锟斤拷: 锟斤拷
*	锟斤拷 锟斤拷 值: 锟斤拷
*********************************************************************************************************
*/
int fgetc(FILE *f)
{

#if 1	/* 锟接达拷锟节斤拷锟斤拷FIFO锟斤拷取1锟斤拷锟斤拷锟斤拷, 只锟斤拷取锟斤拷锟斤拷锟捷才凤拷锟斤拷 */
	uint8_t ucData;

	while(comGetChar(COM1, &ucData) == 0);

	return ucData;
#else
	/* 锟饺达拷锟斤拷锟斤拷1锟斤拷锟斤拷锟斤拷锟斤拷 */
	while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);

	return (int)USART_ReceiveData(USART1);
#endif
}

/***************************** 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 www.armfly.com (END OF FILE) *********************************/
