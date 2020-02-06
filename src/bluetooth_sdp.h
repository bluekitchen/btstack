/**
 * bluetooth_sdp.h generated from Bluetooth SIG website for BTstack by tool/bluetooth_sdp.py
 * www.bluetooth.com/specifications/assigned-numbers/service-discovery
 */

#ifndef BLUETOOTH_SDP_H
#define BLUETOOTH_SDP_H

/**
 * Protocol Identifiers
 */
#define BLUETOOTH_PROTOCOL_SDP                                                     0x0001 // Bluetooth Core Specification
#define BLUETOOTH_PROTOCOL_UDP                                                     0x0002 // [NO USE BY PROFILES]
#define BLUETOOTH_PROTOCOL_RFCOMM                                                  0x0003 // RFCOMM with TS 07.10
#define BLUETOOTH_PROTOCOL_TCP                                                     0x0004 // [NO USE BY PROFILES]
#define BLUETOOTH_PROTOCOL_TCS_BIN                                                 0x0005 // Telephony Control Specification / TCS Binary [DEPRECATED]
#define BLUETOOTH_PROTOCOL_TCS_AT                                                  0x0006 // [NO USE BY PROFILES]
#define BLUETOOTH_PROTOCOL_ATT                                                     0x0007 // Attribute Protocol
#define BLUETOOTH_PROTOCOL_OBEX                                                    0x0008 // IrDA Interoperability
#define BLUETOOTH_PROTOCOL_IP                                                      0x0009 // [NO USE BY PROFILES]
#define BLUETOOTH_PROTOCOL_FTP                                                     0x000A // [NO USE BY PROFILES]
#define BLUETOOTH_PROTOCOL_HTTP                                                    0x000C // [NO USE BY PROFILES]
#define BLUETOOTH_PROTOCOL_WSP                                                     0x000E // [NO USE BY PROFILES]
#define BLUETOOTH_PROTOCOL_BNEP                                                    0x000F // Bluetooth Network Encapsulation Protocol (BNEP)
#define BLUETOOTH_PROTOCOL_UPNP                                                    0x0010 // Extended Service Discovery Profile (ESDP) [DEPRECATED]
#define BLUETOOTH_PROTOCOL_HIDP                                                    0x0011 // Human Interface Device Profile (HID)
#define BLUETOOTH_PROTOCOL_HARDCOPY_CONTROL_CHANNEL                                0x0012 // Hardcopy Cable Replacement Profile (HCRP)
#define BLUETOOTH_PROTOCOL_HARDCOPY_DATA_CHANNEL                                   0x0014 // See Hardcopy Cable Replacement Profile (HCRP)
#define BLUETOOTH_PROTOCOL_HARDCOPY_NOTIFICATION                                   0x0016 // Hardcopy Cable Replacement Profile (HCRP)
#define BLUETOOTH_PROTOCOL_AVCTP                                                   0x0017 // Audio/Video Control Transport Protocol (AVCTP)
#define BLUETOOTH_PROTOCOL_AVDTP                                                   0x0019 // Audio/Video Distribution Transport Protocol (AVDTP)
#define BLUETOOTH_PROTOCOL_CMTP                                                    0x001B // Common ISDN Access Profile (CIP) [DEPRECATED]
#define BLUETOOTH_PROTOCOL_MCAP_CONTROL_CHANNEL                                    0x001E // Multi-Channel Adaptation Protocol (MCAP)
#define BLUETOOTH_PROTOCOL_MCAP_DATA_CHANNEL                                       0x001F // Multi-Channel Adaptation Protocol (MCAP)
#define BLUETOOTH_PROTOCOL_L2CAP                                                   0x0100 // Bluetooth Core Specification

/**
 * Service Classes
 */
#define BLUETOOTH_SERVICE_CLASS_SERVICE_DISCOVERY_SERVER                           0x1000 // Bluetooth Core Specification
#define BLUETOOTH_SERVICE_CLASS_BROWSE_GROUP_DESCRIPTOR                            0x1001 // Bluetooth Core Specification
#define BLUETOOTH_SERVICE_CLASS_SERIAL_PORT                                        0x1101 // Serial Port Profile (SPP) NOTE: The example SDP record in SPP v1.0 does not include a BluetoothProfileDescriptorList attribute, but some implementations may also use this UUID for the Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_LAN_ACCESS_USING_PPP                               0x1102 // LAN Access Profile [DEPRECATED] NOTE: Used as both Service Class Identifier and Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_DIALUP_NETWORKING                                  0x1103 // Dial-up Networking Profile (DUN) NOTE: Used as both Service Class Identifier and Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_IR_MC_SYNC                                         0x1104 // Synchronization Profile (SYNC) NOTE: Used as both Service Class Identifier and Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_OBEX_OBJECT_PUSH                                   0x1105 // Object Push Profile (OPP) NOTE: Used as both Service Class Identifier and Profile.
#define BLUETOOTH_SERVICE_CLASS_OBEX_FILE_TRANSFER                                 0x1106 // File Transfer Profile (FTP) NOTE: Used as both Service Class Identifier and Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_IR_MC_SYNC_COMMAND                                 0x1107 // Synchronization Profile (SYNC)
#define BLUETOOTH_SERVICE_CLASS_HEADSET                                            0x1108 // Headset Profile (HSP) NOTE: Used as both Service Class Identifier and Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_CORDLESS_TELEPHONY                                 0x1109 // Cordless Telephony Profile (CTP) NOTE: Used as both Service Class Identifier and Profile Identifier. [DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_AUDIO_SOURCE                                       0x110A // Advanced Audio Distribution Profile (A2DP)
#define BLUETOOTH_SERVICE_CLASS_AUDIO_SINK                                         0x110B // Advanced Audio Distribution Profile (A2DP)
#define BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_TARGET                           0x110C // Audio/Video Remote Control Profile (AVRCP)
#define BLUETOOTH_SERVICE_CLASS_ADVANCED_AUDIO_DISTRIBUTION                        0x110D // Advanced Audio Distribution Profile (A2DP)
#define BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL                                  0x110E // Audio/Video Remote Control Profile (AVRCP) NOTE: Used as both Service Class Identifier and Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_CONTROLLER                       0x110F // Audio/Video Remote Control Profile (AVRCP) NOTE: The AVRCP specification v1.3 and later require that 0x110E also be included in the ServiceClassIDList before 0x110F for backwards compatibility.
#define BLUETOOTH_SERVICE_CLASS_INTERCOM                                           0x1110 // Intercom Profile (ICP) NOTE: Used as both Service Class Identifier and Profile Identifier. [DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_FAX                                                0x1111 // Fax Profile (FAX) NOTE: Used as both Service Class Identifier and Profile Identifier. [DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_HEADSET_AUDIO_GATEWAY_AG                           0x1112 // Headset Profile (HSP)
#define BLUETOOTH_SERVICE_CLASS_WAP                                                0x1113 // Interoperability Requirements for Bluetooth technology as a WAP, Bluetooth SIG [DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_WAP_CLIENT                                         0x1114 // Interoperability Requirements for Bluetooth technology as a WAP, Bluetooth SIG [DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_PANU                                               0x1115 // Personal Area Networking Profile (PAN) NOTE: Used as both Service Class Identifier and Profile Identifier for PANU role.
#define BLUETOOTH_SERVICE_CLASS_NAP                                                0x1116 // Personal Area Networking Profile (PAN) NOTE: Used as both Service Class Identifier and Profile Identifier for NAP role.
#define BLUETOOTH_SERVICE_CLASS_GN                                                 0x1117 // Personal Area Networking Profile (PAN) NOTE: Used as both Service Class Identifier and Profile Identifier for GN role.
#define BLUETOOTH_SERVICE_CLASS_DIRECT_PRINTING                                    0x1118 // Basic Printing Profile (BPP)
#define BLUETOOTH_SERVICE_CLASS_REFERENCE_PRINTING                                 0x1119 // See Basic Printing Profile (BPP)
#define BLUETOOTH_SERVICE_CLASS_BASIC_IMAGING_PROFILE                              0x111A // Basic Imaging Profile (BIP)
#define BLUETOOTH_SERVICE_CLASS_IMAGING_RESPONDER                                  0x111B // Basic Imaging Profile (BIP)
#define BLUETOOTH_SERVICE_CLASS_IMAGING_AUTOMATIC_ARCHIVE                          0x111C // Basic Imaging Profile (BIP)
#define BLUETOOTH_SERVICE_CLASS_IMAGING_REFERENCED_OBJECTS                         0x111D // Basic Imaging Profile (BIP)
#define BLUETOOTH_SERVICE_CLASS_HANDSFREE                                          0x111E // Hands-Free Profile (HFP) NOTE: Used as both Service Class Identifier and Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_HANDSFREE_AUDIO_GATEWAY                            0x111F // Hands-free Profile (HFP)
#define BLUETOOTH_SERVICE_CLASS_DIRECT_PRINTING_REFERENCE_OBJECTS_SERVICE          0x1120 // Basic Printing Profile (BPP)
#define BLUETOOTH_SERVICE_CLASS_REFLECTED_UI                                       0x1121 // Basic Printing Profile (BPP)
#define BLUETOOTH_SERVICE_CLASS_BASIC_PRINTING                                     0x1122 // Basic Printing Profile (BPP)
#define BLUETOOTH_SERVICE_CLASS_PRINTING_STATUS                                    0x1123 // Basic Printing Profile (BPP)
#define BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE                     0x1124 // Human Interface Device (HID) NOTE: Used as both Service Class Identifier and Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_HARDCOPY_CABLE_REPLACEMENT                         0x1125 // Hardcopy Cable Replacement Profile (HCRP)
#define BLUETOOTH_SERVICE_CLASS_HCR_PRINT                                          0x1126 // Hardcopy Cable Replacement Profile (HCRP)
#define BLUETOOTH_SERVICE_CLASS_HCR_SCAN                                           0x1127 // Hardcopy Cable Replacement Profile (HCRP)
#define BLUETOOTH_SERVICE_CLASS_COMMON_ISDN_ACCESS                                 0x1128 // Common ISDN Access Profile (CIP) NOTE: Used as both Service Class Identifier and Profile Identifier. [DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_SIM_ACCESS                                         0x112D // SIM Access Profile (SAP) NOTE: Used as both Service Class Identifier and Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS_PCE                               0x112E // Phonebook Access Profile (PBAP)
#define BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS_PSE                               0x112F // Phonebook Access Profile (PBAP)
#define BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS                                   0x1130 // Phonebook Access Profile (PBAP)
#define BLUETOOTH_SERVICE_CLASS_HEADSET_HS                                         0x1131 // Headset Profile (HSP) NOTE: See erratum #3507. 0x1108 and 0x1203 should also be included in the ServiceClassIDList before 0x1131 for backwards compatibility.
#define BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_SERVER                              0x1132 // Message Access Profile (MAP)
#define BLUETOOTH_SERVICE_CLASS_MESSAGE_NOTIFICATION_SERVER                        0x1133 // Message Access Profile (MAP)
#define BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_PROFILE                             0x1134 // Message Access Profile (MAP)
#define BLUETOOTH_SERVICE_CLASS_GNSS                                               0x1135 // Global Navigation Satellite System Profile (GNSS)
#define BLUETOOTH_SERVICE_CLASS_GNSS_SERVER                                        0x1136 // Global Navigation Satellite System Profile (GNSS)
#define BLUETOOTH_SERVICE_CLASS_3D_DISPLAY                                         0x1137 // 3D Synchronization Profile (3DSP)
#define BLUETOOTH_SERVICE_CLASS_3D_GLASSES                                         0x1138 // 3D Synchronization Profile (3DSP)
#define BLUETOOTH_SERVICE_CLASS_3D_SYNCHRONIZATION                                 0x1139 // 3D Synchronization Profile (3DSP)
#define BLUETOOTH_SERVICE_CLASS_MPS_PROFILE_UUID                                   0x113A // Multi-Profile Specification (MPS)
#define BLUETOOTH_SERVICE_CLASS_MPS_SC_UUID                                        0x113B // Multi-Profile Specification (MPS)
#define BLUETOOTH_SERVICE_CLASS_CTN_ACCESS_SERVICE                                 0x113C // Calendar, Task, andNotes (CTN)Profile
#define BLUETOOTH_SERVICE_CLASS_CTN_NOTIFICATION_SERVICE                           0x113D // CalendarTasksandNotes (CTN)Profile
#define BLUETOOTH_SERVICE_CLASS_CTN_PROFILE                                        0x113E // CalendarTasksandNotes (CTN)Profile
#define BLUETOOTH_SERVICE_CLASS_PNP_INFORMATION                                    0x1200 // Device Identification (DID) NOTE: Used as both Service Class Identifier and Profile Identifier.
#define BLUETOOTH_SERVICE_CLASS_GENERIC_NETWORKING                                 0x1201 // N/A
#define BLUETOOTH_SERVICE_CLASS_GENERIC_FILE_TRANSFER                              0x1202 // N/A
#define BLUETOOTH_SERVICE_CLASS_GENERIC_AUDIO                                      0x1203 // N/A
#define BLUETOOTH_SERVICE_CLASS_GENERIC_TELEPHONY                                  0x1204 // N/A
#define BLUETOOTH_SERVICE_CLASS_UPNP_SERVICE                                       0x1205 // Enhanced Service Discovery Profile (ESDP) [DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_UPNP_IP_SERVICE                                    0x1206 // Enhanced Service Discovery Profile (ESDP) [DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_ESDP_UPNP_IP_PAN                                   0x1300 // Enhanced Service Discovery Profile (ESDP) [DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_ESDP_UPNP_IP_LAP                                   0x1301 // Enhanced Service Discovery Profile (ESDP)[DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_ESDP_UPNP_L2CAP                                    0x1302 // Enhanced Service Discovery Profile (ESDP)[DEPRECATED]
#define BLUETOOTH_SERVICE_CLASS_VIDEO_SOURCE                                       0x1303 // Video Distribution Profile (VDP)
#define BLUETOOTH_SERVICE_CLASS_VIDEO_SINK                                         0x1304 // Video Distribution Profile (VDP)
#define BLUETOOTH_SERVICE_CLASS_VIDEO_DISTRIBUTION                                 0x1305 // Video Distribution Profile (VDP)
#define BLUETOOTH_SERVICE_CLASS_HDP                                                0x1400 // Health Device Profile
#define BLUETOOTH_SERVICE_CLASS_HDP_SOURCE                                         0x1401 // Health Device Profile (HDP)
#define BLUETOOTH_SERVICE_CLASS_HDP_SINK                                           0x1402 // Health Device Profile (HDP)

/**
 * Attributes
 */
#define BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT                                     0x1002 // Bluetooth Core Specification
#define BLUETOOTH_ATTRIBUTE_SUPPORTED_FEATURES                                     0x0311 // 
#define BLUETOOTH_ATTRIBUTE_GOEP_L2CAP_PSM                                         0x0200 // 
#define BLUETOOTH_ATTRIBUTE_SUPPORTED_CAPABILITIES                                 0x0310 // 
#define BLUETOOTH_ATTRIBUTE_SUPPORTED_FUNCTIONS                                    0x0312 // 
#define BLUETOOTH_ATTRIBUTE_TOTAL_IMAGING_DATA_CAPACITY                            0x0313 // 
#define BLUETOOTH_ATTRIBUTE_DOCUMENT_FORMATS_SUPPORTED                             0x0350 // 
#define BLUETOOTH_ATTRIBUTE_CHARACTER_REPERTOIRES_SUPPORTED                        0x0352 // 
#define BLUETOOTH_ATTRIBUTE_XHTML_PRINT_IMAGE_FORMATS_SUPPORTED                    0x0354 // 
#define BLUETOOTH_ATTRIBUTE_COLOR_SUPPORTED                                        0x0356 // 
#define BLUETOOTH_ATTRIBUTE_1284_ID                                                0x0358 // 
#define BLUETOOTH_ATTRIBUTE_PRINTER_NAME                                           0x035A // 
#define BLUETOOTH_ATTRIBUTE_PRINTER_LOCATION                                       0x035C // 
#define BLUETOOTH_ATTRIBUTE_DUPLEX_SUPPORTED                                       0x035E // 
#define BLUETOOTH_ATTRIBUTE_MEDIA_TYPES_SUPPORTED                                  0x0360 // 
#define BLUETOOTH_ATTRIBUTE_MAX_MEDIA_WIDTH                                        0x0362 // 
#define BLUETOOTH_ATTRIBUTE_MAX_MEDIA_LENGTH                                       0x0364 // 
#define BLUETOOTH_ATTRIBUTE_ENHANCED_LAYOUT_SUPPORTED                              0x0366 // 
#define BLUETOOTH_ATTRIBUTE_RUI_FORMATS_SUPPORTED                                  0x0368 // 
#define BLUETOOTH_ATTRIBUTE_REFERENCE_PRINTING_RUI_SUPPORTED                       0x0370 // 
#define BLUETOOTH_ATTRIBUTE_DIRECT_PRINTING_RUI_SUPPORTED                          0x0372 // 
#define BLUETOOTH_ATTRIBUTE_REFERENCE_PRINTING_TOP_URL                             0x0374 // 
#define BLUETOOTH_ATTRIBUTE_DIRECT_PRINTING_TOP_URL                                0x0376 // 
#define BLUETOOTH_ATTRIBUTE_PRINTER_ADMIN_RUI_TOP_URL                              0x0378 // 
#define BLUETOOTH_ATTRIBUTE_DEVICE_NAME                                            0x037A // 
#define BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE                                  0x0000 // 
#define BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST                                  0x0001 // 
#define BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_STATE                                   0x0002 // 
#define BLUETOOTH_ATTRIBUTE_SERVICE_ID                                             0x0003 // 
#define BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST                               0x0004 // 
#define BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST                                      0x0005 // 
#define BLUETOOTH_ATTRIBUTE_LANGUAGE_BASE_ATTRIBUTE_ID_LIST                        0x0006 // 
#define BLUETOOTH_ATTRIBUTE_SERVICE_INFO_TIME_TO_LIVE                              0x0007 // 
#define BLUETOOTH_ATTRIBUTE_SERVICE_AVAILABILITY                                   0x0008 // 
#define BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST                      0x0009 // 
#define BLUETOOTH_ATTRIBUTE_DOCUMENTATION_URL                                      0x000A // 
#define BLUETOOTH_ATTRIBUTE_CLIENT_EXECUTABLE_URL                                  0x000B // 
#define BLUETOOTH_ATTRIBUTE_ICON_URL                                               0x000C // 
#define BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS                   0x000D // 
#define BLUETOOTH_ATTRIBUTE_VERSION_NUMBER_LIST                                    0x0200 // 
#define BLUETOOTH_ATTRIBUTE_SERVICE_DATABASE_STATE                                 0x0201 // 
#define BLUETOOTH_ATTRIBUTE_SPECIFICATION_ID                                       0x0200 // 
#define BLUETOOTH_ATTRIBUTE_VENDOR_ID                                              0x0201 // 
#define BLUETOOTH_ATTRIBUTE_PRODUCT_ID                                             0x0202 // 
#define BLUETOOTH_ATTRIBUTE_VERSION                                                0x0203 // 
#define BLUETOOTH_ATTRIBUTE_PRIMARY_RECORD                                         0x0204 // 
#define BLUETOOTH_ATTRIBUTE_VENDOR_ID_SOURCE                                       0x0205 // 
#define BLUETOOTH_ATTRIBUTE_NETWORK                                                0x0301 // 
#define BLUETOOTH_ATTRIBUTE_FRIENDLY_NAME                                          0x0304 // 
#define BLUETOOTH_ATTRIBUTE_DEVICE_LOCATION                                        0x0306 // 
#define BLUETOOTH_ATTRIBUTE_REMOTE_AUDIO_VOLUME_CONTROL                            0x0302 // 
#define BLUETOOTH_ATTRIBUTE_SUPPORT_FEATURES_LIST                                  0x0200 // 
#define BLUETOOTH_ATTRIBUTE_DATA_EXCHANGE_SPECIFICATION                            0x0301 // 
#define BLUETOOTH_ATTRIBUTE_MCAP_SUPPORTED_PROCEDURES                              0x0302 // 
#define BLUETOOTH_ATTRIBUTE_HID_DEVICE_RELEASE_NUMBER                              0x0200 // 
#define BLUETOOTH_ATTRIBUTE_HID_PARSER_VERSION                                     0x0201 // 
#define BLUETOOTH_ATTRIBUTE_HID_DEVICE_SUBCLASS                                    0x0202 // 
#define BLUETOOTH_ATTRIBUTE_HID_COUNTRY_CODE                                       0x0203 // 
#define BLUETOOTH_ATTRIBUTE_HID_VIRTUAL_CABLE                                      0x0204 // 
#define BLUETOOTH_ATTRIBUTE_HID_RECONNECT_INITIATE                                 0x0205 // 
#define BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST                                    0x0206 // 
#define BLUETOOTH_ATTRIBUTE_HIDLANGID_BASE_LIST                                    0x0207 // 
#define BLUETOOTH_ATTRIBUTE_HIDSDP_DISABLE                                         0x0208 // 
#define BLUETOOTH_ATTRIBUTE_HID_BATTERY_POWER                                      0x0209 // 
#define BLUETOOTH_ATTRIBUTE_HID_REMOTE_WAKE                                        0x020A // 
#define BLUETOOTH_ATTRIBUTE_HID_PROFILE_VERSION                                    0x020B // 
#define BLUETOOTH_ATTRIBUTE_HID_SUPERVISION_TIMEOUT                                0x020C // 
#define BLUETOOTH_ATTRIBUTE_HID_NORMALLY_CONNECTABLE                               0x020D // 
#define BLUETOOTH_ATTRIBUTE_HID_BOOT_DEVICE                                        0x020E // 
#define BLUETOOTH_ATTRIBUTE_HIDSSR_HOST_MAX_LATENCY                                0x020F // 
#define BLUETOOTH_ATTRIBUTE_HIDSSR_HOST_MIN_TIMEOUT                                0x0210 // 
#define BLUETOOTH_ATTRIBUTE_MAS_INSTANCE_ID                                        0x0315 // 
#define BLUETOOTH_ATTRIBUTE_SUPPORTED_MESSAGE_TYPES                                0x0316 // 
#define BLUETOOTH_ATTRIBUTE_MAP_SUPPORTED_FEATURES                                 0x0317 // 
#define BLUETOOTH_ATTRIBUTE_SERVICE_VERSION                                        0x0300 // 
#define BLUETOOTH_ATTRIBUTE_SUPPORTED_FORMATS_LIST                                 0x0303 // 
#define BLUETOOTH_ATTRIBUTE_IP_SUBNET                                              0x0200 // 
#define BLUETOOTH_ATTRIBUTE_SECURITY_DESCRIPTION                                   0x030A // 
#define BLUETOOTH_ATTRIBUTE_NET_ACCESS_TYPE                                        0x030B // 
#define BLUETOOTH_ATTRIBUTE_MAX_NET_ACCESSRATE                                     0x030C // 
#define BLUETOOTH_ATTRIBUTE_IPV4_SUBNET                                            0x030D // 
#define BLUETOOTH_ATTRIBUTE_IPV6_SUBNET                                            0x030E // 
#define BLUETOOTH_ATTRIBUTE_SUPPORTED_REPOSITORIES                                 0x0314 // 
#define BLUETOOTH_ATTRIBUTE_PBAP_SUPPORTED_FEATURES                                0x0317 // 
#define BLUETOOTH_ATTRIBUTE_SUPPORTED_DATA_STORES_LIST                             0x0301 // 
#define BLUETOOTH_ATTRIBUTE_MPSD_SCENARIOS                                         0x0200 // 
#define BLUETOOTH_ATTRIBUTE_MPMD_SCENARIOS                                         0x0201 // 
#define BLUETOOTH_ATTRIBUTE_SUPPORTED_PROFILES_AND_PROTOCOLS                       0x0202 // 
#define BLUETOOTH_ATTRIBUTE_CAS_INSTANCE_ID                                        0x0315 // 
#define BLUETOOTH_ATTRIBUTE_CTN_SUPPORTED_FEATURES                                 0x0317 // 
#define BLUETOOTH_ATTRIBUTE_GNSS_SUPPORTED_FEATURES                                0x0200

#endif
