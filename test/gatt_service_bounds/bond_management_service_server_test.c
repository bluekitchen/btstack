#include "read_bounds.h"
#include "ble/gatt-service/bond_management_service_server.c"

int main(void){
    bm_supported_features_value_handle = 1;
    bm_supported_features = 0x123456;
    const uint8_t expected[] = {0x56, 0x34, 0x12};
    check_read_bounds(bond_management_service_read_callback, 1, expected, sizeof(expected));
    return 0;
}
