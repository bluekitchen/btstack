/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "hci_transport_h4.c"

/*
 *  port.c
 * 
 *  BTstack port for the EM9304 Development Kit consisting of an
 *  - STM32 Nucleo L053 Board with an
 *  - EM9304 Bluetooth Controller in the default SPI Slave configuration
 */

#include <string.h>
#include "stm32l0xx_hal.h"
#include "port.h"
#include "main.h"   // pin definitions

#include "bluetooth.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_ring_buffer.h"
#include "hci_dump.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"
#include "hci_transport_em9304_spi.h"
#include "btstack_debug.h"
#include "btstack_chipset_em9301.h"

#ifdef ENABLE_SEGGER_RTT
#include "SEGGER_RTT.h"
#include "hci_dump_segger_rtt_stdout.h"
#else
#include "hci_dump_embedded_stdout.h"
#endif

// retarget printf
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#ifndef ENABLE_SEGGER_RTT

int _write(int file, char *ptr, int len){
    uint8_t cr = '\r';
    if (file == STDOUT_FILENO || file == STDERR_FILENO) {
        int i;
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
}

#endif

int _read(int file, char * ptr, int len){
    (void)(file);
    (void)(ptr);
    (void)(len);
    return -1;
}

int _close(int file){
    (void)(file);
    return -1;
}

int _isatty(int file){
    (void)(file);
    return -1;
}

int _lseek(int file){
    (void)(file);
    return -1;
}

int _fstat(int file){
    (void)(file);
    return -1;
}

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

// EM 9304 SPI Master HCI Implementation

#define STS_SLAVE_READY 0xc0

// SPI Write Command
static const uint8_t hal_spi_em9304_write_command[] = {
    0x42,
    0x00,
};

// SPI Read Command
static const uint8_t hal_spi_em9304_read_command[] = {
    0x81,
    0x00,
};

const uint8_t hci_reset_2[] = { 0x01, 0x03, 0x0c, 0x00 };

static volatile enum {
    SPI_EM9304_IDLE,
    SPI_EM9304_RX_W4_READ_COMMAND_SENT,
    SPI_EM9304_RX_READ_COMMAND_SENT,
    SPI_EM9304_RX_W4_DATA_RECEIVED,
    SPI_EM9304_RX_DATA_RECEIVED,
    SPI_EM9304_TX_W4_RDY,
    SPI_EM9304_TX_W4_WRITE_COMMAND_SENT,
    SPI_EM9304_TX_WRITE_COMMAND_SENT,
    SPI_EM9304_TX_W4_DATA_SENT,
    SPI_EM9304_TX_DATA_SENT,
} hal_spi_em9304_state;

#define SPI_EM9304_RX_BUFFER_SIZE     64
#define SPI_EM9304_TX_BUFFER_SIZE     64
#define SPI_EM9304_RING_BUFFER_SIZE  128

static uint8_t  hal_spi_em9304_slave_status[2];

static const uint8_t hal_spi_em9304_zeros[SPI_EM9304_TX_BUFFER_SIZE];

static uint8_t  hal_spi_em9304_rx_buffer[SPI_EM9304_RX_BUFFER_SIZE];
static uint16_t hal_spi_em9304_rx_request_len;
static uint16_t hal_spi_em9304_tx_request_len;

static btstack_ring_buffer_t hal_uart_dma_rx_ring_buffer;
static uint8_t hal_uart_dma_rx_ring_buffer_storage[SPI_EM9304_RING_BUFFER_SIZE];

static const uint8_t  * hal_uart_dma_tx_data;
static uint16_t         hal_uart_dma_tx_size;

static uint8_t  * hal_uart_dma_rx_buffer;
static uint16_t   hal_uart_dma_rx_len;

static void dummy_handler(void);

// handlers
static void (*rx_done_handler)(void) = &dummy_handler;
static void (*tx_done_handler)(void) = &dummy_handler;

static inline void hal_spi_em9304_trigger_run_loop(void){
    btstack_run_loop_embedded_trigger();
}

static inline int hal_spi_em9304_rdy(void){
    return HAL_GPIO_ReadPin(SPI1_RDY_GPIO_Port, SPI1_RDY_Pin) == GPIO_PIN_SET;
}

static void hal_spi_em9304_reset(void){
    btstack_ring_buffer_init(&hal_uart_dma_rx_ring_buffer, &hal_uart_dma_rx_ring_buffer_storage[0], SPI_EM9304_RING_BUFFER_SIZE);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi){
    switch (hal_spi_em9304_state){
        case SPI_EM9304_RX_W4_READ_COMMAND_SENT:
            hal_spi_em9304_state = SPI_EM9304_RX_READ_COMMAND_SENT;
            hal_spi_em9304_trigger_run_loop();
            break;
        case SPI_EM9304_TX_W4_WRITE_COMMAND_SENT:
            hal_spi_em9304_state = SPI_EM9304_TX_WRITE_COMMAND_SENT;
            hal_spi_em9304_trigger_run_loop();
            break;
        case SPI_EM9304_RX_W4_DATA_RECEIVED:
            hal_spi_em9304_state = SPI_EM9304_RX_DATA_RECEIVED;
            hal_spi_em9304_trigger_run_loop();
            break;
        default:
            break;
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi){
    switch (hal_spi_em9304_state){
        case SPI_EM9304_RX_W4_DATA_RECEIVED:
            hal_spi_em9304_state = SPI_EM9304_RX_DATA_RECEIVED;
            hal_spi_em9304_trigger_run_loop();
            break;
        default:
            break;
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi){
    switch (hal_spi_em9304_state){
        case SPI_EM9304_TX_W4_DATA_SENT:
            hal_spi_em9304_state = SPI_EM9304_TX_DATA_SENT;
            hal_spi_em9304_trigger_run_loop();
            break;
        default:
            break;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
    if (hal_spi_em9304_rdy()){
        hal_spi_em9304_trigger_run_loop();
    }
}

static void hal_spi_em9304_transfer_rx_data(void){
    while (1){
        int bytes_available = btstack_ring_buffer_bytes_available(&hal_uart_dma_rx_ring_buffer);
        log_debug("transfer_rx_data: ring buffer has %u -> hci buffer needs %u", bytes_available, hal_uart_dma_rx_len);

        if (!bytes_available) return;
        if (!hal_uart_dma_rx_len) return;

        int bytes_to_copy = btstack_min(bytes_available, hal_uart_dma_rx_len);
        uint32_t bytes_read;
        btstack_ring_buffer_read(&hal_uart_dma_rx_ring_buffer, hal_uart_dma_rx_buffer, bytes_to_copy, &bytes_read);
        hal_uart_dma_rx_buffer += bytes_read;
        hal_uart_dma_rx_len    -= bytes_read;

        if (hal_uart_dma_rx_len == 0){
            (*rx_done_handler)();
        }
    }
}

static void hal_spi_em9304_start_tx_transaction(void){
    // wait for RDY
    hal_spi_em9304_state = SPI_EM9304_TX_W4_RDY;

    // chip select
    HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);
}

static void hal_spi_em9304_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    (void) ds;
    (void) callback_type;

    uint16_t max_bytes_to_send;

    switch (hal_spi_em9304_state){
        case SPI_EM9304_IDLE:
            // RDY && space in RX Buffer
            if (hal_spi_em9304_rdy() 
            && (btstack_ring_buffer_bytes_free(&hal_uart_dma_rx_ring_buffer) >= SPI_EM9304_RX_BUFFER_SIZE) ){

                // chip select
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);

                // send read command
                hal_spi_em9304_state = SPI_EM9304_RX_W4_READ_COMMAND_SENT;
                HAL_SPI_TransmitReceive_DMA(&hspi1, (uint8_t*) hal_spi_em9304_read_command, hal_spi_em9304_slave_status, sizeof(hal_spi_em9304_read_command));

            } else if (hal_uart_dma_tx_size){
                hal_spi_em9304_start_tx_transaction();
            }
            break;

        case SPI_EM9304_RX_READ_COMMAND_SENT:
            // check slave status
            log_debug("RX: STS1 0x%02X, STS2 0x%02X", hal_spi_em9304_slave_status[0], hal_spi_em9304_slave_status[1]);

            // check if ready
            if ((hal_spi_em9304_slave_status[0] != STS_SLAVE_READY)){
                // chip deselect & retry
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
                hal_spi_em9304_state = SPI_EM9304_IDLE;
                break;
            }

            // read data and send '0's
            hal_spi_em9304_state = SPI_EM9304_RX_W4_DATA_RECEIVED;
            hal_spi_em9304_rx_request_len = hal_spi_em9304_slave_status[1];
            HAL_SPI_TransmitReceive_DMA(&hspi1, (uint8_t*) hal_spi_em9304_zeros, &hal_spi_em9304_rx_buffer[0], hal_spi_em9304_rx_request_len);
            break;

        case SPI_EM9304_RX_DATA_RECEIVED:

            // chip deselect & done
            HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
            hal_spi_em9304_state = SPI_EM9304_IDLE;

            // move data into ring buffer
            btstack_ring_buffer_write(&hal_uart_dma_rx_ring_buffer, hal_spi_em9304_rx_buffer, hal_spi_em9304_rx_request_len);
            hal_spi_em9304_rx_request_len = 0;

            // deliver new data
            hal_spi_em9304_transfer_rx_data();
            break;

        case SPI_EM9304_TX_W4_RDY:
            // check if ready
            if (!hal_spi_em9304_rdy()) break;

            // send write command
            hal_spi_em9304_state = SPI_EM9304_TX_W4_WRITE_COMMAND_SENT;
            HAL_SPI_TransmitReceive_DMA(&hspi1, (uint8_t*) hal_spi_em9304_write_command, hal_spi_em9304_slave_status, sizeof(hal_spi_em9304_write_command));
            break;

        case SPI_EM9304_TX_WRITE_COMMAND_SENT:

            // check slave status and em9304 rx buffer space
            log_debug("TX: STS1 0x%02X, STS2 0x%02X", hal_spi_em9304_slave_status[0], hal_spi_em9304_slave_status[1]);
            max_bytes_to_send = hal_spi_em9304_slave_status[1];
            if ((hal_spi_em9304_slave_status[0] != STS_SLAVE_READY) || (max_bytes_to_send == 0)){
                // chip deselect & retry
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
                hal_spi_em9304_state = SPI_EM9304_IDLE;
                break;
            }

            // number bytes to send
            hal_spi_em9304_tx_request_len = btstack_min(hal_uart_dma_tx_size, max_bytes_to_send);

            // send command
            hal_spi_em9304_state = SPI_EM9304_TX_W4_DATA_SENT;
            HAL_SPI_Transmit_DMA(&hspi1, (uint8_t*) hal_uart_dma_tx_data, hal_spi_em9304_tx_request_len);
            break;

        case SPI_EM9304_TX_DATA_SENT:

            // chip deselect & done
            HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
            hal_spi_em9304_state = SPI_EM9304_IDLE;

            // chunk processed
            hal_uart_dma_tx_size -= hal_spi_em9304_tx_request_len;
            hal_uart_dma_tx_data += hal_spi_em9304_tx_request_len;
            hal_spi_em9304_tx_request_len = 0;

            // handle TX Complete
            if (hal_uart_dma_tx_size){
                // more data to send
                hal_spi_em9304_start_tx_transaction();
            } else {
                // notify higher layer
                (*tx_done_handler)();
            }
            break;

        default:
            break;
    }
}

//
// #include "hal_uart_dma.h"

static void dummy_handler(void){};

void hal_spi_em9304_power_cycle(void){
    HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_SET);
}

void hal_uart_dma_init(void){
    hal_spi_em9304_power_cycle();
    hal_spi_em9304_reset();
}

void hal_uart_dma_set_block_received( void (*the_block_handler)(void)){
    rx_done_handler = the_block_handler;
}

void hal_uart_dma_set_block_sent( void (*the_block_handler)(void)){
    tx_done_handler = the_block_handler;
}

int  hal_uart_dma_set_baud(uint32_t baud){
    return 0;
}

void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t length){
    hal_uart_dma_tx_data = buffer;
    hal_uart_dma_tx_size = length;
    hal_spi_em9304_process(NULL, 0);
}

void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t length){
    log_debug("hal_uart_dma_receive_block: len %u, ring buffer has %u, UART_RX_LEN %u", length, btstack_ring_buffer_bytes_available(&hal_uart_dma_rx_ring_buffer), hal_uart_dma_rx_len);
    hal_uart_dma_rx_buffer = buffer;
    hal_uart_dma_rx_len    = length;
    hal_spi_em9304_transfer_rx_data();
    hal_spi_em9304_process(NULL, 0);
}

void hal_uart_dma_set_csr_irq_handler( void (*csr_irq_handler)(void)){
}

void hal_uart_dma_set_sleep(uint8_t sleep){
}

// dummy config
static const hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    4000000,
    1,
    NULL
};

static btstack_packet_callback_registration_t hci_event_callback_registration;
int btstack_main(int argc, char ** argv);

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
        default:
            break;
    }
}

// data source to keep SPI transport working
static btstack_data_source_t transport_data_source;

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

    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&transport_data_source, &hal_spi_em9304_process);
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&transport_data_source);

    // init HCI
    hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), &config);
    hci_set_chipset(btstack_chipset_em9301_instance());

#if 0
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
#endif

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // hand over to btstack embedded code
    btstack_main(0, NULL);

    // go
    btstack_run_loop_execute();
}
