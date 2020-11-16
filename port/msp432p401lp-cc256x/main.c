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
#include "btstack_tlv.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "hci_dump.h"
#include "btstack_tlv_flash_bank.h"
#include "hal_flash_bank_msp432.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "ble/le_device_db_tlv.h"

static void delay_ms(uint32_t ms);

static hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    460800,      // main baudrate
    1,      // flow control
    NULL,
};

static hal_flash_bank_msp432_t   hal_flash_bank_context;
static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;

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

extern int _end;

void * _sbrk(int incr){
    static unsigned char *heap = NULL;
    unsigned char *prev_heap;

    if (heap == NULL) {
        heap = (unsigned char *)&_end;
    }
    prev_heap = heap;

    heap += incr;

    return prev_heap;
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

// HAL LED
#include "hal_led.h"
void hal_led_toggle(void){
    static bool on = false;
    if (on){
        on = false;
        GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
   } else {
        on = true;
        GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0);
    }
}

// HAL UART DMA

// DMA Control Table
// if not all channels are used, the alignment can be finer
// GCC
__attribute__ ((aligned (1024)))
static DMA_ControlTable MSP_EXP432P401RLP_DMAControlTable[32];

// RX Ping Pong Buffer - similar to circular buffer on other MCUs
#define HAL_DMA_RX_BUFFER_SIZE 64
static uint8_t hal_dma_rx_ping_pong_buffer[2 * HAL_DMA_RX_BUFFER_SIZE];

// active buffer and position to read from
static uint8_t  hal_dma_rx_active_buffer = 0;
static uint16_t hal_dma_rx_offset;

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

#if 0
// Unclear
#define BLUETOOTH_TX_PORT        GPIO_PORT_P3
#define BLUETOOTH_TX_PIN         GPIO_PIN2
#define BLUETOOTH_RX_PORT        GPIO_PORT_P3
#define BLUETOOTH_RX_PIN         GPIO_PIN3
#define BLUETOOTH_RTS_PORT       GPIO_PORT_P6
#define BLUETOOTH_RTS_PIN        GPIO_PIN6
#define BLUETOOTH_CTS_PORT       GPIO_PORT_P5
#define BLUETOOTH_CTS_PIN        GPIO_PIN6
#define BLUETOOTH_nSHUTDOWN_PORT GPIO_PORT_P2
#define BLUETOOTH_nSHUTDOWN_PIN  GPIO_PIN5
#else
// EM Wireless BoosterPack with CC256x module
#define BLUETOOTH_TX_PORT        GPIO_PORT_P3
#define BLUETOOTH_TX_PIN         GPIO_PIN2
#define BLUETOOTH_RX_PORT        GPIO_PORT_P3
#define BLUETOOTH_RX_PIN         GPIO_PIN3
#define BLUETOOTH_RTS_PORT       GPIO_PORT_P3
#define BLUETOOTH_RTS_PIN        GPIO_PIN6
#define BLUETOOTH_CTS_PORT       GPIO_PORT_P5
#define BLUETOOTH_CTS_PIN        GPIO_PIN2
#define BLUETOOTH_nSHUTDOWN_PORT GPIO_PORT_P6
#define BLUETOOTH_nSHUTDOWN_PIN  GPIO_PIN4
#endif

/* UART Configuration Parameter. These are the configuration parameters to
 * make the eUSCI A UART module to operate with a 115200 baud rate. These
 * values were calculated using the online calculator that TI provides
 * at:
 * http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSP430BaudRateConverter/index.html
 */
static eUSCI_UART_ConfigV1 uartConfig =
{
        EUSCI_A_UART_CLOCKSOURCE_SMCLK,          // SMCLK Clock Source
        0 ,                                      // BRDIV  (template)
        0,                                       // UCxBRF (template)
        0 ,                                      // UCxBRS (template)
        EUSCI_A_UART_NO_PARITY,                  // No Parity
        EUSCI_A_UART_LSB_FIRST,                  // MSB First
        EUSCI_A_UART_ONE_STOP_BIT,               // One stop bit
        EUSCI_A_UART_MODE,                       // UART mode: normal
        0,                                       // Oversampling (template)
        EUSCI_A_UART_8_BIT_LEN,                  // 8 bit
};

// table
static struct baudrate_config {
    uint32_t baudrate;
    uint8_t  clock_prescalar;
    uint8_t  first_mod_reg;
    uint8_t  second_mod_reg;
    uint8_t  oversampling;
} baudrate_configs[] = {
    // Config for 48 Mhz
    {   57600, 52,  1,   37, 1},
    {  115200, 26,  1,  111, 1},
    {  230400, 13,  0,   37, 1},
    {  460800,  6,  8,   32, 1},
    {  921600,  3,  4,    2, 1},
    { 1000000,  3,  0,    0, 1},
    { 2000000,  1,  8,    0, 1},
    { 3000000,  1,  0,    0, 1},
    { 4000000, 12,  0,    0, 0},
};

// return true if ok
static bool hal_uart_dma_config(uint32_t baud){
    int index = -1;
    int i;
    for (i=0;i<sizeof(baudrate_configs)/sizeof(struct baudrate_config);i++){
        if (baudrate_configs[i].baudrate == baud){
            index = i;
            break;
        }
    }
    if (index < 0) return false;

    uartConfig.clockPrescalar = baudrate_configs[index].clock_prescalar;
    uartConfig.firstModReg    = baudrate_configs[index].first_mod_reg;
    uartConfig.secondModReg   = baudrate_configs[index].second_mod_reg;
    uartConfig.overSampling   = baudrate_configs[index].oversampling ? EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION : EUSCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION;
    printf("Pre %u, first %u, second %u\n", uartConfig.clockPrescalar, uartConfig.firstModReg, uartConfig.secondModReg );
    return true;
}

static void hal_dma_rx_start_transfer(uint8_t buffer){
    uint32_t channel = DMA_CH5_EUSCIA2RX | (buffer == 0 ? UDMA_PRI_SELECT : UDMA_ALT_SELECT);
    MAP_DMA_setChannelTransfer(channel, UDMA_MODE_PINGPONG, (void *) UART_getReceiveBufferAddressForDMA(EUSCI_A2_BASE),
       (uint8_t *) &hal_dma_rx_ping_pong_buffer[buffer * HAL_DMA_RX_BUFFER_SIZE], HAL_DMA_RX_BUFFER_SIZE);
}

static uint16_t hal_dma_rx_bytes_avail(uint8_t buffer, uint16_t offset){
    uint32_t channel = DMA_CH5_EUSCIA2RX | (buffer == 0 ? UDMA_PRI_SELECT : UDMA_ALT_SELECT);
    return HAL_DMA_RX_BUFFER_SIZE - MAP_DMA_getChannelSize(channel) - offset;
}

void DMA_INT1_IRQHandler(void)
{
    MAP_DMA_clearInterruptFlag(DMA_CH4_EUSCIA2TX & 0x0F);
    MAP_DMA_disableChannel(DMA_CH4_EUSCIA2TX & 0x0F);
    (*tx_done_handler)();
}

void DMA_INT2_IRQHandler(void)
{
    MAP_DMA_clearInterruptFlag(DMA_CH5_EUSCIA2RX & 0x0F);
    // raise our RTS
    GPIO_setOutputHighOnPin(BLUETOOTH_CTS_PORT, BLUETOOTH_CTS_PIN);
}

static void hal_uart_dma_harvest(void){
    if (bytes_to_read == 0) return;
    // get bytes from current buffer
    uint16_t bytes_avail = hal_dma_rx_bytes_avail(hal_dma_rx_active_buffer, hal_dma_rx_offset);
    uint16_t bytes_to_copy = btstack_min(bytes_avail, bytes_to_read);
    memcpy(rx_buffer_ptr, &hal_dma_rx_ping_pong_buffer[hal_dma_rx_active_buffer * HAL_DMA_RX_BUFFER_SIZE + hal_dma_rx_offset], bytes_to_copy);
    rx_buffer_ptr += bytes_to_copy;
    bytes_to_read -= bytes_to_copy;
    hal_dma_rx_offset += bytes_to_copy;
    // if current buffer fully processed, restart DMA transfer and switch to next buffer
    if (hal_dma_rx_offset == HAL_DMA_RX_BUFFER_SIZE){
        hal_dma_rx_offset = 0;
        hal_dma_rx_start_transfer(hal_dma_rx_active_buffer);
        hal_dma_rx_active_buffer = 1 - hal_dma_rx_active_buffer;
        GPIO_setOutputLowOnPin(BLUETOOTH_CTS_PORT, BLUETOOTH_CTS_PIN);
    }
    if (bytes_to_read == 0){
        (*rx_done_handler)();
    }
}

void hal_uart_dma_init(void){
    // nShutdown
    MAP_GPIO_setAsOutputPin(BLUETOOTH_nSHUTDOWN_PORT, BLUETOOTH_nSHUTDOWN_PIN);
    // BT-CTS
    MAP_GPIO_setAsOutputPin(BLUETOOTH_CTS_PORT, BLUETOOTH_CTS_PIN);
    GPIO_setOutputHighOnPin(BLUETOOTH_CTS_PORT, BLUETOOTH_CTS_PIN);
    // BT-RTS
    MAP_GPIO_setAsInputPinWithPullDownResistor(BLUETOOTH_RTS_PORT, BLUETOOTH_RTS_PIN);
    // UART pins
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(BLUETOOTH_TX_PORT, BLUETOOTH_TX_PIN, GPIO_PRIMARY_MODULE_FUNCTION);
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(BLUETOOTH_RX_PORT, BLUETOOTH_RX_PIN, GPIO_PRIMARY_MODULE_FUNCTION);

    // UART

    /* Configuring and enable UART Module */
    MAP_UART_initModule(EUSCI_A2_BASE, &uartConfig);
    MAP_UART_enableModule(EUSCI_A2_BASE);

    // DMA

    /* Configuring DMA module */
    MAP_DMA_enableModule();
    MAP_DMA_setControlBase(MSP_EXP432P401RLP_DMAControlTable);

    /* Assign DMA channel 4 to EUSCI_A2_TX, channel 5 to EUSCI_A2_RX */
    MAP_DMA_assignChannel(DMA_CH4_EUSCIA2TX);
    MAP_DMA_assignChannel(DMA_CH5_EUSCIA2RX);

    /* Setup the RX and TX transfer characteristics */
    MAP_DMA_setChannelControl(DMA_CH4_EUSCIA2TX | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_1);
    MAP_DMA_setChannelControl(DMA_CH5_EUSCIA2RX | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_NONE | UDMA_DST_INC_8 | UDMA_ARB_1);

    /* Enable DMA interrupt for both channels */
    MAP_DMA_assignInterrupt(INT_DMA_INT1, DMA_CH4_EUSCIA2TX & 0x0f);
    MAP_DMA_assignInterrupt(INT_DMA_INT2, DMA_CH5_EUSCIA2RX & 0x0f);

    /* Clear interrupt flags */
    MAP_DMA_clearInterruptFlag(DMA_CH4_EUSCIA2TX & 0x0F);
    MAP_DMA_clearInterruptFlag(DMA_CH5_EUSCIA2RX & 0x0F);

    /* Enable Interrupts */
    MAP_Interrupt_enableInterrupt(INT_DMA_INT1);
    MAP_Interrupt_enableInterrupt(INT_DMA_INT2);
    MAP_DMA_enableInterrupt(INT_DMA_INT1);
    MAP_DMA_enableInterrupt(INT_DMA_INT2);

    // power cycle
    MAP_GPIO_setOutputLowOnPin(BLUETOOTH_nSHUTDOWN_PORT, BLUETOOTH_nSHUTDOWN_PIN);
    delay_ms(10);
    MAP_GPIO_setOutputHighOnPin(BLUETOOTH_nSHUTDOWN_PORT, BLUETOOTH_nSHUTDOWN_PIN);
    delay_ms(200);

    // setup ping pong rx
    hal_dma_rx_start_transfer(0);
    hal_dma_rx_start_transfer(1);

    hal_dma_rx_active_buffer = 0;
    hal_dma_rx_offset = 0;

    MAP_DMA_enableChannel(DMA_CH5_EUSCIA2RX & 0x0F);

    // ready
    GPIO_setOutputLowOnPin(BLUETOOTH_CTS_PORT, BLUETOOTH_CTS_PIN);
}

int  hal_uart_dma_set_baud(uint32_t baud){
    hal_uart_dma_config(baud);
    MAP_UART_disableModule(EUSCI_A2_BASE);
    /* BaudRate Control Register */
    uint32_t moduleInstance = EUSCI_A2_BASE;
    const eUSCI_UART_ConfigV1 *config = &uartConfig;
    EUSCI_A_CMSIS(moduleInstance)->BRW = config->clockPrescalar;
    EUSCI_A_CMSIS(moduleInstance)->MCTLW = ((config->secondModReg << 8) + (config->firstModReg << 4) + config->overSampling);
    MAP_UART_enableModule(EUSCI_A2_BASE);
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
    MAP_DMA_setChannelTransfer(DMA_CH4_EUSCIA2TX | UDMA_PRI_SELECT, UDMA_MODE_BASIC, (uint8_t *) data,
                               (void *) MAP_UART_getTransmitBufferAddressForDMA(EUSCI_A2_BASE),
                               len);
    MAP_DMA_enableChannel(DMA_CH4_EUSCIA2TX & 0x0F);
}

// int used to indicate a request for more new data
void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len){
    printf("rx: %u\n", len);
    rx_buffer_ptr = buffer;
    bytes_to_read = len;
    hal_cpu_disable_irqs();
    hal_uart_dma_harvest();
    hal_cpu_enable_irqs();
}

// End of HAL UART DMA

// HAL TIME Implementation

static volatile uint32_t systick;

void SysTick_Handler(void){
    systick++;
    // process received data
    hal_uart_dma_harvest();
}


static void init_systick(void){
    // Configuring SysTick to trigger every ms (48 Mhz / 48000 = 1 ms)
    MAP_SysTick_enableModule();
    MAP_SysTick_setPeriod(48000);
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

#include "SEGGER_RTT.h"

// HAL FLASH MSP432 Configuration - use two last 4kB sectors
#define HAL_FLASH_BANK_SIZE      4096
#define HAL_FLASH_BANK_0_SECTOR  FLASH_SECTOR30
#define HAL_FLASH_BANK_1_SECTOR  FLASH_SECTOR31
#define HAL_FLASH_BANK_0_ADDR    0x3E000
#define HAL_FLASH_BANK_1_ADDR    0x3F000

int btstack_main(const int argc, const char * argvp[]);

int main(void)
{
    /* Halting the Watchdog */
    MAP_WDT_A_holdTimer();

    init_systick();

    // init led
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);
    hal_led_toggle();

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    hci_dump_open( NULL, HCI_DUMP_STDOUT );

    // init HCI
    hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), (void*) &config);
    hci_set_chipset(btstack_chipset_cc256x_instance());

    // setup TLV Flash Sector implementation
    const hal_flash_bank_t * hal_flash_bank_impl = hal_flash_bank_msp432_init_instance(
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

#if 0
    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

#endif

    // hand over to btstack embedded code
    btstack_main(0, NULL);

    hci_power_control(HCI_POWER_ON);

    // go
    btstack_run_loop_execute();
}


