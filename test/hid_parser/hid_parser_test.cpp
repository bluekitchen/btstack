
// *****************************************************************************
//
// HID Parser Test
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_hid_parser.h"
#include "hci_dump_posix_fs.h"

const uint8_t mouse_descriptor_without_report_id[] = {
    0x05, 0x01, /*  Usage Page (Desktop),               */
    0x09, 0x02, /*  Usage (Mouse),                      */
    0xA1, 0x01, /*  Collection (Application),           */
    0x09, 0x01, /*      Usage (Pointer),                */
    0xA0,       /*      Collection (Physical),          */
    0x05, 0x09, /*          Usage Page (Button),        */
    0x19, 0x01, /*          Usage Minimum (01h),        */
    0x29, 0x03, /*          Usage Maximum (03h),        */
    0x14,       /*          Logical Minimum (0),        */
    0x25, 0x01, /*          Logical Maximum (1),        */
    0x75, 0x01, /*          Report Size (1),            */
    0x95, 0x03, /*          Report Count (3),           */
    0x81, 0x02, /*          Input (Variable),           */
    0x75, 0x05, /*          Report Size (5),            */
    0x95, 0x01, /*          Report Count (1),           */
    0x81, 0x01, /*          Input (Constant),           */
    0x05, 0x01, /*          Usage Page (Desktop),       */
    0x09, 0x30, /*          Usage (X),                  */
    0x09, 0x31, /*          Usage (Y),                  */
    0x15, 0x81, /*          Logical Minimum (-127),     */
    0x25, 0x7F, /*          Logical Maximum (127),      */
    0x75, 0x08, /*          Report Size (8),            */
    0x95, 0x02, /*          Report Count (2),           */
    0x81, 0x06, /*          Input (Variable, Relative), */
    0xC0,       /*      End Collection,                 */
    0xC0        /*  End Collection                      */
};

const uint8_t mouse_descriptor_with_report_id[] = {
    0x05, 0x01, /*  Usage Page (Desktop),               */
    0x09, 0x02, /*  Usage (Mouse),                      */
    0xA1, 0x01, /*  Collection (Application),           */
    
    0x85,  0x01,                    // Report ID 1

    0x09, 0x01, /*      Usage (Pointer),                */
    0xA0,       /*      Collection (Physical),          */
    0x05, 0x09, /*          Usage Page (Button),        */
    0x19, 0x01, /*          Usage Minimum (01h),        */
    0x29, 0x03, /*          Usage Maximum (03h),        */
    0x14,       /*          Logical Minimum (0),        */
    0x25, 0x01, /*          Logical Maximum (1),        */
    0x75, 0x01, /*          Report Size (1),            */
    0x95, 0x03, /*          Report Count (3),           */
    0x81, 0x02, /*          Input (Variable),           */
    0x75, 0x05, /*          Report Size (5),            */
    0x95, 0x01, /*          Report Count (1),           */
    0x81, 0x01, /*          Input (Constant),           */
    0x05, 0x01, /*          Usage Page (Desktop),       */
    0x09, 0x30, /*          Usage (X),                  */
    0x09, 0x31, /*          Usage (Y),                  */
    0x15, 0x81, /*          Logical Minimum (-127),     */
    0x25, 0x7F, /*          Logical Maximum (127),      */
    0x75, 0x08, /*          Report Size (8),            */
    0x95, 0x02, /*          Report Count (2),           */
    0x81, 0x06, /*          Input (Variable, Relative), */
    0xC0,       /*      End Collection,                 */
    0xC0        /*  End Collection                      */
};

// from USB HID Specification 1.1, Appendix B.1
const uint8_t hid_descriptor_keyboard_boot_mode[] = {

    0x05, 0x01,                    // Usage Page (Generic Desktop)
    0x09, 0x06,                    // Usage (Keyboard)
    0xa1, 0x01,                    // Collection (Application)

    // Modifier byte

    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x05, 0x07,                    //   Usage Page (Key codes)
    0x19, 0xe0,                    //   Usage Minimum (Keyboard LeftControl)
    0x29, 0xe7,                    //   Usage Maxium (Keyboard Right GUI)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)

    // Reserved byte

    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x81, 0x03,                    //   Input (Constant, Variable, Absolute)

    // LED report + padding

    0x95, 0x05,                    //   Report Count (5)
    0x75, 0x01,                    //   Report Size (1)
    0x05, 0x08,                    //   Usage Page (LEDs)
    0x19, 0x01,                    //   Usage Minimum (Num Lock)
    0x29, 0x05,                    //   Usage Maxium (Kana)
    0x91, 0x02,                    //   Output (Data, Variable, Absolute)

    0x95, 0x01,                    //   Report Count (1)
    0x75, 0x03,                    //   Report Size (3)
    0x91, 0x03,                    //   Output (Constant, Variable, Absolute)

    // Keycodes

    0x95, 0x06,                    //   Report Count (6)
    0x75, 0x08,                    //   Report Size (8)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0xff,                    //   Logical Maximum (1)
    0x05, 0x07,                    //   Usage Page (Key codes)
    0x19, 0x00,                    //   Usage Minimum (Reserved (no event indicated))
    0x29, 0xff,                    //   Usage Maxium (Reserved)
    0x81, 0x00,                    //   Input (Data, Array)

    0xc0,                          // End collection  
};

const uint8_t combo_descriptor_with_report_ids[] = {

    0x05, 0x01, /*  Usage Page (Desktop),               */
    0x09, 0x02, /*  Usage (Mouse),                      */
    0xA1, 0x01, /*  Collection (Application),           */
    
    0x85, 0x01, // Report ID 1

    0x09, 0x01, /*      Usage (Pointer),                */
    0xA0,       /*      Collection (Physical),          */
    0x05, 0x09, /*          Usage Page (Button),        */
    0x19, 0x01, /*          Usage Minimum (01h),        */
    0x29, 0x03, /*          Usage Maximum (03h),        */
    0x14,       /*          Logical Minimum (0),        */
    0x25, 0x01, /*          Logical Maximum (1),        */
    0x75, 0x01, /*          Report Size (1),            */
    0x95, 0x03, /*          Report Count (3),           */
    0x81, 0x02, /*          Input (Variable),           */
    0x75, 0x05, /*          Report Size (5),            */
    0x95, 0x01, /*          Report Count (1),           */
    0x81, 0x01, /*          Input (Constant),           */
    0x05, 0x01, /*          Usage Page (Desktop),       */
    0x09, 0x30, /*          Usage (X),                  */
    0x09, 0x31, /*          Usage (Y),                  */
    0x15, 0x81, /*          Logical Minimum (-127),     */
    0x25, 0x7F, /*          Logical Maximum (127),      */
    0x75, 0x08, /*          Report Size (8),            */
    0x95, 0x02, /*          Report Count (2),           */
    0x81, 0x06, /*          Input (Variable, Relative), */
    0xC0,       /*      End Collection,                 */
    0xC0,       /*  End Collection                      */

    0xa1, 0x01,                    // Collection (Application)
    
    0x85, 0x02, // Report ID 2

    // Modifier byte

    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x05, 0x07,                    //   Usage Page (Key codes)
    0x19, 0xe0,                    //   Usage Minimum (Keyboard LeftControl)
    0x29, 0xe7,                    //   Usage Maxium (Keyboard Right GUI)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)

    // Reserved byte

    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x81, 0x03,                    //   Input (Constant, Variable, Absolute)

    // LED report + padding

    0x95, 0x05,                    //   Report Count (5)
    0x75, 0x01,                    //   Report Size (1)
    0x05, 0x08,                    //   Usage Page (LEDs)
    0x19, 0x01,                    //   Usage Minimum (Num Lock)
    0x29, 0x05,                    //   Usage Maxium (Kana)
    0x91, 0x02,                    //   Output (Data, Variable, Absolute)

    0x95, 0x01,                    //   Report Count (1)
    0x75, 0x03,                    //   Report Size (3)
    0x91, 0x03,                    //   Output (Constant, Variable, Absolute)

    // Keycodes

    0x95, 0x06,                    //   Report Count (6)
    0x75, 0x08,                    //   Report Size (8)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0xff,                    //   Logical Maximum (1)
    0x05, 0x07,                    //   Usage Page (Key codes)
    0x19, 0x00,                    //   Usage Minimum (Reserved (no event indicated))
    0x29, 0xff,                    //   Usage Maxium (Reserved)
    0x81, 0x00,                    //   Input (Data, Array)

    0xc0,                          // En

};

const uint8_t mouse_report_without_id_positive_xy[]    = {       0x03, 0x02, 0x03 };
const uint8_t mouse_report_without_id_negative_xy[]    = {       0x03, 0xFE, 0xFD };
const uint8_t mouse_report_with_id_1[]    = { 0x01, 0x03, 0x02, 0x03 };

const uint8_t keyboard_report1[] = { 0x01, 0x00, 0x04, 0x05, 0x06, 0x00, 0x00, 0x00 };

const uint8_t combo_report1[]    = { 0x01, 0x03, 0x02, 0x03 };
const uint8_t combo_report2[]    = { 0x02, 0x01, 0x00,  0x04, 0x05, 0x06, 0x00, 0x00, 0x00 };



static void expect_field(btstack_hid_parser_t * parser, uint16_t expected_usage_page, uint16_t expected_usage, int32_t expected_value){
    // printf("expected - usage page %02x, usage %04x, value %02x (bit pos %u)\n", expected_usage_page, expected_usage, expected_value, parser->report_pos_in_bit);
    CHECK_EQUAL(1, btstack_hid_parser_has_more(parser));
    uint16_t usage_page;
    uint16_t usage;
    int32_t value;
    btstack_hid_parser_get_field(parser, &usage_page, &usage, &value);
    CHECK_EQUAL(expected_usage_page, usage_page);
    CHECK_EQUAL(expected_usage, usage);
    CHECK_EQUAL(expected_value, value);
}

// test
TEST_GROUP(HID){
	void setup(void){
    }
};

TEST(HID, MouseWithoutReportID){
    static btstack_hid_parser_t hid_parser;
    btstack_hid_parser_init(&hid_parser, mouse_descriptor_without_report_id, sizeof(mouse_descriptor_without_report_id), HID_REPORT_TYPE_INPUT, mouse_report_without_id_positive_xy, sizeof(mouse_report_without_id_positive_xy));
    expect_field(&hid_parser, 9, 1, 1);
    expect_field(&hid_parser, 9, 2, 1);
    expect_field(&hid_parser, 9, 3, 0);
    expect_field(&hid_parser, 1, 0x30, 2);
    expect_field(&hid_parser, 1, 0x31, 3);
    CHECK_EQUAL(24, hid_parser.report_pos_in_bit);
    CHECK_EQUAL(0, btstack_hid_parser_has_more(&hid_parser));
}

TEST(HID, MouseWithoutReportIDSigned){
    static btstack_hid_parser_t hid_parser;
    btstack_hid_parser_init(&hid_parser, mouse_descriptor_without_report_id, sizeof(mouse_descriptor_without_report_id), HID_REPORT_TYPE_INPUT, mouse_report_without_id_negative_xy, sizeof(mouse_report_without_id_negative_xy));
    expect_field(&hid_parser, 9, 1, 1);
    expect_field(&hid_parser, 9, 2, 1);
    expect_field(&hid_parser, 9, 3, 0);
    expect_field(&hid_parser, 1, 0x30, -2);
    expect_field(&hid_parser, 1, 0x31, -3);
    CHECK_EQUAL(24, hid_parser.report_pos_in_bit);
    CHECK_EQUAL(0, btstack_hid_parser_has_more(&hid_parser));
}

TEST(HID, MouseWithReportID){
    static btstack_hid_parser_t hid_parser;
    btstack_hid_parser_init(&hid_parser, mouse_descriptor_with_report_id, sizeof(mouse_descriptor_with_report_id), HID_REPORT_TYPE_INPUT, mouse_report_with_id_1, sizeof(mouse_report_with_id_1));
    expect_field(&hid_parser, 9, 1, 1);
    expect_field(&hid_parser, 9, 2, 1);
    expect_field(&hid_parser, 9, 3, 0);
    expect_field(&hid_parser, 1, 0x30, 2);
    expect_field(&hid_parser, 1, 0x31, 3);
    CHECK_EQUAL(32, hid_parser.report_pos_in_bit);
    CHECK_EQUAL(0, btstack_hid_parser_has_more(&hid_parser));
}

TEST(HID, BootKeyboard){
    static btstack_hid_parser_t hid_parser;
    btstack_hid_parser_init(&hid_parser, hid_descriptor_keyboard_boot_mode, sizeof(hid_descriptor_keyboard_boot_mode), HID_REPORT_TYPE_INPUT, keyboard_report1, sizeof(keyboard_report1));
    expect_field(&hid_parser, 7, 0xe0, 1);
    expect_field(&hid_parser, 7, 0xe1, 0);
    expect_field(&hid_parser, 7, 0xe2, 0);
    expect_field(&hid_parser, 7, 0xe3, 0);
    expect_field(&hid_parser, 7, 0xe4, 0);
    expect_field(&hid_parser, 7, 0xe5, 0);
    expect_field(&hid_parser, 7, 0xe6, 0);
    expect_field(&hid_parser, 7, 0xe7, 0);
    expect_field(&hid_parser, 7, 0x04, 1);
    expect_field(&hid_parser, 7, 0x05, 1);
    expect_field(&hid_parser, 7, 0x06, 1);
    expect_field(&hid_parser, 7, 0x00, 1);
    expect_field(&hid_parser, 7, 0x00, 1);
    expect_field(&hid_parser, 7, 0x00, 1);
    CHECK_EQUAL(64, hid_parser.report_pos_in_bit);
    CHECK_EQUAL(0, btstack_hid_parser_has_more(&hid_parser));
}

TEST(HID, Combo1){
    static btstack_hid_parser_t hid_parser;
    btstack_hid_parser_init(&hid_parser, combo_descriptor_with_report_ids, sizeof(combo_descriptor_with_report_ids), HID_REPORT_TYPE_INPUT, combo_report1, sizeof(combo_report1));
    expect_field(&hid_parser, 9, 1, 1);
    expect_field(&hid_parser, 9, 2, 1);
    expect_field(&hid_parser, 9, 3, 0);
    expect_field(&hid_parser, 1, 0x30, 2);
    expect_field(&hid_parser, 1, 0x31, 3);
    CHECK_EQUAL(32, hid_parser.report_pos_in_bit);
    CHECK_EQUAL(0, btstack_hid_parser_has_more(&hid_parser));
}

TEST(HID, Combo2){
    static btstack_hid_parser_t hid_parser;
    btstack_hid_parser_init(&hid_parser, combo_descriptor_with_report_ids, sizeof(combo_descriptor_with_report_ids), HID_REPORT_TYPE_INPUT, combo_report2, sizeof(combo_report2));
    expect_field(&hid_parser, 7, 0xe0, 1);
    expect_field(&hid_parser, 7, 0xe1, 0);
    expect_field(&hid_parser, 7, 0xe2, 0);
    expect_field(&hid_parser, 7, 0xe3, 0);
    expect_field(&hid_parser, 7, 0xe4, 0);
    expect_field(&hid_parser, 7, 0xe5, 0);
    expect_field(&hid_parser, 7, 0xe6, 0);
    expect_field(&hid_parser, 7, 0xe7, 0);
    expect_field(&hid_parser, 7, 0x04, 1);
    expect_field(&hid_parser, 7, 0x05, 1);
    expect_field(&hid_parser, 7, 0x06, 1);
    expect_field(&hid_parser, 7, 0x00, 1);
    expect_field(&hid_parser, 7, 0x00, 1);
    expect_field(&hid_parser, 7, 0x00, 1);
    CHECK_EQUAL(72, hid_parser.report_pos_in_bit);
    CHECK_EQUAL(0, btstack_hid_parser_has_more(&hid_parser));
}

TEST(HID, GetReportSize){
    int report_size = 0;
    const uint8_t * hid_descriptor =  combo_descriptor_with_report_ids;
    uint16_t hid_descriptor_len = sizeof(combo_descriptor_with_report_ids);
    report_size = btstack_hid_get_report_size_for_id(1, HID_REPORT_TYPE_INPUT, hid_descriptor_len, hid_descriptor);
    CHECK_EQUAL(3, report_size);

    hid_descriptor = hid_descriptor_keyboard_boot_mode;
    hid_descriptor_len = sizeof(hid_descriptor_keyboard_boot_mode);
    report_size = btstack_hid_get_report_size_for_id(0, HID_REPORT_TYPE_OUTPUT, hid_descriptor_len, hid_descriptor);
    CHECK_EQUAL(1, report_size);
    report_size = btstack_hid_get_report_size_for_id(0, HID_REPORT_TYPE_INPUT, hid_descriptor_len, hid_descriptor);
    CHECK_EQUAL(8, report_size);
}


int main (int argc, const char * argv[]){
    // log into file using HCI_DUMP_PACKETLOGGER format
    const char * pklg_path = "hci_dump.pklg";
    hci_dump_posix_fs_open(pklg_path, HCI_DUMP_PACKETLOGGER);
    hci_dump_init(hci_dump_posix_fs_get_instance());
    printf("Packet Log: %s\n", pklg_path);

    return CommandLineTestRunner::RunAllTests(argc, argv);
}
