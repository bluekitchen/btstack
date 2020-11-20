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

#include "queue.h"
#include "task.h"
#include "event_groups.h"

// quick mock to add freertos support to coverage

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

// hal_time_ms
uint32_t hal_time_ms(void){ return 0; }

BaseType_t xQueueSendToBack(QueueHandle_t xQueue, const void * pvItemToQueue, TickType_t xTicksToWait){
	return 0;
}
BaseType_t xQueueSendToBackFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken){
	return 0;
}
BaseType_t xQueueReceive(QueueHandle_t xQueue,void *pvBuffer,TickType_t xTicksToWait){
	return 0;
}
QueueHandle_t xQueueCreateStatic( UBaseType_t uxQueueLength, UBaseType_t uxItemSize, uint8_t *pucQueueStorageBuffer, StaticQueue_t *pxQueueBuffer ){
	return 0;
}

EventGroupHandle_t xEventGroupCreate( void ){ 
	return 0;
}
EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet ){
	return 0;
}
BaseType_t xEventGroupSetBitsFromISR( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken ){
	return 0;
}
EventBits_t xEventGroupWaitBits( const EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait ){
	return 0;	
}
TaskHandle_t xTaskGetCurrentTaskHandle( void ){
	return 0;
}

TEST_GROUP(FreeRTOS){
    void setup(void){
    }
};

TEST(FreeRTOS, Init){

}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
