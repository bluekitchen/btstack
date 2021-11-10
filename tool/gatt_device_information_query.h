
// gatt_device_information_query.h generated from ../example/gatt_device_information_query.gatt for BTstack
// it needs to be regenerated when the .gatt file is updated. 

// To generate gatt_device_information_query.h:
// ./compile_gatt.py ../example/gatt_device_information_query.gatt gatt_device_information_query.h

// att db format version 1

// binary attribute representation:
// - size in bytes (16), flags(16), handle (16), uuid (16/128), value(...)

#include <stdint.h>

// Reference: https://en.cppreference.com/w/cpp/feature_test
#if __cplusplus >= 200704L
constexpr
#endif
const uint8_t profile_data[] =
{
    // ATT DB Version
    1,

    // 0x0001 PRIMARY_SERVICE-GAP_SERVICE
    0x0a, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x28, 0x00, 0x18, 
    // 0x0002 CHARACTERISTIC-GAP_DEVICE_NAME-READ
    0x0d, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x28, 0x02, 0x03, 0x00, 0x00, 0x2a, 
    // 0x0003 VALUE-GAP_DEVICE_NAME-READ-'GATT Device Informaition Query'
    // READ_ANYBODY
    0x26, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x2a, 0x47, 0x41, 0x54, 0x54, 0x20, 0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x20, 0x49, 0x6e, 0x66, 0x6f, 0x72, 0x6d, 0x61, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x51, 0x75, 0x65, 0x72, 0x79, 

    // END
    0x00, 0x00, 
}; // total size 25 bytes 


//
// list service handle ranges
//
#define ATT_SERVICE_GAP_SERVICE_START_HANDLE 0x0001
#define ATT_SERVICE_GAP_SERVICE_END_HANDLE 0x0003
#define ATT_SERVICE_GAP_SERVICE_01_START_HANDLE 0x0001
#define ATT_SERVICE_GAP_SERVICE_01_END_HANDLE 0x0003

//
// list mapping between characteristics and handles
//
#define ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_VALUE_HANDLE 0x0003
