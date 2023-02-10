#define __BTSTACK_FILE__ "port.c"

// include STM32 first to avoid warning about redefinition of UNUSED
#include "stm32f4xx_hal.h"
#include "core_cm4.h"
#include "main.h"

#include "port.h"
#include "btstack.h"
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_chipset_csr.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"
#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db_static.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hal_flash_bank_stm32.h"
#include <stdio.h>

#ifdef ENABLE_SEGGER_RTT
#include "SEGGER_RTT.h"
#include "hci_dump_segger_rtt_stdout.h"
#else
#include "hci_dump_embedded_stdout.h"
#endif

extern UART_HandleTypeDef huart2;//system log uart or hci log uart
extern UART_HandleTypeDef huart3;//hci uart

#define ENABLE_SYSTEM_LOG 1

#if ENABLE_SYSTEM_LOG == 1
#define SYSTEM_LOG_UART	  huart2
#else
#define HCI_LOG_UART	  huart2
#endif

#define HCI_DRIVER_UART	  huart3
//
static btstack_packet_callback_registration_t hci_event_callback_registration;

static const hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    0,
    1,
    NULL
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
	__asm__("wfe");	// go to sleep if event flag isn't set. if set, just clear it. IRQs set event flag
}

void hal_cpu_reset(void){
    NVIC_SystemReset();
}

/*close all peripherals that opend at bootup*/
void hal_cpu_deinit(void){
    HAL_DeInit();
    HAL_RCC_DeInit();
}

void hal_cpu_remap_vecotr_table(uint32_t vector_table){
    SCB->VTOR = (vector_table & 0xFFFFFFF8);
}

uint8_t hal_cpu_is_msp_valid(uint32_t msp){
    return ((msp & 0x2FFE0000 ) == 0x20020000);
}

#ifndef ENABLE_SEGGER_RTT
// hal_stdin.h
#include "hal_stdin.h"
static uint8_t stdin_buffer[1];
static void (*stdin_handler)(char c);
void hal_stdin_setup(void (*handler)(char c)){
    stdin_handler = handler;
    // start receiving
#if ENABLE_SYSTEM_LOG
	HAL_UART_Receive_IT(&SYSTEM_LOG_UART, &stdin_buffer[0], 1);
#endif
}

static void stdin_rx_complete(void){
    if (stdin_handler){
        (*stdin_handler)(stdin_buffer[0]);
    }
#if ENABLE_SYSTEM_LOG
    HAL_UART_Receive_IT(&SYSTEM_LOG_UART, &stdin_buffer[0], 1);
#endif
}
#endif
// hal_uart_dma.h

// hal_uart_dma.c implementation
#include "hal_uart_dma.h"

static void dummy_handler(void);

// handlers
static void (*rx_done_handler)(void) = &dummy_handler;
static void (*tx_done_handler)(void) = &dummy_handler;
static void (*cts_irq_handler)(void) = &dummy_handler;

static void dummy_handler(void){};
static int hal_uart_needed_during_sleep;

void hal_uart_dma_set_sleep(uint8_t sleep){
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
	if (huart == &HCI_DRIVER_UART){
		(*tx_done_handler)();
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if (huart == &HCI_DRIVER_UART){
		(*rx_done_handler)();
	}
#ifndef ENABLE_SEGGER_RTT
#if ENABLE_SYSTEM_LOG
    if (huart == &SYSTEM_LOG_UART){
        stdin_rx_complete();
    }
#endif
#endif
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
}

void hal_uart_dma_set_csr_irq_handler( void (*the_irq_handler)(void)){
}

int hal_uart_dma_set_baud(uint32_t baud) {
    HAL_UART_Abort_IT(&HCI_DRIVER_UART);
    HAL_UART_DeInit(&HCI_DRIVER_UART);
    MX_DMA_DeInit();

	HCI_DRIVER_UART.Init.BaudRate = baud;
    MX_DMA_Init();
	HAL_UART_Init(&HCI_DRIVER_UART);

    return 0;
}

void hal_uart_dma_send_block(const uint8_t *data, uint16_t size){
    HAL_UART_Transmit_DMA( &HCI_DRIVER_UART, (uint8_t *) data, size);
}

void hal_uart_dma_receive_block(uint8_t *data, uint16_t size){
	HAL_UART_Receive_DMA( &HCI_DRIVER_UART, data, size );
}

void hal_uart_dma_send_block_for_hci(const uint8_t *data, uint16_t size){
#if ENABLE_SYSTEM_LOG == 0
	HAL_UART_Transmit( &HCI_LOG_UART, (uint8_t *) data, size, HAL_MAX_DELAY );
#endif
}

void hci_log_through_frontline(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len)
{
typedef enum {
    BT_HCI_LOG_CMD     = 0x01,
    BT_HCI_LOG_ACL_OUT = 0x02,
    BT_HCI_LOG_ACL_IN  = 0x04,
    BT_HCI_LOG_EVT     = 0x08
} bt_hci_log_type_t;

#define BT_HCI_LOG_HEADER_LEGNTH 5 //header + direction + sizeof(log_length)
#define BT_UART_CMD 0x01
#define	BT_UART_ACL 0x02
#define	BT_UART_EVT 0x04

    bt_hci_log_type_t type;
    uint16_t data_tatal_length = 0;
    uint16_t index = 0, i = 0;
    uint8_t *buf = NULL;
    uint8_t check_sum = 0;

    if (packet_type == BT_UART_EVT && packet[0] > 0x3e && packet[0] != 0xFF) {
        return;
    }

    if (in == 0) {
        if (packet_type == BT_UART_CMD) {
            type = BT_HCI_LOG_CMD;
        } else if (packet_type == BT_UART_ACL) {
            type = BT_HCI_LOG_ACL_OUT;
        } else {
            return;
        }
    } else if (in == 1) {
        if (packet_type == BT_UART_ACL) {
            type = BT_HCI_LOG_ACL_IN;
        } else if (packet_type == BT_UART_EVT) {
            type = BT_HCI_LOG_EVT;
        } else {
            return;
        }
    }

    data_tatal_length = BT_HCI_LOG_HEADER_LEGNTH + len + 1;//1:check sum
    buf = (uint8_t *)malloc(data_tatal_length);
    //BT_ASSERT(buf);

    buf[index++] = 0xF5;
    buf[index++] = 0x5A;
    buf[index++] = type;
    buf[index++] = len & 0xFF;
    buf[index++] = (len >> 8) & 0xFF;
    for (i = 0; i < len; index++, i++) {
        buf[index] = packet[i];
    }
    for (i = 0; i < data_tatal_length - 1; i++) {
        check_sum += buf[i];
    }
    buf[index] = check_sum;

    hal_uart_dma_send_block_for_hci(buf, data_tatal_length);

    free(buf);
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
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
int _write(int file, char *ptr, int len);
int _write(int file, char *ptr, int len){
	uint8_t cr = '\r';
	int i;

#if ENABLE_SYSTEM_LOG
	if (file == STDOUT_FILENO || file == STDERR_FILENO) {
		for (i = 0; i < len; i++) {
			if (ptr[i] == '\n') {
				HAL_UART_Transmit( &SYSTEM_LOG_UART, &cr, 1, HAL_MAX_DELAY );
			}
			HAL_UART_Transmit( &SYSTEM_LOG_UART, (uint8_t *) &ptr[i], 1, HAL_MAX_DELAY );
		}
		return i;
	}
#else
	errno = EIO;
	return -1;
#endif
}
#endif

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
                // assert manufacturer is CSR
                if (manufacturer != BLUETOOTH_COMPANY_ID_CAMBRIDGE_SILICON_RADIO){
                    printf("ERROR: Expected Bluetooth Chipset from CSR but got manufacturer 0x%04x\n", manufacturer);
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
void port_main(void) {
#if !defined(MCUBOOT_IMG_BOOTLOADER)
    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // uncomment to enable packet logger
#ifdef ENABLE_SEGGER_RTT
    hci_dump_init(hci_dump_segger_rtt_stdout_get_instance());
#else
    hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif
	// init HCI
    hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), (void*) &config);
    hci_set_chipset(btstack_chipset_csr_instance());

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
#ifdef ENABLE_CLASSIC   
    // setup Link Key DB using TLV
    const btstack_link_key_db_t * btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context); 
    hci_set_link_key_db(btstack_link_key_db);
#endif
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
#else
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());
#endif
	// hand over to btstack embedded code
    btstack_main(0, NULL);

    // go
    btstack_run_loop_execute();
}
