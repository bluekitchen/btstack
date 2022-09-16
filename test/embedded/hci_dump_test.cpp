#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hci_dump.h"
#include "btstack_util.h"

static void hci_dump_embedded_stdout_log_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len){
}
static void hci_dump_embedded_stdout_log_message(int log_level, const char * format, va_list argptr){
}
static void hci_dump_embedded_stdout_reset(void){}

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
        }
        void teardown(void){
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
    hci_dump_setup_header_packetlogger(buffer, 0, 0, HCI_EVENT_PACKET, 0, 0);
    hci_dump_setup_header_packetlogger(buffer, 0, 0, HCI_COMMAND_DATA_PACKET, 0, 0);
    hci_dump_setup_header_packetlogger(buffer, 0, 0, LOG_MESSAGE_PACKET, 0, 0);
    hci_dump_setup_header_packetlogger(buffer, 0, 0, 0x77, 0, 0);
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

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
