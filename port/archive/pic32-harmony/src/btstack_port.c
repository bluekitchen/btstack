//
// BTstack Port for the Microchip PIC32 Harmony Platfrom
//

#include "btstack_port.h"

#include "system_config.h"

#include "btstack_chipset_csr.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_uart_slip_wrapper.h"
#include "hci.h"
#include "hci_dump.h"
#include "hci_transport.h"

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
#include "hal_tick.h"

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
    uint32_t wake = btstack_run_loop_embedded_get_ticks() + delay / hal_tick_get_tick_period_in_ms();
    while (wake > btstack_run_loop_embedded_get_ticks()){
        SYS_Tasks();
    };
}

/// HAL CPU ///
#include "hal_cpu.h"

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
#include "hal_uart_dma.h"

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
    printf("Bluetooth power cycle: Reset ON\n");
    SYS_PORTS_PinClear(PORTS_ID_0, BT_RESET_PORT, BT_RESET_BIT);
    msleep(250);
    printf("Bluetooth power cycle: Reset OFF\n");
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

    // BCSP on CSR requires even parity
    // PLIB_USART_LineControlModeSelect(BT_USART_ID, USART_8E1);

    PLIB_USART_TransmitterEnable(BT_USART_ID);
    // PLIB_USART_TransmitterInterruptModeSelect(bluetooth_uart_id, USART_TRANSMIT_FIFO_IDLE);

    // allow overrun mode: not needed for H4. CSR with BCSP/H5 does not enable RTS/CTS
    PLIB_USART_RunInOverflowEnable(BT_USART_ID);

    PLIB_USART_ReceiverEnable(BT_USART_ID);
    // PLIB_USART_ReceiverInterruptModeSelect(bluetooth_uart_id, USART_RECEIVE_FIFO_ONE_CHAR);
    
    PLIB_USART_Enable(BT_USART_ID);

    // enable _RESET
    SYS_PORTS_PinDirectionSelect(PORTS_ID_0, SYS_PORTS_DIRECTION_OUTPUT, BT_RESET_PORT, BT_RESET_BIT); 
    
    bluetooth_power_cycle();

    // After reset, CTS is high and we need to wait until CTS is low again

    // HACK: CTS doesn't seem to work right now
    msleep(250);
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
void hal_uart_dma_set_sleep(uint8_t sleep){
    // not needed for regualr h4 module (but needed for eHCILL or h5)
    UNUSED(sleep);
}

int  hal_uart_dma_set_baud(uint32_t baud){
//    PLIB_USART_Disable(BT_USART_ID);
//    PLIB_USART_BaudRateSet(BT_USART_ID, SYS_CLK_PeripheralFrequencyGet(CLK_BUS_PERIPHERAL_1), baud);
//    PLIB_USART_Enable(BT_USART_ID);
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
static hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    0,  // main baudrate
    1,  // flow control
    NULL,
};

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != BTSTACK_EVENT_STATE) return;
    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
    printf("BTstack up and running.\n");
}

void BTSTACK_Initialize ( void )
{
    printf("\n\nBTstack_Initialize()\n");

    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // hci_dump_open(NULL, HCI_DUMP_STDOUT);

    // setup uart driver
    const btstack_uart_block_t * uart_block_driver = btstack_uart_block_embedded_instance();
    const btstack_uart_t * uart_slip_driver = btstack_uart_slip_wrapper_instance(uart_block_driver);

    const hci_transport_t * transport = hci_transport_h5_instance(uart_slip_driver);
    hci_init(transport, &config);
    hci_set_chipset(btstack_chipset_csr_instance());

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    btstack_main(0, NULL);
}


void BTSTACK_Tasks(void){
    
    while (bytes_to_read && PLIB_USART_ReceiverDataIsAvailable(BT_USART_ID)) {
        *rx_buffer_ptr++ = PLIB_USART_ReceiverByteReceive(BT_USART_ID);
        bytes_to_read--;
        if (bytes_to_read == 0){
            (*rx_done_handler)();
        }
    }
    
    if(PLIB_USART_ReceiverOverrunHasOccurred(BT_USART_ID))
    {
        // printf("RX Overrun!\n");
        PLIB_USART_ReceiverOverrunErrorClear(BT_USART_ID);
    }
    
    while (bytes_to_write && !PLIB_USART_TransmitterBufferIsFull(BT_USART_ID)){
        PLIB_USART_TransmitterByteSend(BT_USART_ID, *tx_buffer_ptr++);
        bytes_to_write--;
        if (bytes_to_write == 0){
            (*tx_done_handler)();
        }
    }

    // BTstack Run Loop
    btstack_run_loop_embedded_execute_once();
}

