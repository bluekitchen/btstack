/*****************************************************************************
 * @file    ble_defs.h
 * @author  MCD Application Team
 * @brief   This file contains definitions used for BLE Stack interface.
 *****************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */

#ifndef BLE_DEFS_H__
#define BLE_DEFS_H__


/* ------------------------------------------------------------------------- */


/* Status codes */
#define BLE_STATUS_SUCCESS                          0x00
#define BLE_STATUS_FAILED                           0x41
#define BLE_STATUS_INVALID_PARAMS                   0x42
#define BLE_STATUS_BUSY                             0x43
#define BLE_STATUS_INVALID_LEN_PDU                  0x44
#define BLE_STATUS_PENDING                          0x45
#define BLE_STATUS_NOT_ALLOWED                      0x46
#define BLE_STATUS_ERROR                            0x47
#define BLE_STATUS_ADDR_NOT_RESOLVED                0x48
#define BLE_STATUS_INVALID_CID                      0x50
#define TIMER_NOT_VALID_LAYER                       0x54
#define TIMER_INSUFFICIENT_RESOURCES                0x55
#define BLE_STATUS_CSRK_NOT_FOUND                   0x5A
#define BLE_STATUS_IRK_NOT_FOUND                    0x5B
#define BLE_STATUS_DEV_NOT_FOUND_IN_DB              0x5C
#define BLE_STATUS_SEC_DB_FULL                      0x5D
#define BLE_STATUS_DEV_NOT_BONDED                   0x5E
#define BLE_STATUS_DEV_IN_BLACKLIST                 0x5F
#define BLE_STATUS_INVALID_HANDLE                   0x60
#define BLE_STATUS_INVALID_PARAMETER                0x61
#define BLE_STATUS_OUT_OF_HANDLE                    0x62
#define BLE_STATUS_INVALID_OPERATION                0x63
#define BLE_STATUS_INSUFFICIENT_RESOURCES           0x64
#define BLE_INSUFFICIENT_ENC_KEYSIZE                0x65
#define BLE_STATUS_CHARAC_ALREADY_EXISTS            0x66

/* Returned when no valid slots are available
 * (e.g. when there are no available state machines).
 */
#define BLE_STATUS_NO_VALID_SLOT                    0x82

/* Returned when a scan window shorter than minimum allowed value has been
 * requested (i.e. 2ms)
 */
#define BLE_STATUS_SCAN_WINDOW_SHORT                0x83

/* Returned when the maximum requested interval to be allocated is shorter
 * then the current anchor period and a there is no submultiple for the
 * current anchor period that is between the minimum and the maximum requested
 * intervals.
 */
#define BLE_STATUS_NEW_INTERVAL_FAILED              0x84

/* Returned when the maximum requested interval to be allocated is greater
 * than the current anchor period and there is no multiple of the anchor
 * period that is between the minimum and the maximum requested intervals.
 */
#define BLE_STATUS_INTERVAL_TOO_LARGE               0x85

/* Returned when the current anchor period or a new one can be found that
 * is compatible to the interval range requested by the new slot but the
 * maximum available length that can be allocated is less than the minimum
 * requested slot length.
 */
#define BLE_STATUS_LENGTH_FAILED                    0x86

/*
 * Library Error Codes
 */
#define BLE_STATUS_TIMEOUT                          0xFF
#define BLE_STATUS_PROFILE_ALREADY_INITIALIZED      0xF0
#define BLE_STATUS_NULL_PARAM                       0xF1 


/* ------------------------------------------------------------------------- */


/* GAP UUIDs
 */
#define GAP_SERVICE_UUID                           0x1800
#define DEVICE_NAME_UUID                           0x2A00
#define APPEARANCE_UUID                            0x2A01
#define PERIPHERAL_PRIVACY_FLAG_UUID               0x2A02
#define RECONNECTION_ADDR_UUID                     0x2A03
#define PERIPHERAL_PREFERRED_CONN_PARAMS_UUID      0x2A04


/* Characteristic value lengths
 */
#define DEVICE_NAME_CHARACTERISTIC_LEN                  8
#define APPEARANCE_CHARACTERISTIC_LEN                   2
#define PERIPHERAL_PRIVACY_CHARACTERISTIC_LEN           1
#define RECONNECTION_ADDR_CHARACTERISTIC_LEN            6
#define PERIPHERAL_PREF_CONN_PARAMS_CHARACTERISTIC_LEN  8


/* Adv. lengths
 */
#define MAX_ADV_DATA_LEN                               31
#define DEVICE_NAME_LEN                                 7
#define BD_ADDR_SIZE                                    6


/* AD types for adv. data and scan response data
 */
#define AD_TYPE_FLAGS                              0x01
#define AD_TYPE_16_BIT_SERV_UUID                   0x02
#define AD_TYPE_16_BIT_SERV_UUID_CMPLT_LIST        0x03
#define AD_TYPE_32_BIT_SERV_UUID                   0x04
#define AD_TYPE_32_BIT_SERV_UUID_CMPLT_LIST        0x05
#define AD_TYPE_128_BIT_SERV_UUID                  0x06
#define AD_TYPE_128_BIT_SERV_UUID_CMPLT_LIST       0x07
#define AD_TYPE_SHORTENED_LOCAL_NAME               0x08
#define AD_TYPE_COMPLETE_LOCAL_NAME                0x09
#define AD_TYPE_TX_POWER_LEVEL                     0x0A
#define AD_TYPE_CLASS_OF_DEVICE                    0x0D
#define AD_TYPE_SEC_MGR_TK_VALUE                   0x10
#define AD_TYPE_SEC_MGR_OOB_FLAGS                  0x11
#define AD_TYPE_SLAVE_CONN_INTERVAL                0x12
#define AD_TYPE_SERV_SOLICIT_16_BIT_UUID_LIST      0x14
#define AD_TYPE_SERV_SOLICIT_128_BIT_UUID_LIST     0x15
#define AD_TYPE_SERVICE_DATA                       0x16
#define AD_TYPE_APPEARANCE                         0x19
#define AD_TYPE_ADVERTISING_INTERVAL               0x1A
#define AD_TYPE_LE_ROLE                            0x1C
#define AD_TYPE_SERV_SOLICIT_32_BIT_UUID_LIST      0x1F
#define AD_TYPE_URI                                0x24
#define AD_TYPE_MANUFACTURER_SPECIFIC_DATA         0xFF


/* Flag bits for Flags AD Type
 */
#define FLAG_BIT_LE_LIMITED_DISCOVERABLE_MODE      0x01
#define FLAG_BIT_LE_GENERAL_DISCOVERABLE_MODE      0x02
#define FLAG_BIT_BR_EDR_NOT_SUPPORTED              0x04
#define FLAG_BIT_LE_BR_EDR_CONTROLLER              0x08
#define FLAG_BIT_LE_BR_EDR_HOST                    0x10


/* Privacy flag values
 */
#define PRIVACY_ENABLED                          0x02
#define PRIVACY_DISABLED                         0x00


/* Intervals in terms of 625 micro sec
 */
#define DIR_CONN_ADV_INT_MIN             0x190  /* 250 ms */
#define DIR_CONN_ADV_INT_MAX             0x320  /* 500 ms */
#define UNDIR_CONN_ADV_INT_MIN           0x800  /* 1.28 s */
#define UNDIR_CONN_ADV_INT_MAX           0x1000 /* 2.56 s */
#define LIM_DISC_ADV_INT_MIN             0x190  /* 250 ms */
#define LIM_DISC_ADV_INT_MAX             0x320  /* 500 ms */
#define GEN_DISC_ADV_INT_MIN             0x800  /* 1.28 s */
#define GEN_DISC_ADV_INT_MAX             0x1000 /* 2.56 s */


/* Timeout values
 */
#define LIM_DISC_MODE_TIMEOUT            180000 /* 180 seconds */
#define PRIVATE_ADDR_INT_TIMEOUT         900000 /* 15 minutes */


/* GAP Roles
 */
#define GAP_PERIPHERAL_ROLE               0x01
#define GAP_BROADCASTER_ROLE              0x02
#define GAP_CENTRAL_ROLE                  0x04
#define GAP_OBSERVER_ROLE                 0x08


/* GAP procedure codes
 * Procedure codes for EVT_BLUE_GAP_PROCEDURE_COMPLETE event
 * and aci_gap_terminate_gap_procedure() command.
 */
#define GAP_LIMITED_DISCOVERY_PROC                   0x01
#define GAP_GENERAL_DISCOVERY_PROC                   0x02
#define GAP_NAME_DISCOVERY_PROC                      0x04
#define GAP_AUTO_CONNECTION_ESTABLISHMENT_PROC       0x08
#define GAP_GENERAL_CONNECTION_ESTABLISHMENT_PROC    0x10
#define GAP_SELECTIVE_CONNECTION_ESTABLISHMENT_PROC  0x20
#define GAP_DIRECT_CONNECTION_ESTABLISHMENT_PROC     0x40
#define GAP_OBSERVATION_PROC                         0x80


/* ------------------------------------------------------------------------- */


/* IO capabilities
 */
#define IO_CAP_DISPLAY_ONLY                  0x00
#define IO_CAP_DISPLAY_YES_NO                0x01
#define IO_CAP_KEYBOARD_ONLY                 0x02
#define IO_CAP_NO_INPUT_NO_OUTPUT            0x03
#define IO_CAP_KEYBOARD_DISPLAY              0x04


/* Authentication requirements
 */
#define NO_BONDING                           0x00
#define BONDING                              0x01


/* MITM protection requirements
 */
#define MITM_PROTECTION_NOT_REQUIRED         0x00
#define MITM_PROTECTION_REQUIRED             0x01


/* Out-Of-Band data
 */
#define OOB_AUTH_DATA_ABSENT                 0x00
#define OOB_AUTH_DATA_PRESENT                0x01


/* Authorization requirements
 */
#define AUTHORIZATION_NOT_REQUIRED           0x00
#define AUTHORIZATION_REQUIRED               0x01


/* Connection authorization
 */
#define CONNECTION_AUTHORIZED                0x01
#define CONNECTION_REJECTED                  0x02


/* Use fixed pin
 */
#define USE_FIXED_PIN_FOR_PAIRING            0x00
#define DONOT_USE_FIXED_PIN_FOR_PAIRING      0x01


/* Link security status
 */
#define SM_LINK_AUTHENTICATED                0x01
#define SM_LINK_AUTHORIZED                   0x02
#define SM_LINK_ENCRYPTED                    0x04


/* SMP pairing failed reason codes
 */
#define PASSKEY_ENTRY_FAILED                 0x01
#define OOB_NOT_AVAILABLE                    0x02
#define AUTH_REQ_CANNOT_BE_MET               0x03
#define CONFIRM_VALUE_FAILED                 0x04
#define PAIRING_NOT_SUPPORTED                0x05
#define INSUFF_ENCRYPTION_KEY_SIZE           0x06
#define CMD_NOT_SUPPORTED                    0x07
#define UNSPECIFIED_REASON                   0x08
#define VERY_EARLY_NEXT_ATTEMPT              0x09
#define SM_INVALID_PARAMS                    0x0A


/* Pairing failed error codes
 * Error codes in EVT_BLUE_GAP_PAIRING_CMPLT event
 */
#define SM_PAIRING_SUCCESS                   0x00
#define SM_PAIRING_TIMEOUT                   0x01
#define SM_PAIRING_FAILED                    0x02


/* ------------------------------------------------------------------------- */


/* Well-Known UUIDs
 */
#define PRIMARY_SERVICE_UUID                       0x2800
#define SECONDARY_SERVICE_UUID                     0x2801
#define INCLUDE_SERVICE_UUID                       0x2802
#define CHARACTERISTIC_UUID                        0x2803
#define CHAR_EXTENDED_PROP_DESC_UUID               0x2900
#define CHAR_USER_DESC_UUID                        0x2901
#define CHAR_CLIENT_CONFIG_DESC_UUID               0x2902
#define CHAR_SERVER_CONFIG_DESC_UUID               0x2903
#define CHAR_FORMAT_DESC_UUID                      0x2904
#define CHAR_AGGR_FMT_DESC_UUID                    0x2905
#define GATT_SERVICE_UUID                          0x1801
#define GAP_SERVICE_UUID                           0x1800
#define SERVICE_CHANGED_UUID                       0x2A05


/* Access permissions for an attribute
 */
#define ATTR_NO_ACCESS                             0x00
#define ATTR_ACCESS_READ_ONLY                      0x01 
#define ATTR_ACCESS_WRITE_REQ_ONLY                 0x02
#define ATTR_ACCESS_READ_WRITE                     0x03
#define ATTR_ACCESS_WRITE_WITHOUT_RESPONSE         0x04
#define ATTR_ACCESS_SIGNED_WRITE_ALLOWED           0x08
#define ATTR_ACCESS_WRITE_ANY                      0x0E


/* Characteristic properties.
 */
#define CHAR_PROP_BROADCAST                      0x01
#define CHAR_PROP_READ                           0x02
#define CHAR_PROP_WRITE_WITHOUT_RESP             0x04
#define CHAR_PROP_WRITE                          0x08
#define CHAR_PROP_NOTIFY                         0x10
#define CHAR_PROP_INDICATE                       0x20
#define CHAR_PROP_SIGNED_WRITE                   0x40
#define CHAR_PROP_EXT                            0x80


/* Security permissions for an attribute.
 */
#define ATTR_PERMISSION_NONE          0x00 /* No security. */
#define ATTR_PERMISSION_AUTHEN_READ   0x01 /* Need authentication to read */
#define ATTR_PERMISSION_AUTHOR_READ   0x02 /* Need authorization to read */
#define ATTR_PERMISSION_ENCRY_READ    0x04 /* Need encryption to read */
#define ATTR_PERMISSION_AUTHEN_WRITE  0x08 /* Need authentication to write */
#define ATTR_PERMISSION_AUTHOR_WRITE  0x10 /* Need authorization to write */
#define ATTR_PERMISSION_ENCRY_WRITE   0x20 /* Need encryption to write */


/* Type of UUID (16 bit or 128 bit.
 */
#define UUID_TYPE_16                               0x01
#define UUID_TYPE_128                              0x02


/* Type of service (primary or secondary
 */
#define PRIMARY_SERVICE                            0x01
#define SECONDARY_SERVICE                          0x02


/* Gatt Event Mask
 * Type of event generated by GATT server
 * See aci_gatt_add_char.
 */
#define GATT_DONT_NOTIFY_EVENTS                       0x00 
#define GATT_NOTIFY_ATTRIBUTE_WRITE                   0x01
#define GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP  0x02
#define GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP   0x04


/* Type of characteristic length
 * See aci_gatt_add_char()
 */
#define CHAR_VALUE_LEN_CONSTANT               0x00
#define CHAR_VALUE_LEN_VARIABLE               0x01


/* Encryption key size
 */
#define MIN_ENCRY_KEY_SIZE                    7
#define MAX_ENCRY_KEY_SIZE                    0x10


/* Format
 */
#define FORMAT_UINT8                          0x04
#define FORMAT_UINT16                         0x06
#define FORMAT_SINT16                         0x0E
#define FORMAT_SINT24                         0x0F


/* Unit
 */
#define UNIT_UNITLESS                         0x2700
#define UNIT_TEMP_CELSIUS                     0x272F
#define UNIT_PRESSURE_BAR                     0x2780


/* ------------------------------------------------------------------------- */


/* Advertising policy for filtering (white list related)
 * See hci_le_set_advertising_parameters
 */
#define NO_WHITE_LIST_USE                      0x00
#define WHITE_LIST_FOR_ONLY_SCAN               0x01
#define WHITE_LIST_FOR_ONLY_CONN               0x02
#define WHITE_LIST_FOR_ALL                     0x03


/* Bluetooth address types
 */
#define PUBLIC_ADDR                            0
#define RANDOM_ADDR                            1
#define STATIC_RANDOM_ADDR                     1
#define RESOLVABLE_PRIVATE_ADDR                2
#define NON_RESOLVABLE_PRIVATE_ADDR            3


/* Directed advertising types
 * Type of advertising during directed advertising
 */
#define HIGH_DUTY_CYCLE_DIRECTED_ADV           1
#define LOW_DUTY_CYCLE_DIRECTED_ADV            4


/* Advertising type
 * See hci_le_set_advertising_parameters
  */
#define ADV_IND                                0x00
#define ADV_DIRECT_IND                         0x01
#define ADV_SCAN_IND                           0x02
#define ADV_NONCONN_IND                        0x03
#define SCAN_RSP                               0x04


/* Lowest allowed interval value for connectable types(20ms)..multiple of 625us
 */
#define ADV_INTERVAL_LOWEST_CONN               0X0020


/* Highest allowed interval value (10.24s)..multiple of 625us.
 */
#define ADV_INTERVAL_HIGHEST                   0X4000


/* Lowest allowed interval value for non connectable types
 * (100ms)..multiple of 625us.
 */
#define ADV_INTERVAL_LOWEST_NONCONN            0X00a0


/* Advertising channels
 */
#define ADV_CH_37                              0x01
#define ADV_CH_38                              0x02
#define ADV_CH_39                              0x04


/* Scan_types Scan types
 */
#define PASSIVE_SCAN                           0
#define ACTIVE_SCAN                            1


/* ------------------------------------------------------------------------- */


/* Configuration values.
 * See aci_hal_write_config_data().
 */
#define CONFIG_DATA_PUBADDR_OFFSET            0x00
#define CONFIG_DATA_DIV_OFFSET                0x06
#define CONFIG_DATA_ER_OFFSET                 0x08
#define CONFIG_DATA_IR_OFFSET                 0x18
#define CONFIG_DATA_RANDOM_ADDRESS_OFFSET     0x2E

/* Length for configuration values.
 * See aci_hal_write_config_data().
 */
#define CONFIG_DATA_PUBADDR_LEN               6
#define CONFIG_DATA_DIV_LEN                   2
#define CONFIG_DATA_ER_LEN                    16
#define CONFIG_DATA_IR_LEN                    16
#define CONFIG_DATA_RANDOM_ADDRESS_LEN        6


/* ------------------------------------------------------------------------- */


#endif /* BLE_DEFS_H__ */
