
/**
 * bluetooth_gatt.h generated from Bluetooth SIG website for BTstack tool/bluetooth_gatt.py
 * 2018-11-13 12:39:09.525663
 */

#ifndef BLUETOOTH_GATT_H
#define BLUETOOTH_GATT_H

/**
 * Assigned numbers from www.bluetooth.com/specifications/gatt/declarations
 */
#define ORG_BLUETOOTH_ATTRIBUTE_GATT_CHARACTERISTIC_DECLARATION                                                       0x2803 // Characteristic Declaration
#define ORG_BLUETOOTH_ATTRIBUTE_GATT_INCLUDE_DECLARATION                                                              0x2802 // Include
#define ORG_BLUETOOTH_ATTRIBUTE_GATT_PRIMARY_SERVICE_DECLARATION                                                      0x2800 // Primary Service
#define ORG_BLUETOOTH_ATTRIBUTE_GATT_SECONDARY_SERVICE_DECLARATION                                                    0x2801 // Secondary Service

/**
 * Assigned numbers from www.bluetooth.com/specifications/gatt/services
 */
#define ORG_BLUETOOTH_SERVICE_ALERT_NOTIFICATION                                                                      0x1811 // Alert Notification Service
#define ORG_BLUETOOTH_SERVICE_AUTOMATION_IO                                                                           0x1815 // Automation IO
#define ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE                                                                         0x180F // Battery Service
#define ORG_BLUETOOTH_SERVICE_BLOOD_PRESSURE                                                                          0x1810 // Blood Pressure
#define ORG_BLUETOOTH_SERVICE_BODY_COMPOSITION                                                                        0x181B // Body Composition
#define ORG_BLUETOOTH_SERVICE_BOND_MANAGEMENT                                                                         0x181E // Bond Management Service
#define ORG_BLUETOOTH_SERVICE_CONTINUOUS_GLUCOSE_MONITORING                                                           0x181F // Continuous Glucose Monitoring
#define ORG_BLUETOOTH_SERVICE_CURRENT_TIME                                                                            0x1805 // Current Time Service
#define ORG_BLUETOOTH_SERVICE_CYCLING_POWER                                                                           0x1818 // Cycling Power
#define ORG_BLUETOOTH_SERVICE_CYCLING_SPEED_AND_CADENCE                                                               0x1816 // Cycling Speed and Cadence
#define ORG_BLUETOOTH_SERVICE_DEVICE_INFORMATION                                                                      0x180A // Device Information
#define ORG_BLUETOOTH_SERVICE_ENVIRONMENTAL_SENSING                                                                   0x181A // Environmental Sensing
#define ORG_BLUETOOTH_SERVICE_FITNESS_MACHINE                                                                         0x1826 // Fitness Machine
#define ORG_BLUETOOTH_SERVICE_GENERIC_ACCESS                                                                          0x1800 //  Generic Access
#define ORG_BLUETOOTH_SERVICE_GENERIC_ATTRIBUTE                                                                       0x1801 // Generic Attribute
#define ORG_BLUETOOTH_SERVICE_GLUCOSE                                                                                 0x1808 // Glucose
#define ORG_BLUETOOTH_SERVICE_HEALTH_THERMOMETER                                                                      0x1809 // Health Thermometer
#define ORG_BLUETOOTH_SERVICE_HEART_RATE                                                                              0x180D // Heart Rate
#define ORG_BLUETOOTH_SERVICE_HTTP_PROXY                                                                              0x1823 // HTTP Proxy
#define ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE                                                                  0x1812 // Human Interface Device
#define ORG_BLUETOOTH_SERVICE_IMMEDIATE_ALERT                                                                         0x1802 // Immediate Alert
#define ORG_BLUETOOTH_SERVICE_INDOOR_POSITIONING                                                                      0x1821 // Indoor Positioning
#define ORG_BLUETOOTH_SERVICE_INSULIN_DELIVERY                                                                        0x183A // Insulin Delivery
#define ORG_BLUETOOTH_SERVICE_INTERNET_PROTOCOL_SUPPORT                                                               0x1820 // Internet Protocol Support Service
#define ORG_BLUETOOTH_SERVICE_LINK_LOSS                                                                               0x1803 // Link Loss
#define ORG_BLUETOOTH_SERVICE_LOCATION_AND_NAVIGATION                                                                 0x1819 // Location and Navigation
#define ORG_BLUETOOTH_SERVICE_MESH_PROVISIONING                                                                       0x1827 // Mesh Provisioning Service
#define ORG_BLUETOOTH_SERVICE_MESH_PROXY                                                                              0x1828 // Mesh Proxy Service
#define ORG_BLUETOOTH_SERVICE_NEXT_DST_CHANGE                                                                         0x1807 // Next DST Change Service
#define ORG_BLUETOOTH_SERVICE_OBJECT_TRANSFER                                                                         0x1825 // Object Transfer Service
#define ORG_BLUETOOTH_SERVICE_PHONE_ALERT_STATUS                                                                      0x180E // Phone Alert Status Service
#define ORG_BLUETOOTH_SERVICE_PULSE_OXIMETER                                                                          0x1822 // Pulse Oximeter Service
#define ORG_BLUETOOTH_SERVICE_RECONNECTION_CONFIGURATION                                                              0x1829 // Reconnection Configuration
#define ORG_BLUETOOTH_SERVICE_REFERENCE_TIME_UPDATE                                                                   0x1806 // Reference Time Update Service
#define ORG_BLUETOOTH_SERVICE_RUNNING_SPEED_AND_CADENCE                                                               0x1814 // Running Speed and Cadence
#define ORG_BLUETOOTH_SERVICE_SCAN_PARAMETERS                                                                         0x1813 // Scan Parameters
#define ORG_BLUETOOTH_SERVICE_TRANSPORT_DISCOVERY                                                                     0x1824 // Transport Discovery
#define ORG_BLUETOOTH_SERVICE_TX_POWER                                                                                0x1804 // Tx Power
#define ORG_BLUETOOTH_SERVICE_USER_DATA                                                                               0x181C // User Data
#define ORG_BLUETOOTH_SERVICE_WEIGHT_SCALE                                                                            0x181D // Weight Scale

/**
 * Assigned numbers, manually added from:
 * btprodspecificationrefs.blob.core.windows.net/assigned-values/16-bit%20UUID%20Numbers%20Document.pdf
 * and processed with tool: bluetooth_gatt_process_uuid_list.py
 */
#define ORG_BLUETOOTH_SERVICE_AUDIO_INPUT_CONTROL                                                                     0x1843 // Audio Input Control
#define ORG_BLUETOOTH_SERVICE_AUDIO_STREAM_CONTROL_SERVICE                                                            0x184E // Audio Stream Control Service
#define ORG_BLUETOOTH_SERVICE_BASIC_AUDIO_ANNOUNCEMENT_SERVICE                                                        0x1851 // Basic Audio Announcement Service
#define ORG_BLUETOOTH_SERVICE_BINARY_SENSOR                                                                           0x183B // Binary Sensor 
#define ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE                                                    0x1852 // Broadcast Audio Announcement Service
#define ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE                                                            0x184F // Broadcast Audio Scan Service
#define ORG_BLUETOOTH_SERVICE_CONSTANT_TONE_EXTENSION                                                                 0x184A // Constant Tone Extension
#define ORG_BLUETOOTH_SERVICE_COORDINATED_SET_IDENTIFICATION_SERVICE                                                  0x1846 // Coordinated Set Identification Service
#define ORG_BLUETOOTH_SERVICE_DEVICE_TIME                                                                             0x1847 // Device Time
#define ORG_BLUETOOTH_SERVICE_EMERGENCY_CONFIGURATION                                                                 0x183C // Emergency Configuration
#define ORG_BLUETOOTH_SERVICE_GENERIC_MEDIA_CONTROL_SERVICE                                                           0x1849 // Generic Media Control Service 
#define ORG_BLUETOOTH_SERVICE_GENERIC_TELEPHONE_BEARER_SERVICE                                                        0x184C // Generic Telephone Bearer Service
#define ORG_BLUETOOTH_SERVICE_MEDIA_CONTROL_SERVICE                                                                   0x1848 // Media Control Service
#define ORG_BLUETOOTH_SERVICE_MICROPHONE_CONTROL                                                                      0x184D // Microphone Control
#define ORG_BLUETOOTH_SERVICE_PHYSICAL_ACTIVITY_MONITOR                                                               0x183E // Physical Activity Monitor
#define ORG_BLUETOOTH_SERVICE_PUBLISHED_AUDIO_CAPABILITIES_SERVICE                                                    0x1850 // Published Audio Capabilities Service
#define ORG_BLUETOOTH_SERVICE_TELEPHONE_BEARER_SERVICE                                                                0x184B // Telephone Bearer Service
#define ORG_BLUETOOTH_SERVICE_VOLUME_CONTROL                                                                          0x1844 // Volume Control
#define ORG_BLUETOOTH_SERVICE_VOLUME_OFFSET_CONTROL                                                                   0x1845 // Volume Offset Control

/**
 * Assigned numbers from www.bluetooth.com/specifications/gatt/characteristics
 */
#define ORG_BLUETOOTH_CHARACTERISTIC_AEROBIC_HEART_RATE_LOWER_LIMIT                                                   0x2A7E // Aerobic Heart Rate Lower Limit
#define ORG_BLUETOOTH_CHARACTERISTIC_AEROBIC_HEART_RATE_UPPER_LIMIT                                                   0x2A84 // Aerobic Heart Rate Upper Limit
#define ORG_BLUETOOTH_CHARACTERISTIC_AEROBIC_THRESHOLD                                                                0x2A7F // Aerobic Threshold
#define ORG_BLUETOOTH_CHARACTERISTIC_AGE                                                                              0x2A80 // Age
#define ORG_BLUETOOTH_CHARACTERISTIC_AGGREGATE                                                                        0x2A5A // Aggregate
#define ORG_BLUETOOTH_CHARACTERISTIC_ALERT_CATEGORY_ID                                                                0x2A43 // Alert Category ID
#define ORG_BLUETOOTH_CHARACTERISTIC_ALERT_CATEGORY_ID_BIT_MASK                                                       0x2A42 // Alert Category ID Bit Mask
#define ORG_BLUETOOTH_CHARACTERISTIC_ALERT_LEVEL                                                                      0x2A06 // Alert Level
#define ORG_BLUETOOTH_CHARACTERISTIC_ALERT_NOTIFICATION_CONTROL_POINT                                                 0x2A44 // Alert Notification Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_ALERT_STATUS                                                                     0x2A3F // Alert Status
#define ORG_BLUETOOTH_CHARACTERISTIC_ALTITUDE                                                                         0x2AB3 // Altitude
#define ORG_BLUETOOTH_CHARACTERISTIC_ANAEROBIC_HEART_RATE_LOWER_LIMIT                                                 0x2A81 // Anaerobic Heart Rate Lower Limit
#define ORG_BLUETOOTH_CHARACTERISTIC_ANAEROBIC_HEART_RATE_UPPER_LIMIT                                                 0x2A82 // Anaerobic Heart Rate Upper Limit
#define ORG_BLUETOOTH_CHARACTERISTIC_ANAEROBIC_THRESHOLD                                                              0x2A83 // Anaerobic Threshold
#define ORG_BLUETOOTH_CHARACTERISTIC_ANALOG                                                                           0x2A58 // Analog
#define ORG_BLUETOOTH_CHARACTERISTIC_ANALOG_OUTPUT                                                                    0x2A59 // Analog Output
#define ORG_BLUETOOTH_CHARACTERISTIC_APPARENT_WIND_DIRECTION                                                          0x2A73 // Apparent Wind Direction
#define ORG_BLUETOOTH_CHARACTERISTIC_APPARENT_WIND_SPEED                                                              0x2A72 // Apparent Wind Speed
#define ORG_BLUETOOTH_CHARACTERISTIC_BAROMETRIC_PRESSURE_TREND                                                        0x2AA3 // Barometric Pressure Trend
#define ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL                                                                    0x2A19 // Battery Level
#define ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_STATE                                                              0x2A1B // Battery Level State
#define ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_POWER_STATE                                                              0x2A1A // Battery Power State
#define ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_FEATURE                                                           0x2A49 // Blood Pressure Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_MEASUREMENT                                                       0x2A35 // Blood Pressure Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_BODY_COMPOSITION_FEATURE                                                         0x2A9B // Body Composition Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_BODY_COMPOSITION_MEASUREMENT                                                     0x2A9C // Body Composition Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_BODY_SENSOR_LOCATION                                                             0x2A38 // Body Sensor Location
#define ORG_BLUETOOTH_CHARACTERISTIC_BOND_MANAGEMENT_CONTROL_POINT                                                    0x2AA4 // Bond Management Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_BOND_MANAGEMENT_FEATURE                                                          0x2AA5 // Bond Management Features
#define ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_INPUT_REPORT                                                       0x2A22 // Boot Keyboard Input Report
#define ORG_BLUETOOTH_CHARACTERISTIC_BOOT_KEYBOARD_OUTPUT_REPORT                                                      0x2A32 // Boot Keyboard Output Report
#define ORG_BLUETOOTH_CHARACTERISTIC_BOOT_MOUSE_INPUT_REPORT                                                          0x2A33 // Boot Mouse Input Report
#define ORG_BLUETOOTH_CHARACTERISTIC_CGM_FEATURE                                                                      0x2AA8 // CGM Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_CGM_MEASUREMENT                                                                  0x2AA7 // CGM Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_CGM_SESSION_RUN_TIME                                                             0x2AAB // CGM Session Run Time
#define ORG_BLUETOOTH_CHARACTERISTIC_CGM_SESSION_START_TIME                                                           0x2AAA // CGM Session Start Time
#define ORG_BLUETOOTH_CHARACTERISTIC_CGM_SPECIFIC_OPS_CONTROL_POINT                                                   0x2AAC // CGM Specific Ops Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_CGM_STATUS                                                                       0x2AA9 // CGM Status
#define ORG_BLUETOOTH_CHARACTERISTIC_CROSS_TRAINER_DATA                                                               0x2ACE // Cross Trainer Data
#define ORG_BLUETOOTH_CHARACTERISTIC_CSC_FEATURE                                                                      0x2A5C // CSC Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_CSC_MEASUREMENT                                                                  0x2A5B // CSC Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TIME                                                                     0x2A2B // Current Time
#define ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_CONTROL_POINT                                                      0x2A66 // Cycling Power Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_FEATURE                                                            0x2A65 // Cycling Power Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_MEASUREMENT                                                        0x2A63 // Cycling Power Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_CYCLING_POWER_VECTOR                                                             0x2A64 // Cycling Power Vector
#define ORG_BLUETOOTH_CHARACTERISTIC_DATABASE_CHANGE_INCREMENT                                                        0x2A99 // Database Change Increment
#define ORG_BLUETOOTH_CHARACTERISTIC_DATE_OF_BIRTH                                                                    0x2A85 // Date of Birth
#define ORG_BLUETOOTH_CHARACTERISTIC_DATE_OF_THRESHOLD_ASSESSMENT                                                     0x2A86 // Date of Threshold Assessment
#define ORG_BLUETOOTH_CHARACTERISTIC_DATE_TIME                                                                        0x2A08 // Date Time
#define ORG_BLUETOOTH_CHARACTERISTIC_DATE_UTC                                                                         0x2AED // Date UTC
#define ORG_BLUETOOTH_CHARACTERISTIC_DAY_DATE_TIME                                                                    0x2A0A // Day Date Time
#define ORG_BLUETOOTH_CHARACTERISTIC_DAY_OF_WEEK                                                                      0x2A09 // Day of Week
#define ORG_BLUETOOTH_CHARACTERISTIC_DESCRIPTOR_VALUE_CHANGED                                                         0x2A7D // Descriptor Value Changed
#define ORG_BLUETOOTH_CHARACTERISTIC_DEW_POINT                                                                        0x2A7B // Dew Point
#define ORG_BLUETOOTH_CHARACTERISTIC_DIGITAL                                                                          0x2A56 // Digital
#define ORG_BLUETOOTH_CHARACTERISTIC_DIGITAL_OUTPUT                                                                   0x2A57 // Digital Output
#define ORG_BLUETOOTH_CHARACTERISTIC_DST_OFFSET                                                                       0x2A0D // DST Offset
#define ORG_BLUETOOTH_CHARACTERISTIC_ELEVATION                                                                        0x2A6C // Elevation
#define ORG_BLUETOOTH_CHARACTERISTIC_EMAIL_ADDRESS                                                                    0x2A87 // Email Address
#define ORG_BLUETOOTH_CHARACTERISTIC_EXACT_TIME_100                                                                   0x2A0B // Exact Time 100
#define ORG_BLUETOOTH_CHARACTERISTIC_EXACT_TIME_256                                                                   0x2A0C // Exact Time 256
#define ORG_BLUETOOTH_CHARACTERISTIC_FAT_BURN_HEART_RATE_LOWER_LIMIT                                                  0x2A88 // Fat Burn Heart Rate Lower Limit
#define ORG_BLUETOOTH_CHARACTERISTIC_FAT_BURN_HEART_RATE_UPPER_LIMIT                                                  0x2A89 // Fat Burn Heart Rate Upper Limit
#define ORG_BLUETOOTH_CHARACTERISTIC_FIRMWARE_REVISION_STRING                                                         0x2A26 // Firmware Revision String
#define ORG_BLUETOOTH_CHARACTERISTIC_FIRST_NAME                                                                       0x2A8A // First Name
#define ORG_BLUETOOTH_CHARACTERISTIC_FITNESS_MACHINE_CONTROL_POINT                                                    0x2AD9 // Fitness Machine Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_FITNESS_MACHINE_FEATURE                                                          0x2ACC // Fitness Machine Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_FITNESS_MACHINE_STATUS                                                           0x2ADA // Fitness Machine Status
#define ORG_BLUETOOTH_CHARACTERISTIC_FIVE_ZONE_HEART_RATE_LIMITS                                                      0x2A8B // Five Zone Heart Rate Limits
#define ORG_BLUETOOTH_CHARACTERISTIC_FLOOR_NUMBER                                                                     0x2AB2 // Floor Number
#define ORG_BLUETOOTH_CHARACTERISTIC_GAP_APPEARANCE                                                                   0x2A01 // Appearance
#define ORG_BLUETOOTH_CHARACTERISTIC_GAP_CENTRAL_ADDRESS_RESOLUTION                                                   0x2AA6 // Central Address Resolution
#define ORG_BLUETOOTH_CHARACTERISTIC_GAP_DEVICE_NAME                                                                  0x2A00 // Device Name
#define ORG_BLUETOOTH_CHARACTERISTIC_GAP_PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS                                   0x2A04 // Peripheral Preferred Connection Parameters
#define ORG_BLUETOOTH_CHARACTERISTIC_GAP_PERIPHERAL_PRIVACY_FLAG                                                      0x2A02 // Peripheral Privacy Flag
#define ORG_BLUETOOTH_CHARACTERISTIC_GAP_RECONNECTION_ADDRESS                                                         0x2A03 // Reconnection Address
#define ORG_BLUETOOTH_CHARACTERISTIC_GATT_SERVICE_CHANGED                                                             0x2A05 // Service Changed
#define ORG_BLUETOOTH_CHARACTERISTIC_GENDER                                                                           0x2A8C // Gender
#define ORG_BLUETOOTH_CHARACTERISTIC_GLUCOSE_FEATURE                                                                  0x2A51 // Glucose Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_GLUCOSE_MEASUREMENT                                                              0x2A18 // Glucose Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_GLUCOSE_MEASUREMENT_CONTEXT                                                      0x2A34 // Glucose Measurement Context
#define ORG_BLUETOOTH_CHARACTERISTIC_GUST_FACTOR                                                                      0x2A74 // Gust Factor
#define ORG_BLUETOOTH_CHARACTERISTIC_HARDWARE_REVISION_STRING                                                         0x2A27 // Hardware Revision String
#define ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_CONTROL_POINT                                                         0x2A39 // Heart Rate Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_MAX                                                                   0x2A8D // Heart Rate Max
#define ORG_BLUETOOTH_CHARACTERISTIC_HEART_RATE_MEASUREMENT                                                           0x2A37 // Heart Rate Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_HEAT_INDEX                                                                       0x2A7A // Heat Index
#define ORG_BLUETOOTH_CHARACTERISTIC_HEIGHT                                                                           0x2A8E // Height
#define ORG_BLUETOOTH_CHARACTERISTIC_HID_CONTROL_POINT                                                                0x2A4C // HID Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_HID_INFORMATION                                                                  0x2A4A // HID Information
#define ORG_BLUETOOTH_CHARACTERISTIC_HIP_CIRCUMFERENCE                                                                0x2A8F // Hip Circumference
#define ORG_BLUETOOTH_CHARACTERISTIC_HTTP_CONTROL_POINT                                                               0x2ABA // HTTP Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_HTTP_ENTITY_BODY                                                                 0x2AB9 // HTTP Entity Body
#define ORG_BLUETOOTH_CHARACTERISTIC_HTTP_HEADERS                                                                     0x2AB7 // HTTP Headers
#define ORG_BLUETOOTH_CHARACTERISTIC_HTTP_STATUS_CODE                                                                 0x2AB8 // HTTP Status Code
#define ORG_BLUETOOTH_CHARACTERISTIC_HTTPS_SECURITY                                                                   0x2ABB // HTTPS Security
#define ORG_BLUETOOTH_CHARACTERISTIC_HUMIDITY                                                                         0x2A6F // Humidity
#define ORG_BLUETOOTH_CHARACTERISTIC_IDD_ANNUNCIATION_STATUS                                                          0x2B22 // IDD Annunciation Status
#define ORG_BLUETOOTH_CHARACTERISTIC_IDD_COMMAND_CONTROL_POINT                                                        0x2B25 // IDD Command Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_IDD_COMMAND_DATA                                                                 0x2B26 // IDD Command Data
#define ORG_BLUETOOTH_CHARACTERISTIC_IDD_FEATURES                                                                     0x2B23 // IDD Features
#define ORG_BLUETOOTH_CHARACTERISTIC_IDD_HISTORY_DATA                                                                 0x2B28 // IDD History Data
#define ORG_BLUETOOTH_CHARACTERISTIC_IDD_RECORD_ACCESS_CONTROL_POINT                                                  0x2B27 // IDD Record Access Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_IDD_STATUS                                                                       0x2B21 // IDD Status
#define ORG_BLUETOOTH_CHARACTERISTIC_IDD_STATUS_CHANGED                                                               0x2B20 // IDD Status Changed
#define ORG_BLUETOOTH_CHARACTERISTIC_IDD_STATUS_READER_CONTROL_POINT                                                  0x2B24 // IDD Status Reader Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST                              0x2A2A // IEEE 11073-20601 Regulatory Certification Data List
#define ORG_BLUETOOTH_CHARACTERISTIC_INDOOR_BIKE_DATA                                                                 0x2AD2 // Indoor Bike Data
#define ORG_BLUETOOTH_CHARACTERISTIC_INDOOR_POSITIONING_CONFIGURATION                                                 0x2AAD // Indoor Positioning Configuration
#define ORG_BLUETOOTH_CHARACTERISTIC_INTERMEDIATE_CUFF_PRESSURE                                                       0x2A36 // Intermediate Cuff Pressure
#define ORG_BLUETOOTH_CHARACTERISTIC_INTERMEDIATE_TEMPERATURE                                                         0x2A1E // Intermediate Temperature
#define ORG_BLUETOOTH_CHARACTERISTIC_IRRADIANCE                                                                       0x2A77 // Irradiance
#define ORG_BLUETOOTH_CHARACTERISTIC_LANGUAGE                                                                         0x2AA2 // Language
#define ORG_BLUETOOTH_CHARACTERISTIC_LAST_NAME                                                                        0x2A90 // Last Name
#define ORG_BLUETOOTH_CHARACTERISTIC_LATITUDE                                                                         0x2AAE // Latitude
#define ORG_BLUETOOTH_CHARACTERISTIC_LN_CONTROL_POINT                                                                 0x2A6B // LN Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_LN_FEATURE                                                                       0x2A6A // LN Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_LOCAL_EAST_COORDINATE                                                            0x2AB1 // Local East Coordinate
#define ORG_BLUETOOTH_CHARACTERISTIC_LOCAL_NORTH_COORDINATE                                                           0x2AB0 // Local North Coordinate
#define ORG_BLUETOOTH_CHARACTERISTIC_LOCAL_TIME_INFORMATION                                                           0x2A0F // Local Time Information
#define ORG_BLUETOOTH_CHARACTERISTIC_LOCATION_AND_SPEED                                                               0x2A67 // Location and Speed Characteristic
#define ORG_BLUETOOTH_CHARACTERISTIC_LOCATION_NAME                                                                    0x2AB5 // Location Name
#define ORG_BLUETOOTH_CHARACTERISTIC_LONGITUDE                                                                        0x2AAF // Longitude
#define ORG_BLUETOOTH_CHARACTERISTIC_MAGNETIC_DECLINATION                                                             0x2A2C // Magnetic Declination
#define ORG_BLUETOOTH_CHARACTERISTIC_MAGNETIC_FLUX_DENSITY_2D                                                         0x2AA0 // Magnetic Flux Density - 2D
#define ORG_BLUETOOTH_CHARACTERISTIC_MAGNETIC_FLUX_DENSITY_3D                                                         0x2AA1 // Magnetic Flux Density - 3D
#define ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING                                                         0x2A29 // Manufacturer Name String
#define ORG_BLUETOOTH_CHARACTERISTIC_MAXIMUM_RECOMMENDED_HEART_RATE                                                   0x2A91 // Maximum Recommended Heart Rate
#define ORG_BLUETOOTH_CHARACTERISTIC_MEASUREMENT_INTERVAL                                                             0x2A21 // Measurement Interval
#define ORG_BLUETOOTH_CHARACTERISTIC_MODEL_NUMBER_STRING                                                              0x2A24 // Model Number String
#define ORG_BLUETOOTH_CHARACTERISTIC_NAVIGATION                                                                       0x2A68 // Navigation
#define ORG_BLUETOOTH_CHARACTERISTIC_NETWORK_AVAILABILITY                                                             0x2A3E // Network Availability
#define ORG_BLUETOOTH_CHARACTERISTIC_NEW_ALERT                                                                        0x2A46 // New Alert
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ACTION_CONTROL_POINT                                                      0x2AC5 // Object Action Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_CHANGED                                                                   0x2AC8 // Object Changed
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_FIRST_CREATED                                                             0x2AC1 // Object First-Created
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_ID                                                                        0x2AC3 // Object ID
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LAST_MODIFIED                                                             0x2AC2 // Object Last-Modified
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_CONTROL_POINT                                                        0x2AC6 // Object List Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_LIST_FILTER                                                               0x2AC7 // Object List Filter
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_NAME                                                                      0x2ABE // Object Name
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_PROPERTIES                                                                0x2AC4 // Object Properties
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_SIZE                                                                      0x2AC0 // Object Size
#define ORG_BLUETOOTH_CHARACTERISTIC_OBJECT_TYPE                                                                      0x2ABF // Object Type
#define ORG_BLUETOOTH_CHARACTERISTIC_OTS_FEATURE                                                                      0x2ABD // OTS Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_PLX_CONTINUOUS_MEASUREMENT                                                       0x2A5F // PLX Continuous Measurement Characteristic
#define ORG_BLUETOOTH_CHARACTERISTIC_PLX_FEATURES                                                                     0x2A60 // PLX Features
#define ORG_BLUETOOTH_CHARACTERISTIC_PLX_SPOT_CHECK_MEASUREMENT                                                       0x2A5E // PLX Spot-Check Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_PNP_ID                                                                           0x2A50 // PnP ID
#define ORG_BLUETOOTH_CHARACTERISTIC_POLLEN_CONCENTRATION                                                             0x2A75 // Pollen Concentration
#define ORG_BLUETOOTH_CHARACTERISTIC_POSITION_2D                                                                      0x2A2F // Position 2D
#define ORG_BLUETOOTH_CHARACTERISTIC_POSITION_3D                                                                      0x2A30 // Position 3D
#define ORG_BLUETOOTH_CHARACTERISTIC_POSITION_QUALITY                                                                 0x2A69 // Position Quality
#define ORG_BLUETOOTH_CHARACTERISTIC_PRESSURE                                                                         0x2A6D // Pressure
#define ORG_BLUETOOTH_CHARACTERISTIC_PROTOCOL_MODE                                                                    0x2A4E // Protocol Mode
#define ORG_BLUETOOTH_CHARACTERISTIC_PULSE_OXIMETRY_CONTROL_POINT                                                     0x2A62 // Pulse Oximetry Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_RAINFALL                                                                         0x2A78 // Rainfall
#define ORG_BLUETOOTH_CHARACTERISTIC_RC_FEATURE                                                                       0x2B1D // RC Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_RC_SETTINGS                                                                      0x2B1E // RC Settings
#define ORG_BLUETOOTH_CHARACTERISTIC_RECONNECTION_CONFIGURATION_CONTROL_POINT                                         0x2B1F // Reconnection Configuration Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_RECORD_ACCESS_CONTROL_POINT                                                      0x2A52 // Record Access Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_REFERENCE_TIME_INFORMATION                                                       0x2A14 // Reference Time Information
#define ORG_BLUETOOTH_CHARACTERISTIC_REMOVABLE                                                                        0x2A3A // Removable
#define ORG_BLUETOOTH_CHARACTERISTIC_REPORT                                                                           0x2A4D // Report
#define ORG_BLUETOOTH_CHARACTERISTIC_REPORT_MAP                                                                       0x2A4B // Report Map
#define ORG_BLUETOOTH_CHARACTERISTIC_RESOLVABLE_PRIVATE_ADDRESS_ONLY                                                  0x2AC9 // Resolvable Private Address Only
#define ORG_BLUETOOTH_CHARACTERISTIC_RESTING_HEART_RATE                                                               0x2A92 // Resting Heart Rate
#define ORG_BLUETOOTH_CHARACTERISTIC_RINGER_CONTROL_POINT                                                             0x2A40 // Ringer Control point
#define ORG_BLUETOOTH_CHARACTERISTIC_RINGER_SETTING                                                                   0x2A41 // Ringer Setting
#define ORG_BLUETOOTH_CHARACTERISTIC_ROWER_DATA                                                                       0x2AD1 // Rower Data
#define ORG_BLUETOOTH_CHARACTERISTIC_RSC_FEATURE                                                                      0x2A54 // RSC Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_RSC_MEASUREMENT                                                                  0x2A53 // RSC Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_SC_CONTROL_POINT                                                                 0x2A55 // SC Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_SCAN_INTERVAL_WINDOW                                                             0x2A4F // Scan Interval Window
#define ORG_BLUETOOTH_CHARACTERISTIC_SCAN_REFRESH                                                                     0x2A31 // Scan Refresh
#define ORG_BLUETOOTH_CHARACTERISTIC_SCIENTIFIC_TEMPERATURE_CELSIUS                                                   0x2A3C // Scientific Temperature Celsius
#define ORG_BLUETOOTH_CHARACTERISTIC_SECONDARY_TIME_ZONE                                                              0x2A10 // Secondary Time Zone
#define ORG_BLUETOOTH_CHARACTERISTIC_SENSOR_LOCATION                                                                  0x2A5D // Sensor Location
#define ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING                                                             0x2A25 // Serial Number String
#define ORG_BLUETOOTH_CHARACTERISTIC_SERVICE_REQUIRED                                                                 0x2A3B // Service Required
#define ORG_BLUETOOTH_CHARACTERISTIC_SOFTWARE_REVISION_STRING                                                         0x2A28 // Software Revision String
#define ORG_BLUETOOTH_CHARACTERISTIC_SPORT_TYPE_FOR_AEROBIC_AND_ANAEROBIC_THRESHOLDS                                  0x2A93 // Sport Type for Aerobic and Anaerobic Thresholds
#define ORG_BLUETOOTH_CHARACTERISTIC_STAIR_CLIMBER_DATA                                                               0x2AD0 // Stair Climber Data
#define ORG_BLUETOOTH_CHARACTERISTIC_STEP_CLIMBER_DATA                                                                0x2ACF // Step Climber Data
#define ORG_BLUETOOTH_CHARACTERISTIC_STRING                                                                           0x2A3D // String
#define ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_HEART_RATE_RANGE                                                       0x2AD7 // Supported Heart Rate Range
#define ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_INCLINATION_RANGE                                                      0x2AD5 // Supported Inclination Range
#define ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_NEW_ALERT_CATEGORY                                                     0x2A47 // Supported New Alert Category
#define ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_POWER_RANGE                                                            0x2AD8 // Supported Power Range
#define ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_RESISTANCE_LEVEL_RANGE                                                 0x2AD6 // Supported Resistance Level Range
#define ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_SPEED_RANGE                                                            0x2AD4 // Supported Speed Range
#define ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_UNREAD_ALERT_CATEGORY                                                  0x2A48 // Supported Unread Alert Category
#define ORG_BLUETOOTH_CHARACTERISTIC_SYSTEM_ID                                                                        0x2A23 // System ID
#define ORG_BLUETOOTH_CHARACTERISTIC_TDS_CONTROL_POINT                                                                0x2ABC // TDS Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE                                                                      0x2A6E // Temperature
#define ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_CELSIUS                                                              0x2A1F // Temperature Celsius
#define ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_FAHRENHEIT                                                           0x2A20 // Temperature Fahrenheit
#define ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_MEASUREMENT                                                          0x2A1C // Temperature Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_TEMPERATURE_TYPE                                                                 0x2A1D // Temperature Type
#define ORG_BLUETOOTH_CHARACTERISTIC_THREE_ZONE_HEART_RATE_LIMITS                                                     0x2A94 // Three Zone Heart Rate Limits
#define ORG_BLUETOOTH_CHARACTERISTIC_TIME_ACCURACY                                                                    0x2A12 // Time Accuracy
#define ORG_BLUETOOTH_CHARACTERISTIC_TIME_BROADCAST                                                                   0x2A15 // Time Broadcast
#define ORG_BLUETOOTH_CHARACTERISTIC_TIME_SOURCE                                                                      0x2A13 // Time Source
#define ORG_BLUETOOTH_CHARACTERISTIC_TIME_UPDATE_CONTROL_POINT                                                        0x2A16 // Time Update Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_TIME_UPDATE_STATE                                                                0x2A17 // Time Update State
#define ORG_BLUETOOTH_CHARACTERISTIC_TIME_WITH_DST                                                                    0x2A11 // Time with DST
#define ORG_BLUETOOTH_CHARACTERISTIC_TIME_ZONE                                                                        0x2A0E // Time Zone
#define ORG_BLUETOOTH_CHARACTERISTIC_TRAINING_STATUS                                                                  0x2AD3 // Training Status
#define ORG_BLUETOOTH_CHARACTERISTIC_TREADMILL_DATA                                                                   0x2ACD // Treadmill Data
#define ORG_BLUETOOTH_CHARACTERISTIC_TRUE_WIND_DIRECTION                                                              0x2A71 // True Wind Direction
#define ORG_BLUETOOTH_CHARACTERISTIC_TRUE_WIND_SPEED                                                                  0x2A70 // True Wind Speed
#define ORG_BLUETOOTH_CHARACTERISTIC_TWO_ZONE_HEART_RATE_LIMIT                                                        0x2A95 // Two Zone Heart Rate Limit
#define ORG_BLUETOOTH_CHARACTERISTIC_TX_POWER_LEVEL                                                                   0x2A07 // Tx Power Level
#define ORG_BLUETOOTH_CHARACTERISTIC_UNCERTAINTY                                                                      0x2AB4 // Uncertainty
#define ORG_BLUETOOTH_CHARACTERISTIC_UNREAD_ALERT_STATUS                                                              0x2A45 // Unread Alert Status
#define ORG_BLUETOOTH_CHARACTERISTIC_URI                                                                              0x2AB6 // URI
#define ORG_BLUETOOTH_CHARACTERISTIC_USER_CONTROL_POINT                                                               0x2A9F // User Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_USER_INDEX                                                                       0x2A9A // User Index
#define ORG_BLUETOOTH_CHARACTERISTIC_UV_INDEX                                                                         0x2A76 // UV Index
#define ORG_BLUETOOTH_CHARACTERISTIC_VO2_MAX                                                                          0x2A96 // VO2 Max
#define ORG_BLUETOOTH_CHARACTERISTIC_WAIST_CIRCUMFERENCE                                                              0x2A97 // Waist Circumference
#define ORG_BLUETOOTH_CHARACTERISTIC_WEIGHT                                                                           0x2A98 // Weight
#define ORG_BLUETOOTH_CHARACTERISTIC_WEIGHT_MEASUREMENT                                                               0x2A9D // Weight Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_WEIGHT_SCALE_FEATURE                                                             0x2A9E // Weight Scale Feature
#define ORG_BLUETOOTH_CHARACTERISTIC_WIND_CHILL                                                                       0x2A79 // Wind Chill

/**
 * Assigned numbers, manually added from:
 * btprodspecificationrefs.blob.core.windows.net/assigned-values/16-bit%20UUID%20Numbers%20Document.pdf
 * and processed with tool: bluetooth_gatt_process_uuid_list.py
 */

#define ORG_BLUETOOTH_CHARACTERISTIC_CLIENT_SUPPORTED_FEATURES                                                        0x2B29 // Client Supported Features
#define ORG_BLUETOOTH_CHARACTERISTIC_DATABASE_HASH                                                                    0x2B2A // Database Hash
#define ORG_BLUETOOTH_CHARACTERISTIC_BSS_CONTROL_POINT                                                                0x2B2B // BSS Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_BSS_RESPONSE                                                                     0x2B2C // BSS Response
#define ORG_BLUETOOTH_CHARACTERISTIC_EMERGENCY_ID                                                                     0x2B2D // Emergency ID
#define ORG_BLUETOOTH_CHARACTERISTIC_EMERGENCY_TEXT                                                                   0x2B2E // Emergency Text
#define ORG_BLUETOOTH_CHARACTERISTIC_ENHANCED_BLOOD_PRESSURE_MEASUREMENT                                              0x2B34 // Enhanced Blood Pressure Measurement
#define ORG_BLUETOOTH_CHARACTERISTIC_ENHANCED_INTERMEDIATE_CUFF_PRESSURE                                              0x2B35 // Enhanced Intermediate Cuff Pressure
#define ORG_BLUETOOTH_CHARACTERISTIC_BLOOD_PRESSURE_RECORD                                                            0x2B36 // Blood Pressure Record
#define ORG_BLUETOOTH_CHARACTERISTIC_BR_EDR_HANDOVER_DATA                                                             0x2B38 // BR-EDR Handover Data
#define ORG_BLUETOOTH_CHARACTERISTIC_BLUETOOTH_SIG_DATA                                                               0x2B39 // Bluetooth SIG Data
#define ORG_BLUETOOTH_CHARACTERISTIC_SERVER_SUPPORTED_FEATURES                                                        0x2B3A // Server Supported Features
#define ORG_BLUETOOTH_CHARACTERISTIC_PHYSICAL_ACTIVITY_MONITOR_FEATURES                                               0x2B3B // Physical Activity Monitor Features
#define ORG_BLUETOOTH_CHARACTERISTIC_GENERAL_ACTIVITY_INSTANTANEOUS_DATA                                              0x2B3C // General Activity Instantaneous Data
#define ORG_BLUETOOTH_CHARACTERISTIC_GENERAL_ACTIVITY_SUMMARY_DATA                                                    0x2B3D // General Activity Summary Data
#define ORG_BLUETOOTH_CHARACTERISTIC_CARDIORESPIRATORY_ACTIVITY_INSTANTANEOUS_DATA                                    0x2B3E // CardioRespiratory Activity Instantaneous Data
#define ORG_BLUETOOTH_CHARACTERISTIC_CARDIORESPIRATORY_ACTIVITY_SUMMARY_DATA                                          0x2B3F // CardioRespiratory Activity Summary Data
#define ORG_BLUETOOTH_CHARACTERISTIC_STEP_COUNTER_ACTIVITY_SUMMARY_DATA                                               0x2B40 // Step Counter Activity Summary Data
#define ORG_BLUETOOTH_CHARACTERISTIC_SLEEP_ACTIVITY_INSTANTANEOUS_DATA                                                0x2B41 // Sleep Activity Instantaneous Data
#define ORG_BLUETOOTH_CHARACTERISTIC_SLEEP_ACTIVITY_SUMMARY_DATA                                                      0x2B42 // Sleep Activity Summary Data
#define ORG_BLUETOOTH_CHARACTERISTIC_PHYSICAL_ACTIVITY_MONITOR_CONTROL_POINT                                          0x2B43 // Physical Activity Monitor Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_SESSION                                                                  0x2B44 // Current Session
#define ORG_BLUETOOTH_CHARACTERISTIC_SESSION                                                                          0x2B45 // Session
#define ORG_BLUETOOTH_CHARACTERISTIC_PREFERRED_UNITS                                                                  0x2B46 // Preferred Units
#define ORG_BLUETOOTH_CHARACTERISTIC_HIGH_RESOLUTION_HEIGHT                                                           0x2B47 // High Resolution Height
#define ORG_BLUETOOTH_CHARACTERISTIC_MIDDLE_NAME                                                                      0x2B48 // Middle Name
#define ORG_BLUETOOTH_CHARACTERISTIC_STRIDE_LENGTH                                                                    0x2B49 // Stride Length
#define ORG_BLUETOOTH_CHARACTERISTIC_HANDEDNESS                                                                       0x2B4A // Handedness
#define ORG_BLUETOOTH_CHARACTERISTIC_DEVICE_WEARING_POSITION                                                          0x2B4B // Device Wearing Position
#define ORG_BLUETOOTH_CHARACTERISTIC_FOUR_ZONE_HEART_RATE_LIMITS                                                      0x2B4C // Four Zone Heart Rate Limits
#define ORG_BLUETOOTH_CHARACTERISTIC_HIGH_INTENSITY_EXERCISE_THRESHOLD                                                0x2B4D // High Intensity Exercise Threshold
#define ORG_BLUETOOTH_CHARACTERISTIC_ACTIVITY_GOAL                                                                    0x2B4E // Activity Goal
#define ORG_BLUETOOTH_CHARACTERISTIC_SEDENTARY_INTERVAL_NOTIFICATION                                                  0x2B4F // Sedentary Interval Notification
#define ORG_BLUETOOTH_CHARACTERISTIC_CALORIC_INTAKE                                                                   0x2B50 // Caloric Intake
#define ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATE                                                                0x2B77 // Audio Input State
#define ORG_BLUETOOTH_CHARACTERISTIC_GAIN_SETTINGS_ATTRIBUTE                                                          0x2B78 // Gain Settings Attribute
#define ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_TYPE                                                                 0x2B79 // Audio Input Type
#define ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_STATUS                                                               0x2B7A // Audio Input Status
#define ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_CONTROL_POINT                                                        0x2B7B // Audio Input Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_INPUT_DESCRIPTION                                                          0x2B7C // Audio Input Description
#define ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_STATE                                                                     0x2B7D // Volume State
#define ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_CONTROL_POINT                                                             0x2B7E // Volume Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_FLAGS                                                                     0x2B7F // Volume Flags
#define ORG_BLUETOOTH_CHARACTERISTIC_OFFSET_STATE                                                                     0x2B80 // Offset State
#define ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_LOCATION                                                                   0x2B81 // Audio Location
#define ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_OFFSET_CONTROL_POINT                                                      0x2B82 // Volume Offset Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_AUDIO_OUTPUT_DESCRIPTION                                                         0x2B83 // Audio Output Description
#define ORG_BLUETOOTH_CHARACTERISTIC_SET_IDENTITY_RESOLVING_KEY_CHARACTERISTIC                                        0x2B84 // Set Identity Resolving Key Characteristic
#define ORG_BLUETOOTH_CHARACTERISTIC_SIZE_CHARACTERISTIC                                                              0x2B85 // Size Characteristic
#define ORG_BLUETOOTH_CHARACTERISTIC_LOCK_CHARACTERISTIC                                                              0x2B86 // Lock Characteristic
#define ORG_BLUETOOTH_CHARACTERISTIC_RANK_CHARACTERISTIC                                                              0x2B87 // Rank Characteristic
#define ORG_BLUETOOTH_CHARACTERISTIC_DEVICE_TIME_FEATURE_                                                             0x2B8E // Device Time Feature 
#define ORG_BLUETOOTH_CHARACTERISTIC_DEVICE_TIME_PARAMETERS                                                           0x2B8F // Device Time Parameters 
#define ORG_BLUETOOTH_CHARACTERISTIC_DEVICE_TIME                                                                      0x2B90 // Device Time
#define ORG_BLUETOOTH_CHARACTERISTIC_DEVICE_TIME_CONTROL_POINT                                                        0x2B91 // Device Time Control Point 
#define ORG_BLUETOOTH_CHARACTERISTIC_TIME_CHANGE_LOG_DATA                                                             0x2B92 // Time Change Log Data 
#define ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_NAME                                                                0x2B93 // Media Player Name
#define ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_OBJECT_ID                                                      0x2B94 // Media Player Icon Object ID 
#define ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_URL                                                            0x2B95 // Media Player Icon URL 
#define ORG_BLUETOOTH_CHARACTERISTIC_TRACK_CHANGED                                                                    0x2B96 // Track Changed
#define ORG_BLUETOOTH_CHARACTERISTIC_TRACK_TITLE                                                                      0x2B97 // Track Title
#define ORG_BLUETOOTH_CHARACTERISTIC_TRACK_DURATION                                                                   0x2B98 // Track Duration
#define ORG_BLUETOOTH_CHARACTERISTIC_TRACK_POSITION                                                                   0x2B99 // Track Position
#define ORG_BLUETOOTH_CHARACTERISTIC_PLAYBACK_SPEED                                                                   0x2B9A // Playback Speed
#define ORG_BLUETOOTH_CHARACTERISTIC_SEEKING_SPEED                                                                    0x2B9B // Seeking Speed
#define ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_SEGMENTS_OBJECT_ID                                                 0x2B9C // Current Track Segments Object ID
#define ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_TRACK_OBJECT_ID                                                          0x2B9D // Current Track Object ID
#define ORG_BLUETOOTH_CHARACTERISTIC_NEXT_TRACK_OBJECT_ID                                                             0x2B9E // Next Track Object ID
#define ORG_BLUETOOTH_CHARACTERISTIC_PARENT_GROUP_OBJECT_ID                                                           0x2B9F // Parent Group Object ID
#define ORG_BLUETOOTH_CHARACTERISTIC_CURRENT_GROUP_OBJECT_ID                                                          0x2BA0 // Current Group Object ID
#define ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDER                                                                    0x2BA1 // Playing Order
#define ORG_BLUETOOTH_CHARACTERISTIC_PLAYING_ORDERS_SUPPORTED                                                         0x2BA2 // Playing Orders Supported
#define ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_STATE                                                                      0x2BA3 // Media State
#define ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT                                                              0x2BA4 // Media Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED                                            0x2BA5 // Media Control Point Opcodes Supported
#define ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_RESULTS_OBJECT_ID                                                         0x2BA6 // Search Results Object ID
#define ORG_BLUETOOTH_CHARACTERISTIC_SEARCH_CONTROL_POINT                                                             0x2BA7 // Search Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_MEDIA_PLAYER_ICON_OBJECT_TYPE                                                    0x2BA9 // Media Player Icon Object Type
#define ORG_BLUETOOTH_CHARACTERISTIC_TRACK_SEGMENTS_OBJECT_TYPE                                                       0x2BAA // Track Segments Object Type
#define ORG_BLUETOOTH_CHARACTERISTIC_TRACK_OBJECT_TYPE                                                                0x2BAB // Track Object Type
#define ORG_BLUETOOTH_CHARACTERISTIC_GROUP_OBJECT_TYPE                                                                0x2BAC // Group Object Type
#define ORG_BLUETOOTH_CHARACTERISTIC_CONSTANT_TONE_EXTENSION_ENABLE                                                   0x2BAD // Constant Tone Extension Enable
#define ORG_BLUETOOTH_CHARACTERISTIC_ADVERTISING_CONSTANT_TONE_EXTENSION_MINIMUM_LENGTH                               0x2BAE // Advertising Constant Tone Extension Minimum Length
#define ORG_BLUETOOTH_CHARACTERISTIC_ADVERTISING_CONSTANT_TONE_EXTENSION_MINIMUM_TRANSMIT_COUNT                       0x2BAF // Advertising Constant Tone Extension Minimum Transmit Count
#define ORG_BLUETOOTH_CHARACTERISTIC_ADVERTISING_CONSTANT_TONE_EXTENSION_TRANSMIT_DURATION                            0x2BB0 // Advertising Constant Tone Extension Transmit Duration
#define ORG_BLUETOOTH_CHARACTERISTIC_ADVERTISING_CONSTANT_TONE_EXTENSION_INTERVAL                                     0x2BB1 // Advertising Constant Tone Extension Interval
#define ORG_BLUETOOTH_CHARACTERISTIC_ADVERTISING_CONSTANT_TONE_EXTENSION_PHY                                          0x2BB2 // Advertising Constant Tone Extension PHY
#define ORG_BLUETOOTH_CHARACTERISTIC_BEARER_PROVIDER_NAME                                                             0x2BB3 // Bearer Provider Name
#define ORG_BLUETOOTH_CHARACTERISTIC_BEARER_UCI                                                                       0x2BB4 // Bearer UCI
#define ORG_BLUETOOTH_CHARACTERISTIC_BEARER_TECHNOLOGY                                                                0x2BB5 // Bearer Technology
#define ORG_BLUETOOTH_CHARACTERISTIC_BEARER_URI_SCHEMES_SUPPORTED_LIST                                                0x2BB6 // Bearer URI Schemes Supported List
#define ORG_BLUETOOTH_CHARACTERISTIC_BEARER_SIGNAL_STRENGTH                                                           0x2BB7 // Bearer Signal Strength
#define ORG_BLUETOOTH_CHARACTERISTIC_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL                                        0x2BB8 // Bearer Signal Strength Reporting Interval
#define ORG_BLUETOOTH_CHARACTERISTIC_BEARER_LIST_CURRENT_CALLS                                                        0x2BB9 // Bearer List Current Calls
#define ORG_BLUETOOTH_CHARACTERISTIC_CONTENT_CONTROL_ID                                                               0x2BBA // Content Control ID
#define ORG_BLUETOOTH_CHARACTERISTIC_STATUS_FLAGS                                                                     0x2BBB // Status Flags
#define ORG_BLUETOOTH_CHARACTERISTIC_INCOMING_CALL_TARGET_BEARER_URI                                                  0x2BBC // Incoming Call Target Bearer URI
#define ORG_BLUETOOTH_CHARACTERISTIC_CALL_STATE                                                                       0x2BBD // Call State
#define ORG_BLUETOOTH_CHARACTERISTIC_CALL_CONTROL_POINT                                                               0x2BBE // Call Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_CALL_CONTROL_POINT_OPTIONAL_OPCODES                                              0x2BBF // Call Control Point Optional Opcodes
#define ORG_BLUETOOTH_CHARACTERISTIC_TERMINATION_REASON                                                               0x2BC0 // Termination Reason
#define ORG_BLUETOOTH_CHARACTERISTIC_INCOMING_CALL                                                                    0x2BC1 // Incoming Call
#define ORG_BLUETOOTH_CHARACTERISTIC_CALL_FRIENDLY_NAME                                                               0x2BC2 // Call Friendly Name
#define ORG_BLUETOOTH_CHARACTERISTIC_MUTE                                                                             0x2BC3 // Mute
#define ORG_BLUETOOTH_CHARACTERISTIC_SINK_ASE                                                                         0x2BC4 // Sink ASE
#define ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_ASE                                                                       0x2BC5 // Source ASE
#define ORG_BLUETOOTH_CHARACTERISTIC_ASE_CONTROL_POINT                                                                0x2BC6 // ASE Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_AUDIO_SCAN_CONTROL_POINT                                               0x2BC7 // Broadcast Audio Scan Control Point
#define ORG_BLUETOOTH_CHARACTERISTIC_BROADCAST_RECEIVE_STATE                                                          0x2BC8 // Broadcast Receive State
#define ORG_BLUETOOTH_CHARACTERISTIC_SINK_PAC                                                                         0x2BC9 // Sink PAC
#define ORG_BLUETOOTH_CHARACTERISTIC_SINK_AUDIO_LOCATIONS                                                             0x2BCA // Sink Audio Locations
#define ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_PAC                                                                       0x2BCB // Source PAC
#define ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_AUDIO_LOCATIONS                                                           0x2BCC // Source Audio Locations
#define ORG_BLUETOOTH_CHARACTERISTIC_AVAILABLE_AUDIO_CONTEXTS                                                         0x2BCD // Available Audio Contexts
#define ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_AUDIO_CONTEXTS                                                         0x2BCE // Supported Audio Contexts


/**
 * Assigned numbers from www.bluetooth.com/specifications/gatt/descriptors
 */
#define ORG_BLUETOOTH_DESCRIPTOR_ES_CONFIGURATION                                                                     0x290B // Environmental Sensing Configuration
#define ORG_BLUETOOTH_DESCRIPTOR_ES_MEASUREMENT                                                                       0x290C // Environmental Sensing Measurement
#define ORG_BLUETOOTH_DESCRIPTOR_ES_TRIGGER_SETTING                                                                   0x290D // Environmental Sensing Trigger Setting
#define ORG_BLUETOOTH_DESCRIPTOR_EXTERNAL_REPORT_REFERENCE                                                            0x2907 // External Report Reference
#define ORG_BLUETOOTH_DESCRIPTOR_GATT_CHARACTERISTIC_AGGREGATE_FORMAT                                                 0x2905 // Characteristic Aggregate Format
#define ORG_BLUETOOTH_DESCRIPTOR_GATT_CHARACTERISTIC_EXTENDED_PROPERTIES                                              0x2900 // Characteristic Extended Properties
#define ORG_BLUETOOTH_DESCRIPTOR_GATT_CHARACTERISTIC_PRESENTATION_FORMAT                                              0x2904 // Characteristic Presentation Format
#define ORG_BLUETOOTH_DESCRIPTOR_GATT_CHARACTERISTIC_USER_DESCRIPTION                                                 0x2901 // Characteristic User Description
#define ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION                                             0x2902 // Client Characteristic Configuration
#define ORG_BLUETOOTH_DESCRIPTOR_GATT_SERVER_CHARACTERISTIC_CONFIGURATION                                             0x2903 // Server Characteristic Configuration
#define ORG_BLUETOOTH_DESCRIPTOR_NUMBER_OF_DIGITALS                                                                   0x2909 // Number of Digitals
#define ORG_BLUETOOTH_DESCRIPTOR_REPORT_REFERENCE                                                                     0x2908 // Report Reference
#define ORG_BLUETOOTH_DESCRIPTOR_TIME_TRIGGER_SETTING                                                                 0x290E // Time Trigger Setting
#define ORG_BLUETOOTH_DESCRIPTOR_VALID_RANGE                                                                          0x2906 // Valid Range
#define ORG_BLUETOOTH_DESCRIPTOR_VALUE_TRIGGER_SETTING                                                                0x290A // Value Trigger Setting
// START(manualy added, missing on Bluetooth Website
#define ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROVISIONING_DATA_IN                                                        0x2ADB // 
#define ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROVISIONING_DATA_OUT                                                       0x2ADC // 
#define ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROXY_DATA_IN                                                               0x2ADD // 
#define ORG_BLUETOOTH_CHARACTERISTIC_MESH_PROXY_DATA_OUT                                                              0x2ADE // 
// END(manualy added, missing on Bluetooth Website

#endif
