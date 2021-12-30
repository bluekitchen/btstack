#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hal_cpu.h"
#include "hal_time_ms.h"

#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_memory.h"

// quick mock

// hal_cpu
void hal_cpu_disable_irqs(void){}
void hal_cpu_enable_irqs(void){}
void hal_cpu_enable_irqs_and_sleep(void){}

// hal_time_ms_h
uint32_t hal_time_ms(void){
    return 0;
}

#define HEARTBEAT_PERIOD_MS 1000

static btstack_timer_source_t timer_1;
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

static void data_source_handler_trigger_exit(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(ds);
    UNUSED(callback_type);
    btstack_run_loop_trigger_exit();
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
    static btstack_context_callback_registration_t callback_registration;
    btstack_run_loop_execute_on_main_thread(&callback_registration);
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

TEST(Embedded, ExitRunLoop){
    btstack_run_loop_set_data_source_handler(&data_source, &data_source_handler_trigger_exit);
    btstack_run_loop_set_data_source_fd(&data_source, 0);
    btstack_run_loop_set_data_source_handle(&data_source, NULL);
    btstack_run_loop_enable_data_source_callbacks(&data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&data_source);
    btstack_run_loop_execute();
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
