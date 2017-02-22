#include "hal_uart_dma.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bt.h"

void dummy_handler(void){};

// handlers
static void (*rx_done_handler)(void) = dummy_handler;
static void (*tx_done_handler)(void) = dummy_handler;

static uint8_t _data[1024];
static uint16_t _data_len;

static void host_send_pkt_available_cb(void)
{
    printf("host_send_pkt_available_cb\n");
}

static int host_recv_pkt_cb(uint8_t *data, uint16_t len)
{
    printf("host_recv_pkt_cb: len = %u, data = [", len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("]\n");
    memcpy(_data, data, len);
    _data_len = len;
    rx_done_handler();
    return 0;
}

static const esp_vhci_host_callback_t vhci_host_cb = {
    .notify_host_send_available = host_send_pkt_available_cb,
    .notify_host_recv = host_recv_pkt_cb,
};

void hal_uart_dma_init(void){
    printf("hal_uart_dma_init\n");
    esp_vhci_host_register_callback(&vhci_host_cb);
}

void hal_uart_dma_set_block_received( void (*block_handler)(void)){
    printf("hal_uart_dma_set_block_received\n");
    rx_done_handler = block_handler;
}

void hal_uart_dma_set_block_sent( void (*block_handler)(void)){
    printf("hal_uart_dma_set_block_sent\n");
    tx_done_handler = block_handler;
}

int hal_uart_dma_set_baud(uint32_t baud){
    printf("hal_uart_dma_set_baud\n");
    return -1;
}

void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t length){
    printf("hal_uart_dma_send_block\n");
    esp_vhci_host_send_packet(buffer, length);
    tx_done_handler();
}

void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len){
    printf("hal_uart_dma_receive_block\n");
    memcpy(buffer, _data, _data_len);
}

