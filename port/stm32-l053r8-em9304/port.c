#include <string.h>
#include "stm32l0xx_hal.h"
#include "port.h"
#include "main.h"   // pin definitions

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

const uint8_t hci_reset[] = { 0x01, 0x03, 0x0c, 0x00 };

static enum {
    SPI_EM9304_IDLE,
    SPI_EM9304_RX_W4_READ_COMMAND_SENT,
    SPI_EM9304_RX_W4_DATA_RECEIVED,
    SPI_EM9304_TX_W4_RDY,
    SPI_EM9304_TX_W4_WRITE_COMMAND_SENT,
    SPI_EM9304_TX_W4_DATA_SENT,
} hal_spi_em9304_state;

#define SPI_EM9304_RX_BUFFER_SIZE 64

static uint8_t  hal_spi_em9304_slave_status[2];

static uint8_t  hal_spi_em9304_rx_buffer[SPI_EM9304_RX_BUFFER_SIZE];
static uint16_t hal_spi_em9304_rx_pos;

static const uint8_t  * hal_spi_em9304_tx_data;
static uint16_t   hal_spi_em9304_tx_size;

// test
static int test_done;
static int event_received;
static int command_sent;

static inline int hal_spi_em9304_rdy(void){
    return HAL_GPIO_ReadPin(SPI1_RDY_GPIO_Port, SPI1_RDY_Pin) == GPIO_PIN_SET;
}

static inline uint16_t hal_spi_em9304_rx_free_bytes(void){
    return SPI_EM9304_RX_BUFFER_SIZE - hal_spi_em9304_rx_pos;
}

static void hal_spi_em9304_reset(void){
    hal_spi_em9304_rx_pos = 0;
}

static void hal_spi_em9304_process(void){
    uint16_t bytes_to_read;
    uint16_t bytes_ready;
    uint16_t max_bytes_to_send;
    uint16_t bytes_to_send;
    int i;

    switch (hal_spi_em9304_state){
        case SPI_EM9304_IDLE:
            // RDY && space in RX Buffer
            if (hal_spi_em9304_rdy() && hal_spi_em9304_rx_free_bytes()){
                // chip select
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);

                // send read command
                HAL_SPI_TransmitReceive(&hspi1, (uint8_t*) hal_spi_em9304_read_command, hal_spi_em9304_slave_status, sizeof(hal_spi_em9304_read_command), HAL_MAX_DELAY);

                // wait for read command sent
                hal_spi_em9304_state = SPI_EM9304_RX_W4_READ_COMMAND_SENT;
            }
            if (hal_spi_em9304_tx_size){
                // chip select
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);
    
                // wait for RDY
                hal_spi_em9304_state = SPI_EM9304_TX_W4_RDY;              
            }
            break;

        case SPI_EM9304_RX_W4_READ_COMMAND_SENT:
            bytes_ready = hal_spi_em9304_slave_status[1];
            bytes_to_read = bytes_ready;
            if (bytes_to_read > hal_spi_em9304_rx_free_bytes()){
                bytes_to_read = hal_spi_em9304_rx_free_bytes();
            }
            // read all data
            HAL_SPI_Receive(&hspi1, &hal_spi_em9304_rx_buffer[hal_spi_em9304_rx_pos], bytes_to_read, HAL_MAX_DELAY);
            hal_spi_em9304_rx_pos += bytes_to_read;
            hal_spi_em9304_state = SPI_EM9304_RX_W4_DATA_RECEIVED;
            break;

        case SPI_EM9304_RX_W4_DATA_RECEIVED:
            // chip deselect
            HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);

            // done
            printf("EVT: ");
            for (i=0;i<hal_spi_em9304_rx_pos;i++){
                printf("%02x ", hal_spi_em9304_rx_buffer[i]);
            }
            printf("\n");
            hal_spi_em9304_state = SPI_EM9304_IDLE;
            event_received = 1;
            break;

        case SPI_EM9304_TX_W4_RDY:
            if (!hal_spi_em9304_rdy()) break;
            // send write command
            HAL_SPI_TransmitReceive(&hspi1, (uint8_t*) hal_spi_em9304_write_command, hal_spi_em9304_slave_status, sizeof(hal_spi_em9304_write_command), HAL_MAX_DELAY);

            // wait for write command sent
            hal_spi_em9304_state = SPI_EM9304_TX_W4_WRITE_COMMAND_SENT;
            break;

        case SPI_EM9304_TX_W4_WRITE_COMMAND_SENT:
            printf("TX: STS1 0x%02X, STS2 0x%02X\n", hal_spi_em9304_slave_status[0], hal_spi_em9304_slave_status[1]);

            // check slave status and rx buffer space
            max_bytes_to_send = hal_spi_em9304_slave_status[1];
            if ((hal_spi_em9304_slave_status[0] != STS_SLAVE_READY) || (max_bytes_to_send == 0)){
                // chip deselect
                HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
                // retry
                hal_spi_em9304_state = SPI_EM9304_IDLE;
            }

            bytes_to_send = hal_spi_em9304_tx_size;
            if (bytes_to_send > max_bytes_to_send){
                bytes_to_send = max_bytes_to_send;
            }

            // send command
            HAL_SPI_Transmit(&hspi1, (uint8_t*) hal_spi_em9304_tx_data, bytes_to_send, HAL_MAX_DELAY);
            hal_spi_em9304_tx_size -= bytes_to_send;

            hal_spi_em9304_state = SPI_EM9304_TX_W4_DATA_SENT;
            break;

        case SPI_EM9304_TX_W4_DATA_SENT:
            // chip deselect
            HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);

            hal_spi_em9304_state = SPI_EM9304_IDLE;
            command_sent = 1;
            break;

        default:
            break;
    }
}

void port_main(void){
    while (1){
        // setp 1: reset
        printf("1. Reset: \n");
        HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_RESET);
        HAL_Delay(10);
        HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_SET);

        // GO
        hal_spi_em9304_reset();
        int step = 0;
        while (!test_done){
            hal_spi_em9304_process();
            switch (step){
                case 0:
                    if (!event_received) continue;
                    event_received = 0;
                    printf("Active State Entered\n");
                    hal_spi_em9304_reset();
                    //
                    hal_spi_em9304_tx_data = hci_reset;
                    hal_spi_em9304_tx_size = sizeof(hci_reset);
                    step++;
                    break;
                case 1:
                    if (!command_sent) continue;
                    command_sent = 0;
                    step++;
                    break;
                case 2:
                    if (!event_received) continue;
                    event_received = 0;
                    hal_spi_em9304_reset();
                    printf("HCI Command Complete Event Received\n");
                    test_done = 1;
                    break;                
            }
        }
        test_done = 0;

        HAL_Delay(1000);
    }
}