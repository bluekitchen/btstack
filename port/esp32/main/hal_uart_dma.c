#include "hal_uart_dma.h"

#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"

/* VHCI function interface */
typedef struct vhci_host_callback {
    void (*notify_host_send_available)(void);               /*!< callback used to notify that the host can send packet to controller */
    int (*notify_host_recv)(uint8_t *data, uint16_t len);   /*!< callback used to notify that the controller has a packet to send to the host*/
} vhci_host_callback_t;

extern bool API_vhci_host_check_send_available(void);
extern void API_vhci_host_send_packet(uint8_t *data, uint16_t len);
extern void API_vhci_host_register_callback(const vhci_host_callback_t *callback);

void hal_uart_dma_init(void){
    printf("hal_uart_dma_init\n");
}

void hal_uart_dma_set_block_received( void (*block_handler)(void)){
    printf("hal_uart_dma_set_block_received\n");
}

void hal_uart_dma_set_block_sent( void (*block_handler)(void)){
    printf("hal_uart_dma_set_block_sent\n");
}

void hal_uart_dma_set_csr_irq_handler( void (*csr_irq_handler)(void)){
    printf("hal_uart_dma_set_csr_irq_handler\n");
}

int hal_uart_dma_set_baud(uint32_t baud){
    printf("hal_uart_dma_set_baud\n");
    return -1;
}

void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t length){
    printf("hal_uart_dma_send_block\n");
    API_vhci_host_send_packet(buffer, length);
}

void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len){
    printf("hal_uart_dma_receive_block\n");
}

void hal_uart_dma_set_sleep(uint8_t sleep){
    printf("hal_uart_dma_set_sleep\n");
}
