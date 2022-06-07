#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_run_loop.h"
#include "btstack_memory.h"

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

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
