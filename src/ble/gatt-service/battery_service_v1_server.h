/*
 * Copyright (C) 2024 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/**
 * @title Battery Service Server v1.1
 * 
 */

#ifndef BATTERY_SERVICE_V1_SERVER_H
#define BATTERY_SERVICE_V1_SERVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The Battery Service allows to query your device's battery level in a standardized way.
 * 
 * To use with your application, add `#import <battery_service.gatt>` to your .gatt file. 
 * After adding it to your .gatt file, you call *battery_service_server_init(value)* with the
 * current value of your battery. The valid range for the battery level is 0-100.
 *
 * If the battery level changes, you can call *battery_service_server_set_battery_value(value)*. 
 * The service supports sending Notifications if the client enables them.
 */

typedef enum {
    BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL = 0,             
    BAS_CHARACTERISTIC_INDEX_BATTERY_LEVEL_STATUS,      
    BAS_CHARACTERISTIC_INDEX_ESTIMATED_SERVICE_DATE,    
    BAS_CHARACTERISTIC_INDEX_BATTERY_CRITCAL_STATUS,    
    BAS_CHARACTERISTIC_INDEX_BATTERY_ENERGY_STATUS,     
    BAS_CHARACTERISTIC_INDEX_BATTERY_TIME_STATUS,       
    BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_STATUS,     
    BAS_CHARACTERISTIC_INDEX_BATTERY_HEALTH_INFORMATION,
    BAS_CHARACTERISTIC_INDEX_BATTERY_INFORMATION,       
    BAS_CHARACTERISTIC_INDEX_MANUFACTURER_NAME_STRING,  
    BAS_CHARACTERISTIC_INDEX_MODEL_NUMBER_STRING,       
    BAS_CHARACTERISTIC_INDEX_SERIAL_NUMBER_STRING,      
    BAS_CHARACTERISTIC_INDEX_NUM
} bas_characteristic_index_t;

// ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_ENERGY_STATUS flags:
#define BATTERY_ENERGY_STATUS_BITMASK_EXTERNAL_SOURCE_POWER_PRESENT                0x01            
#define BATTERY_ENERGY_STATUS_BITMASK_PRESENT_VOLTAGE_PRESENT                      0x02    
#define BATTERY_ENERGY_STATUS_BITMASK_AVAILABLE_ENERGY_PRESENT                     0x04    
#define BATTERY_ENERGY_STATUS_BITMASK_AVAILABLE_BATTERY_CAPACITY_PRESENT           0x08                
#define BATTERY_ENERGY_STATUS_BITMASK_CHARGE_RATE_PRESENT                          0x10
#define BATTERY_ENERGY_STATUS_BITMASK_AVAILABLE_ENERGY_AT_LAST_CHARGE_PRESENT      0x20                    
#define BATTERY_ENERGY_STATUS_BITMASK_RFU                                          0x40

#define BATTERY_LEVEL_STATUS_BITMASK_IDENTIFIER_PRESENT                            0x01  
#define BATTERY_LEVEL_STATUS_BITMASK_BATTERY_LEVEL_PRESENT                         0x02
#define BATTERY_LEVEL_STATUS_BITMASK_ADDITIONAL_STATUS_PRESENT                     0x04
#define BATTERY_LEVEL_STATUS_BITMASK_RFU                                           0x08

#define BATTERY_LEVEL_POWER_STATE_BITMASK_EXTERNAL_BATTERY_PRESENT                 0x0001    // 0 = No, 1 = Yes      
#define BATTERY_LEVEL_POWER_STATE_BITMASK_WIRED_EXTERNAL_POWER_SOURCE_CONNECTED    0x0004    // 0 = No, 1 = Yes, 2 = Unknown, 3 = RFU
#define BATTERY_LEVEL_POWER_STATE_BITMASK_WIRELESS_EXTERNAL_POWER_SOURCE_CONNECTED 0x0010    // 0 = No, 1 = Yes, 2 = Unknown, 3 = RFU
#define BATTERY_LEVEL_POWER_STATE_BITMASK_BATTERY_CHARGE_STATE                     0x0040    // 0 = Unknown, 1 = Charging, 2 = Discharging: Active 3 = Discharging: Inactive 
#define BATTERY_LEVEL_POWER_STATE_BITMASK_BATTERY_CHARGE_LEVEL                     0x0100    // 0 = Unknown, 1 = Good, 2 = Low, 3 = Critical    
#define BATTERY_LEVEL_POWER_STATE_BITMASK_CHARGING_TYPE                            0x0400    // 0 = Unknown or Not Charging 1 = Constant Current, 2 = Constant Voltage, 3 = Trickle, 4 = Float, 5–7 = RFU              
#define BATTERY_LEVEL_POWER_STATE_BITMASK_CHARGING_FAULT_REASON                    0x2000    // Bit 12: Battery, Bit 13: External Power source, Bit 14: Other
#define BATTERY_LEVEL_POWER_STATE_BITMASK_RFU                                      0x4000

#define BATTERY_LEVEL_ADDITIONAL_STATUS_BITMASK_SERVICE_REQUIRED                   0x01       // 0 = No, 1 = Yes, 2 = Unknown, 3 = RFU
#define BATTERY_LEVEL_ADDITIONAL_STATUS_BITMASK_BATTERY_FAULT                      0x02       // 0 = No or Unknown, 1 = Yes
#define BATTERY_LEVEL_ADDITIONAL_STATUS_BITMASK_RFU                                0x04

#define BATTERY_CRITCAL_STATUS_BITMASK_CRITICAL_POWER_STATE                        0x01
#define BATTERY_CRITCAL_STATUS_BITMASK_IMMEDIATE_SERVICE_REQUIRED                  0x02
#define BATTERY_CRITCAL_STATUS_BITMASK_RFU                                         0x04

#define BATTERY_TIME_STATUS_BITMASK_TIME_UNTIL_DISCHARGED_ON_STANDBY_PRESENT       0x01
#define BATTERY_TIME_STATUS_BITMASK_TIME_UNTIL_RECHARGED_PRESENT                   0x02
#define BATTERY_TIME_STATUS_BITMASK_RFU                                            0x04

#define BATTERY_HEALTH_STATUS_BITMASK_HEALTH_SUMMARY_PRESENT                       0x01
#define BATTERY_HEALTH_STATUS_BITMASK_CYCLE_COUNT_PRESENT                          0x02
#define BATTERY_HEALTH_STATUS_BITMASK_CURRENT_TEMPERATURE_PRESENT                  0x04
#define BATTERY_HEALTH_STATUS_BITMASK_DEEP_DISCHARGE_COUNT_PRESENT                 0x08
#define BATTERY_HEALTH_STATUS_BITMASK_RFU                                          0x10

#define BATTERY_HEALTH_INFORMATION_BITMASK_CYCLE_COUNT_DESIGNED_LIFETIME_PRESENT   0x01
#define BATTERY_HEALTH_INFORMATION_BITMASK_DESIGNED_OPERATING_TEMPERATURE_PRESENT  0x02
#define BATTERY_HEALTH_INFORMATION_BITMASK_RFU                                     0x04

#define BATTERY_INFORMATION_BITMASK_MANUFACTURE_DATE_PRESENT                       0x0001    
#define BATTERY_INFORMATION_BITMASK_EXPIRATION_DATE_PRESENT                        0x0002    
#define BATTERY_INFORMATION_BITMASK_DESIGNED_CAPACITY_PRESENT                      0x0004    
#define BATTERY_INFORMATION_BITMASK_LOW_ENERGY_PRESENT                             0x0008    
#define BATTERY_INFORMATION_BITMASK_CRITICAL_ENERGY_PRESENT                        0x0010    
#define BATTERY_INFORMATION_BITMASK_CHEMISTRY_PRESENT                              0x0020    
#define BATTERY_INFORMATION_BITMASK_NOMINAL_VOLTAGE_PRESENT                        0x0040
#define BATTERY_INFORMATION_BITMASK_AGGREGATION_GROUP_PRESENT                      0x0080
#define BATTERY_INFORMATION_BITMASK_RFU                                            0x0100

#define BATTERY_INFROMATION_FEATURE_BITMASK_REPLACEABLE                             0x01
#define BATTERY_INFROMATION_FEATURE_BITMASK_RECHARGEABLE                            0x02
#define BATTERY_INFROMATION_FEATURE_BITMASK_RFU                                     0x04

struct battery_service_v1;

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
} btstack_utc_date_t;

typedef struct {
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
} btstack_utc_time_t;

typedef struct {
    hci_con_handle_t con_handle;

    btstack_context_callback_registration_t  scheduled_tasks_callback;
    uint8_t scheduled_tasks;
    uint16_t configurations[BAS_CHARACTERISTIC_INDEX_NUM];

    struct battery_service_v1 * service;
} battery_service_v1_server_connection_t;

typedef struct {
    uint16_t value_handle;
    uint16_t client_configuration_handle;
} bas_characteristic_t;

typedef struct {
    uint8_t  flags;
    uint16_t power_state_flags;

    uint16_t identifier;
    uint8_t  battery_level;
    uint8_t  additional_status_flags;
} battery_level_status_t;

typedef struct {
    uint8_t  flags;
    uint16_t external_source_power_medfloat16;
    uint16_t present_voltage_medfloat16;
    uint16_t available_energy_medfloat16;
    uint16_t available_battery_capacity_medfloat16;
    uint16_t charge_rate_medfloat16;
    uint16_t available_energy_at_last_charge_medfloat16;
} battery_energy_status_t;

typedef struct {
    uint8_t flags;

    // A value of 0xFFFFFF represents: Unknown
    // A value of 0xFFFFFE represents: Greater than 0xFFFFFD
    uint32_t time_until_discharged_minutes;
    uint32_t time_until_discharged_on_standby_minutes;
    uint32_t time_until_recharged_minutes;
} battery_time_status_t;

typedef struct {
    uint8_t  flags;

    uint8_t  summary;                 // Allowed range is 0 to 100.
    uint16_t cycle_count;
    int8_t   current_temperature_degree_celsius;
    uint16_t deep_discharge_count;
} battery_health_status_t;

typedef struct {
    uint8_t  flags;
    uint16_t cycle_count_designed_lifetime;

    // A raw value of 0x7F represents: Greater than 126. 
    // A raw value of 0x80 represents: Less than -127.
    int8_t   min_designed_operating_temperature_degree_celsius;
    int8_t   max_designed_operating_temperature_degree_celsius;
} battery_health_information_t;

typedef struct {
    uint16_t flags;
    uint8_t  features;

    uint32_t manufacture_date_days;
    uint32_t expiration_date_days;

    uint16_t designed_capacity_kWh_medfloat16;
    uint16_t low_energy_kWh_medfloat16;
    uint16_t critical_energy_kWh_medfloat16;
    uint8_t  chemistry;
    uint16_t nominal_voltage_medfloat16;
    uint8_t  aggregation_group; // 0: not in group, 255: RFU
} battery_information_t;

typedef struct battery_service_v1 {
    btstack_linked_item_t item;

    // service
    uint16_t start_handle;
    uint16_t end_handle;
    uint8_t index;

    att_service_handler_t    service_handler;

    bas_characteristic_t  characteristics[BAS_CHARACTERISTIC_INDEX_NUM];
    
    // ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL
    uint8_t  battery_value;

    // ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_STATUS       
    const battery_level_status_t * level_status;
    
    // ORG_BLUETOOTH_CHARACTERISTIC_ESTIMATED_SERVICE_DATE 
    uint32_t estimated_service_date_days;

    // ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_CRITCAL_STATUS
    uint8_t battery_critcal_status_flags;

    // ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_ENERGY_STATUS
    const battery_energy_status_t * energy_status;

    // ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_TIME_STATUS        
    const battery_time_status_t * time_status;

    // ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_HEALTH_STATUS
    const battery_health_status_t * health_status;

    // ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_HEALTH_INFORMATION
    const battery_health_information_t * health_information;

    // ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_INFORMATION
    const battery_information_t * information;

    // ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING   
    const char * manufacturer_name;

    // ORG_BLUETOOTH_CHARACTERISTIC_MODEL_NUMBER_STRING        
    const char * model_number;

    // ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING       
    const char *  serial_number;

    uint8_t connections_max_num;
    battery_service_v1_server_connection_t * connections;
} battery_service_v1_t;

/* API_START */

/**
 * @brief Init Battery Service Server with ATT DB
 */
void battery_service_v1_server_init(void);

void battery_service_v1_server_register(battery_service_v1_t * service, battery_service_v1_server_connection_t * connections, uint8_t connection_max_num);

void battery_service_v1_server_deregister(battery_service_v1_t * service);

/**
 * @brief Update battery value
 * @note triggers notifications if subscribed
 * @param service
 * @param battery_value in range 0-100
 * @return ERROR_CODE_SUCCESS if value differs from old one, otherwise:
 *              - ERROR_CODE_REPEATED_ATTEMPTS, or
 *              - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE
 */
uint8_t battery_service_v1_server_set_battery_value(battery_service_v1_t * service, uint8_t battery_value);


void battery_service_v1_server_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

