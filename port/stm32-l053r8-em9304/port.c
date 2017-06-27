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
#include "hci_dump.h"
#include "btstack_debug.h"

// retarget printf
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

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

#define SPI_EM9304_RX_BUFFER_SIZE 64

static uint8_t  hal_spi_em9304_slave_status[2];

static uint8_t  hal_spi_em9304_rx_buffer[SPI_EM9304_RX_BUFFER_SIZE];
static uint16_t hal_spi_em9304_rx_pos;

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
    // run_loop_triggered = 1;
}

static inline int hal_spi_em9304_rdy(void){
    return HAL_GPIO_ReadPin(SPI1_RDY_GPIO_Port, SPI1_RDY_Pin) == GPIO_PIN_SET;
}

static inline uint16_t hal_spi_em9304_rx_free_bytes(void){
    return SPI_EM9304_RX_BUFFER_SIZE - hal_spi_em9304_rx_pos;
}

static void hal_spi_em9304_reset(void){
    hal_spi_em9304_rx_pos = 0;
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
    log_debug("transfer_rx_data: spi rx buffer has %u -> hci buffer needs %u", hal_spi_em9304_rx_pos, hal_uart_dma_rx_len);
    while (hal_spi_em9304_rx_pos && hal_uart_dma_rx_len){
        uint16_t bytes_to_copy = hal_uart_dma_rx_len;
        if (hal_uart_dma_rx_len > hal_spi_em9304_rx_pos){
            bytes_to_copy = hal_spi_em9304_rx_pos;
        }
        memcpy(hal_uart_dma_rx_buffer, hal_spi_em9304_rx_buffer, bytes_to_copy);
        hal_uart_dma_rx_buffer += bytes_to_copy;
        hal_uart_dma_rx_len    -= bytes_to_copy;
        hal_spi_em9304_rx_pos  -= bytes_to_copy;

        // shift rest of data - could be skipped if ring buffer is used
        if (hal_spi_em9304_rx_pos){
            log_debug("transfer_rx_data: move %u bytes down", hal_spi_em9304_rx_pos);
            memmove(hal_spi_em9304_rx_buffer, &hal_spi_em9304_rx_buffer[bytes_to_copy], hal_spi_em9304_rx_pos);
        }

        if (hal_uart_dma_rx_len == 0){
            (*rx_done_handler)();
        }
    }
}

static void hal_spi_em9304_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    (void) ds;
    (void) callback_type;

    uint16_t bytes_to_read;
    uint16_t bytes_ready;
    uint16_t max_bytes_to_send;
    uint16_t bytes_to_send;

    switch (hal_spi_em9304_state){
        case SPI_EM9304_IDLE:
            // RDY && space in RX Buffer
            if (hal_spi_em9304_rdy() && hal_spi_em9304_rx_free_bytes() && hal_uart_dma_rx_len){
            // if (hal_spi_em9304_rdy() && hal_spi_em9304_rx_free_bytes()){
                // chip select
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);

                // wait for read command sent
                hal_spi_em9304_state = SPI_EM9304_RX_W4_READ_COMMAND_SENT;

                // send read command
                HAL_SPI_TransmitReceive_DMA(&hspi1, (uint8_t*) hal_spi_em9304_read_command, hal_spi_em9304_slave_status, sizeof(hal_spi_em9304_read_command));
            } else if (hal_uart_dma_tx_size){
                // chip select
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);
    
                // wait for RDY
                hal_spi_em9304_state = SPI_EM9304_TX_W4_RDY;
            }
            break;

        case SPI_EM9304_RX_READ_COMMAND_SENT:
            // check slave status
            log_debug("RX: STS1 0x%02X, STS2 0x%02X", hal_spi_em9304_slave_status[0], hal_spi_em9304_slave_status[1]);
            if ((hal_spi_em9304_slave_status[0] != STS_SLAVE_READY)){
                // chip deselect
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
                // retry
                hal_spi_em9304_state = SPI_EM9304_IDLE;
                break;
            }

            bytes_ready = hal_spi_em9304_slave_status[1];
            bytes_to_read = bytes_ready;
            if (bytes_to_read > hal_spi_em9304_rx_free_bytes()){
                bytes_to_read = hal_spi_em9304_rx_free_bytes();
            }

            // wait for data received
            hal_spi_em9304_state = SPI_EM9304_RX_W4_DATA_RECEIVED;

            // read all data
            HAL_SPI_Receive_DMA(&hspi1, &hal_spi_em9304_rx_buffer[hal_spi_em9304_rx_pos], bytes_to_read);
            hal_spi_em9304_rx_pos += bytes_to_read;
            break;

        case SPI_EM9304_RX_DATA_RECEIVED:
            // chip deselect
            HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);

            // done
            hal_spi_em9304_state = SPI_EM9304_IDLE;

            // transfer data
            hal_spi_em9304_transfer_rx_data();
            break;

        case SPI_EM9304_TX_W4_RDY:
            if (!hal_spi_em9304_rdy()) break;

            // wait for write command sent
            hal_spi_em9304_state = SPI_EM9304_TX_W4_WRITE_COMMAND_SENT;

            // send write command
            HAL_SPI_TransmitReceive_DMA(&hspi1, (uint8_t*) hal_spi_em9304_write_command, hal_spi_em9304_slave_status, sizeof(hal_spi_em9304_write_command));

            break;

        case SPI_EM9304_TX_WRITE_COMMAND_SENT:

            // check slave status and rx buffer space
            log_debug("TX: STS1 0x%02X, STS2 0x%02X", hal_spi_em9304_slave_status[0], hal_spi_em9304_slave_status[1]);
            max_bytes_to_send = hal_spi_em9304_slave_status[1];
            if ((hal_spi_em9304_slave_status[0] != STS_SLAVE_READY) || (max_bytes_to_send == 0)){
                // chip deselect
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
                // retry
                hal_spi_em9304_state = SPI_EM9304_IDLE;
                break;
            }

            bytes_to_send = hal_uart_dma_tx_size;
            if (bytes_to_send > max_bytes_to_send){
                bytes_to_send = max_bytes_to_send;
            }

            // wait for tx data sent
            hal_spi_em9304_state = SPI_EM9304_TX_W4_DATA_SENT;

            // send command
            HAL_SPI_Transmit_DMA(&hspi1, (uint8_t*) hal_uart_dma_tx_data, bytes_to_send);
            hal_uart_dma_tx_size -= bytes_to_send;
            break;

        case SPI_EM9304_TX_DATA_SENT:
            // chip deselect
            HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);

            hal_spi_em9304_state = SPI_EM9304_IDLE;
            (*tx_done_handler)();
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
    log_debug("hal_uart_dma_receive_block: len %u", length);
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

    hci_dump_open( NULL, HCI_DUMP_STDOUT );

    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&transport_data_source, &hal_spi_em9304_process);
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&transport_data_source);

    // init HCI
    hci_init(hci_transport_h4_instance(btstack_uart_block_embedded_instance()), &config);

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