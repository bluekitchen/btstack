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

#define STS_SLAVE_READY 0xc0

// SPI Write Command
static const uint8_t write_command[] = {
    0x42,
    0x00,
};

// SPI Read Command
static const uint8_t read_command[] = {
    0x81,
    0x00,
};

const uint8_t hci_reset[] = { 0x01, 0x03, 0x0c, 0x00 };

void read_event(void){
    uint8_t status[2];
    uint8_t event[10];

    // wait for RDY
    HAL_GPIO_WritePin(DEBUG_0_GPIO_Port, DEBUG_0_Pin, GPIO_PIN_SET);
    // printf("EVT: Wait for ready\n");
    while (HAL_GPIO_ReadPin(SPI1_RDY_GPIO_Port, SPI1_RDY_Pin) == GPIO_PIN_RESET){};
    HAL_GPIO_WritePin(DEBUG_0_GPIO_Port, DEBUG_0_Pin, GPIO_PIN_RESET);

    // chip select
    printf("EVT: Select\n");
    HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);

    // send read command
    printf("EVT: Send read request\n");
    HAL_SPI_TransmitReceive(&hspi1, (uint8_t*) read_command, status, sizeof(read_command), HAL_MAX_DELAY);

    printf("EVT: STS1 0x%02X, STS2 0x%02X\n", status[0], status[1]);

    // read all data
    HAL_SPI_Receive(&hspi1, &event[0], status[1], HAL_MAX_DELAY);

    HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);

    // dump
    int i;
    for (i=0;i<status[1];i++){
        printf("%02x ", event[i]);
    }
    printf("\n");

    printf("EVT: Done\n");
}

void port_main(void){
    HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);
    uint8_t status[2];

    while (1){

        printf("Reset: \n");
        HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_RESET);
        HAL_Delay(10);
        HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_SET);

        read_event();
        
        // chip select
        printf("CMD: Select\n");
        HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_RESET);

        // wait for RDY
        HAL_GPIO_WritePin(DEBUG_0_GPIO_Port, DEBUG_0_Pin, GPIO_PIN_SET);
        // printf("CMD: Wait for ready\n");
        while (HAL_GPIO_ReadPin(SPI1_RDY_GPIO_Port, SPI1_RDY_Pin) == GPIO_PIN_RESET){};
        HAL_GPIO_WritePin(DEBUG_0_GPIO_Port, DEBUG_0_Pin, GPIO_PIN_RESET);

        printf("CMD: Send write request\n");

        // send write command
        HAL_SPI_TransmitReceive(&hspi1, (uint8_t*) write_command, status, sizeof(write_command), HAL_MAX_DELAY);

        printf("CMD: STS1 0x%02X, STS2 0x%02X\n", status[0], status[1]);

        // check slave status
        if (status[0] != STS_SLAVE_READY) continue;

        printf("CMD: Slave ready\n");

        // check size
        if (status[1] < sizeof(hci_reset)) continue;

        printf("CMD: Enough buffer, send HCI Reset\n");

        // send command
        HAL_SPI_Transmit(&hspi1, (uint8_t*) hci_reset, sizeof(hci_reset), HAL_MAX_DELAY);

        // done
        HAL_GPIO_WritePin(SPI1_CSN_GPIO_Port, SPI1_CSN_Pin, GPIO_PIN_SET);

        printf("CMD: Done\n");

        read_event();
    }
}