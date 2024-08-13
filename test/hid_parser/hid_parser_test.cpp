
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
#include "hci_dump_posix_stdout.h"

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

// https://tank-mouse.com/
const uint8_t tank_mouse_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x03,        //   Report ID (3)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x03,        //     Usage Maximum (0x03)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x05,        //     Report Count (5)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x03,        //     Report Size (3)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x01,        //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x16, 0x01, 0x80,  //     Logical Minimum (-32767)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0xC0,              // End Collection
    0x06, 0x01, 0xFF,  // Usage Page (Vendor Defined 0xFF01)
    0x09, 0x01,        // Usage (0x01)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x05,        //   Report ID (5)
    0x09, 0x05,        //   Usage (0x05)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              // End Collection
    // 89 bytes
};

// Xbox Wireless Controller FW 5.17.3202.0
const uint8_t xbox_wireless_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x15, 0x00,        //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
    0x95, 0x02,        //     Report Count (2)
    0x75, 0x10,        //     Report Size (16)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x09, 0x32,        //     Usage (Z)
    0x09, 0x35,        //     Usage (Rz)
    0x15, 0x00,        //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
    0x95, 0x02,        //     Report Count (2)
    0x75, 0x10,        //     Report Size (16)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x02,        //   Usage Page (Sim Ctrls)
    0x09, 0xC5,        //   Usage (Brake)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x0A,        //   Report Size (10)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x06,        //   Report Size (6)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x02,        //   Usage Page (Sim Ctrls)
    0x09, 0xC4,        //   Usage (Accelerator)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x0A,        //   Report Size (10)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x06,        //   Report Size (6)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x01,        //   Logical Minimum (1)
    0x25, 0x08,        //   Logical Maximum (8)
    0x35, 0x00,        //   Physical Minimum (0)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x66, 0x14, 0x00,  //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x35, 0x00,        //   Physical Minimum (0)
    0x45, 0x00,        //   Physical Maximum (0)
    0x65, 0x00,        //   Unit (None)
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x0F,        //   Usage Maximum (0x0F)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x0F,        //   Report Count (15)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0C,        //   Usage Page (Consumer)
    0x0A, 0xB2, 0x00,  //   Usage (Record)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x01,        //   Report Size (1)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x07,        //   Report Size (7)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0F,        //   Usage Page (PID Page)
    0x09, 0x21,        //   Usage (0x21)
    0x85, 0x03,        //   Report ID (3)
    0xA1, 0x02,        //   Collection (Logical)
    0x09, 0x97,        //     Usage (0x97)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x00,        //     Logical Maximum (0)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x03,        //     Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x70,        //     Usage (0x70)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x64,        //     Logical Maximum (100)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x04,        //     Report Count (4)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x50,        //     Usage (0x50)
    0x66, 0x01, 0x10,  //     Unit (System: SI Linear, Time: Seconds)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0xA7,        //     Usage (0xA7)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x65, 0x00,        //     Unit (None)
    0x55, 0x00,        //     Unit Exponent (0)
    0x09, 0x7C,        //     Usage (0x7C)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xC0,              // End Collection
    // 283 bytes
};

const uint8_t mouse_report_without_id_positive_xy[]    = {       0x03, 0x02, 0x03 };
const uint8_t mouse_report_without_id_negative_xy[]    = {       0x03, 0xFE, 0xFD };
const uint8_t mouse_report_with_id_1[]    = { 0x01, 0x03, 0x02, 0x03 };

const uint8_t keyboard_report1[] = { 0x01, 0x00, 0x04, 0x05, 0x06, 0x00, 0x00, 0x00 };

const uint8_t combo_report1[]    = { 0x01, 0x03, 0x02, 0x03 };
const uint8_t combo_report2[]    = { 0x02, 0x01, 0x00,  0x04, 0x05, 0x06, 0x00, 0x00, 0x00 };

const uint8_t tank_mouse_report[] = { 0x03, 0x03, 0xFD, 0xFF, 0xF6, 0xFF, 0x00};
const uint8_t xbox_wireless_report[] = {0x01, 0xB9, 0xF0, 0xFB, 0xC3, 0xE3, 0x80, 0x97, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00};


static void expect_field(btstack_hid_parser_t * parser, uint16_t expected_usage_page, uint16_t expected_usage, int32_t expected_value){
    // printf("expected - usage page %02x, usage %04x, value %02x\n", expected_usage_page, expected_usage, expected_value);
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
    CHECK_EQUAL(0, btstack_hid_parser_has_more(&hid_parser));
}

TEST(HID, MouseWithReportID){
    static btstack_hid_parser_t hid_parser;
    CHECK_EQUAL(3, btstack_hid_get_report_size_for_id(1, HID_REPORT_TYPE_INPUT, mouse_descriptor_with_report_id,
                                                      sizeof(mouse_descriptor_with_report_id)));
    btstack_hid_parser_init(&hid_parser, mouse_descriptor_with_report_id, sizeof(mouse_descriptor_with_report_id), HID_REPORT_TYPE_INPUT, mouse_report_with_id_1, sizeof(mouse_report_with_id_1));
    expect_field(&hid_parser, 9, 1, 1);
    expect_field(&hid_parser, 9, 2, 1);
    expect_field(&hid_parser, 9, 3, 0);
    expect_field(&hid_parser, 1, 0x30, 2);
    expect_field(&hid_parser, 1, 0x31, 3);
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
    CHECK_EQUAL(0, btstack_hid_parser_has_more(&hid_parser));
}

TEST(HID, TankMouse){
    static btstack_hid_parser_t hid_parser;
    CHECK_EQUAL(6, btstack_hid_get_report_size_for_id(3, HID_REPORT_TYPE_INPUT, tank_mouse_descriptor,
                                                      sizeof(tank_mouse_descriptor)));
    btstack_hid_parser_init(&hid_parser, tank_mouse_descriptor, sizeof(tank_mouse_descriptor), HID_REPORT_TYPE_INPUT, tank_mouse_report, sizeof(tank_mouse_report));
    expect_field(&hid_parser, 9, 1, 1);
    expect_field(&hid_parser, 9, 2, 1);
    expect_field(&hid_parser, 9, 3, 0);
    // no usages for 4th and 5th report as usage min..max only allows for 3 button
    expect_field(&hid_parser, 1, 0x30, -3);
    expect_field(&hid_parser, 1, 0x31, -10);
    expect_field(&hid_parser, 1, 0x38, 0);
    CHECK_EQUAL(0, btstack_hid_parser_has_more(&hid_parser));
}

TEST(HID, XboxWireless){
    static btstack_hid_parser_t hid_parser;
    CHECK_EQUAL(16, btstack_hid_get_report_size_for_id(1, HID_REPORT_TYPE_INPUT, xbox_wireless_descriptor,
                                                       sizeof(xbox_wireless_descriptor)));
    btstack_hid_parser_init(&hid_parser, xbox_wireless_descriptor, sizeof(xbox_wireless_descriptor), HID_REPORT_TYPE_INPUT, xbox_wireless_report, sizeof(xbox_wireless_report));
    expect_field(&hid_parser, 1, 0x30, 0xf0b9);
    expect_field(&hid_parser, 1, 0x31, 0xc3fb);
    expect_field(&hid_parser, 1, 0x32, 0x80e3);
    expect_field(&hid_parser, 1, 0x35, 0x8197);
    expect_field(&hid_parser, 2, 0xc5, 0);
    expect_field(&hid_parser, 2, 0xc4, 0);
    expect_field(&hid_parser, 1, 0x39, 0);
    expect_field(&hid_parser, 9, 1, 0);
    expect_field(&hid_parser, 9, 2, 0);
    expect_field(&hid_parser, 9, 3, 0);
    expect_field(&hid_parser, 9, 4, 0);
    expect_field(&hid_parser, 9, 5, 0);
    expect_field(&hid_parser, 9, 6, 0);
    expect_field(&hid_parser, 9, 7, 0);
    expect_field(&hid_parser, 9, 8, 0);
    expect_field(&hid_parser, 9, 9, 0);
    expect_field(&hid_parser, 9, 10, 0);
    expect_field(&hid_parser, 9, 11, 0);
    expect_field(&hid_parser, 9, 12, 0);
    expect_field(&hid_parser, 9, 13, 0);
    expect_field(&hid_parser, 9, 14, 1);
    expect_field(&hid_parser, 9, 15, 0);
    expect_field(&hid_parser, 12, 0xb2, 0);
    CHECK_EQUAL(0, btstack_hid_parser_has_more(&hid_parser));
}

TEST(HID, GetReportSize){
    int report_size = 0;
    const uint8_t * hid_descriptor =  combo_descriptor_with_report_ids;
    uint16_t hid_descriptor_len = sizeof(combo_descriptor_with_report_ids);
    report_size = btstack_hid_get_report_size_for_id(1, HID_REPORT_TYPE_INPUT, hid_descriptor, hid_descriptor_len);
    CHECK_EQUAL(3, report_size);

    hid_descriptor = hid_descriptor_keyboard_boot_mode;
    hid_descriptor_len = sizeof(hid_descriptor_keyboard_boot_mode);
    report_size = btstack_hid_get_report_size_for_id(HID_REPORT_ID_UNDEFINED, HID_REPORT_TYPE_OUTPUT, hid_descriptor,
                                                     hid_descriptor_len);
    CHECK_EQUAL(1, report_size);
    report_size = btstack_hid_get_report_size_for_id(HID_REPORT_ID_UNDEFINED, HID_REPORT_TYPE_INPUT, hid_descriptor,
                                                     hid_descriptor_len);
    CHECK_EQUAL(8, report_size);
}

TEST(HID, UsageIteratorBootKeyboard){
    const uint8_t * hid_descriptor =  hid_descriptor_keyboard_boot_mode;
    uint16_t hid_descriptor_len = sizeof(hid_descriptor_keyboard_boot_mode);
    btstack_hid_usage_iterator_t iterator;
    btstack_hid_usage_iterator_init(&iterator, hid_descriptor, hid_descriptor_len, HID_REPORT_TYPE_INPUT);
    while (btstack_hid_usage_iterator_has_more(&iterator)){
        btstack_hid_usage_item_t item;
        btstack_hid_usage_iterator_get_item(&iterator, &item);
        // printf("Report ID 0x%04x, bitpos %3u, usage page 0x%04x, usage 0x%04x, size %u\n", item.report_id, item.bit_pos, item.usage_page, item.usage, item.size);
    }
}

TEST(HID, UsageIteratorCombo1){
    const uint8_t * hid_descriptor =  combo_descriptor_with_report_ids;
    uint16_t hid_descriptor_len = sizeof(combo_descriptor_with_report_ids);
    btstack_hid_usage_iterator_t iterator;
    btstack_hid_usage_iterator_init(&iterator, hid_descriptor, hid_descriptor_len, HID_REPORT_TYPE_INPUT);
    while (btstack_hid_usage_iterator_has_more(&iterator)){
        btstack_hid_usage_item_t item;
        btstack_hid_usage_iterator_get_item(&iterator, &item);
        // printf("Report ID 0x%04x, bitpos %3u, usage page 0x%04x, usage 0x%04x, size %u\n", item.report_id, item.bit_pos, item.usage_page, item.usage, item.size);
    }
}

TEST(HID, ValidateReportIdBootKeyboard){
    const uint8_t * hid_descriptor =  hid_descriptor_keyboard_boot_mode;
    uint16_t hid_descriptor_len = sizeof(hid_descriptor_keyboard_boot_mode);
    CHECK_EQUAL(HID_REPORT_ID_VALID, btstack_hid_report_id_valid(HID_REPORT_ID_UNDEFINED, hid_descriptor, hid_descriptor_len));
    CHECK_EQUAL(HID_REPORT_ID_UNDECLARED, btstack_hid_report_id_valid(1, hid_descriptor, hid_descriptor_len));
}

TEST(HID, ValidateReportIdMouseWithReportId){
    const uint8_t * hid_descriptor =  mouse_descriptor_with_report_id;
    uint16_t hid_descriptor_len = sizeof(mouse_descriptor_with_report_id);
    CHECK_EQUAL(HID_REPORT_ID_UNDECLARED, btstack_hid_report_id_valid(HID_REPORT_ID_UNDEFINED, hid_descriptor, hid_descriptor_len));
    CHECK_EQUAL(HID_REPORT_ID_VALID, btstack_hid_report_id_valid(1, hid_descriptor, hid_descriptor_len));
}

int main (int argc, const char * argv[]){
#if 1
    // log into file using HCI_DUMP_PACKETLOGGER format
    const char * pklg_path = "hci_dump.pklg";
    hci_dump_posix_fs_open(pklg_path, HCI_DUMP_PACKETLOGGER);
    hci_dump_init(hci_dump_posix_fs_get_instance());
    printf("Packet Log: %s\n", pklg_path);
#else
    hci_dump_init(hci_dump_posix_stdout_get_instance());
#endif
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
