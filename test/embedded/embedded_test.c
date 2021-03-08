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
#include "btstack_debug.h"

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

static btstack_timer_source_t timer_1;
static btstack_timer_source_t timer_2;
static btstack_data_source_t  data_source;
static bool data_source_called;
static bool timer_called;

static void heartbeat_timeout_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    timer_called = true;
}
static void data_source_handler(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(ds);
    UNUSED(callback_type);
    data_source_called = true;
}

TEST_GROUP(Embedded){

    void setup(void){
        // start with BTstack init - especially configure HCI Transport
        btstack_memory_init();
        btstack_run_loop_init(btstack_run_loop_embedded_get_instance());
        btstack_run_loop_set_timer_handler(&timer_1, heartbeat_timeout_handler);
    }
    void teardown(void){
        btstack_run_loop_deinit();
        btstack_memory_deinit();
    }
};

TEST(Embedded, Init){
    btstack_run_loop_set_timer(&timer_1, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&timer_1);

    btstack_run_loop_embedded_execute_once();
    btstack_run_loop_embedded_trigger();
    btstack_run_loop_embedded_execute_once();
    btstack_run_loop_get_time_ms();
    btstack_run_loop_timer_dump();
    btstack_run_loop_remove_timer(&timer_1);
    (void) btstack_run_loop_get_timer_context(&timer_1);
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

TEST_GROUP(RunLoopBase){
        void setup(void){
            btstack_memory_init();
            btstack_run_loop_base_init();
            data_source_called = false;
            timer_called = false;
        }
        void teardown(void){
            btstack_memory_deinit();
        }
};

TEST(RunLoopBase, DataSource){
    btstack_run_loop_set_data_source_handler(&data_source, &data_source_handler);
    btstack_run_loop_base_enable_data_source_callbacks(&data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_base_add_data_source(&data_source);
    btstack_run_loop_base_disable_data_source_callbacks(&data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_base_remove_data_source(&data_source);
}

TEST(RunLoopBase,Timer){
    const uint32_t timeout_1 = 7;
    const uint32_t timeout_2 = 10;
    const uint32_t now_1   =  5;
    const uint32_t now_2   = 11;
    const uint32_t now_3   = 33;

    btstack_run_loop_set_timer_handler(&timer_1, heartbeat_timeout_handler);
    timer_1.timeout = timeout_1;

    btstack_run_loop_set_timer_handler(&timer_2, heartbeat_timeout_handler);
    timer_2.timeout = timeout_2;

    // test add/remove

    // add timer 2
    btstack_run_loop_base_add_timer(&timer_2);
    CHECK(btstack_run_loop_base_timers != NULL);
    // remove timer
    btstack_run_loop_base_remove_timer(&timer_2);
    CHECK(btstack_run_loop_base_timers == NULL);

    // add timer 2
    btstack_run_loop_base_add_timer(&timer_2);
    // add timer 1
    btstack_run_loop_base_add_timer(&timer_1);
    // dump timers
    btstack_run_loop_base_dump_timer();
    // process timers for now_1
    btstack_run_loop_base_process_timers(now_1);
    CHECK(timer_called == false);
    CHECK(btstack_run_loop_base_timers != NULL);
    // get next timeout
    int next_timeout = btstack_run_loop_base_get_time_until_timeout(now_1);
    CHECK(next_timeout == (timeout_1 - now_1));
    // get next timeout in future - timeout should be 0 = now
    next_timeout = btstack_run_loop_base_get_time_until_timeout(now_3);
    CHECK(next_timeout == 0);
    // process timers for now_2
    btstack_run_loop_base_process_timers(now_2);
    CHECK(timer_called == true);
    CHECK(btstack_run_loop_base_timers == NULL);
    // get next timeout
    next_timeout = btstack_run_loop_base_get_time_until_timeout(now_2);
    CHECK(next_timeout == -1);
}

TEST(RunLoopBase, Overrun){
    const uint32_t t1 = 0xfffffff0UL;   // -16
    const uint32_t t2 = 0xfffffff8UL;   //  -8
    const uint32_t t3 = 50UL;
    int next_timeout;

    btstack_run_loop_set_timer_handler(&timer_1, heartbeat_timeout_handler);
    timer_1.timeout = t1 + 20;  // overrun
    // add timer
    btstack_run_loop_base_add_timer(&timer_1);
    // verify timeout until correct
    next_timeout = btstack_run_loop_base_get_time_until_timeout(t1);
    CHECK(next_timeout == 20);
    // process timers for t2
    btstack_run_loop_base_process_timers(t2);
    CHECK(timer_called == false);
    // process timers for t3
    btstack_run_loop_base_process_timers(t3);
    CHECK(timer_called == true);
}


TEST_GROUP(BTstackUtil){
    void setup(void){
    }
    void teardown(void){
    }
};

TEST(BTstackUtil, bd_addr_cmp){
    bd_addr_t a = {0};
    bd_addr_t b = {0};
    CHECK_EQUAL(0, bd_addr_cmp(a, b));
    
    bd_addr_t a1 = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    bd_addr_t b1 = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    CHECK_EQUAL(0, bd_addr_cmp(a1, b1));

    bd_addr_t a3 = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    bd_addr_t b3 = {0xCB, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    CHECK(0 != bd_addr_cmp(a3, b3));
}

TEST(BTstackUtil, bd_addr_copy){
    bd_addr_t a = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    bd_addr_t b = {0};
    CHECK(0 != bd_addr_cmp(a, b));

    bd_addr_copy(a,b);
    CHECK_EQUAL(0, bd_addr_cmp(a, b));
}

TEST(BTstackUtil, little_endian_read){
    const uint8_t buffer[] = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    uint32_t value;

    value = little_endian_read_16(buffer, 0);
    CHECK_EQUAL(0xECBC, value);

    value = little_endian_read_24(buffer, 0);
    CHECK_EQUAL(0x5DECBC, value);
    
    value = little_endian_read_32(buffer, 0);
    CHECK_EQUAL(0xE65DECBC, value);
}

TEST(BTstackUtil, little_endian_store){
    uint8_t buffer[6];
    uint32_t expected_value = 0xE65DECBC;
    uint32_t value;

    memset(buffer, 0, sizeof(buffer));
    little_endian_store_16(buffer, 0, expected_value);
    value = little_endian_read_16(buffer, 0);
    CHECK_EQUAL(0xECBC, value);

    memset(buffer, 0, sizeof(buffer));
    little_endian_store_24(buffer, 0, expected_value);
    value = little_endian_read_24(buffer, 0);
    CHECK_EQUAL(0x5DECBC, value);

    memset(buffer, 0, sizeof(buffer));
    little_endian_store_32(buffer, 0, expected_value);
    value = little_endian_read_32(buffer, 0);
    CHECK_EQUAL(0xE65DECBC, value);
}

TEST(BTstackUtil, big_endian_read){
    const uint8_t buffer[] = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    uint32_t value;

    value = big_endian_read_16(buffer, 0);
    CHECK_EQUAL(0xBCEC, value);

    value = big_endian_read_24(buffer, 0);
    CHECK_EQUAL(0xBCEC5D, value);
    
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0xBCEC5DE6, value);
}

TEST(BTstackUtil, big_endian_store){
    uint8_t buffer[6];
    uint32_t expected_value = 0xE65DECBC;
    uint32_t value;

    memset(buffer, 0, sizeof(buffer));
    big_endian_store_16(buffer, 0, expected_value);
    value = big_endian_read_16(buffer, 0);
    CHECK_EQUAL(0xECBC, value);

    memset(buffer, 0, sizeof(buffer));
    big_endian_store_24(buffer, 0, expected_value);
    value = big_endian_read_24(buffer, 0);
    CHECK_EQUAL(0x5DECBC, value);

    memset(buffer, 0, sizeof(buffer));
    big_endian_store_32(buffer, 0, expected_value);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0xE65DECBC, value);
}

TEST(BTstackUtil, reverse_bytes){
    uint8_t src[32];
    uint8_t buffer[32];
    uint32_t value;

    int i;
    for (i = 0; i < sizeof(src); i++){
        src[i] = i + 1;
    }
#ifdef ENABLE_PRINTF_HEXDUMP
    printf_hexdump(src, 32);
#endif
    
    memset(buffer, 0, sizeof(buffer));
    reverse_bytes(src, buffer, sizeof(buffer));
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x201F1E1D, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_24(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x03020100, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_48(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x06050403, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_56(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x07060504, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_64(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x08070605, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_128(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x100F0E0D, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_256(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x201F1E1D, value);
}

TEST(BTstackUtil, reverse_bd_addr){
    bd_addr_t src = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    bd_addr_t dest = {0};
    reverse_bd_addr(src, dest);

    uint32_t value = big_endian_read_32(dest, 0);
    CHECK_EQUAL(0x0315E65D, value);
}

TEST(BTstackUtil, btstack_min_max){
    uint32_t a = 30;
    uint32_t b = 100;

    CHECK_EQUAL(a, btstack_min(a,b));
    CHECK_EQUAL(a, btstack_min(b,a));

    CHECK_EQUAL(b, btstack_max(a,b));
    CHECK_EQUAL(b, btstack_max(b,a));
}

TEST(BTstackUtil, char_for_nibble){
    CHECK_EQUAL('A', char_for_nibble(10));
    CHECK_EQUAL('?', char_for_nibble(20));
    // CHECK_EQUAL('?', char_for_nibble(-5));
}

TEST(BTstackUtil, nibble_for_char){
    CHECK_EQUAL(0, nibble_for_char('0'));
    CHECK_EQUAL(5, nibble_for_char('5'));
    CHECK_EQUAL(9, nibble_for_char('9'));

    CHECK_EQUAL(10, nibble_for_char('a'));
    CHECK_EQUAL(12, nibble_for_char('c'));
    CHECK_EQUAL(15, nibble_for_char('f'));
    
    CHECK_EQUAL(10, nibble_for_char('A'));
    CHECK_EQUAL(12, nibble_for_char('C'));
    CHECK_EQUAL(15, nibble_for_char('F'));

    CHECK_EQUAL(-1, nibble_for_char('-'));
}

TEST(BTstackUtil, logging){
    uint8_t data[6 * 16 + 5] = {0x54, 0x65, 0x73, 0x74, 0x20, 0x6c, 0x6f, 0x67, 0x20, 0x69, 0x6e, 0x66, 0x6f};
#ifdef ENABLE_LOG_DEBUG
    log_debug_hexdump(data, sizeof(data));
#endif

#ifdef ENABLE_LOG_INFO
    log_info_hexdump(data, sizeof(data));
    sm_key_t key = {0x54, 0x65, 0x73, 0x74, 0x20, 0x6c, 0x6f, 0x67, 0x20, 0x69, 0x6e, 0x66, 0x6f, 0x01, 0x014, 0xFF};
    log_info_key("test key", key);
#endif
}

TEST(BTstackUtil, uuids){
    uint32_t shortUUID = 0x44445555;
    uint8_t uuid[16];
    uint8_t uuid128[] = {0x44, 0x44, 0x55, 0x55, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
    
    memset(uuid, 0, sizeof(uuid));
    uuid_add_bluetooth_prefix(uuid, shortUUID);
    MEMCMP_EQUAL(uuid128, uuid, sizeof(uuid));

    CHECK_EQUAL(1, uuid_has_bluetooth_prefix(uuid128));

    uuid128[5] = 0xff;
    CHECK(1 != uuid_has_bluetooth_prefix(uuid128));

    char * uuid128_str = uuid128_to_str(uuid128);
    STRCMP_EQUAL("44445555-00FF-1000-8000-00805F9B34FB", uuid128_str);
}

TEST(BTstackUtil, bd_addr_utils){
    const bd_addr_t addr = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    char * device_addr_string = "BC:EC:5D:E6:15:03";

    char * addr_str = bd_addr_to_str(addr);
    STRCMP_EQUAL(device_addr_string, addr_str);

    uint8_t device_name[50];
    strcpy((char *)device_name, "Device name 00:00:00:00:00:00");
    btstack_replace_bd_addr_placeholder(device_name, strlen((const char*) device_name), addr);
    STRCMP_EQUAL("Device name BC:EC:5D:E6:15:03", (const char *) device_name);

    bd_addr_t device_addr;
    CHECK_EQUAL(1, sscanf_bd_addr(device_addr_string, device_addr));
    MEMCMP_EQUAL(addr, device_addr, sizeof(addr));

    CHECK_EQUAL(1, sscanf_bd_addr("BC EC 5D E6 15 03", device_addr));
    CHECK_EQUAL(1, sscanf_bd_addr("BC-EC-5D-E6-15-03", device_addr));

    CHECK_EQUAL(0, sscanf_bd_addr("", device_addr));
    CHECK_EQUAL(0, sscanf_bd_addr("GG-EC-5D-E6-15-03", device_addr));
    CHECK_EQUAL(0, sscanf_bd_addr("8G-EC-5D-E6-15-03", device_addr));
    CHECK_EQUAL(0, sscanf_bd_addr("BCxECx5DxE6:15:03", device_addr));

}

TEST(BTstackUtil, atoi){
    CHECK_EQUAL(102, btstack_atoi("102"));
    CHECK_EQUAL(0, btstack_atoi("-102"));
    CHECK_EQUAL(1, btstack_atoi("1st"));
    CHECK_EQUAL(1, btstack_atoi("1st2"));
}

TEST(BTstackUtil, string_len_for_uint32){
    CHECK_EQUAL(1, string_len_for_uint32(9)); 
    CHECK_EQUAL(2, string_len_for_uint32(19)); 
    CHECK_EQUAL(3, string_len_for_uint32(109)); 
    CHECK_EQUAL(4, string_len_for_uint32(1009)); 
    CHECK_EQUAL(5, string_len_for_uint32(10009)); 
    CHECK_EQUAL(6, string_len_for_uint32(100009)); 
    CHECK_EQUAL(7, string_len_for_uint32(1000009)); 
    CHECK_EQUAL(8, string_len_for_uint32(10000009)); 
    CHECK_EQUAL(9, string_len_for_uint32(100000009)); 
    CHECK_EQUAL(10, string_len_for_uint32(1000000000)); 
}

TEST(BTstackUtil, count_set_bits_uint32){
    CHECK_EQUAL(4, count_set_bits_uint32(0x0F));
}

TEST(BTstackUtil, crc8){
    uint8_t data[] = {1,2,3,4,5,6,7,8,9};
    CHECK_EQUAL(84, btstack_crc8_calc(data, sizeof(data)));

    CHECK_EQUAL(0, btstack_crc8_check(data, sizeof(data), 84));
    CHECK_EQUAL(1, btstack_crc8_check(data, sizeof(data), 74));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
