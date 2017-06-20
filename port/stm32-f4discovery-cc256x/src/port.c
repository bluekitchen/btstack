#include "port.h"
#include "btstack.h"
#include "btstack_debug.h"
#include "btstack_chipset_cc256x.h"
#include "btstack_run_loop_embedded.h"
#include "classic/btstack_link_key_db_static.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hal_flash_sector.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_sector.h"
#include "stm32f4xx_hal.h"

#define __BTSTACK_FILE__ "port.c"

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

static void dummy_handler(void){};
static int hal_uart_needed_during_sleep;

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

//	if (sleep){
//		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
//	}
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
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            printf("BTstack up and running.\n");
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_local_version_information)){
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

// hal_flash_sector.h
typedef struct {
	uint32_t   sector_size;
	uint32_t   sectors[2];
	uintptr_t  banks[2];

} hal_flash_sector_stm32_t;

static uint32_t hal_flash_sector_stm32_get_size(void * context){
	hal_flash_sector_stm32_t * self = (hal_flash_sector_stm32_t *) context;
	return self->sector_size;
}

static void hal_flash_sector_stm32_erase(void * context, int bank){
	hal_flash_sector_stm32_t * self = (hal_flash_sector_stm32_t *) context;
	if (bank > 1) return;
	FLASH_EraseInitTypeDef eraseInit;
	eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
	eraseInit.Sector = self->sectors[bank];
	eraseInit.NbSectors = 1;
	eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_1;	// safe value
	uint32_t sectorError;
	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(&eraseInit, &sectorError);
	HAL_FLASH_Lock();
}

static void hal_flash_sector_stm32_read(void * context, int bank, uint32_t offset, uint8_t * buffer, uint32_t size){
	hal_flash_sector_stm32_t * self = (hal_flash_sector_stm32_t *) context;

	if (bank > 1) return;
	if (offset > self->sector_size) return;
	if ((offset + size) > self->sector_size) return;

	memcpy(buffer, ((uint8_t *) self->banks[bank]) + offset, size);
}

static void hal_flash_sector_stm32_write(void * context, int bank, uint32_t offset, const uint8_t * data, uint32_t size){
	hal_flash_sector_stm32_t * self = (hal_flash_sector_stm32_t *) context;

	if (bank > 1) return;
	if (offset > self->sector_size) return;
	if ((offset + size) > self->sector_size) return;

	unsigned int i;
	HAL_FLASH_Unlock();
	for (i=0;i<size;i++){
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, self->banks[bank] + offset +i, data[i]);
	}
	HAL_FLASH_Lock();
}

static const hal_flash_sector_t hal_flash_sector_stm32_impl = {
	/* uint32_t (*get_size)() */ &hal_flash_sector_stm32_get_size,
	/* void (*erase)(int);    */ &hal_flash_sector_stm32_erase,
	/* void (*read)(..);      */ &hal_flash_sector_stm32_read,
	/* void (*write)(..);     */ &hal_flash_sector_stm32_write,
};

static const hal_flash_sector_t * hal_flash_sector_stm32_init_instance(hal_flash_sector_stm32_t * context, uint32_t sector_size,
		uint32_t bank_0_sector, uint32_t bank_1_sector, uintptr_t bank_0_addr, uintptr_t bank_1_addr){
	context->sector_size = sector_size;
	context->sectors[0] = bank_0_sector;
	context->sectors[1] = bank_1_sector;
	context->banks[0]   = bank_0_addr;
	context->banks[1]   = bank_1_addr;
	return &hal_flash_sector_stm32_impl;
}

static btstack_tlv_flash_sector_t btstack_tlv_flash_sector_context;
static hal_flash_sector_stm32_t   hal_flash_sector_context;

#define HAL_FLASH_SECTOR_SIZE (128 * 1024)
#define HAL_FLASH_SECTOR_BANK_0_ADDR 0x080C0000
#define HAL_FLASH_SECTOR_BANK_1_ADDR 0x080E0000
#define HAL_FLASH_SECTOR_BANK_0_SECTOR FLASH_SECTOR_10
#define HAL_FLASH_SECTOR_BANK_1_SECTOR FLASH_SECTOR_11

//
int btstack_main(int argc, char ** argv);
void port_main(void){

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

	 hci_dump_open( NULL, HCI_DUMP_STDOUT );

	// init HCI
    hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), (void*) &config);
    hci_set_chipset(btstack_chipset_cc256x_instance());

    // setup Link Key DB
    const hal_flash_sector_t * hal_flash_sector_impl = hal_flash_sector_stm32_init_instance(
    		&hal_flash_sector_context,
    		HAL_FLASH_SECTOR_SIZE,
			HAL_FLASH_SECTOR_BANK_0_SECTOR,
			HAL_FLASH_SECTOR_BANK_1_SECTOR,
			HAL_FLASH_SECTOR_BANK_0_ADDR,
			HAL_FLASH_SECTOR_BANK_1_ADDR);
    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_flash_sector_init_instance(
    		&btstack_tlv_flash_sector_context,
			hal_flash_sector_impl,
			&hal_flash_sector_context);
    const btstack_link_key_db_t * btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_flash_sector_context);
    hci_set_link_key_db(btstack_link_key_db);

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

	// hand over to btstack embedded code
    btstack_main(0, NULL);

    // go
    btstack_run_loop_execute();
}
