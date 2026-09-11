#include "read_bounds.h"
#include "ble/gatt-service/nordic_spp_service_server.c"

int main(void){
    nordic_spp_tx_client_configuration_handle = 1;
    nordic_spp_tx_client_configuration_value = 0x1234;
    const uint8_t expected[] = {0x34, 0x12};
    check_read_bounds(nordic_spp_service_read_callback, 1, expected, sizeof(expected));
    return 0;
}
