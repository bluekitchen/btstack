#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "l2cap_signaling.h"

TEST_GROUP(L2CAP_LE_Signaling){
    void setup(void){
    }
    void teardown(void){
    }
};

static uint16_t l2cap_send_le_signaling_packet(hci_con_handle_t handle, L2CAP_SIGNALING_COMMANDS cmd, int identifier, ...){
    static uint8_t acl_buffer[100];
    va_list argptr;
    va_start(argptr, identifier);
    uint16_t len = l2cap_create_signaling_packet(acl_buffer, handle, 0x00, L2CAP_CID_SIGNALING_LE, cmd, identifier, argptr);
    va_end(argptr);
    return len;
}

#if 0
// invalid cmds trigger assert
TEST(L2CAP_LE_Signaling, l2cap_create_signaling_le_invalid_cmd){
    uint16_t size = l2cap_send_le_signaling_packet((hci_con_handle_t) 0x01, (L2CAP_SIGNALING_COMMANDS)(CONNECTION_PARAMETER_UPDATE_REQUEST-1), 1);
    CHECK_EQUAL(0, size);
}

TEST(L2CAP_LE_Signaling, l2cap_create_signaling_le_invalid_cmd_0){
    uint16_t size = l2cap_send_le_signaling_packet((hci_con_handle_t) 0x01, (L2CAP_SIGNALING_COMMANDS)0, 1);
    CHECK_EQUAL(0, size);
}

TEST(L2CAP_LE_Signaling, l2cap_create_signaling_le_invalid_cmd_FF){
    uint16_t size = l2cap_send_le_signaling_packet((hci_con_handle_t) 0x01, (L2CAP_SIGNALING_COMMANDS)0xFF, 1);
    CHECK_EQUAL(0, size);
}
#endif

TEST(L2CAP_LE_Signaling, l2cap_create_signaling_le_invalid_cmd_M_format){
    uint16_t size = l2cap_send_le_signaling_packet((hci_con_handle_t) 0x01, COMMAND_WITH_INVALID_FORMAT, 1);
    CHECK_EQUAL(12, size);
}

TEST(L2CAP_LE_Signaling, l2cap_create_signaling_le_valid_cmd_2_format){
    uint16_t size = l2cap_send_le_signaling_packet((hci_con_handle_t) 0x01, INFORMATION_REQUEST, 1, 0);
    CHECK_EQUAL(14, size);
}

TEST(L2CAP_LE_Signaling, l2cap_create_signaling_le_valid_cmd_C_format){
    uint16_t cids[2] = { 0x3333, 0xffff };
    uint16_t size = l2cap_send_le_signaling_packet((hci_con_handle_t) 0x01, L2CAP_CREDIT_BASED_RECONFIGURE_REQUEST, 1, 0x1111, 0x2222, cids);
    CHECK_EQUAL(18, size);
}

TEST(L2CAP_LE_Signaling, l2cap_create_signaling_le_valid_cmd_D_format){
    uint16_t size = l2cap_send_le_signaling_packet((hci_con_handle_t) 0x01, ECHO_REQUEST, 1, 0, NULL);
    CHECK_EQUAL(12, size);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
