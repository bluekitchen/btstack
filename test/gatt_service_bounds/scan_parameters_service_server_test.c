#include "read_bounds.h"
#include "ble/gatt-service/scan_parameters_service_server.c"

int main(void){
    scan_refresh_value_handle_client_configuration = 1;
    scan_refresh_value_client_configuration = 0x1234;
    const uint8_t expected[] = {0x34, 0x12};
    check_read_bounds(scan_parameters_service_read_callback, 1, expected, sizeof(expected));
    return 0;
}
