#include "read_bounds.h"
#include "ble/gatt-service/hids_device.c"

static void get_report(hci_con_handle_t handle, hid_report_type_t type, uint16_t id, uint16_t max_size, uint8_t * buffer){
    UNUSED(handle); UNUSED(type); UNUSED(id);
    memset(buffer, 0x5a, max_size);
}

int main(void){
    hids_device.hid_boot_mouse_input_value_handle = 1;
    hids_device.hid_boot_keyboard_input_value_handle = 2;
    hids_device.hid_boot_keyboard_output_value_handle = 3;
    hids_device.hid_control_point_value_handle = 4;
    hids_device_get_report_callback = get_report;
    const uint16_t sizes[] = {3, 8, 1};
    for (unsigned i = 0; i < 3; i++){
        assert(att_read_callback(1, i + 1, 0, NULL, 0) == sizes[i]);
        for (uint16_t capacity = 0; capacity <= sizes[i] + 1; capacity++){
            uint8_t * buffer = malloc(capacity + 1u);
            memset(buffer, 0xa5, capacity + 1u);
            uint16_t copied = att_read_callback(1, i + 1, 0, buffer, capacity);
            assert(buffer[capacity] == 0xa5);
            assert(copied == (capacity < sizes[i] ? capacity : sizes[i]));
            for (unsigned j = 0; j < copied; j++) assert(buffer[j] == 0x5a);
            free(buffer);
        }
    }
    const uint8_t control[] = {0};
    check_read_bounds(att_read_callback, 4, control, sizeof(control));
    hids_device_report_t report = {0};
    report.value_handle = 5;
    report.size = 64;
    hids_device.hid_reports = &report;
    hids_device.hid_input_reports_num = 1;
    uint8_t buffer[1];
    assert(att_read_callback(1, 5, 0, NULL, 0) == 64);
    assert(att_read_callback(1, 5, 0, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(buffer[0] == 0x5a);
    return 0;
}
