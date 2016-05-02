#ifndef _SYSTEM_CONFIG_H
#define _SYSTEM_CONFIG_H

// from tutorial
#define _PLIB_UNSUPPORTED

// from bsp
// *****************************************************************************
/* Clock System Service Configuration Options
*/
#define SYS_CLK_FREQ                        96000000ul
#define SYS_CLK_BUS_PERIPHERAL_1            48000000ul
#define SYS_CLK_UPLL_BEFORE_DIV2_FREQ       96000000ul
#define SYS_CLK_CONFIG_PRIMARY_XTAL         12000000ul
#define SYS_CLK_CONFIG_SECONDARY_XTAL       0ul

// from tutorial
/* TMR Driver Configuration Options */
#define DRV_TMR_INSTANCES_NUMBER 1
#define DRV_TMR_INTERRUPT_MODE false
#define DRV_TMR_CLIENTS_NUMBER 1
#define DRV_TMR_COUNT_WIDTH                 16
#define DRV_TMR_ALARM_ENABLE
#define DRV_TMR_ALARM_PERIODIC true

/* System Clock Frequency */
#define SYS_CLK_CONFIG_FREQ_ERROR_LIMIT 10
#define SYS_CLOCK_FREQENCY (96000000)

/* TMR Driver Initialization Data */
/* TMR Driver Initialization Data */
#define APP_TMR_DRV_INDEX                   0
#define APP_TMR_DRV_POWER_MODE              SYS_MODULE_POWER_RUN_FULL
#define APP_TMR_DRV_HW_ID                   TMR_ID_2
#define APP_TMR_DRV_CLOCK_SOURCE            DRV_TMR_CLKSOURCE_INTERNAL
#define APP_TMR_DRV_PRESCALE                TMR_PRESCALE_VALUE_256
#define APP_TMR_DRV_INT_SOURCE              INT_SOURCE_TIMER_2
#define APP_TMR_DRV_OPERATION_MODE          DRV_TMR_OPERATION_MODE_16_BIT

/* Console output */
#define APP_DEBUG_USART_ID USART_ID_1
#define APP_DEBUG_USART_BAUD 115200

/* Bluetooth configuration */
#define BT_USART_ID     USART_ID_2
#define BT_USART_BAUD   115200
#define BT_RESET_PORT   PORT_CHANNEL_G
#define BT_RESET_BIT    PORTS_BIT_POS_15

#endif /* _SYSTEM_CONFIG_H */