#include <assert.h>
#include <stdatomic.h>

#include "btstack_debug.h"
#include "hal_uart_dma.h"

#include "btstack_defines.h"
#include "btstack_util.h"

#ifndef HAVE_HAL_UART_BUFFERS
#error "The ESP32 UART HAL buffers data in hardware FIFOs and must be built with HAVE_HAL_UART_BUFFERS enabled in btstack_config.h."
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "hal/uart_ll.h"
#include "soc/uart_periph.h"
#include "soc/uart_struct.h"

#define UART_NO                  (CONFIG_BTSTACK_UART_NUM)

#define UART_TX_PIN              (CONFIG_BTSTACK_UART_TX_PIN)
#define UART_RX_PIN              (CONFIG_BTSTACK_UART_RX_PIN)
#define UART_RTS_PIN             (CONFIG_BTSTACK_UART_RTS_PIN)
#define UART_CTS_PIN             (CONFIG_BTSTACK_UART_CTS_PIN)
#define UART_NRESET              (CONFIG_BTSTACK_UART_NRESET_PIN)

#define HAL_UART_DMA_RX_FLOW_CTRL_THRESHOLD   120u
#define HAL_UART_DMA_RX_THRESHOLD              (HAL_UART_DMA_RX_FLOW_CTRL_THRESHOLD / 2)

#if HAL_UART_DMA_RX_FLOW_CTRL_THRESHOLD <= HAL_UART_DMA_RX_THRESHOLD
#error "HAL_UART_DMA_RX_FLOW_CTRL_THRESHOLD must be larger than HAL_UART_DMA_RX_THRESHOLD to prevent hang"
#endif

static uart_config_t uart_config = {
    .source_clk = UART_SCLK_DEFAULT,
    .baud_rate  = 1000000,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_CTS_RTS,
    .rx_flow_ctrl_thresh = HAL_UART_DMA_RX_FLOW_CTRL_THRESHOLD,
};

typedef void (*callback_t)();
static callback_t receive_callback;
static callback_t send_callback;

static const char *TAG = "hal_uart";

typedef struct {
    uint8_t *buf;
    uint16_t nbytes;
} io_cb_t;

static io_cb_t rx_transfer;
static io_cb_t tx_transfer;

/*
 * Receive path:
 * - hal_uart_dma_receive_block() stores the target buffer/length and first drains any
 *   bytes already present in the hardware RX FIFO.
 * - If the block is still incomplete, the driver programs RXFIFO_FULL to
 *   min(HAL_UART_DMA_RX_THRESHOLD, remaining_bytes) and enables RX interrupts.
 * - The ISR drains FIFO contents into rx_transfer until the full block has been copied,
 *   then it disables RX interrupts and calls receive_callback().
 *
 * The RX threshold must stay below the hardware flow-control threshold. Otherwise RTS
 * could stop the remote sender before RXFIFO_FULL fires, which would leave bytes queued
 * in the FIFO without triggering the ISR to drain them.
 */
#define HAL_UART_DMA_RX_INTS           UART_RXFIFO_FULL_INT_ENA_M
#define HAL_UART_DMA_RX_INT_CLEARS     UART_RXFIFO_FULL_INT_CLR_M

static void IRAM_ATTR hal_uart_dma_fill_tx_fifo(uart_dev_t *uart) {
    uint16_t space = uart_ll_get_txfifo_len(uart);
    uint16_t chunk = (uint16_t) btstack_min(space, tx_transfer.nbytes);
    if (chunk > 0) {
        uart_ll_write_txfifo(uart, tx_transfer.buf, chunk);
        tx_transfer.nbytes -= chunk;
        tx_transfer.buf    += chunk;
    }
}

static void IRAM_ATTR hal_uart_dma_read_rx_fifo(uart_dev_t *uart) {
    uint16_t available = uart_ll_get_rxfifo_len(uart);
    uint16_t chunk = (uint16_t) btstack_min(available, rx_transfer.nbytes);
    if (chunk > 0) {
        uart_ll_read_rxfifo(uart, rx_transfer.buf, chunk);
        rx_transfer.nbytes -= chunk;
        rx_transfer.buf    += chunk;
    }
}

// custom interrupt handler
static void IRAM_ATTR hal_uart_dma_isr(void *arg) {
    uart_dev_t *uart = (uart_dev_t *)arg;
    uint32_t status = uart_ll_get_intsts_mask(uart);

    // RX
    if ((status & UART_RXFIFO_FULL_INT_ST_M) != 0) {
        uart_ll_clr_intsts_mask(uart, HAL_UART_DMA_RX_INT_CLEARS);
        hal_uart_dma_read_rx_fifo(uart);
        if (rx_transfer.nbytes == 0) {
            uart_ll_disable_intr_mask(uart, HAL_UART_DMA_RX_INTS);
            receive_callback();
        } else {
            // get length of next chunk
            uint16_t chunk = (uint16_t) btstack_min(HAL_UART_DMA_RX_THRESHOLD, rx_transfer.nbytes);
            uart_ll_set_rxfifo_full_thr(uart, chunk);
        }
    }

    // TX
    if ((status & UART_TXFIFO_EMPTY_INT_ST_M) != 0){
        uart_ll_clr_intsts_mask(uart, UART_TXFIFO_EMPTY_INT_CLR_M);

        hal_uart_dma_fill_tx_fifo(uart);
        if (tx_transfer.nbytes == 0) {
            uart_ll_disable_intr_mask(uart, UART_TXFIFO_EMPTY_INT_ENA_M);
            send_callback();
        }
    }
}

/**
 * @brief Init and open device
 */
void hal_uart_dma_init(void) {

#if defined(UART_NRESET) && (UART_NRESET >= 0)
    // Configure GPIO15 as output
    gpio_config_t io_conf_nreset = {
        .pin_bit_mask = (1ULL<<UART_NRESET),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_nreset);

    // Set NRESET to LOW
    ESP_LOGI(TAG, "nRESET: LOW");
    gpio_set_level(UART_NRESET, 0);

    // wait for 100 ms
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Set NRESET to HIGH
    ESP_LOGI(TAG, "nRESET: HIGH");
    gpio_set_level(UART_NRESET, 1);

    // wait for 100 ms
    vTaskDelay(100 / portTICK_PERIOD_MS);
#endif

    // Configure UART - UART controls RTS
    ESP_ERROR_CHECK(uart_param_config(UART_NO, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NO, UART_TX_PIN, UART_RX_PIN, UART_RTS_PIN, UART_CTS_PIN));
    ESP_LOGI(TAG, "UART #%u TX=%d RX=%d RTS=%d CTS=%d baud=%lu flowcontrol=%s",
             UART_NO, UART_TX_PIN, UART_RX_PIN, UART_RTS_PIN, UART_CTS_PIN,
             (unsigned long) uart_config.baud_rate,
             uart_config.flow_ctrl == UART_HW_FLOWCTRL_DISABLE ? "off" : "on");

#ifdef CONFIG_EXAMPLE_HCI_UART_INVERT_RTS
    // On ESP32-P4, RTS is HIGH when we're ready to receive
    // this is opposite to common practice but can be fixed by inverting the signal

    // Has not been tested on other ESP32 chips other then ESP32-P4
    ESP_ERROR_CHECK(uart_set_line_inverse(UART_NO, UART_SIGNAL_RTS_INV));
#endif

    // disable default interrupts
    uart_dev_t *uart = UART_LL_GET_HW(UART_NO);
    uart_ll_disable_intr_mask(uart, 0xFFFFFFFF);

    // setup interrupt handler
    int intr_alloc_flags = 0;
#ifdef CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif
    esp_intr_alloc(uart_periph_signal[UART_NO].irq,
                   intr_alloc_flags,
                   hal_uart_dma_isr,
                   UART_LL_GET_HW(UART_NO),
                   NULL);

    // configure TX Empty threshold
    uart_ll_set_txfifo_empty_thr(uart, 10);
    printf("UART #%u, baud %u\n", UART_NO, uart_config.baud_rate);
    printf("Initial txfifo len: %lu\n", uart_ll_get_txfifo_len(uart));
}

/**
 * @brief Set callback for block received - can be called from ISR context
 * @param callback
 */
void hal_uart_dma_set_block_received( void (*callback)(void)) {
    receive_callback = callback;
}

/**
 * @brief Set callback for block sent - can be called from ISR context
 * @param callback
 */
void hal_uart_dma_set_block_sent( void (*callback)(void)) {
#ifdef ENABLE_UART_SYNCHRONOUS_WRITE
    if (callback == NULL){
        tx_transfer.nbytes = 0;
        uart_dev_t *uart = UART_LL_GET_HW(UART_NO);
        uart_ll_disable_intr_mask(uart, UART_TXFIFO_EMPTY_INT_ENA_M);
    }
#else
    btstack_assert(callback != NULL);
#endif
    send_callback = callback;
}

void hal_uart_dma_set_csr_irq_handler( void (*csr_irq_handler)(void)){
    UNUSED(csr_irq_handler);
}

void hal_uart_dma_set_sleep(uint8_t sleep) {
    UNUSED(sleep);
}

/**
 * @brief Set baud rate
 * @note During baud change, TX line should stay high and no data should be received on RX accidentally
 * @param baudrate
 */
int  hal_uart_dma_set_baud(uint32_t baud) {
    uart_config.baud_rate = baud;
    ESP_ERROR_CHECK(uart_param_config(UART_NO, &uart_config));
    ESP_LOGI(TAG, "UART #%u set_baud %lu", UART_NO, (unsigned long) baud);
    return 0;
}

#ifdef HAVE_UART_DMA_SET_FLOWCONTROL
/**
 * @brief Set flowcontrol
 * @param flowcontrol enabled
 */
int  hal_uart_dma_set_flowcontrol(int flowcontrol) {
    if( flowcontrol == BTSTACK_UART_FLOWCONTROL_ON ) {
        uart_config.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS;
    } else {
        uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    }
    ESP_ERROR_CHECK(uart_param_config(UART_NO, &uart_config));
    ESP_LOGI(TAG, "UART #%u set_flowcontrol %s", UART_NO, flowcontrol == BTSTACK_UART_FLOWCONTROL_ON ? "on" : "off");
    return 0;
}
#endif

/**
 * @brief Receive block. When done, callback set by hal_uart_dma_set_block_received must be called
 * @param buffer
 * @param lengh
 */
bool hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len) {

    btstack_assert(rx_transfer.nbytes == 0);

    // store transfer
    rx_transfer.buf = (uint8_t *)buffer;
    rx_transfer.nbytes = len;

    // RX interrupts are off, we can fill buffer from fifo
    uart_dev_t *uart = UART_LL_GET_HW(UART_NO);
    hal_uart_dma_read_rx_fifo(uart);

    // enable interrupt if we need more data
    bool transfer_complete = rx_transfer.nbytes == 0;
    if (transfer_complete == false) {
        uart_dev_t *uart = UART_LL_GET_HW(UART_NO);
        uint16_t chunk = (uint16_t) btstack_min(HAL_UART_DMA_RX_THRESHOLD, rx_transfer.nbytes);
        uart_ll_set_rxfifo_full_thr(uart, chunk);
        uart_ll_clr_intsts_mask(uart, HAL_UART_DMA_RX_INT_CLEARS);
        uart_ll_ena_intr_mask(uart, HAL_UART_DMA_RX_INTS);
    }
    return transfer_complete;
}

#ifdef ENABLE_UART_SYNCHRONOUS_WRITE
static void hal_uart_dma_send_block_sync(const uint8_t *buffer, uint16_t len) {
    uart_dev_t *uart = UART_LL_GET_HW(UART_NO);
    TickType_t blocked_since = 0;

    while (len > 0) {
        uint16_t space = uart_ll_get_txfifo_len(uart);
        if (space == 0) {
            if (blocked_since == 0){
                blocked_since = xTaskGetTickCount();
            } else {
                TickType_t blocked_ticks = xTaskGetTickCount() - blocked_since;
                if ((blocked_ticks % pdMS_TO_TICKS(1000)) == 0){
                    ESP_LOGW(TAG, "UART #%u TX stalled for %lu ms, waiting for remote to resume",
                             UART_NO, (unsigned long) (blocked_ticks * portTICK_PERIOD_MS));
                }
            }
            vTaskDelay(1);
            continue;
        }

        blocked_since = 0;

        uint16_t chunk = (uint16_t) btstack_min(space, len);
        uart_ll_write_txfifo(uart, buffer, chunk);
        buffer += chunk;
        len -= chunk;
    }
}
#endif

/**
 * @brief Send block. When done, callback set by hal_uart_set_block_sent must be called
 * @param buffer
 * @param lengh
 */
bool hal_uart_dma_send_block(const uint8_t *buffer, uint16_t len) {
    btstack_assert(tx_transfer.nbytes == 0);

#ifdef ENABLE_UART_SYNCHRONOUS_WRITE
    if (send_callback == NULL) {
        hal_uart_dma_send_block_sync(buffer, len);
        return true;
    }
#endif

    // store transfer
    tx_transfer.buf = (uint8_t *)buffer;
    tx_transfer.nbytes = len;

    // TX Empty interrupt is off, we can start filling
    uart_dev_t *uart = UART_LL_GET_HW(UART_NO);
    hal_uart_dma_fill_tx_fifo(uart);

    // enable interrupt if there's more data (in this case, the tx fifo is full and we're above the threshold)
    bool transfer_complete = tx_transfer.nbytes == 0;
    if (transfer_complete == false) {
        uart_dev_t *uart = UART_LL_GET_HW(UART_NO);
        uart_ll_clr_intsts_mask(uart, UART_TXFIFO_EMPTY_INT_CLR_M);
        uart_ll_ena_intr_mask(uart, UART_TXFIFO_EMPTY_INT_ENA_M);
    }
    return transfer_complete;
}
