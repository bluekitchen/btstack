#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hci_dump.h"
#include "hci_dump_buffered.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"

#include <string.h>

static uint32_t mock_time_ms;
static int reset_count;
static int logged_packet_count;
static int logged_message_count;

typedef struct {
    uint8_t packet_type;
    uint8_t in;
    uint16_t len;
    uint8_t data[32];
} logged_packet_t;

static logged_packet_t logged_packets[8];

static void mock_run_loop_init(void){
    btstack_run_loop_base_init();
    mock_time_ms = 0;
}

static void mock_run_loop_set_timer(btstack_timer_source_t * timer, uint32_t timeout_in_ms){
    timer->timeout = mock_time_ms + timeout_in_ms + 1u;
}

static void mock_run_loop_execute(void){
}

static void mock_run_loop_poll_data_sources_from_irq(void){
}

static void mock_run_loop_execute_on_main_thread(btstack_context_callback_registration_t * callback_registration){
    btstack_run_loop_base_add_callback(callback_registration);
}

static void mock_run_loop_trigger_exit(void){
}

static uint32_t mock_run_loop_get_time_ms(void){
    return mock_time_ms;
}

static const btstack_run_loop_t mock_run_loop = {
    &mock_run_loop_init,
    &btstack_run_loop_base_add_data_source,
    &btstack_run_loop_base_remove_data_source,
    &btstack_run_loop_base_enable_data_source_callbacks,
    &btstack_run_loop_base_disable_data_source_callbacks,
    &mock_run_loop_set_timer,
    &btstack_run_loop_base_add_timer,
    &btstack_run_loop_base_remove_timer,
    &mock_run_loop_execute,
    &btstack_run_loop_base_dump_timer,
    &mock_run_loop_get_time_ms,
    &mock_run_loop_poll_data_sources_from_irq,
    &mock_run_loop_execute_on_main_thread,
    &mock_run_loop_trigger_exit,
};

static void mock_run_loop_process_timers(uint32_t now_ms){
    mock_time_ms = now_ms;
    btstack_run_loop_base_process_timers(now_ms);
}

static void reset_logged_packets(void){
    memset(logged_packets, 0, sizeof(logged_packets));
    logged_packet_count = 0;
    logged_message_count = 0;
    reset_count = 0;
}

static void hci_dump_embedded_stdout_log_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len){
    CHECK_TRUE(logged_packet_count < (int)(sizeof(logged_packets) / sizeof(logged_packets[0])));
    CHECK_TRUE(len <= sizeof(logged_packets[0].data));
    if ((logged_packet_count >= (int)(sizeof(logged_packets) / sizeof(logged_packets[0]))) || (len > sizeof(logged_packets[0].data))) {
        return;
    }
    logged_packets[logged_packet_count].packet_type = packet_type;
    logged_packets[logged_packet_count].in = in;
    logged_packets[logged_packet_count].len = len;
    if ((len > 0u) && (packet != NULL)) {
        memcpy(logged_packets[logged_packet_count].data, packet, len);
    }
    if (packet_type == LOG_MESSAGE_PACKET) {
        logged_message_count++;
    }
    logged_packet_count++;
}
static void hci_dump_embedded_stdout_log_message(int log_level, const char * format, va_list argptr){
    UNUSED(log_level);
    UNUSED(format);
    UNUSED(argptr);
}
static void hci_dump_embedded_stdout_reset(void){
    reset_count++;
}

static const hci_dump_t hci_dump_instance_without_reset = {
    // void (*reset)(void);
    NULL,
    // void (*log_packet)(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
    &hci_dump_embedded_stdout_log_packet,
    // void (*log_message)(int log_level, const char * format, va_list argptr);
    &hci_dump_embedded_stdout_log_message,
};

static const hci_dump_t hci_dump_instance_with_reset = {
        // void (*reset)(void);
        &hci_dump_embedded_stdout_reset,
        // void (*log_packet)(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
        &hci_dump_embedded_stdout_log_packet,
        // void (*log_message)(int log_level, const char * format, va_list argptr);
        &hci_dump_embedded_stdout_log_message,
};

TEST_GROUP(hci_dump){
        void setup(void){
            btstack_run_loop_init(&mock_run_loop);
            reset_logged_packets();
        }
        void teardown(void){
            btstack_run_loop_deinit();
        }
};

TEST(hci_dump, init){
    hci_dump_init(&hci_dump_instance_without_reset);
    hci_dump_set_max_packets(0);
    hci_dump_enable_packet_log(false);
}

TEST(hci_dump, log_level){
    hci_dump_init(&hci_dump_instance_without_reset);
    hci_dump_log(HCI_DUMP_LOG_LEVEL_DEBUG - 1, "");
    hci_dump_log(HCI_DUMP_LOG_LEVEL_ERROR + 1, "");
    hci_dump_enable_log_level(HCI_DUMP_LOG_LEVEL_DEBUG - 1, 0);
    hci_dump_enable_log_level(HCI_DUMP_LOG_LEVEL_ERROR + 1, 0);
    hci_dump_enable_log_level(HCI_DUMP_LOG_LEVEL_ERROR, 1);
}

TEST(hci_dump, log_not_enabled){
    hci_dump_init(&hci_dump_instance_without_reset);
    hci_dump_enable_packet_log(false);
    hci_dump_packet(0, 0, NULL, 0);
}

TEST(hci_dump, log_reset){
    hci_dump_init(&hci_dump_instance_without_reset);
    hci_dump_set_max_packets(1);
    hci_dump_packet(0, 0, NULL, 0);
    hci_dump_packet(0, 0, NULL, 0);
}

TEST(hci_dump, log_max_packets){
    hci_dump_init(&hci_dump_instance_with_reset);
    hci_dump_set_max_packets(1);
    hci_dump_packet(0, 0, NULL, 0);
    hci_dump_packet(0, 0, NULL, 0);
}

TEST(hci_dump, header_setup_packetlogger){
    uint8_t buffer[100];
    hci_dump_setup_header_packetlogger(buffer, 0, 0, HCI_COMMAND_DATA_PACKET, 0, 0);
    hci_dump_setup_header_packetlogger(buffer, 0, 0, HCI_ACL_DATA_PACKET, 0, 0);
    hci_dump_setup_header_packetlogger(buffer, 0, 0, HCI_SCO_DATA_PACKET, 0, 0);
    hci_dump_setup_header_packetlogger(buffer, 0, 0, HCI_ISO_DATA_PACKET, 0, 0);
    hci_dump_setup_header_packetlogger(buffer, 0, 0, HCI_EVENT_PACKET, 0, 0);
    hci_dump_setup_header_packetlogger(buffer, 0, 0, HCI_COMMAND_DATA_PACKET, 0, 0);
    hci_dump_setup_header_packetlogger(buffer, 0, 0, LOG_MESSAGE_PACKET, 0, 0);
    hci_dump_setup_header_packetlogger(buffer, 0, 0, 0x77, 0, 0);
}

TEST(hci_dump, header_setup_packetlogger_incoming_sco_and_iso){
    uint8_t buffer[100];

    memset(buffer, 0, sizeof(buffer));
    hci_dump_setup_header_packetlogger(buffer, 0, 0, HCI_SCO_DATA_PACKET, 1, 0);
    CHECK_EQUAL(0x09, buffer[12]);

    memset(buffer, 0, sizeof(buffer));
    hci_dump_setup_header_packetlogger(buffer, 0, 0, HCI_ISO_DATA_PACKET, 1, 0);
    CHECK_EQUAL(0x0d, buffer[12]);
}

TEST(hci_dump, header_setup_bluez){
    uint8_t buffer[100];
    hci_dump_setup_header_bluez(buffer, 0, 0, HCI_COMMAND_DATA_PACKET, 0, 0);
    hci_dump_setup_header_bluez(buffer, 0, 0, HCI_ACL_DATA_PACKET, 0, 0);
    hci_dump_setup_header_bluez(buffer, 0, 0, HCI_SCO_DATA_PACKET, 0, 0);
    hci_dump_setup_header_bluez(buffer, 0, 0, HCI_EVENT_PACKET, 0, 0);
    hci_dump_setup_header_bluez(buffer, 0, 0, HCI_COMMAND_DATA_PACKET, 0, 0);
    hci_dump_setup_header_bluez(buffer, 0, 0, LOG_MESSAGE_PACKET, 0, 0);
    hci_dump_setup_header_bluez(buffer, 0, 0, 0x77, 0, 0);
}

TEST(hci_dump, header_setup_btsnoop){
    uint8_t buffer[100];
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, HCI_COMMAND_DATA_PACKET, 0, 0);
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, HCI_ACL_DATA_PACKET, 0, 0);
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, HCI_ACL_DATA_PACKET, 1, 0);
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, HCI_SCO_DATA_PACKET, 0, 0);
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, HCI_EVENT_PACKET, 0, 0);
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, HCI_COMMAND_DATA_PACKET, 0, 0);
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, LOG_MESSAGE_PACKET, 0, 0);
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, 0x77, 0, 0);
}

TEST(hci_dump, header_setup_btsnoop_sco_and_iso_directions){
    uint8_t buffer[100];

    memset(buffer, 0, sizeof(buffer));
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, HCI_SCO_DATA_PACKET, 1, 0);
    CHECK_EQUAL(0x0007, big_endian_read_32(buffer, 8));

    memset(buffer, 0, sizeof(buffer));
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, HCI_ISO_DATA_PACKET, 0, 0);
    CHECK_EQUAL(0x0012, big_endian_read_32(buffer, 8));

    memset(buffer, 0, sizeof(buffer));
    hci_dump_setup_header_btsnoop(buffer, 0, 0, 0, HCI_ISO_DATA_PACKET, 1, 0);
    CHECK_EQUAL(0x0013, big_endian_read_32(buffer, 8));
}

TEST(hci_dump, buffered_explicit_flush){
    uint8_t buffer[16];
    uint8_t packet[] = { 0x01, 0x02, 0x03 };

    hci_dump_buffered_init(&hci_dump_instance_with_reset, buffer, sizeof(buffer), 1000);
    hci_dump_init(hci_dump_buffered_get_instance());

    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, packet, sizeof(packet));
    CHECK_EQUAL(0, logged_packet_count);

    hci_dump_buffered_flush();

    CHECK_EQUAL(1, logged_packet_count);
    CHECK_EQUAL(HCI_COMMAND_DATA_PACKET, logged_packets[0].packet_type);
    CHECK_EQUAL(0, logged_packets[0].in);
    CHECK_EQUAL((int)sizeof(packet), logged_packets[0].len);
    MEMCMP_EQUAL(packet, logged_packets[0].data, sizeof(packet));
}

TEST(hci_dump, buffered_flush_on_timeout){
    uint8_t buffer[16];
    uint8_t packet[] = { 0xaa, 0xbb };

    hci_dump_buffered_init(&hci_dump_instance_with_reset, buffer, sizeof(buffer), 5);
    hci_dump_init(hci_dump_buffered_get_instance());

    hci_dump_packet(HCI_EVENT_PACKET, 1, packet, sizeof(packet));
    CHECK_EQUAL(0, logged_packet_count);

    mock_run_loop_process_timers(5);
    CHECK_EQUAL(0, logged_packet_count);

    mock_run_loop_process_timers(6);
    CHECK_EQUAL(1, logged_packet_count);
    CHECK_EQUAL(HCI_EVENT_PACKET, logged_packets[0].packet_type);
    CHECK_EQUAL(1, logged_packets[0].in);
    MEMCMP_EQUAL(packet, logged_packets[0].data, sizeof(packet));
}

TEST(hci_dump, buffered_flush_on_full_buffer){
    uint8_t buffer[8];
    uint8_t packet_a[] = { 0x11 };
    uint8_t packet_b[] = { 0x22 };

    hci_dump_buffered_init(&hci_dump_instance_with_reset, buffer, sizeof(buffer), 0);
    hci_dump_init(hci_dump_buffered_get_instance());

    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, packet_a, sizeof(packet_a));
    CHECK_EQUAL(0, logged_packet_count);

    hci_dump_packet(HCI_EVENT_PACKET, 1, packet_b, sizeof(packet_b));
    CHECK_EQUAL(1, logged_packet_count);
    CHECK_EQUAL(HCI_COMMAND_DATA_PACKET, logged_packets[0].packet_type);
    MEMCMP_EQUAL(packet_a, logged_packets[0].data, sizeof(packet_a));

    hci_dump_buffered_flush();
    CHECK_EQUAL(2, logged_packet_count);
    CHECK_EQUAL(HCI_EVENT_PACKET, logged_packets[1].packet_type);
    CHECK_EQUAL(1, logged_packets[1].in);
    MEMCMP_EQUAL(packet_b, logged_packets[1].data, sizeof(packet_b));
}

TEST(hci_dump, buffered_forwards_oversized_packet){
    uint8_t buffer[8];
    uint8_t packet[] = { 0, 1, 2, 3, 4 };

    hci_dump_buffered_init(&hci_dump_instance_with_reset, buffer, sizeof(buffer), 1000);
    hci_dump_init(hci_dump_buffered_get_instance());

    hci_dump_packet(HCI_ACL_DATA_PACKET, 1, packet, sizeof(packet));

    CHECK_EQUAL(1, logged_packet_count);
    CHECK_EQUAL(HCI_ACL_DATA_PACKET, logged_packets[0].packet_type);
    CHECK_EQUAL(1, logged_packets[0].in);
    MEMCMP_EQUAL(packet, logged_packets[0].data, sizeof(packet));
}

TEST(hci_dump, buffered_buffers_messages){
    uint8_t buffer[32];

    hci_dump_buffered_init(&hci_dump_instance_with_reset, buffer, sizeof(buffer), 0);
    hci_dump_init(hci_dump_buffered_get_instance());

    hci_dump_log(HCI_DUMP_LOG_LEVEL_INFO, "test %u", 7);
    CHECK_EQUAL(0, logged_packet_count);

    hci_dump_buffered_flush();

    CHECK_EQUAL(1, logged_packet_count);
    CHECK_EQUAL(1, logged_message_count);
    CHECK_EQUAL(LOG_MESSAGE_PACKET, logged_packets[0].packet_type);
    STRCMP_EQUAL("test 7", (const char *) logged_packets[0].data);
}

TEST(hci_dump, buffered_reset_clears_buffer_and_forwards_reset){
    uint8_t buffer[16];
    uint8_t packet[] = { 0x33 };

    hci_dump_buffered_init(&hci_dump_instance_with_reset, buffer, sizeof(buffer), 1000);
    hci_dump_init(hci_dump_buffered_get_instance());
    hci_dump_set_max_packets(1);

    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, packet, sizeof(packet));
    CHECK_EQUAL(0, logged_packet_count);

    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, packet, sizeof(packet));
    CHECK_EQUAL(1, reset_count);
    CHECK_EQUAL(0, logged_packet_count);

    hci_dump_buffered_flush();
    CHECK_EQUAL(1, logged_packet_count);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
