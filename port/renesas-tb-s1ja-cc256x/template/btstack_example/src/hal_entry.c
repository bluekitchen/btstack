/*
 * Copyright (C) 2019 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "hal_entry.c"

#include "../src/synergy_gen/hal_data.h"

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

#define nShutdown_pin IOPORT_PORT_01_PIN_12
#define rts_pin       IOPORT_PORT_03_PIN_03

// handlers
static void (*rx_done_handler)(void);
static void (*tx_done_handler)(void);
static void (*cts_irq_handler)(void);

// ringbuffer to deal with eager fifo
static uint8_t rx_ring_buffer_storage[64];
static btstack_ring_buffer_t rx_ring_buffer;

static volatile uint8_t * rx_buffer;
static volatile uint16_t rx_len;

void hal_uart_dma_set_sleep(uint8_t sleep){
    // TODO: configure RTS as GPIO and raise
    (void) sleep;
}

static void nShutdown_low(void){
    g_ioport.p_api->pinWrite(nShutdown_pin, IOPORT_LEVEL_LOW);
}

static void nShutdown_high(void){
    g_ioport.p_api->pinWrite(nShutdown_pin, IOPORT_LEVEL_HIGH);
}

// reset Bluetooth using n_shutdown
static void bluetooth_power_cycle(void){
    nShutdown_low();
    R_BSP_SoftwareDelay( 250, BSP_DELAY_UNITS_MILLISECONDS);
    nShutdown_high();
    R_BSP_SoftwareDelay( 250, BSP_DELAY_UNITS_MILLISECONDS);
}

void user_uart_callback(uart_callback_args_t *p_args){
    switch (p_args->event){
        case UART_EVENT_TX_DATA_EMPTY:
            (*tx_done_handler)();
            break;
        case UART_EVENT_RX_CHAR:
            if (rx_len > 0){
                *rx_buffer++ = (uint8_t) p_args->data;
                rx_len--;
                if (rx_len == 0) {
                    g_ioport.p_api->pinWrite(rts_pin, IOPORT_LEVEL_HIGH);
                    (*rx_done_handler)();
                }
            } else {
                // store in ring buffer
                uint8_t data = (uint8_t) p_args->data;
                btstack_ring_buffer_write(&rx_ring_buffer, &data, 1);
            }
            break;
        case UART_EVENT_ERR_RXBUF_OVERFLOW:
            log_info("UART_EVENT_ERR_RXBUF_OVERFLOW");
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
    // TODO: configure CTS GPIO as edge falling edge trigger
    cts_irq_handler = the_irq_handler;
}

int  hal_uart_dma_set_baud(uint32_t baud){
    ssp_err_t error = g_uart0.p_api->baudSet(g_uart0.p_ctrl, baud);
    if (error != SSP_SUCCESS){
        log_error("hal_uart_dma_set_baud error 0x%x", error);
    }
    return 0;
}

void hal_uart_dma_send_block(const uint8_t *data, uint16_t size){
    g_uart0.p_api->write(g_uart0.p_ctrl, data, size);
}

void hal_uart_dma_receive_block(uint8_t *data, uint16_t size){
    // fill from  ring buffer
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
    g_ioport.p_api->pinWrite(rts_pin, IOPORT_LEVEL_LOW);
}

// actual port

#include "bluetooth.h"
#include "bluetooth_company_id.h"
#include "btstack_chipset_cc256x.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "hci_dump_embedded_stdout.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"
#include "btstack_memory.h"
#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hal_flash_bank_synergy.h"

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

// port.c
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;

static hal_flash_bank_synergy_t  hal_flash_bank_context;
#define HAL_FLASH_BANK_SIZE     ( 10224 )
#define HAL_FLASH_BANK_0_ADDR   ( 0x40100000 )
#define HAL_FLASH_BANK_1_ADDR   ( 0x40100400 )

static const hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    2000000,
    1,
    NULL
};

int btstack_main(int argc, const char * argv[]);
void hal_entry(void) {

    // init hal
    g_hal_init();

    // open uart, timer, flash
    g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg);
    g_timer0.p_api->open(g_timer0.p_ctrl, g_timer0.p_cfg);
    g_flash0.p_api->open(g_flash0.p_ctrl, g_flash0.p_cfg);

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // enable HCI logging
    // hci_dump_init(hci_dump_embedded_stdout_get_instance());

    // init HCI
    hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), (void*) &config);
    hci_set_chipset(btstack_chipset_cc256x_instance());

    // setup TLV Flash Sector implementation
    const hal_flash_bank_t * hal_flash_bank_impl = hal_flash_bank_synergy_init_instance(
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
