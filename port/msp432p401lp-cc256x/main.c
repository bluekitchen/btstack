/*
 ******************************************************************************/
/* DriverLib Includes */
#include "driverlib.h"

/* Standard Includes */
#include <stdint.h>

#include <stdbool.h>
#include <string.h>

#include <stdio.h>
#include "btstack_config.h"
#include "btstack_chipset_cc256x.h"
#include "btstack_defines.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "hci_dump.h"

static hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    1000000,      // main baudrate
    1,      // flow control
    NULL,
};

#ifndef ENABLE_SEGGER_RTT

/**
 * Use USART_CONSOLE as a console.
 * This is a syscall for newlib
 * @param file
 * @param ptr
 * @param len
 * @return
 */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
int _write(int file, char *ptr, int len);
int _write(int file, char *ptr, int len){
#if 1
    uint8_t cr = '\r';
    int i;

    if (file == STDOUT_FILENO || file == STDERR_FILENO) {
        for (i = 0; i < len; i++) {
            if (ptr[i] == '\n') {
                HAL_UART_Transmit( &huart2, &cr, 1, HAL_MAX_DELAY );
            }
            HAL_UART_Transmit( &huart2, (uint8_t *) &ptr[i], 1, HAL_MAX_DELAY );
        }
        return i;
    }
    errno = EIO;
    return -1;
#else
    return len;
#endif
}

#endif

#if 1
int _read(int file, char * ptr, int len){
    UNUSED(file);
    UNUSED(ptr);
    UNUSED(len);
    return -1;
}


int _close(int file){
    UNUSED(file);
    return -1;
}

int _isatty(int file){
    UNUSED(file);
    return -1;
}

int _lseek(int file){
    UNUSED(file);
    return -1;
}

int _fstat(int file){
    UNUSED(file);
    return -1;
}

void * _sbrk(int incr){
    UNUSED(incr);
    return NULL;
}
#endif

#if 1
// with current Makefile, compiler, and linker flags, printf will call malloc -> sbrk
// for now, use SEGGER's printf

#include "SEGGER_RTT.h"

int SEGGER_RTT_vprintf(unsigned BufferIndex, const char * sFormat, va_list * pParamList);

int printf(const char * format, ...){
    va_list argptr;
    va_start(argptr, format);
    SEGGER_RTT_vprintf(0, format, &argptr);
     va_end(argptr);
}

int vprintf(const char * format,  va_list argptr){
    SEGGER_RTT_vprintf(0, format, &argptr);
}
#endif

// hal_cpu.h implementation
#include "hal_cpu.h"

void hal_cpu_disable_irqs(void){
    __disable_irq();
}

void hal_cpu_enable_irqs(void){
    __enable_irq();
}

void hal_cpu_enable_irqs_and_sleep(void){
    __enable_irq();
    __asm__("wfe"); // go to sleep if event flag isn't set. if set, just clear it. IRQs set event flag
}

// HAL TIME Implementation

static volatile uint32_t systick;

void SysTick_Handler(void)
{
    systick++;
}


static void init_systick(void){
    // Configuring SysTick to trigger every ms (48 Mhz / 48000 = 1 ms)
    MAP_SysTick_enableModule();
    MAP_SysTick_setPeriod(24000);
    // MAP_Interrupt_enableSleepOnIsrExit();
    MAP_SysTick_enableInterrupt();
}

static void delay_ms(uint32_t ms){
    uint32_t delay_until = systick + ms;
    while (systick < delay_until);
}

uint32_t hal_time_ms(void){
    return systick;
}

// HAL UART DMA

// rx state
static uint16_t  bytes_to_read = 0;
static uint8_t * rx_buffer_ptr = 0;

// tx state
static uint16_t  bytes_to_write = 0;
static uint8_t * tx_buffer_ptr = 0;

// handlers
static void (*rx_done_handler)(void);
static void (*tx_done_handler)(void);

/*
Pin 3: BTTX=UCA2RXD-P3.2
Pin 4: BTRX=UCA2TXD-P3.3
Pin 19: BTSHUTDN=GPIO-P2.5
Pin 36: BTRTS=GPIO-P6.6
Pin 37: BTCTS=GPIO-P5.6
*/

/* UART Configuration Parameter. These are the configuration parameters to
 * make the eUSCI A UART module to operate with a 115200 baud rate. These
 * values were calculated using the online calculator that TI provides
 * at:
 * http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSP430BaudRateConverter/index.html
 */
static eUSCI_UART_Config uartConfig =
{
        EUSCI_A_UART_CLOCKSOURCE_SMCLK,          // SMCLK Clock Source
        13,                                      // BRDIV = 13
        0,                                       // UCxBRF = 0
        37,                                      // UCxBRS = 37
        EUSCI_A_UART_NO_PARITY,                  // No Parity
        EUSCI_A_UART_LSB_FIRST,                  // MSB First
        EUSCI_A_UART_ONE_STOP_BIT,               // One stop bit
        EUSCI_A_UART_MODE,                       // UART mode
        EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION  // Oversampling
};

// table 
static struct baudrate_config {
    uint32_t baudrate;
    uint8_t  clock_prescalar;
    uint8_t  first_mod_reg;
    uint8_t  second_mod_reg;
    uint8_t  oversampling;
} baudrate_configs[] = {
    {  115200, 13,  0, 37, 1},
    {  230400,  6,  8, 32, 1},
    {  460800,  3,  4,  2, 1},
    {  921600,  1, 10,  0, 1},
    { 1000000,  1,  8,  0, 1},
    { 2000000, 12,  0,  0, 0},
    { 3000000,  8,  0,  0, 0},
};

static inline void hal_uart_dma_enable_rx(void){
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN6);
}

static inline void hal_uart_dma_disable_rx(void){
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN6);
}

void EUSCIA2_IRQHandler(void){ 

    uint32_t status = MAP_UART_getEnabledInterruptStatus(EUSCI_A2_BASE);

    MAP_UART_clearInterruptFlag(EUSCI_A2_BASE, status);

    // RX Complete
    if (status & EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG){

        if (bytes_to_read) {
            *rx_buffer_ptr = UART_receiveData(EUSCI_A2_BASE);
            ++rx_buffer_ptr;
            --bytes_to_read;
        }
        if (bytes_to_read == 0){
            hal_uart_dma_disable_rx();

            // disable RXIE
            UART_disableInterrupt(EUSCI_A2_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);

            // done
            (*rx_done_handler)();
        }
    }

    // TX IDLE
    if (status & EUSCI_A_UART_TRANSMIT_INTERRUPT_FLAG){
        if (bytes_to_write){
            // start TX
            MAP_UART_transmitData(EUSCI_A2_BASE, *tx_buffer_ptr);
            ++tx_buffer_ptr;
            --bytes_to_write;
        }
        if (bytes_to_write == 0){
            // disable TXIE interrupt
            UART_disableInterrupt(EUSCI_A2_BASE, EUSCI_A_UART_TRANSMIT_INTERRUPT);

            // done
            (*tx_done_handler)();
        }
    }
}

void hal_uart_dma_init(void){
    // nShutdown
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN5);
    // BT-CTS
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN6);
    // BT-RTS
    MAP_GPIO_setAsInputPinWithPullDownResistor(GPIO_PORT_P6, GPIO_PIN6);
    // UART pins
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P3,
             GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);

    // configure UART
    /* Configuring UART Module */
    MAP_UART_initModule(EUSCI_A2_BASE, &uartConfig);

    /* Enable UART module */
    MAP_UART_enableModule(EUSCI_A2_BASE);

    /* Enable UART interrupts in general */
    Interrupt_enableInterrupt(INT_EUSCIA2);

    // power cycle
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN5);
    delay_ms(10);
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN5);
    delay_ms(200);
}

int  hal_uart_dma_set_baud(uint32_t baud){
    int index = -1;
    int i;
    for (i=0;i<sizeof(baudrate_configs)/sizeof(struct baudrate_config);i++){
        if (baudrate_configs[i].baudrate == baud){
            index = i;
        }
    }
    if (index < 0) return -1;

    MAP_UART_disableModule(EUSCI_A2_BASE);

    uartConfig.clockPrescalar = baudrate_configs[index].clock_prescalar;
    uartConfig.firstModReg    = baudrate_configs[index].first_mod_reg;
    uartConfig.secondModReg   = baudrate_configs[index].second_mod_reg;
    uartConfig.overSampling   = baudrate_configs[index].oversampling;
    MAP_UART_initModule(EUSCI_A2_BASE, &uartConfig);

    MAP_UART_enableModule(EUSCI_A2_BASE);
    UNUSED(baud);
    return 0;
}

void hal_uart_dma_set_sleep(uint8_t sleep){
    UNUSED(sleep);
}

void hal_uart_dma_set_csr_irq_handler( void (*the_irq_handler)(void)){
    UNUSED(the_irq_handler);
#ifdef HAVE_CTS_IRQ
#endif
}

void hal_uart_dma_set_block_received( void (*the_block_handler)(void)){
    rx_done_handler = the_block_handler;
}

void hal_uart_dma_set_block_sent( void (*the_block_handler)(void)){
    tx_done_handler = the_block_handler;
}

void hal_uart_dma_send_block(const uint8_t * data, uint16_t len){
    tx_buffer_ptr = (uint8_t *) data;
    bytes_to_write = len;

    // start sending
    if (!bytes_to_write) return;

    // enable TX interrupt -> starts transmission
    UART_enableInterrupt(EUSCI_A2_BASE, EUSCI_A_UART_TRANSMIT_INTERRUPT);
}

// int used to indicate a request for more new data
void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len){
    rx_buffer_ptr = buffer;
    bytes_to_read = len;
    
    UART_enableInterrupt(EUSCI_A2_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);

    hal_uart_dma_enable_rx();   // RTS low
}

// End of HAL UART DMA

#if 0
static uint8_t hci_reset[] = { 1, 3, 0x0c, 0};
static uint8_t rx_buffer[10];
static volatile int w4_tc;
static volatile int w4_rc;
static void tc_callback(void){
    w4_tc = 0;
}
static void rc_callback(void){
    w4_rc = 0;
}
#endif

#include "SEGGER_RTT.h"

int main(void)
{
    volatile uint32_t ii;

    /* Halting the Watchdog */
    MAP_WDT_A_holdTimer();

    init_systick();

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    hci_dump_open( NULL, HCI_DUMP_STDOUT );

    // init HCI
    hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), (void*) &config);
    hci_set_chipset(btstack_chipset_cc256x_instance());

#if 0
    // setup TLV Flash Sector implementation
    const hal_flash_bank_t * hal_flash_bank_impl = hal_flash_bank_stm32_init_instance(
            &hal_flash_bank_context,
            HAL_FLASH_BANK_SIZE,
            HAL_FLASH_BANK_0_SECTOR,
            HAL_FLASH_BANK_1_SECTOR,
            HAL_FLASH_BANK_0_ADDR,
            HAL_FLASH_BANK_1_ADDR);
    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(
            &btstack_tlv_flash_bank_context,
            hal_flash_bank_impl,
            &hal_flash_bank_context);

    // setup global tlv
    btstack_tlv_set_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);

    // setup Link Key DB using TLV
    const btstack_link_key_db_t * btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
    hci_set_link_key_db(btstack_link_key_db);

    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, &btstack_tlv_flash_bank_context);

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // hand over to btstack embedded code
    btstack_main(0, NULL);
#endif

    hci_power_control(HCI_POWER_ON);

    // go
    btstack_run_loop_execute();


#if 0
    tx_done_handler = &tc_callback;
    rx_done_handler = &rc_callback;
    while (1){
        // depends on systick
        hal_uart_dma_init();
        w4_tc = 1;
        w4_rc = 1;
        memset(rx_buffer, 0, sizeof(rx_buffer));
        hal_uart_dma_receive_block(&rx_buffer[0], 7);
        hal_uart_dma_send_block(hci_reset, sizeof(hci_reset));
        while (w4_tc); 
        while (w4_rc);
        delay_ms(10);
    }
#endif

    /* Configuring P1.0 as output */
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);

    while (1)
    {
        /* Delay Loop */
        for(ii=0;ii<5000;ii++)
        {
        }
        // SEGGER_RTT_printf(0, "Hi! tick %u\n", (int) systick / 1000);
        MAP_GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);
    }
}


