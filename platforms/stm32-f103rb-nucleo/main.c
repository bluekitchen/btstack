/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */
 
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/exti.h>
#include <libopencmsis/core_cm3.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <btstack/run_loop.h>
#include "hci.h"
#include "bt_control_cc256x.h"
#include "btstack_memory.h"
#include "remote_device_db.h"

// STDOUT_FILENO and STDERR_FILENO are defined by <unistd.h> with GCC
// (this is a hack for IAR)
#ifndef STDOUT_FILENO
#define STDERR_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

// Configuration
// LED2 on PA5
// Debug: USART2, TX on PA2
// Bluetooth: USART3. TX PB10, RX PB11, CTS PB13 (in), RTS PB14 (out), N_SHUTDOWN PB15
#define GPIO_LED2 GPIO5
#define USART_CONSOLE USART2
#define GPIO_BT_N_SHUTDOWN GPIO15

#define GPIO_DEBUG_0 GPIO1
#define GPIO_DEBUG_1 GPIO2

// btstack code starts there
void btstack_main(void);

static void bluetooth_power_cycle(void);

// hal_tick.h inmplementation
#include <btstack/hal_tick.h>

static void dummy_handler(void);
static void (*tick_handler)(void) = &dummy_handler;
static int hal_uart_needed_during_sleep = 1;

static void dummy_handler(void){};

void hal_tick_init(void){
	systick_set_reload(800000);	// 1/4 of clock -> 250 ms tick
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_counter_enable();
	systick_interrupt_enable();
}

int  hal_tick_get_tick_period_in_ms(void){
    return 100;
}

void hal_tick_set_handler(void (*handler)(void)){
    if (handler == NULL){
        tick_handler = &dummy_handler;
        return;
    }
    tick_handler = handler;
}

void sys_tick_handler(void){
	(*tick_handler)();
}

static void msleep(uint32_t delay) {
	uint32_t wake = embedded_get_ticks() + delay / hal_tick_get_tick_period_in_ms();
	while (wake > embedded_get_ticks());
}

// hal_led.h implementation
#include <btstack/hal_led.h>
void hal_led_off(void);
void hal_led_on(void);

void hal_led_off(void){
	gpio_clear(GPIOA, GPIO_LED2);
}
void hal_led_on(void){
	gpio_set(GPIOA, GPIO_LED2);
}
void hal_led_toggle(void){
	gpio_toggle(GPIOA, GPIO_LED2);
}

// hal_cpu.h implementation
#include <btstack/hal_cpu.h>

void hal_cpu_disable_irqs(void){
	__disable_irq();
}

void hal_cpu_enable_irqs(void){
	__enable_irq();
}

void hal_cpu_enable_irqs_and_sleep(void){
	hal_led_off();
	__enable_irq();
	__asm__("wfe");	// go to sleep if event flag isn't set. if set, just clear it. IRQs set event flag

	// note: hal_uart_needed_during_sleep can be used to disable peripheral clock if it's not needed for a timer
	hal_led_on();
}

// hal_uart_dma.c implementation
#include <btstack/hal_uart_dma.h>

// handlers
static void (*rx_done_handler)(void) = dummy_handler;
static void (*tx_done_handler)(void) = dummy_handler;
static void (*cts_irq_handler)(void) = dummy_handler;

static void hal_uart_manual_rts_set(void){
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO_USART3_RTS);
}

static void hal_uart_manual_rts_clear(void){
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART3_RTS);
}

void hal_uart_dma_set_sleep(uint8_t sleep){
	if (sleep){
		hal_uart_manual_rts_set();
	} else {
		hal_uart_manual_rts_clear();
	}
	hal_uart_needed_during_sleep = !sleep;
}

// DMA1_CHANNEL2 UART3_TX
void dma1_channel2_isr(void) {
	if ((DMA1_ISR & DMA_ISR_TCIF2) != 0) {
		DMA1_IFCR |= DMA_IFCR_CTCIF2;
		dma_disable_transfer_complete_interrupt(DMA1, DMA_CHANNEL2);
		usart_disable_tx_dma(USART3);
		dma_disable_channel(DMA1, DMA_CHANNEL2);
		(*tx_done_handler)();
	}
}

// DMA1_CHANNEL2 UART3_RX
void dma1_channel3_isr(void){
	if ((DMA1_ISR & DMA_ISR_TCIF3) != 0) {
		DMA1_IFCR |= DMA_IFCR_CTCIF3;
		dma_disable_transfer_complete_interrupt(DMA1, DMA_CHANNEL3);
		usart_disable_rx_dma(USART3);
		dma_disable_channel(DMA1, DMA_CHANNEL3);

		gpio_set(GPIOB, GPIO_DEBUG_1);
		// hal_uart_manual_rts_set();
		(*rx_done_handler)();
	}
}

// CTS RISING ISR
void exti15_10_isr(void){
	exti_reset_request(EXTI13);
	(*cts_irq_handler)();
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

void hal_uart_dma_set_csr_irq_handler( void (*the_irq_handler)(void)){
	if (the_irq_handler){
		/* Configure the EXTI13 interrupt (USART3_CTS is on PB13) */
		nvic_enable_irq(NVIC_EXTI15_10_IRQ);
		exti_select_source(EXTI13, GPIOB);
		exti_set_trigger(EXTI13, EXTI_TRIGGER_RISING);
		exti_enable_request(EXTI13);
	} else {
		exti_disable_request(EXTI13);
		nvic_disable_irq(NVIC_EXTI15_10_IRQ);
	}
    cts_irq_handler = the_irq_handler;
}

int  hal_uart_dma_set_baud(uint32_t baud){
	usart_disable(USART3);
	usart_set_baudrate(USART3, baud);
	usart_enable(USART3);
	return 0;
}

void hal_uart_dma_send_block(const uint8_t *data, uint16_t size){

	// printf("hal_uart_dma_send_block size %u\n", size);
	/*
	 * USART3_TX Using DMA_CHANNEL2 
	 */

	/* Reset DMA channel*/
	dma_channel_reset(DMA1, DMA_CHANNEL2);

	dma_set_peripheral_address(DMA1, DMA_CHANNEL2, (uint32_t)&USART3_DR);
	dma_set_memory_address(DMA1, DMA_CHANNEL2, (uint32_t)data);
	dma_set_number_of_data(DMA1, DMA_CHANNEL2, size);
	dma_set_read_from_memory(DMA1, DMA_CHANNEL2);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL2);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL2, DMA_CCR_PSIZE_8BIT);
	dma_set_memory_size(DMA1, DMA_CHANNEL2, DMA_CCR_MSIZE_8BIT);
	dma_set_priority(DMA1, DMA_CHANNEL2, DMA_CCR_PL_VERY_HIGH);
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL2);
	dma_enable_channel(DMA1, DMA_CHANNEL2);
    usart_enable_tx_dma(USART3);
}

void hal_uart_dma_receive_block(uint8_t *data, uint16_t size){

	// hal_uart_manual_rts_clear();
	gpio_clear(GPIOB, GPIO_DEBUG_1);

	/*
	 * USART3_RX is on DMA_CHANNEL3
	 */

	// printf("hal_uart_dma_receive_block req size %u\n", size);

	/* Reset DMA channel*/
	dma_channel_reset(DMA1, DMA_CHANNEL3);

	dma_set_peripheral_address(DMA1, DMA_CHANNEL3, (uint32_t)&USART3_DR);
	dma_set_memory_address(DMA1, DMA_CHANNEL3, (uint32_t)data);
	dma_set_number_of_data(DMA1, DMA_CHANNEL3, size);
	dma_set_read_from_peripheral(DMA1, DMA_CHANNEL3);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL3);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL3, DMA_CCR_PSIZE_8BIT);
	dma_set_memory_size(DMA1, DMA_CHANNEL3, DMA_CCR_MSIZE_8BIT);
	dma_set_priority(DMA1, DMA_CHANNEL3, DMA_CCR_PL_HIGH);
	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL3);
	dma_enable_channel(DMA1, DMA_CHANNEL3);
    usart_enable_rx_dma(USART3);
}

// end of hal_uart

/**
 * Use USART_CONSOLE as a console.
 * This is a syscall for newlib
 * @param file
 * @param ptr
 * @param len
 * @return
 */
int _write(int file, char *ptr, int len);
int _write(int file, char *ptr, int len){
	int i;

	if (file == STDOUT_FILENO || file == STDERR_FILENO) {
		for (i = 0; i < len; i++) {
			if (ptr[i] == '\n') {
				usart_send_blocking(USART_CONSOLE, '\r');
			}
			usart_send_blocking(USART_CONSOLE, ptr[i]);
		}
		return i;
	}
	errno = EIO;
	return -1;
}

static void clock_setup(void){
	/* Enable clocks for GPIO port A (for GPIO_USART1_TX) and USART1 + USART2. */
	/* needs to be done before initializing other peripherals */
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_USART2);
	rcc_periph_clock_enable(RCC_USART3);
	rcc_periph_clock_enable(RCC_DMA1);
	rcc_periph_clock_enable(RCC_AFIO); // needed by EXTI interrupts
}

static void gpio_setup(void){
	/* Set GPIO5 (in GPIO port A) to 'output push-pull'. [LED] */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO_LED2);

	// PB1 and PB2 as debug output
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO_DEBUG_0 | GPIO_DEBUG_1);
}

static void debug_usart_setup(void){
	/* Setup GPIO pin GPIO_USART2_TX/GPIO2 on GPIO port A for transmit. */
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART2_TX);

	/* Setup UART parameters. */
	usart_set_baudrate(USART2, 9600);
	usart_set_databits(USART2, 8);
	usart_set_stopbits(USART2, USART_STOPBITS_1);
	usart_set_mode(USART2, USART_MODE_TX);
	usart_set_parity(USART2, USART_PARITY_NONE);
	usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);

	/* Finally enable the USART. */
	usart_enable(USART2);
}

static void bluetooth_setup(void){
	printf("\nBluetooth starting...\n");

	// n_shutdown as output
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,GPIO_BT_N_SHUTDOWN);

	// tx output
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART3_TX);
	// rts output (default to 1)
	gpio_set(GPIOB, GPIO_USART3_RTS);
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART3_RTS);
	// rx input
	gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO_USART3_RX);
	// cts as input
	gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO_USART3_CTS);


	/* Setup UART parameters. */
	usart_set_baudrate(USART3, 115200);
	usart_set_databits(USART3, 8);
	usart_set_stopbits(USART3, USART_STOPBITS_1);
	usart_set_mode(USART3, USART_MODE_TX_RX);
	usart_set_parity(USART3, USART_PARITY_NONE);
	usart_set_flow_control(USART3, USART_FLOWCONTROL_RTS);

	/* Finally enable the USART. */
	usart_enable(USART3);

	// TX
	nvic_set_priority(NVIC_DMA1_CHANNEL2_IRQ, 0);
	nvic_enable_irq(NVIC_DMA1_CHANNEL2_IRQ);

	// RX
	nvic_set_priority(NVIC_DMA1_CHANNEL3_IRQ, 0);
	nvic_enable_irq(NVIC_DMA1_CHANNEL3_IRQ);
}

// reset Bluetooth using n_shutdown
static void bluetooth_power_cycle(void){
	printf("Bluetooth power cycle\n");
	gpio_clear(GPIOA, GPIO_LED2);
	gpio_clear(GPIOB, GPIO_BT_N_SHUTDOWN);
	msleep(250);
	gpio_set(GPIOA, GPIO_LED2);
	gpio_set(GPIOB, GPIO_BT_N_SHUTDOWN);
	msleep(500);
}


// after HCI Reset, use 115200. Then increase baud reate to 468000.
// (on nucleo board without external crystall, running at 8 Mhz, 1 mbps was not possible)
static const hci_uart_config_t hci_uart_config_cc256x = {
    NULL,
    115200,
    460800,
    0
};

int main(void)
{
	clock_setup();
	gpio_setup();
	hal_tick_init();
	debug_usart_setup();
	bluetooth_setup();

	// start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    run_loop_init(RUN_LOOP_EMBEDDED);
    
    // init HCI
    hci_transport_t    * transport = hci_transport_h4_dma_instance();
    bt_control_t       * control   = bt_control_cc256x_instance();
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, (void*) &hci_uart_config_cc256x, control, remote_db);

    // enable eHCILL
    bt_control_cc256x_enable_ehcill(1);

	// hand over to btstack embedded code 
    btstack_main();

    // go
    run_loop_execute();

	return 0;
}
