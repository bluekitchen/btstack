/**
 * bluetooth_data_types.h generated from Bluetooth SIG website for BTstack by tool/bluetooth_data_types.py
 * https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile
 * 2018-10-05 15:01:05.830879
 */

#ifndef BLUETOOTH_DATA_TYPES_H
#define BLUETOOTH_DATA_TYPES_H

#define BLUETOOTH_DATA_TYPE_FLAGS                                              0x01 // Flags
#define BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS      0x02 // Incomplete List of 16-bit Service Class UUIDs
#define BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS        0x03 // Complete List of 16-bit Service Class UUIDs
#define BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_32_BIT_SERVICE_CLASS_UUIDS      0x04 // Incomplete List of 32-bit Service Class UUIDs
#define BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_32_BIT_SERVICE_CLASS_UUIDS        0x05 // Complete List of 32-bit Service Class UUIDs
#define BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS     0x06 // Incomplete List of 128-bit Service Class UUIDs
#define BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS       0x07 // Complete List of 128-bit Service Class UUIDs
#define BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME                               0x08 // Shortened Local Name
#define BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME                                0x09 // Complete Local Name
#define BLUETOOTH_DATA_TYPE_TX_POWER_LEVEL                                     0x0A // Tx Power Level
#define BLUETOOTH_DATA_TYPE_CLASS_OF_DEVICE                                    0x0D // Class of Device
#define BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_HASH_C                              0x0E // Simple Pairing Hash C
#define BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_HASH_C_192                          0x0E // Simple Pairing Hash C-192
#define BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_RANDOMIZER_R                        0x0F // Simple Pairing Randomizer R
#define BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_RANDOMIZER_R_192                    0x0F // Simple Pairing Randomizer R-192
#define BLUETOOTH_DATA_TYPE_DEVICE_ID                                          0x10 // Device ID
#define BLUETOOTH_DATA_TYPE_SECURITY_MANAGER_TK_VALUE                          0x10 // Security Manager TK Value
#define BLUETOOTH_DATA_TYPE_SECURITY_MANAGER_OUT_OF_BAND_FLAGS                 0x11 // Security Manager Out of Band Flags
#define BLUETOOTH_DATA_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE                    0x12 // Slave Connection Interval Range
#define BLUETOOTH_DATA_TYPE_LIST_OF_16_BIT_SERVICE_SOLICITATION_UUIDS          0x14 // List of 16-bit Service Solicitation UUIDs
#define BLUETOOTH_DATA_TYPE_LIST_OF_128_BIT_SERVICE_SOLICITATION_UUIDS         0x15 // List of 128-bit Service Solicitation UUIDs
#define BLUETOOTH_DATA_TYPE_SERVICE_DATA                                       0x16 // Service Data
#define BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID                           0x16 // Service Data - 16-bit UUID
#define BLUETOOTH_DATA_TYPE_PUBLIC_TARGET_ADDRESS                              0x17 // Public Target Address
#define BLUETOOTH_DATA_TYPE_RANDOM_TARGET_ADDRESS                              0x18 // Random Target Address
#define BLUETOOTH_DATA_TYPE_APPEARANCE                                         0x19 // Appearance
#define BLUETOOTH_DATA_TYPE_ADVERTISING_INTERVAL                               0x1A // Advertising Interval
#define BLUETOOTH_DATA_TYPE_LE_BLUETOOTH_DEVICE_ADDRESS                        0x1B // LE Bluetooth Device Address
#define BLUETOOTH_DATA_TYPE_LE_ROLE                                            0x1C // LE Role
#define BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_HASH_C_256                          0x1D // Simple Pairing Hash C-256
#define BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_RANDOMIZER_R_256                    0x1E // Simple Pairing Randomizer R-256
#define BLUETOOTH_DATA_TYPE_LIST_OF_32_BIT_SERVICE_SOLICITATION_UUIDS          0x1F // List of 32-bit Service Solicitation UUIDs
#define BLUETOOTH_DATA_TYPE_SERVICE_DATA_32_BIT_UUID                           0x20 // Service Data - 32-bit UUID
#define BLUETOOTH_DATA_TYPE_SERVICE_DATA_128_BIT_UUID                          0x21 // Service Data - 128-bit UUID
#define BLUETOOTH_DATA_TYPE_LE_SECURE_CONNECTIONS_CONFIRMATION_VALUE           0x22 // LE Secure Connections Confirmation Value
#define BLUETOOTH_DATA_TYPE_LE_SECURE_CONNECTIONS_RANDOM_VALUE                 0x23 // LE Secure Connections Random Value
#define BLUETOOTH_DATA_TYPE_URI                                                0x24 // URI
#define BLUETOOTH_DATA_TYPE_INDOOR_POSITIONING                                 0x25 // Indoor Positioning
#define BLUETOOTH_DATA_TYPE_TRANSPORT_DISCOVERY_DATA                           0x26 // Transport Discovery Data
#define BLUETOOTH_DATA_TYPE_LE_SUPPORTED_FEATURES                              0x27 // LE Supported Features
#define BLUETOOTH_DATA_TYPE_CHANNEL_MAP_UPDATE_INDICATION                      0x28 // Channel Map Update Indication
#define BLUETOOTH_DATA_TYPE_PB_ADV                                             0x29 // PB-ADV
#define BLUETOOTH_DATA_TYPE_MESH_MESSAGE                                       0x2A // Mesh Message
#define BLUETOOTH_DATA_TYPE_MESH_BEACON                                        0x2B // Mesh Beacon
#define BLUETOOTH_DATA_TYPE_3D_INFORMATION_DATA                                0x3D // 3D Information Data
#define BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA                         0xFF // Manufacturer Specific Data

#endif
