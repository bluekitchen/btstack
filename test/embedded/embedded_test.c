#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "ble/att_db.h"
#include "ble/att_db_util.h"
#include "btstack_util.h"
#include "bluetooth.h"
#include "btstack_audio.h"

#include "hal_audio.h"
#include "hal_cpu.h"
#include "hal_em9304_spi.h"
#include "hal_flash_bank.h"
#include "hal_flash_bank_memory.h"
#include "hal_led.h"
#include "hal_stdin.h"
#include "hal_tick.h"
#include "hal_time_ms.h"
#include "hal_uart_dma.h"

#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_memory.h"
// quick mock

// hal_cpu
void hal_cpu_disable_irqs(void){}
void hal_cpu_enable_irqs(void){}
void hal_cpu_enable_irqs_and_sleep(void){}

// hal_em9304_spi
void hal_em9304_spi_init(void){}
void hal_em9304_spi_deinit(void){}
void hal_em9304_spi_set_ready_callback(void (*callback)(void)){}
void hal_em9304_spi_set_transfer_done_callback(void (*callback)(void)){}
void hal_em9304_spi_enable_ready_interrupt(void){}
void hal_em9304_spi_disable_ready_interrupt(void){}
int  hal_em9304_spi_get_ready(){ return 0; }
void hal_em9304_spi_set_chip_select(int enable){}
int  hal_em9304_spi_get_fullduplex_support(void){ return 0; }
void hal_em9304_spi_transceive(const uint8_t * tx_data, uint8_t * rx_data, uint16_t len){}
void hal_em9304_spi_transmit(const uint8_t * tx_data, uint16_t len){}
void hal_em9304_spi_receive(uint8_t * rx_data, uint16_t len){}

// hal_stdin_setup
void hal_stdin_setup(void (*handler)(char c)){}

// hal_uart_dma
void hal_uart_dma_init(void){}
void hal_uart_dma_set_block_received( void (*callback)(void)){}
void hal_uart_dma_set_block_sent( void (*callback)(void)){}
int  hal_uart_dma_set_baud(uint32_t baud){ return 0; }
int  hal_uart_dma_set_flowcontrol(int flowcontrol){ return 0;}
void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t length){}
void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t len){}
void hal_uart_dma_set_csr_irq_handler( void (*csr_irq_handler)(void)){}
void hal_uart_dma_set_sleep(uint8_t sleep){}
int  hal_uart_dma_get_supported_sleep_modes(void){ return 0; }
// void hal_uart_dma_set_sleep_mode(btstack_uart_sleep_mode_t sleep_mode){}

// hal_audio_sink
void hal_audio_sink_init(uint8_t channels, uint32_t sample_rate,void (*buffer_played_callback)(uint8_t buffer_index)){}
uint16_t hal_audio_sink_get_num_output_buffers(void){ return 0; }
uint16_t hal_audio_sink_get_num_output_buffer_samples(void){ return 0; }
int16_t * hal_audio_sink_get_output_buffer(uint8_t buffer_index){ return NULL; }
void hal_audio_sink_start(void){}
void hal_audio_sink_stop(void){}
void hal_audio_sink_close(void){}
void hal_audio_source_init(uint8_t channels, uint32_t sample_rate, void (*buffer_recorded_callback)(const int16_t * buffer, uint16_t num_samples)){}
void hal_audio_source_start(void){}
void hal_audio_source_stop(void){}
void hal_audio_source_close(void){}


#define HEARTBEAT_PERIOD_MS 1000

static btstack_timer_source_t heartbeat;
static btstack_data_source_t  data_source;
static void heartbeat_timeout_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
}
static void data_source_handler(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(ds);
    UNUSED(callback_type);
}

TEST_GROUP(Embedded){

    void setup(void){
        // start with BTstack init - especially configure HCI Transport
        btstack_memory_init();
        btstack_run_loop_init(btstack_run_loop_embedded_get_instance());
        btstack_run_loop_set_timer_handler(&heartbeat, heartbeat_timeout_handler);
    }
    void teardown(void){
        btstack_run_loop_deinit();
        btstack_memory_deinit();
    }
};

TEST(Embedded, Init){
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);

    btstack_run_loop_embedded_execute_once();
    btstack_run_loop_embedded_trigger();
    btstack_run_loop_embedded_execute_once();
    btstack_run_loop_get_time_ms();
    btstack_run_loop_timer_dump();
    btstack_run_loop_remove_timer(&heartbeat);
    (void) btstack_run_loop_get_timer_context(&heartbeat);
}

TEST(Embedded, DataSource){
    btstack_run_loop_set_data_source_handler(&data_source, &data_source_handler);
    btstack_run_loop_set_data_source_fd(&data_source, 0);
    btstack_run_loop_set_data_source_handle(&data_source, NULL);
    btstack_run_loop_enable_data_source_callbacks(&data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_disable_data_source_callbacks(&data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&data_source);
    btstack_run_loop_remove_data_source(&data_source);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
