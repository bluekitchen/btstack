//
// BTstack Port for the Microchip PIC32 Harmony Platfrom
//

#include "btstack_port.h"
#include "system_config.h"
#include "bt_control_csr.h"
#include <btstack/run_loop.h>
#include "hci_dump.h"
#include "hci.h"
#include "hci_transport.h"
#include "debug.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "driver/tmr/drv_tmr.h"
#include "peripheral/usart/plib_usart.h"
#include "system/ports/sys_ports.h"


// 
int btstack_main(int argc, const char * argv[]);


/// HAL Tick ///
#include <btstack/hal_tick.h>

#define APP_TMR_ALARM_PERIOD                48825
#define APP_LED_PORT                        PORT_CHANNEL_A
#define APP_LED_PIN                         PORTS_BIT_POS_4

static void dummy_handler(void);
static void (*tick_handler)(void);
static int hal_uart_needed_during_sleep = 1;

static void dummy_handler(void){};
static DRV_HANDLE handleTmr;
static bool ledIsOn;

static void sys_tick_handler (  uintptr_t context, uint32_t alarmCount ){
    if (!ledIsOn) {
        ledIsOn = true;
        SYS_PORTS_PinSet(PORTS_ID_0, APP_LED_PORT, APP_LED_PIN);
    }
    else
    {
        ledIsOn = false;
        SYS_PORTS_PinClear(PORTS_ID_0, APP_LED_PORT, APP_LED_PIN);
    }

    (*tick_handler)();
}

void hal_tick_init(void){
    ledIsOn = false;
    handleTmr = DRV_TMR_Open(APP_TMR_DRV_INDEX, DRV_IO_INTENT_EXCLUSIVE);
    if( DRV_HANDLE_INVALID == handleTmr ){
        log_error("Timer init failed");
        return;
    }
    DRV_TMR_Alarm16BitRegister(handleTmr, APP_TMR_ALARM_PERIOD, true, (uintptr_t)NULL, sys_tick_handler);
    DRV_TMR_Start(handleTmr);
    SYS_PORTS_PinDirectionSelect(PORTS_ID_0, SYS_PORTS_DIRECTION_OUTPUT, APP_LED_PORT, APP_LED_PIN);
}

int  hal_tick_get_tick_period_in_ms(void){
    return 250;
}

void hal_tick_set_handler(void (*handler)(void)){
    if (handler == NULL){
        tick_handler = &dummy_handler;
        return;
    }
    tick_handler = handler;
}

static void msleep(uint32_t delay) {
    uint32_t wake = embedded_get_ticks() + delay / hal_tick_get_tick_period_in_ms();
    while (wake > embedded_get_ticks()){
        SYS_Tasks();
    };
}

/// HAL CPU ///
#include <btstack/hal_cpu.h>

void hal_cpu_disable_irqs(void){
    // TODO implement
}

void hal_cpu_enable_irqs(void){
    // TODO implement
}

void hal_cpu_enable_irqs_and_sleep(void){
    // TODO implement
}


/// HAL UART DMA ///
#include <btstack/hal_uart_dma.h>

// handlers
static void (*rx_done_handler)(void) = dummy_handler;
static void (*tx_done_handler)(void) = dummy_handler;
static void (*cts_irq_handler)(void) = dummy_handler;

// rx state
static uint16_t  bytes_to_read = 0;
static uint8_t * rx_buffer_ptr = 0;

// tx state
static uint16_t  bytes_to_write = 0;
static uint8_t * tx_buffer_ptr = 0;


// reset Bluetooth using n_shutdown
static void bluetooth_power_cycle(void){
    printf("Bluetooth power cycle Reset ON\n");
    SYS_PORTS_PinClear(PORTS_ID_0, BT_RESET_PORT, BT_RESET_BIT);
    msleep(250);
    printf("Bluetooth power cycle Reset OFF\n");
    SYS_PORTS_PinSet(PORTS_ID_0, BT_RESET_PORT, BT_RESET_BIT);
}

void hal_uart_dma_init(void){

    bytes_to_write = 0;
    bytes_to_read = 0;

    /* PPS Input Remapping */
    PLIB_PORTS_RemapInput(PORTS_ID_0, INPUT_FUNC_U2RX, INPUT_PIN_RPF4 );
    PLIB_PORTS_RemapInput(PORTS_ID_0, INPUT_FUNC_U2CTS, INPUT_PIN_RPB2 );

    /* PPS Output Remapping */
    PLIB_PORTS_RemapOutput(PORTS_ID_0, OUTPUT_FUNC_U2RTS, OUTPUT_PIN_RPG9 );
    PLIB_PORTS_RemapOutput(PORTS_ID_0, OUTPUT_FUNC_U2TX, OUTPUT_PIN_RPF5 );

    /* Initialize USART */
    PLIB_USART_BaudRateSet(BT_USART_ID, SYS_CLK_PeripheralFrequencyGet(CLK_BUS_PERIPHERAL_1), BT_USART_BAUD);
    PLIB_USART_HandshakeModeSelect(BT_USART_ID, USART_HANDSHAKE_MODE_FLOW_CONTROL);
    PLIB_USART_OperationModeSelect(BT_USART_ID, USART_ENABLE_TX_RX_CTS_RTS_USED);
    PLIB_USART_LineControlModeSelect(BT_USART_ID, USART_8N1);
    PLIB_USART_TransmitterEnable(BT_USART_ID);
//    PLIB_USART_TransmitterInterruptModeSelect(bluetooth_uart_id, USART_TRANSMIT_FIFO_IDLE);
    PLIB_USART_ReceiverEnable(BT_USART_ID);
//    PLIB_USART_ReceiverInterruptModeSelect(bluetooth_uart_id, USART_RECEIVE_FIFO_ONE_CHAR);

    PLIB_USART_Enable(BT_USART_ID);

    // enable _RESET
    SYS_PORTS_PinDirectionSelect(PORTS_ID_0, SYS_PORTS_DIRECTION_OUTPUT, BT_RESET_PORT, BT_RESET_BIT);

    bluetooth_power_cycle();

    // After reset, CTS is high and we need to wait until CTS is low again

    // HACK: CTS doesn't seem to work right now
    msleep(250);

    // HACK: CSR seems to do an auto-baud on the uart, which makes the first HCI RESET fail
    // 2 options: a) check for CTS going high within 10 ms, b) just send HCI RESET twice

//    const uint8_t hci_reset_cmd[] = {0x01, 0x03, 0x0c, 0x00};
//    int pos = 0;
//    while(pos < sizeof(hci_reset_cmd)){
//        if (PLIB_USART_TransmitterIsEmpty(BT_USART_ID)){
//            PLIB_USART_TransmitterByteSend(BT_USART_ID, hci_reset_cmd[pos]);
//            pos++;
//        }
//    }
//    msleep(250);

}

void hal_uart_dma_set_block_received( void (*the_block_handler)(void)){
    rx_done_handler = the_block_handler;
}

void hal_uart_dma_set_block_sent( void (*the_block_handler)(void)){
    tx_done_handler = the_block_handler;
}

void hal_uart_dma_set_csr_irq_handler( void (*the_irq_handler)(void)){
    // not needed for regular H4 module (but needed for TI's eHCILL)
}

int  hal_uart_dma_set_baud(uint32_t baud){
    PLIB_USART_Disable(BT_USART_ID);
    PLIB_USART_BaudRateSet(BT_USART_ID, SYS_CLK_PeripheralFrequencyGet(CLK_BUS_PERIPHERAL_1), baud);
    PLIB_USART_Enable(BT_USART_ID);
    return 0;
}

void hal_uart_dma_send_block(const uint8_t *data, uint16_t size){
    tx_buffer_ptr = (uint8_t *) data;
    bytes_to_write = size;}


void hal_uart_dma_receive_block(uint8_t *data, uint16_t size){
    // printf("hal_uart_dma_receive_block req size %u\n", size);
    rx_buffer_ptr = data;
    bytes_to_read = size;
}

///
static const hci_uart_config_t hci_uart_config_csr = {
    NULL,
    115200,
    0,  // 1000000,
    0
};

void BTSTACK_Initialize ( void )
{
    printf("\n\nBTstack_Initialize()\n");

    btstack_memory_init();
    run_loop_init(RUN_LOOP_EMBEDDED);

    hci_dump_open(NULL, HCI_DUMP_STDOUT);

    hci_transport_t * transport = hci_transport_h4_dma_instance();
    bt_control_t    * control   = bt_control_csr_instance();
    hci_init(transport, (void*) &hci_uart_config_csr, control, NULL);

    // hci_power_control(HCI_POWER_ON);
    btstack_main(0, NULL);
}


void BTSTACK_Tasks(void){
    if (bytes_to_read && PLIB_USART_ReceiverDataIsAvailable(BT_USART_ID)) {
        *rx_buffer_ptr++ = PLIB_USART_ReceiverByteReceive(BT_USART_ID);
        bytes_to_read--;
        if (bytes_to_read == 0){
            (*rx_done_handler)();
        }
    }

    if (bytes_to_write && PLIB_USART_TransmitterIsEmpty(BT_USART_ID)){
        PLIB_USART_TransmitterByteSend(BT_USART_ID, *tx_buffer_ptr++);
        bytes_to_write--;
        if (bytes_to_write == 0){
            (*tx_done_handler)();
        }
    }

    // BTstack Run Loop
    embedded_execute_once();
}

