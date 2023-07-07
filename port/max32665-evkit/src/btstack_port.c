/* *****************************************************************************
* Copyright (C) Analog Devices, All rights Reserved.
*
* This software is protected by copyright laws of the United States and
* of foreign countries. This material may also be protected by patent laws
* and technology transfer regulations of the United States and of foreign
* countries. This software is furnished under a license agreement and/or a
* nondisclosure agreement and may only be used or reproduced in accordance
* with the terms of those agreements. Dissemination of this information to
* any party or parties not specified in the license agreement and/or
* nondisclosure agreement is expressly prohibited.
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
* property whatsoever. Analog Devices, Inc. retains all
* ownership rights.
**************************************************************************** */


#include <stdio.h>
#include <string.h>

// MXC
#include "lp.h"
#include "uart.h"
#include "board.h"
#include "mxc_device.h"
#include "uart_regs.h"
#include "uart.h"
#include "led.h"

// BTstack Core
#include "btstack_debug.h"
#include "btstack.h"
#include "btstack_config.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_tlv_none.h"

#include "hci_dump_embedded_stdout.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"

// BTstack HALs
#include "hal_tick.h"
#include "hal_stdin.h"
// hal_led.h implementation
#include "hal_led.h"
#include "btstack_port.h"

#include "hal_flash_bank_mxc.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "btstack_link_key_db_tlv.h"
#include "le_device_db_tlv.h"

#define HAL_FLASH_BANK_SIZE 0x2000
#define HAL_FLASH_BANK_1_ADDR 0x1007FFFF
#define HAL_FLASH_BANK_0_ADDR 0x10080000

static uint32_t baud_rate;
volatile uint8_t transaction_complete = 1;
volatile mxc_uart_req_t hci_tx_request;
volatile mxc_uart_req_t hci_rx_request;

// rx state
static int bytes_to_read = 0;
static uint8_t *rx_buffer_ptr = 0;

// tx state
static int bytes_to_write = 0;
static uint8_t *tx_buffer_ptr = 0;

static void dummy_handler(void){};
static void (*rx_done_handler)(void) = dummy_handler;
static void (*tx_done_handler)(void) = dummy_handler;
void DMA_Handler(void)
{
	MXC_DMA_Handler(MXC_DMA0);
}
void UART0_IRQHandler(void)
{
	LED_On(0);
	MXC_UART_AsyncHandler(MXC_UART0);
	LED_Off(0);
}
void UART1_IRQHandler(void)
{
	LED_On(0);
	MXC_UART_AsyncHandler(MXC_UART1);
	LED_Off(0);
}
void UART2_IRQHandler(void)
{
	LED_On(0);
	MXC_UART_AsyncHandler(MXC_UART2);
	LED_Off(0);
}

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
static void send_handler()
{

	transaction_complete = 1;
	(*tx_done_handler)();
}

void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t len)
{

	hci_tx_request.callback = send_handler;
	hci_tx_request.uart = MXC_UART_GET_UART(HCI_UART);
	hci_tx_request.txData = buffer;
	hci_tx_request.txLen = len;
	hci_tx_request.rxData = NULL;
	hci_tx_request.rxLen = 0;

	int ret = MXC_UART_TransactionAsync(&hci_tx_request);

	if (ret != E_NO_ERROR)
	{
		printf("Failed to start transaction");
	}

}
static void read_handler(mxc_uart_req_t *req, int error)
{
	transaction_complete = 1;
	(*rx_done_handler)();
}
void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len)
{

	
	rx_buffer_ptr = buffer;
	bytes_to_read = len;

}

void hal_uart_init(void)
{
	uint8_t uartNum;
	mxc_uart_regs_t *uart;
	uint32_t irqn;
	int result;

	// MXC_DMA_ReleaseChannel(0);
	// MXC_NVIC_SetVector(DMA0_IRQn, DMA_Handler);
	// NVIC_EnableIRQ(DMA0_IRQn);

	uartNum = HCI_UART;
	uart = MXC_UART_GET_UART(uartNum);
	irqn = MXC_UART_GET_IRQ(uartNum);

	result = MXC_UART_Init(uart, baud_rate, HCI_UART_MAP);

	MXC_UART_SetDataSize(uart, 8);
	MXC_UART_SetStopBits(uart, MXC_UART_STOP_1);
	MXC_UART_SetParity(uart, MXC_UART_PARITY_DISABLE);

	NVIC_EnableIRQ(irqn);
	/* Set the interrupt priority lower than the default */
	NVIC_SetPriority(irqn, 1);

	return result;
}

void hal_btstack_run_loop_execute_once(void)
{
	int rx_avail;
	int num_rx_bytes;
	int tx_avail;
	int rx_bytes;
	int tx_bytes;
	int ret;

	while (bytes_to_read)
	{
		rx_avail = MXC_UART_GetRXFIFOAvailable(MXC_UART_GET_UART(HCI_UART));
		if (!rx_avail)
			break;

		if (bytes_to_read > rx_avail)
			num_rx_bytes = rx_avail;
		else
			num_rx_bytes = bytes_to_read;

		ret = MXC_UART_Read(MXC_UART_GET_UART(HCI_UART), rx_buffer_ptr, &num_rx_bytes);
		if (ret < 0)
			break;

		rx_buffer_ptr += num_rx_bytes;
		bytes_to_read -= num_rx_bytes;

		if (bytes_to_read < 0)
		{
			bytes_to_read = 0;
		}

		if (bytes_to_read == 0)
		{
			(*rx_done_handler)();
		}
	}

	
	btstack_run_loop_embedded_execute_once();
}

int hal_uart_dma_set_baud(uint32_t baud)
{
	baud_rate = baud;
	printf("BAUD RATE IS = %d \n", baud);
	hal_uart_init();
	return baud_rate;
}

void hal_uart_dma_init(void)
{
	bytes_to_write = 0;
	bytes_to_read = 0;
	hal_uart_dma_set_baud(115200);
}

void hal_uart_dma_set_block_received(void (*block_handler)(void))
{
	rx_done_handler = block_handler;
}

void hal_uart_dma_set_block_sent(void (*block_handler)(void))
{

	tx_done_handler = block_handler;
}

void hal_uart_dma_set_csr_irq_handler(void (*csr_irq_handler)(void))
{
}

void hal_uart_dma_set_sleep(uint8_t sleep)
{
}

void init_slow_clock(void)
{
}

int bt_comm_init()
{
	int error = 0;
	int cnt = 0;

	hal_tick_init();
	hal_delay_us(1);

	return 0;
}

static hci_transport_config_uart_t config = {
	HCI_TRANSPORT_CONFIG_UART,
	115200,
	0,
	1, // flow control
	"max32665",
};

void hal_led_off(void)
{
	LED_Off(0);
}

void hal_led_on(void)
{
	LED_On(0);
}

void hal_led_toggle(void)
{
	LED_Toggle(0);
}

// hal_stdin.h
static uint8_t stdin_buffer[1];
static void (*stdin_handler)(char c);

static volatile mxc_uart_req_t uart_byte_request;

static void uart_rx_handler(mxc_uart_req_t *request, int error)
{
	if (stdin_handler)
	{
		(*stdin_handler)(stdin_buffer[0]);
	}
	MXC_UART_ReadAsync(MXC_UART_GET_UART(CONSOLE_UART), &uart_byte_request);
}

void hal_stdin_setup(void (*handler)(char c))
{
	// set handler
	stdin_handler = handler;

	/* set input handler */

	uart_byte_request.uart = MXC_UART_GET_UART(CONSOLE_UART);
	uart_byte_request.rxData = stdin_buffer;
	uart_byte_request.txData = NULL;
	uart_byte_request.rxLen = sizeof(uint8_t);
	uart_byte_request.txLen = 0;

	MXC_UART_Transaction(&uart_byte_request);
}

static hal_flash_bank_mxc_t hal_flash_bank_context;
static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;

/******************************************************************************/
int bluetooth_main(void)
{
	LED_Off(1);
	LED_On(1);
	LED_Off(0);


	
	bt_comm_init();
	/* BT Stack Initialization */
	btstack_memory_init();
	btstack_run_loop_init(btstack_run_loop_embedded_get_instance());
	gap_discoverable_control(1);
	
	// enable packet logger
	hci_dump_init(hci_dump_embedded_stdout_get_instance());

	

	/* Init HCI */
	const hci_transport_t *transport = hci_transport_h4_instance(btstack_uart_block_embedded_instance());

	

	// setup TLV Flash Bank implementation


	const btstack_tlv_t *btstack_tlv_impl = btstack_tlv_none_init_instance();
	// setup global tlv
	btstack_tlv_set_instance(btstack_tlv_impl, NULL);

	// setup LE Device DB using TLV
	le_device_db_tlv_configure(btstack_tlv_impl, NULL);

	// hci_set_chipset(btstack_chipset_cc256x_instance());
	hci_init(transport, &config);

	// go
	btstack_main(0, (void *)NULL);
	return 0;
}
