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

#include "hal_tick.h"
#include <stdio.h>
#include <string.h>
#include "lp.h"
#include "uart.h"
#include "board.h"
#include "btstack_debug.h"

#include "btstack.h"
#include "btstack_config.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_chipset_cc256x.h"
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
}
void hal_led_on(void){
}
void hal_led_toggle(void){
}

int bluetooth_main(void)
{
	bt_comm_init();
	/* BT Stack Initialization */
	btstack_memory_init();
	btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

	/* Init HCI */
	const hci_transport_t * transport = hci_transport_h4_instance(btstack_uart_block_embedded_instance());
	const btstack_link_key_db_t *link_key_db = NULL;

	hci_init(transport, &config);
	hci_set_link_key_db(link_key_db);

	hci_set_chipset(btstack_chipset_cc256x_instance());
	btstack_main(0, (void *)NULL);

	return 0;
}
