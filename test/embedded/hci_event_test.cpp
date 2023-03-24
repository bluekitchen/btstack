#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hci_event.h"
#include "btstack_util.h"

uint8_t hci_event_buffer[257];
hci_event_t event_1 = { 1, 0, "1"};
hci_event_t event_2 = { 1, 0, "2"};
hci_event_t event_3 = { 1, 0, "3"};
hci_event_t event_4 = { 1, 0, "4"};

static uint16_t create_hci_event(const hci_event_t *event, ...){
    va_list argptr;
    va_start(argptr, event);
    uint16_t len = hci_event_create_from_template_and_arglist(hci_event_buffer, sizeof(hci_event_buffer), event, argptr);
    va_end(argptr);
    return len - 2;
}

TEST_GROUP(hci_event){
    void setup(void){
    }
    void teardown(void){
    }
};

TEST(hci_event, format_1){
    const uint8_t expected_buffer[] = { 0x55};
    uint8_t value = 0x55;
    uint16_t size = create_hci_event(&event_1, value);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_event_buffer[2], sizeof(expected_buffer));
}

TEST(hci_event, format_2){
    const uint8_t expected_buffer[] = { 0x34, 0x12 };
    uint16_t value = 0x1234;
    uint16_t size = create_hci_event(&event_2, value);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_event_buffer[2], sizeof(expected_buffer));
}

TEST(hci_event, format_3){
    const uint8_t expected_buffer[] = { 0x56, 0x34, 0x12};
    uint32_t value = 0x123456;
    uint16_t size = create_hci_event(&event_3, value);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_event_buffer[2], sizeof(expected_buffer));
}

TEST(hci_event, format_4){
    const uint8_t expected_buffer[] = { 0x78, 0x56, 0x34, 0x12};
    uint32_t value = 0x12345678;
    uint16_t size = create_hci_event(&event_4, value);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_event_buffer[2], sizeof(expected_buffer));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
