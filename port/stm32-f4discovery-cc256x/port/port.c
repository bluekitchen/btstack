#define BTSTACK_FILE__ "port.c"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

// include STM32 first to avoid warning about redefinition of UNUSED
#include "stm32f4xx_hal.h"
#include "main.h"

#include "port.h"
#include "btstack.h"
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_chipset_cc256x.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"
#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db_static.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hal_flash_bank_stm32.h"

#ifdef ENABLE_SEGGER_RTT
#include "SEGGER_RTT.h"
#include "hci_dump_segger_rtt_stdout.h"
#else
#include "hci_dump_embedded_stdout.h"
#endif

//
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

//
static btstack_packet_callback_registration_t hci_event_callback_registration;

static const hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    4000000,
    1,
    NULL,
    BTSTACK_UART_PARITY_OFF
};

// hal_time_ms.h
#include "hal_time_ms.h"
uint32_t hal_time_ms(void){
    return HAL_GetTick();
}

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

// hal_stdin.h
#include "hal_stdin.h"
static uint8_t stdin_buffer[1];
static void (*stdin_handler)(char c);
void hal_stdin_setup(void (*handler)(char c)){
    stdin_handler = handler;
    // start receiving
    HAL_UART_Receive_IT(&huart2, &stdin_buffer[0], 1);
}

static void stdin_rx_complete(void){
    if (stdin_handler){
        (*stdin_handler)(stdin_buffer[0]);
    }
    HAL_UART_Receive_IT(&huart2, &stdin_buffer[0], 1);
}

// hal_uart_dma.h

// hal_uart_dma.c implementation
#include "hal_uart_dma.h"

static void dummy_handler(void);

// handlers
static void (*rx_done_handler)(void) = &dummy_handler;
static void (*tx_done_handler)(void) = &dummy_handler;
static void (*cts_irq_handler)(void) = &dummy_handler;

static int hal_uart_needed_during_sleep;

static void dummy_handler(void){}

void hal_uart_dma_set_sleep(uint8_t sleep){

    // RTS is on PD12 - manually set it during sleep
    GPIO_InitTypeDef RTS_InitStruct;
    RTS_InitStruct.Pin = GPIO_PIN_12;
    RTS_InitStruct.Pull = GPIO_NOPULL;
    RTS_InitStruct.Alternate = GPIO_AF7_USART3;
    if (sleep){
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
        RTS_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        RTS_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    } else {
        RTS_InitStruct.Mode = GPIO_MODE_AF_PP;
        RTS_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    }

    HAL_GPIO_Init(GPIOD, &RTS_InitStruct);

//  if (sleep){
//      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
//  }
    hal_uart_needed_during_sleep = !sleep;
}

// reset Bluetooth using n_shutdown
static void bluetooth_power_cycle(void){
    printf("Bluetooth power cycle\n");
    HAL_GPIO_WritePin( CC_nSHUTD_GPIO_Port, CC_nSHUTD_Pin, GPIO_PIN_RESET );
    HAL_Delay( 250 );
    HAL_GPIO_WritePin( CC_nSHUTD_GPIO_Port, CC_nSHUTD_Pin, GPIO_PIN_SET );
    HAL_Delay( 500 );
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
    if (huart == &huart3){
        (*tx_done_handler)();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
    if (huart == &huart3){
        (*rx_done_handler)();
    }
    if (huart == &huart2){
        stdin_rx_complete();
    }
}

void hal_uart_dma_init(void){
    bluetooth_power_cycle();
}
void hal_uart_dma_set_block_received( void (*the_block_handler)(void)){
    rx_done_handler = the_block_handler;
}

void hal_uart_dma_set_block_sent( void (*the_block_handler)(void)){
    tx_done_handler = the_block_handler;
}

void EXTI15_10_IRQHandler(void){
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_11);
    if (cts_irq_handler){
        (*cts_irq_handler)();
    }
}

void hal_uart_dma_set_csr_irq_handler( void (*the_irq_handler)(void)){

    GPIO_InitTypeDef CTS_InitStruct = {
        .Pin       = GPIO_PIN_11,
        .Mode      = GPIO_MODE_AF_PP,
        .Pull      = GPIO_PULLUP,
        .Speed     = GPIO_SPEED_FREQ_VERY_HIGH,
        .Alternate = GPIO_AF7_USART3,
    };

    if( the_irq_handler )  {
        /* Configure the EXTI11 interrupt (USART3_CTS is on PD11) */
        HAL_NVIC_EnableIRQ( EXTI15_10_IRQn );
        CTS_InitStruct.Mode = GPIO_MODE_IT_RISING;
        CTS_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init( GPIOD, &CTS_InitStruct );
        log_info("enabled CTS irq");
    }
    else  {
        CTS_InitStruct.Mode = GPIO_MODE_AF_PP;
        CTS_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init( GPIOD, &CTS_InitStruct );
        HAL_NVIC_DisableIRQ( EXTI15_10_IRQn );
        log_info("disabled CTS irq");
    }
    cts_irq_handler = the_irq_handler;
}

int  hal_uart_dma_set_baud(uint32_t baud){
    huart3.Init.BaudRate = baud;
    HAL_UART_Init(&huart3);
    return 0;
}

void hal_uart_dma_send_block(const uint8_t *data, uint16_t size){
    HAL_UART_Transmit_DMA( &huart3, (uint8_t *) data, size);
}

void hal_uart_dma_receive_block(uint8_t *data, uint16_t size){
    HAL_UART_Receive_DMA( &huart3, data, size );
}

#ifndef ENABLE_SEGGER_RTT

/**
 * Use USART_CONSOLE as a console.
 * This is a syscall for newlib
 * @param file
 * @param ptr
 * @param len
 * @return
 */

ssize_t _write(int file, const void *buf, size_t len);
ssize_t _write(int file, const void *buf, size_t len){
#if 1
    uint8_t cr = '\r';
    int i;
    uint8_t *ptr = buf;
    if (file == STDOUT_FILENO || file == STDERR_FILENO) {
        for (i = 0; i < len; i++) {
            if (ptr[i] == '\n') {
                HAL_UART_Transmit( &huart2, &cr, 1, HAL_MAX_DELAY );
            }
            HAL_UART_Transmit( &huart2, &ptr[i], 1, HAL_MAX_DELAY );
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

ssize_t _read(int fd, void * buf, size_t count){
    UNUSED(fd);
    UNUSED(buf);
    UNUSED(count);
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

int _kill (pid_t pid, int sig) {
    UNUSED(pid);
    UNUSED(sig);
    return -1;
}

pid_t _getpid (void) {
    return 0;
}

// main.c
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            gap_local_bd_addr(local_addr);
            printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            if (hci_event_command_complete_get_command_opcode(packet) == HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION){
                uint16_t manufacturer   = little_endian_read_16(packet, 10);
                uint16_t lmp_subversion = little_endian_read_16(packet, 12);
                // assert manufacturer is TI
                if (manufacturer != BLUETOOTH_COMPANY_ID_TEXAS_INSTRUMENTS_INC){
                    printf("ERROR: Expected Bluetooth Chipset from TI but got manufacturer 0x%04x\n", manufacturer);
                    break;
                }
                // assert correct init script is used based on expected lmp_subversion
                if (lmp_subversion != btstack_chipset_cc256x_lmp_subversion()){
                    printf("Error: LMP Subversion does not match initscript! ");
                    printf("Your initscripts is for %s chipset\n", btstack_chipset_cc256x_lmp_subversion() < lmp_subversion ? "an older" : "a newer");
                    printf("Please update Makefile to include the appropriate bluetooth_init_cc256???.c file\n");
                    break;
                }
            }
            break;
        default:
            break;
    }
}


static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;
static hal_flash_bank_stm32_t   hal_flash_bank_context;

#define HAL_FLASH_BANK_SIZE (128 * 1024)
#define HAL_FLASH_BANK_0_ADDR 0x080C0000
#define HAL_FLASH_BANK_1_ADDR 0x080E0000
#define HAL_FLASH_BANK_0_SECTOR FLASH_SECTOR_10
#define HAL_FLASH_BANK_1_SECTOR FLASH_SECTOR_11

//
int btstack_main(int argc, char ** argv);
void port_main(void){

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // uncomment to enable packet logger
#ifdef ENABLE_SEGGER_RTT
    // hci_dump_init(hci_dump_segger_rtt_stdout_get_instance());
#else
    // hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif

    // init HCI
    hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), (void*) &config);
    hci_set_chipset(btstack_chipset_cc256x_instance());

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

#ifdef HAVE_HAL_AUDIO
    // setup audio
    btstack_audio_sink_set_instance(btstack_audio_embedded_sink_get_instance());
    btstack_audio_source_set_instance(btstack_audio_embedded_source_get_instance());
#endif

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // hand over to btstack embedded code
    btstack_main(0, NULL);

    // go
    btstack_run_loop_execute();
}

#if 0

// Help with debugging hard faults - from FreeRTOS docu
// https://www.freertos.org/Debugging-Hard-Faults-On-Cortex-M-Microcontrollers.html

void prvGetRegistersFromStack( uint32_t *pulFaultStackAddress );
void prvGetRegistersFromStack( uint32_t *pulFaultStackAddress ) {

    /* These are volatile to try and prevent the compiler/linker optimising them
    away as the variables never actually get used.  If the debugger won't show the
    values of the variables, make them global my moving their declaration outside
    of this function. */
    volatile uint32_t r0;
    volatile uint32_t r1;
    volatile uint32_t r2;
    volatile uint32_t r3;
    volatile uint32_t r12;
    volatile uint32_t lr; /* Link register. */
    volatile uint32_t pc; /* Program counter. */
    volatile uint32_t psr;/* Program status register. */

    r0  = pulFaultStackAddress[ 0 ];
    r1  = pulFaultStackAddress[ 1 ];
    r2  = pulFaultStackAddress[ 2 ];
    r3  = pulFaultStackAddress[ 3 ];

    r12 = pulFaultStackAddress[ 4 ];
    lr  = pulFaultStackAddress[ 5 ];
    pc  = pulFaultStackAddress[ 6 ];
    psr = pulFaultStackAddress[ 7 ];

    /* When the following line is hit, the variables contain the register values. */
    for( ;; );
}

/* The prototype shows it is a naked function - in effect this is just an
assembly function. */
void HardFault_Handler( void ) __attribute__( ( naked ) );

/* The fault handler implementation calls a function called
prvGetRegistersFromStack(). */
void HardFault_Handler(void)
{
    __asm volatile
    (
        " tst lr, #4                                                \n"
        " ite eq                                                    \n"
        " mrseq r0, msp                                             \n"
        " mrsne r0, psp                                             \n"
        " ldr r1, [r0, #24]                                         \n"
        " ldr r2, handler2_address_const                            \n"
        " bx r2                                                     \n"
        " handler2_address_const: .word prvGetRegistersFromStack    \n"
    );
}

#endif
