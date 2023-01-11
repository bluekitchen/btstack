/***********************************************************************************************************************
 * Copyright [2020-2022] Renesas Electronics Corporation and/or its affiliates.  All Rights Reserved.
 *
 * This software and documentation are supplied by Renesas Electronics America Inc. and may only be used with products
 * of Renesas Electronics Corp. and its affiliates ("Renesas").  No other uses are authorized.  Renesas products are
 * sold pursuant to Renesas terms and conditions of sale.  Purchasers are solely responsible for the selection and use
 * of Renesas products and Renesas assumes no liability.  No license, express or implied, to any intellectual property
 * right is granted by Renesas. This software is protected under all applicable laws, including copyright laws. Renesas
 * reserves the right to change or discontinue this software and/or this documentation. THE SOFTWARE AND DOCUMENTATION
 * IS DELIVERED TO YOU "AS IS," AND RENESAS MAKES NO REPRESENTATIONS OR WARRANTIES, AND TO THE FULLEST EXTENT
 * PERMISSIBLE UNDER APPLICABLE LAW, DISCLAIMS ALL WARRANTIES, WHETHER EXPLICITLY OR IMPLICITLY, INCLUDING WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT, WITH RESPECT TO THE SOFTWARE OR
 * DOCUMENTATION.  RENESAS SHALL HAVE NO LIABILITY ARISING OUT OF ANY SECURITY VULNERABILITY OR BREACH.  TO THE MAXIMUM
 * EXTENT PERMITTED BY LAW, IN NO EVENT WILL RENESAS BE LIABLE TO YOU IN CONNECTION WITH THE SOFTWARE OR DOCUMENTATION
 * (OR ANY PERSON OR ENTITY CLAIMING RIGHTS DERIVED FROM YOU) FOR ANY LOSS, DAMAGES, OR CLAIMS WHATSOEVER, INCLUDING,
 * WITHOUT LIMITATION, ANY DIRECT, CONSEQUENTIAL, SPECIAL, INDIRECT, PUNITIVE, OR INCIDENTAL DAMAGES; ANY LOST PROFITS,
 * OTHER ECONOMIC DAMAGE, PROPERTY DAMAGE, OR PERSONAL INJURY; AND EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH LOSS, DAMAGES, CLAIMS OR COSTS.
 **********************************************************************************************************************/

#define BTSTACK_FILE__ "hal_entry.c"

#include "hal_data.h"

#include "bluetooth.h"
#include "bluetooth_company_id.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "hci_dump_embedded_stdout.h"
#include "hci_dump_segger_rtt_stdout.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"
#include "btstack_memory.h"
#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hal_flash_bank_fsp.h"
#include "segger_rtt.h"

#include <stdio.h>
#include <inttypes.h>

#define HAL_UART_RTS_MANUAL

static const hci_transport_config_uart_t config = {
        HCI_TRANSPORT_CONFIG_UART,
        460800,
        0,
        1,
        NULL,
        0
};

void R_BSP_WarmStart(bsp_warm_start_event_t event);

extern bsp_leds_t g_bsp_leds;

static const enum e_bsp_io_port_pin_t BLUETOOTH_PIN_RESET = BSP_IO_PORT_01_PIN_15;
static const enum e_bsp_io_port_pin_t BLUETOOTH_PIN_RTS = BSP_IO_PORT_02_PIN_05;

// hal_time_ms.h implementation
#include "hal_time_ms.h"

volatile uint32_t time_ms;

void timer_1ms(timer_callback_args_t *p_args){
    (void) p_args;
    time_ms++;
}

uint32_t hal_time_ms(void){
    return time_ms;
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

// hal_uart_dma.h implementation
#include "hal_uart_dma.h"
#include "btstack_debug.h"
#include "btstack_ring_buffer.h"
#include "btstack_util.h"

// handlers
static void (*rx_done_handler)(void);
static void (*tx_done_handler)(void);

// ringbuffer to deal with eager fifo
static uint8_t rx_ring_buffer_storage[64];
static btstack_ring_buffer_t rx_ring_buffer;

static volatile uint8_t * rx_buffer;
static volatile uint16_t rx_len;

void hal_uart_dma_set_sleep(uint8_t sleep){
    // TODO: configure RTS as GPIO and raise
    (void) sleep;
}

static void bluetooth_power_cycle(void) {
    R_IOPORT_PinWrite(&g_ioport_ctrl, BLUETOOTH_PIN_RESET, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, BLUETOOTH_PIN_RESET, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(30, BSP_DELAY_UNITS_MILLISECONDS);
}


void user_uart_callback(uart_callback_args_t *p_args){
    switch (p_args->event){
        case UART_EVENT_TX_DATA_EMPTY:
            (*tx_done_handler)();
            break;

        case UART_EVENT_RX_COMPLETE:
            log_info("UART_EVENT_RX_COMPLETE");
            break;

        case UART_EVENT_RX_CHAR:
           if (rx_len > 0){
                *rx_buffer++ = (uint8_t) p_args->data;
                rx_len--;
                if (rx_len == 0) {
#ifdef HAL_UART_RTS_MANUAL
                    R_IOPORT_PinWrite(&g_ioport_ctrl, BLUETOOTH_PIN_RTS, BSP_IO_LEVEL_HIGH);
#endif
                    (*rx_done_handler)();
               }
           } else {
               // store in ring buffer
                uint8_t data = (uint8_t) p_args->data;
                btstack_ring_buffer_write(&rx_ring_buffer, &data, 1);
           }
            break;
        case UART_EVENT_ERR_OVERFLOW:
            log_error("UART_EVENT_ERR_RXBUF_OVERFLOW");
            break;
        default:
            break;
    }
}

void hal_uart_dma_init(void){
    bluetooth_power_cycle();
    btstack_ring_buffer_init(&rx_ring_buffer, rx_ring_buffer_storage, sizeof(rx_ring_buffer_storage));
}

void hal_uart_dma_set_block_received( void (*the_block_handler)(void)){
    rx_done_handler = the_block_handler;
}

void hal_uart_dma_set_block_sent( void (*the_block_handler)(void)){
    tx_done_handler = the_block_handler;
}

void hal_uart_dma_set_csr_irq_handler( void (*the_irq_handler)(void)){
    UNUSED(the_irq_handler);
    // not implemented
}

int  hal_uart_dma_set_baud(uint32_t baud){
    log_info("set baud %" PRIu32 ", not implemented", baud);
    return 0;
}

void hal_uart_dma_send_block(const uint8_t *data, uint16_t size){
    (void) R_SCI_UART_Write(&g_uart0_ctrl, data, size);
}

void hal_uart_dma_receive_block(uint8_t *data, uint16_t size){
    uint32_t number_of_bytes_read = 0;
    btstack_ring_buffer_read(&rx_ring_buffer, data, size, &number_of_bytes_read);
    size -= number_of_bytes_read;
    data += number_of_bytes_read;
    if (size == 0){
        (*rx_done_handler)();
        return;
    }

    // Clear RTS and read from UART
    rx_buffer = data;
    rx_len = size;
#ifdef HAL_UART_RTS_MANUAL
    R_IOPORT_PinWrite(&g_ioport_ctrl, BLUETOOTH_PIN_RTS, BSP_IO_LEVEL_LOW);
#endif
}

// assert implementation
void btstack_assert_failed(const char * file, uint16_t line_nr){
    printf("Assert failed: %s:/%u\n", file, line_nr);
    while(1);
}

// hal_led.h implementation
#include "hal_led.h"
void hal_led_toggle(void){
    log_info("LED Toggle");
}

// retarget
#ifdef ENABLE_SEGGER_RTT
int _write(int file, char *ptr, int len);

// copy from SEGGER_RTT_Syscalls_GCC.c
int _write(int file, char *ptr, int len) {
    (void) file;  /* Not used, avoid warning */
    SEGGER_RTT_Write(0, ptr, len);
    return len;
}
#else
/**
 * Use USART_CONSOLE as a console.
 * This is a syscall for newlib
 */
int _write(int file, char *ptr, int len){
    UNUSED(file);
    UNUSED(ptr);
    UNUSED(len);
    // TODO: implement write buffer
    return -1;
}
#endif

void _fini(void);
void _fini (void){
    btstack_assert(false);
}

// port

// RA6M4 has 8 kB of Data Flash, which can be erased in 64 byte blocks,
// treat it as 2 x 4 kB banks
static hal_flash_bank_fsp_t  hal_flash_bank_context;
#define HAL_FLASH_BANK_SIZE     ( 4096 )
#define HAL_FLASH_BANK_0_ADDR   ( 0x08000000 )
#define HAL_FLASH_BANK_1_ADDR   ( 0x08001000 )

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;

int btstack_main(int argc, const char * argv[]);

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
        default:
            break;
    }
}

void hal_entry (void)
{
#if BSP_TZ_SECURE_BUILD
    /* Enter non-secure code */
    R_BSP_NonSecureEnter();
#endif

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // enable HCI logging
#if 0
#ifdef ENABLE_SEGGER_RTT
    hci_dump_init(hci_dump_segger_rtt_stdout_get_instance());
#else
    hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif
#endif

    hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), (void*) &config);

    // setup TLV Flash Sector implementation
    const hal_flash_bank_t * hal_flash_bank_impl = hal_flash_bank_fsp_init_instance(
            &hal_flash_bank_context,
            HAL_FLASH_BANK_SIZE,
            HAL_FLASH_BANK_0_ADDR,
            HAL_FLASH_BANK_1_ADDR);

    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(
            &btstack_tlv_flash_bank_context,
            hal_flash_bank_impl,
            &hal_flash_bank_context);

    // setup global tlv
    btstack_tlv_set_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);

    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, &btstack_tlv_flash_bank_context);

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // hand over to btstack embedded code
    btstack_main(0, NULL);

    // go
    btstack_run_loop_execute();
}

/*******************************************************************************************************************//**
 * This function is called at various points during the startup process.  This implementation uses the event that is
 * called right before main() to set up the pins.
 *
 * @param[in]  event    Where at in the start up process the code is currently at
 **********************************************************************************************************************/
void R_BSP_WarmStart (bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_RESET == event)
    {
#if BSP_FEATURE_FLASH_LP_VERSION != 0

        /* Enable reading from data flash. */
        R_FACI_LP->DFLCTL = 1U;

        /* Would normally have to wait tDSTOP(6us) for data flash recovery. Placing the enable here, before clock and
         * C runtime initialization, should negate the need for a delay since the initialization will typically take more than 6us. */
#endif
    }

    if (BSP_WARM_START_POST_C == event)
    {
        /* C runtime environment and system clocks are setup. */

        // init peripherals
        fsp_err_t  err;
        g_hal_init();

        err = R_IOPORT_Open(&g_ioport_ctrl, g_ioport.p_cfg);
        assert(FSP_SUCCESS == err);

        err = R_SCI_UART_Open(&g_uart0_ctrl, g_uart0.p_cfg);
        assert(FSP_SUCCESS == err);

        err = R_GPT_Open(&g_timer0_ctrl, &g_timer0_cfg);
        (void) R_GPT_Start(&g_timer0_ctrl);

        err = R_FLASH_HP_Open(&g_flash0_ctrl, &g_flash0_cfg);
        assert(FSP_SUCCESS == err);

     }
}
