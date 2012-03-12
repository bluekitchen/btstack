/*=============================================================================
* (C) Copyright Albis Technologies Ltd 2011
*==============================================================================
*                STM32 Example Code
*==============================================================================
* File name:     bsp_debug.c
*
* Notes:         STM32 evaluation board UM0780 debug utilities BSP.
*============================================================================*/

#include <stdarg.h>

#include "bsp.h"
#include "bsp_debug.h"
#include "T32_Term.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"

#define DEBUG_T32_TERM_IN_USE   0

#define DEBUG_UART_IN_USE       0
#define UART_2_REMAP            1

#define DEBUG_UART_1            1
#define DEBUG_UART_2            2
#define DEBUG_UART_3            3

/*=============================================================================
=============================================================================*/
static void writeByteSerialPort(const char b)
{
#if(DEBUG_UART_IN_USE == DEBUG_UART_1)
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) != SET);
    USART_SendData(USART1, b);
#endif

#if(DEBUG_UART_IN_USE == DEBUG_UART_2)
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) != SET);
    USART_SendData(USART2, b);
#endif

#if(DEBUG_UART_IN_USE == DEBUG_UART_3)
    while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) != SET);
    USART_SendData(USART3, b);
#endif

#if(DEBUG_T32_TERM_IN_USE > 0)
    while(T32_Term_TXStatus(t32_termport) != 0);
    T32_Term_Put(t32_termport, b);
#endif
}

/*=============================================================================
=============================================================================*/
static void writeStringSerialPort(const char *s)
{
    int total = 0;

    while(s[total])
    {
        writeByteSerialPort(s[total++]);
    }
}

/*=============================================================================
=============================================================================*/
static void dbgOutNumHex(unsigned long n, long depth)
{
    if(depth)
    {
        depth--;
    }

    if((n & ~0xf) || depth)
    {
        dbgOutNumHex(n >> 4, depth);
        n &= 0xf;
    }

    if(n < 10)
    {
        writeByteSerialPort((unsigned char)(n + '0'));
    }
    else
    {
        writeByteSerialPort((unsigned char)(n - 10 + 'A'));
    }
}

/*=============================================================================
=============================================================================*/
static void dbgOutNumDecimal(unsigned long n)
{
    if(n >= 10)
    {
        dbgOutNumDecimal(n / 10);
        n %= 10;
    }
    writeByteSerialPort((unsigned char)(n + '0'));
}

/*=============================================================================
=============================================================================*/
void initSerialDebug(void)
{
#if(DEBUG_UART_IN_USE > 0)
    GPIO_InitTypeDef gpio_init;
    USART_InitTypeDef usart_init;
    USART_ClockInitTypeDef usart_clk_init;

    /* ----------------- INIT USART STRUCT ---------------- */
    usart_init.USART_BaudRate = 115200 / 2;
    usart_init.USART_WordLength = USART_WordLength_8b;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    usart_clk_init.USART_Clock = USART_Clock_Disable;
    usart_clk_init.USART_CPOL = USART_CPOL_Low;
    usart_clk_init.USART_CPHA = USART_CPHA_2Edge;
    usart_clk_init.USART_LastBit = USART_LastBit_Disable;

#if(DEBUG_UART_IN_USE == DEBUG_UART_1)
    BSP_PeriphEn(BSP_PERIPH_ID_USART1);

    /* ----------------- SETUP USART1 GPIO ---------------- */
#if(UART_1_REMAP > 0)
    BSP_PeriphEn(BSP_PERIPH_ID_IOPB);
    BSP_PeriphEn(BSP_PERIPH_ID_IOPD);
    BSP_PeriphEn(BSP_PERIPH_ID_AFIO);
    GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE);

    /* Configure GPIOB.6 as push-pull.                      */
    gpio_init.GPIO_Pin = GPIO_Pin_6;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio_init);

    /* Configure GPIOB.7 as input floating.                 */
    gpio_init.GPIO_Pin = GPIO_Pin_7;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio_init);
#else
    BSP_PeriphEn(BSP_PERIPH_ID_IOPA);

    /* Configure GPIOA.9 as push-pull.                      */
    gpio_init.GPIO_Pin = GPIO_Pin_9;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio_init);

    /* Configure GPIOA.10 as input floating.                */
    gpio_init.GPIO_Pin = GPIO_Pin_10;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio_init);
#endif /* UART_1_REMAP */

    /* ------------------ SETUP USART1 -------------------- */
    USART_Init(USART1, &usart_init);
    USART_ClockInit(USART1, &usart_clk_init);
    USART_Cmd(USART1, ENABLE);

#ifdef UART_IRQ
    BSP_IntVectSet(BSP_INT_ID_USART1, debug_uart_isr);
    BSP_IntEn(BSP_INT_ID_USART1);
#endif /* UART_IRQ */
#endif /* DEBUG_UART_1 */

#if(DEBUG_UART_IN_USE == DEBUG_UART_2)
    BSP_PeriphEn(BSP_PERIPH_ID_USART2);

    /* ----------------- SETUP USART2 GPIO ---------------- */
#if(UART_2_REMAP > 0)
    BSP_PeriphEn(BSP_PERIPH_ID_IOPD);
    BSP_PeriphEn(BSP_PERIPH_ID_AFIO);
    GPIO_PinRemapConfig(GPIO_Remap_USART2, ENABLE);

    /* Configure GPIOD.5 as push-pull.                      */
    gpio_init.GPIO_Pin = GPIO_Pin_5;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &gpio_init);

    /* Configure GPIOD.6 as input floating.                 */
    gpio_init.GPIO_Pin = GPIO_Pin_6;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &gpio_init);
#else
    BSP_PeriphEn(BSP_PERIPH_ID_IOPA);

    /* Configure GPIOA.2 as push-pull.                      */
    gpio_init.GPIO_Pin = GPIO_Pin_2;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio_init);

    /* Configure GPIOA.3 as input floating.                 */
    gpio_init.GPIO_Pin = GPIO_Pin_3;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio_init);
#endif /* UART_2_REMAP */

    /* ------------------ SETUP USART2 -------------------- */
    USART_Init(USART2, &usart_init);
    USART_ClockInit(USART2, &usart_clk_init);
    USART_Cmd(USART2, ENABLE);

#ifdef UART_IRQ
    BSP_IntVectSet(BSP_INT_ID_USART2, debug_uart_isr);
    BSP_IntEn(BSP_INT_ID_USART2);
#endif /* UART_IRQ */
#endif /* DEBUG_UART_2 */

#if(DEBUG_UART_IN_USE == DEBUG_UART_3)
    BSP_PeriphEn(BSP_PERIPH_ID_USART3);

    /* ----------------- SETUP USART3 GPIO ---------------- */
#if(UART_3_REMAP_PARTIAL > 0)
    BSP_PeriphEn(BSP_PERIPH_ID_IOPC);
    BSP_PeriphEn(BSP_PERIPH_ID_AFIO);
    GPIO_PinRemapConfig(GPIO_PartialRemap_USART3, ENABLE);

    /* Configure GPIOC.10 as push-pull.                     */
    gpio_init.GPIO_Pin = GPIO_Pin_10;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC, &gpio_init);

    /* Configure GPIOC.11 as input floating.                */
    gpio_init.GPIO_Pin = GPIO_Pin_11;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &gpio_init);

#elif(UART_3_REMAP_FULL > 0)
    BSP_PeriphEn(BSP_PERIPH_ID_IOPD);
    BSP_PeriphEn(BSP_PERIPH_ID_AFIO);
    GPIO_PinRemapConfig(GPIO_FullRemap_USART3, ENABLE);

    /* Configure GPIOD.8 as push-pull.                      */
    gpio_init.GPIO_Pin = GPIO_Pin_8;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &gpio_init);

    /* Configure GPIOD.9 as input floating.                 */
    gpio_init.GPIO_Pin = GPIO_Pin_9;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &gpio_init);

#else
    BSP_PeriphEn(BSP_PERIPH_ID_IOPB);

    /* Configure GPIOB.10 as push-pull.                     */
    gpio_init.GPIO_Pin = GPIO_Pin_10;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio_init);

    /* Configure GPIOB.11 as input floating.                */
    gpio_init.GPIO_Pin = GPIO_Pin_11;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio_init);
#endif /* UART_3_REMAP_FULL */

    /* ------------------ SETUP USART3 -------------------- */
    USART_Init(USART3, &usart_init);
    USART_ClockInit(USART3, &usart_clk_init);
    USART_Cmd(USART3, ENABLE);

#ifdef UART_IRQ
    BSP_IntVectSet(BSP_INT_ID_USART3, debug_uart_isr);
    BSP_IntEn(BSP_INT_ID_USART3);
#endif /* UART_IRQ */
#endif /* DEBUG_UART_3 */
#endif /* DEBUG_UART_IN_USE */
}

/*=============================================================================
=============================================================================*/
void printos(const char *sz, ...)
{
    unsigned char c;
    va_list vl;

    va_start(vl, sz);

    while (*sz)
    {
        c = *sz++;
        switch (c)
        {
        case '%':
            c = *sz++;
            switch (c) {
            case 'x':
                dbgOutNumHex(va_arg(vl, unsigned long), 0);
                break;
            case 'B':
                dbgOutNumHex(va_arg(vl, unsigned long), 2);
                break;
            case 'H':
                dbgOutNumHex(va_arg(vl, unsigned long), 4);
                break;
            case 'X':
                dbgOutNumHex(va_arg(vl, unsigned long), 8);
                break;
            case 'd':
                {
                    long    l;

                    l = va_arg(vl, long);
                    if (l < 0) {
                        writeByteSerialPort('-');
                        l = - l;
                    }
                    dbgOutNumDecimal((unsigned long)l);
                }
                break;
            case 'u':
                dbgOutNumDecimal(va_arg(vl, unsigned long));
                break;
            case 's':
                writeStringSerialPort(va_arg(vl, char *));
                break;
            case '%':
                writeByteSerialPort('%');
                break;
            case 'c':
                c = (unsigned char)va_arg(vl, unsigned int);
                writeByteSerialPort(c);
                break;

            default:
                writeByteSerialPort(' ');
                break;
            }
            break;
        case '\r':
            if (*sz == '\n')
                sz ++;
            c = '\n';
            /* fall through */
        case '\n':
            writeByteSerialPort('\r');
            /* fall through */
        default:
            writeByteSerialPort(c);
        }
    }

    va_end(vl);
}

/*=============================================================================
=============================================================================*/
void restartSystem(void)
{
    BSP_LED_On(3);

    /* TODO */
    for(;;);
}

/*=============================================================================
=============================================================================*/
void fatalErrorHandler(const int reset,
                       const char *fileName,
                       unsigned short lineNumber)
{
    printos("\r\nFATAL SW ERROR IN %s at %d!\r\n", fileName, lineNumber);

    restartSystem();
}
