/*******************************************************************************
* Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
* Author: Ismail H. Kose <ismail.kose@maximintegrated.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/

#include <stdio.h>
#include <string.h>

// MXC
#include "lp.h"
#include "uart.h"
#include "board.h"
#include "led.h"

// BTstack Core
#include "btstack_debug.h"
#include "btstack.h"
#include "btstack_config.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_chipset_cc256x.h"
#include "hci_dump_embedded_stdout.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"

// BTstack HALs
#include "hal_tick.h"
#include "hal_stdin.h"

#include "btstack_port.h"

#define CC256X_UART_ID             0
#define UART_RXFIFO_USABLE     (MXC_UART_FIFO_DEPTH-3)

static uint32_t baud_rate;

// rx state
static int  bytes_to_read = 0;
static uint8_t * rx_buffer_ptr = 0;

// tx state
static int bytes_to_write = 0;
static uint8_t * tx_buffer_ptr = 0;

const gpio_cfg_t PAN1326_SLOW_CLK = { PORT_1, PIN_7, GPIO_FUNC_GPIO,
		GPIO_PAD_NORMAL };
const gpio_cfg_t PAN1326_nSHUTD = { PORT_1, PIN_6, GPIO_FUNC_GPIO,
		GPIO_PAD_NORMAL };
const gpio_cfg_t PAN1326_HCIRTS = { PORT_0, PIN_3, GPIO_FUNC_GPIO,
		GPIO_PAD_INPUT_PULLUP };
const gpio_cfg_t PAN1326_HCICTS = { PORT_0, PIN_2, GPIO_FUNC_GPIO,
		GPIO_PAD_NORMAL };

static void dummy_handler(void) {};
static void (*rx_done_handler)(void) = dummy_handler;
static void (*tx_done_handler)(void) = dummy_handler;



void hal_cpu_disable_irqs(void)
{
	__disable_irq();
}

void hal_cpu_enable_irqs(void)
{
	__enable_irq();
}
void hal_cpu_enable_irqs_and_sleep(void)
{
	__enable_irq();
	/* TODO: Add sleep mode */
}

void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t len)
{
	tx_buffer_ptr = (uint8_t *)buffer;
	bytes_to_write = len;
}

void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len)
{
	rx_buffer_ptr = buffer;
	bytes_to_read = len;
}

void hal_btstack_run_loop_execute_once(void)
{
	int rx_avail;
	int num_rx_bytes;
	int tx_avail;
	int rx_bytes;
	int tx_bytes;
	int ret;

    while (bytes_to_read) {
		rx_avail = UART_NumReadAvail(MXC_UART_GET_UART(CC256X_UART_ID));
		if (!rx_avail)
			break;

		if (bytes_to_read > rx_avail)
			num_rx_bytes = rx_avail;
		else
			num_rx_bytes = bytes_to_read;

		ret = UART_Read(MXC_UART_GET_UART(CC256X_UART_ID), rx_buffer_ptr, num_rx_bytes, &rx_bytes);
		if (ret < 0)
			break;

		rx_buffer_ptr += rx_bytes;
        bytes_to_read -= rx_bytes;

		 if (bytes_to_read < 0) {
			bytes_to_read = 0;
		}

         if (bytes_to_read == 0){
             (*rx_done_handler)();
         }
     }

     while (bytes_to_write) {
		tx_avail = UART_NumWriteAvail(MXC_UART_GET_UART(CC256X_UART_ID));
		if (!tx_avail)
			break;

		if (bytes_to_write > tx_avail)
			tx_bytes = tx_avail;
		else
			tx_bytes = bytes_to_write;

		ret = UART_Write(MXC_UART_GET_UART(CC256X_UART_ID), tx_buffer_ptr, tx_bytes);
		if (ret < 0)
			break;
		bytes_to_write -= tx_bytes;
		tx_buffer_ptr += tx_bytes;
		if (bytes_to_write < 0) {
			bytes_to_write = 0;
		}

        if (bytes_to_write == 0){
             (*tx_done_handler)();
        }
     }

	btstack_run_loop_embedded_execute_once();
}

void hal_uart_init(void)
{
	int error = 0;
	uart_cfg_t cfg;

	cfg.parity = UART_PARITY_DISABLE;
	cfg.size = UART_DATA_SIZE_8_BITS;
	cfg.extra_stop = 0;
	cfg.cts = 1;
	cfg.rts = 1;

	cfg.baud = baud_rate;

	sys_cfg_uart_t sys_cfg;
	sys_cfg.clk_scale = CLKMAN_SCALE_AUTO;

	sys_cfg.io_cfg = (ioman_cfg_t )IOMAN_UART(0,
			IOMAN_MAP_B, // io_map
			IOMAN_MAP_B, // cts_map
			IOMAN_MAP_B, // rts_map
			1, // io_en
			1, // cts_en
			1); //rts_en

	if ((error = UART_Init(MXC_UART_GET_UART(CC256X_UART_ID), &cfg, &sys_cfg)) != E_NO_ERROR) {
		printf("Error initializing UART %d\n", error);
		while (1);
	} else {
		printf("BTSTACK UART Initialized\n");
	}

	MXC_UART_GET_UART(CC256X_UART_ID)->ctrl |= MXC_F_UART_CTRL_CTS_POLARITY | MXC_F_UART_CTRL_RTS_POLARITY;
	MXC_UART_GET_UART(CC256X_UART_ID)->ctrl &= ~((MXC_UART_FIFO_DEPTH - 4) << (MXC_F_UART_CTRL_RTS_LEVEL_POS));
	MXC_UART_GET_UART(CC256X_UART_ID)->ctrl |= ((UART_RXFIFO_USABLE) << MXC_F_UART_CTRL_RTS_LEVEL_POS);
}

int hal_uart_dma_set_baud(uint32_t baud){
	baud_rate = baud;
	printf("BAUD RATE IS = %d \n", baud);
	hal_uart_init();
	return baud_rate;
}

void hal_uart_dma_init(void){
	bytes_to_write = 0;
	bytes_to_read = 0;
	hal_uart_dma_set_baud(115200);
}

void hal_uart_dma_set_block_received( void (*block_handler)(void)){
	rx_done_handler = block_handler;
}

void hal_uart_dma_set_block_sent( void (*block_handler)(void)){

	tx_done_handler = block_handler;
}

void hal_uart_dma_set_csr_irq_handler( void (*csr_irq_handler)(void)){

}

void hal_uart_dma_set_sleep(uint8_t sleep){

}

void init_slow_clock(void)
{
	MXC_PWRSEQ->reg0 &= ~(MXC_F_PWRSEQ_REG0_PWR_RTCEN_RUN | MXC_F_PWRSEQ_REG0_PWR_RTCEN_SLP);
	MXC_PWRSEQ->reg4 &= ~MXC_F_PWRSEQ_REG4_PWR_PSEQ_32K_EN;
	MXC_PWRSEQ->reg0 |= MXC_F_PWRSEQ_REG0_PWR_RTCEN_RUN | MXC_F_PWRSEQ_REG0_PWR_RTCEN_SLP; // Enable RTC
	hal_delay_us(1);
	MXC_PWRSEQ->reg4 |= MXC_F_PWRSEQ_REG4_PWR_PSEQ_32K_EN; // Enable the RTC out of P1.7
}

int bt_comm_init() {
	int error = 0;
	int cnt = 0;

	hal_tick_init();
	hal_delay_us(1);

	/* HCI module RTS as input with 25k pullup */
	if ((error = GPIO_Config(&PAN1326_HCIRTS)) != E_NO_ERROR) {
		printf("Error setting PAN1326_HCIRTS %d\n", error);
	}
	GPIO_OutSet(&PAN1326_HCIRTS);

	init_slow_clock();
	/*
	 * when enabling the P1.7 RTC output, P1.6 will be hardcoded to an input with 25k pullup enabled.
	 * There is an internal pullup, so when it is set as an input, it will float high.
	 * The PAN1326B data sheet says the NSHUTD pin is pulled down, but the input impedance is stated at 1Meg Ohm,
	 * The so the 25k pullup should be enough to reach the minimum 1.42V to enable the device.
	 * */

	/* Force PAN1326 shutdown to be output and take it out of reset */
	if ((error = GPIO_Config(&PAN1326_nSHUTD)) != E_NO_ERROR) {
		printf("Error setting PAN1326_nSHUTD %d\n", error);
	}
	GPIO_OutSet(&PAN1326_nSHUTD);

	/*Check the module is ready to receive data */
	while (GPIO_InGet(&PAN1326_HCIRTS)) {
		cnt++;
	}

	printf("%s CC256X init completed. cnt: %d \n", __func__, cnt);
	return 0;
}

static hci_transport_config_uart_t config = {
	    HCI_TRANSPORT_CONFIG_UART,
	    115200,
	    4000000,
	    1, // flow control
	    "max32630fthr",
	};

// hal_led.h implementation
#include "hal_led.h"
void hal_led_off(void){
	LED_Off(LED_BLUE);
}

void hal_led_on(void){
	LED_On(LED_BLUE);
}

void hal_led_toggle(void){
	LED_Toggle(LED_BLUE);
}

// hal_stdin.h
static uint8_t stdin_buffer[1];
static void (*stdin_handler)(char c);

static uart_req_t uart_byte_request;

static void uart_rx_handler(uart_req_t *request, int error)
{
    if (stdin_handler){
        (*stdin_handler)(stdin_buffer[0]);
    }
	UART_ReadAsync(MXC_UART_GET_UART(CONSOLE_UART), &uart_byte_request);
}

void hal_stdin_setup(void (*handler)(char c)){
    // set handler
    stdin_handler = handler;

	/* set input handler */
	uart_byte_request.callback = uart_rx_handler;
	uart_byte_request.data = stdin_buffer;
	uart_byte_request.len = sizeof(uint8_t);
	UART_ReadAsync(MXC_UART_GET_UART(CONSOLE_UART), &uart_byte_request);
}

#if 0

#include "btstack_stdin.h"

static btstack_data_source_t stdin_data_source;
static void (*stdin_handler)(char c);

static uart_req_t uart_byte_request;
static volatile int stdin_character_received;
static uint8_t stdin_buffer[1];

static void stdin_rx_complete(void) {
    stdin_character_received = 1;
}

static void uart_rx_handler(uart_req_t *request, int error)
{
	stdin_rx_complete();
}

static void stdin_process(struct btstack_data_source *ds, btstack_data_source_callback_type_t callback_type){
    if (!stdin_character_received) return;
    if (stdin_handler){
        (*stdin_handler)(stdin_buffer[0]);
    }
    stdin_character_received = 0;
	UART_ReadAsync(MXC_UART_GET_UART(CONSOLE_UART), &uart_byte_request);
}

static void btstack_stdin_handler(char c){
    stdin_character_received = 1;
    btstack_run_loop_embedded_trigger();
    printf("Received: %c\n", c);
}

void btstack_stdin_setup(void (*handler)(char c)){
    // set handler
    stdin_handler = handler;

    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&stdin_data_source, &stdin_process);
    btstack_run_loop_enable_data_source_callbacks(&stdin_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&stdin_data_source);

	/* set input handler */
	uart_byte_request.callback = uart_rx_handler;
	uart_byte_request.data = stdin_buffer;
	uart_byte_request.len = sizeof(uint8_t);
	UART_ReadAsync(MXC_UART_GET_UART(CONSOLE_UART), &uart_byte_request);
}
#endif

#include "hal_flash_bank_mxc.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "btstack_link_key_db_tlv.h"
#include "le_device_db_tlv.h"

#define HAL_FLASH_BANK_SIZE    0x2000
#define HAL_FLASH_BANK_0_ADDR  0x1FC000
#define HAL_FLASH_BANK_1_ADDR  0x1FE000

static hal_flash_bank_mxc_t hal_flash_bank_context;
static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;


/******************************************************************************/
int bluetooth_main(void)
{
	LED_Off(LED_GREEN);
	LED_On(LED_RED);
	LED_Off(LED_BLUE);

	bt_comm_init();
	/* BT Stack Initialization */
	btstack_memory_init();
	btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

	// enable packet logger
    // hci_dump_init(hci_dump_embedded_stdout_get_instance());

	/* Init HCI */
	const hci_transport_t * transport = hci_transport_h4_instance(btstack_uart_block_embedded_instance());
	hci_init(transport, &config);
	hci_set_chipset(btstack_chipset_cc256x_instance());

    // setup TLV Flash Bank implementation
    const hal_flash_bank_t * hal_flash_bank_impl = hal_flash_bank_mxc_init_instance(
		&hal_flash_bank_context,
		HAL_FLASH_BANK_SIZE,
			HAL_FLASH_BANK_0_ADDR,
			HAL_FLASH_BANK_1_ADDR);
    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(
		&btstack_tlv_flash_bank_context,
			hal_flash_bank_impl,
			&hal_flash_bank_context);

    // setup Link Key DB using TLV
    const btstack_link_key_db_t * btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
    hci_set_link_key_db(btstack_link_key_db);

    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, &btstack_tlv_flash_bank_context);

    // go
	btstack_main(0, (void *)NULL);
	return 0;
}
