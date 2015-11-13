//
// Minimal support for printf to USART
// - current version is just blocking

#include "app.h"
#include "system_config.h"
#include "peripheral/usart/plib_usart.h"
#include "system/clk/sys_clk.h"
#include "system/ports/sys_ports.h"
#include "app_debug.h"

/// Debug Output ///

// called by printf
void _mon_putc (char c)
{
    while (!PLIB_USART_TransmitterIsEmpty(APP_DEBUG_USART_ID));
    PLIB_USART_TransmitterByteSend(APP_DEBUG_USART_ID, c);;
}

void APP_Debug_Initialize(void){

    // PPS Output Mapping:
    PLIB_PORTS_RemapOutput(PORTS_ID_0, OUTPUT_FUNC_U1TX, OUTPUT_PIN_RPD3 );

    /* Initialize USART */
    PLIB_USART_BaudRateSet(APP_DEBUG_USART_ID, SYS_CLK_PeripheralFrequencyGet(CLK_BUS_PERIPHERAL_1), APP_DEBUG_USART_BAUD);
    PLIB_USART_HandshakeModeSelect(APP_DEBUG_USART_ID, USART_HANDSHAKE_MODE_FLOW_CONTROL);
    PLIB_USART_OperationModeSelect(APP_DEBUG_USART_ID, USART_ENABLE_TX_RX_USED);
    PLIB_USART_LineControlModeSelect(APP_DEBUG_USART_ID, USART_8N1);
    PLIB_USART_TransmitterEnable(APP_DEBUG_USART_ID);

    PLIB_USART_Enable(APP_DEBUG_USART_ID);
}