/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * @title GATT Client
 *
 */

#ifndef btstack_gatt_client_h
#define btstack_gatt_client_h

#include "hci.h"

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    P_READY,
    P_W2_SEND_SERVICE_QUERY,
    P_W4_SERVICE_QUERY_RESULT,
    P_W2_SEND_SERVICE_WITH_UUID_QUERY,
    P_W4_SERVICE_WITH_UUID_RESULT,
    
    P_W2_SEND_ALL_CHARACTERISTICS_OF_SERVICE_QUERY,
    P_W4_ALL_CHARACTERISTICS_OF_SERVICE_QUERY_RESULT,
    P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY,
    P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT,
    
    P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY,
    P_W4_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT,
    
    P_W2_SEND_INCLUDED_SERVICE_QUERY,
    P_W4_INCLUDED_SERVICE_QUERY_RESULT,
    P_W2_SEND_INCLUDED_SERVICE_WITH_UUID_QUERY,
    P_W4_INCLUDED_SERVICE_UUID_WITH_QUERY_RESULT,
    
    P_W2_SEND_READ_CHARACTERISTIC_VALUE_QUERY,
    P_W4_READ_CHARACTERISTIC_VALUE_RESULT,
    
    P_W2_SEND_READ_BLOB_QUERY,
    P_W4_READ_BLOB_RESULT,

    P_W2_SEND_READ_BY_TYPE_REQUEST,
    P_W4_READ_BY_TYPE_RESPONSE,
    
    P_W2_SEND_READ_MULTIPLE_REQUEST,
    P_W4_READ_MULTIPLE_RESPONSE,

    P_W2_SEND_WRITE_CHARACTERISTIC_VALUE,
    P_W4_WRITE_CHARACTERISTIC_VALUE_RESULT,
    
    P_W2_PREPARE_WRITE,
    P_W4_PREPARE_WRITE_RESULT,
    P_W2_PREPARE_RELIABLE_WRITE,
    P_W4_PREPARE_RELIABLE_WRITE_RESULT,
    
    P_W2_EXECUTE_PREPARED_WRITE,
    P_W4_EXECUTE_PREPARED_WRITE_RESULT,
    P_W2_CANCEL_PREPARED_WRITE,
    P_W4_CANCEL_PREPARED_WRITE_RESULT,
    P_W2_CANCEL_PREPARED_WRITE_DATA_MISMATCH,
    P_W4_CANCEL_PREPARED_WRITE_DATA_MISMATCH_RESULT,
    
#ifdef ENABLE_GATT_FIND_INFORMATION_FOR_CCC_DISCOVERY
    P_W2_SEND_FIND_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY,
    P_W4_FIND_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT,
#else
    P_W2_SEND_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY,
    P_W4_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT,
#endif
    P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION,
    P_W4_CLIENT_CHARACTERISTIC_CONFIGURATION_RESULT,

    P_W2_SEND_READ_CHARACTERISTIC_DESCRIPTOR_QUERY,
    P_W4_READ_CHARACTERISTIC_DESCRIPTOR_RESULT,
    
    P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY,
    P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT,
    
    P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR,
    P_W4_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT,

    // all long writes use this    
    P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR,
    P_W4_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT,
    P_W2_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR,
    P_W4_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT,

    // gatt reliable write API use this (manual version of the above)
    P_W2_PREPARE_WRITE_SINGLE,
    P_W4_PREPARE_WRITE_SINGLE_RESULT,

    P_W4_IDENTITY_RESOLVING,
    P_W4_CMAC_READY,
    P_W4_CMAC_RESULT,
    P_W2_SEND_SIGNED_WRITE,
    P_W4_SEND_SINGED_WRITE_DONE,

    P_W2_SDP_QUERY,
    P_W4_SDP_QUERY,
    P_W4_L2CAP_CONNECTION,
} gatt_client_state_t;
    
    
typedef enum{
    SEND_MTU_EXCHANGE,
    SENT_MTU_EXCHANGE,
    MTU_EXCHANGED,
    MTU_AUTO_EXCHANGE_DISABLED
} gatt_client_mtu_t;

typedef struct gatt_client{
    btstack_linked_item_t    item;
    // TODO: rename gatt_client_state -> state
    gatt_client_state_t gatt_client_state;

    // user callback 
    btstack_packet_handler_t callback;
    
    // can write without response callback
    btstack_packet_handler_t write_without_response_callback;

    // can write without response requests
    btstack_linked_list_t write_without_response_requests;

    // regular gatt query requests
    btstack_linked_list_t query_requests;

    hci_con_handle_t con_handle;

#ifdef ENABLE_GATT_OVER_CLASSIC
    bd_addr_t addr;
    uint16_t  l2cap_psm;
    uint16_t  l2cap_cid;
    btstack_context_callback_registration_t sdp_query_request;
#endif

    uint16_t          mtu;
    gatt_client_mtu_t mtu_state;
    
    uint16_t uuid16;
    uint8_t  uuid128[16];
    
    uint16_t start_group_handle;
    uint16_t end_group_handle;
    
    uint16_t query_start_handle;
    uint16_t query_end_handle;
    
    uint8_t  characteristic_properties;
    uint16_t characteristic_start_handle;
    
    uint16_t attribute_handle;
    uint16_t attribute_offset;
    uint16_t attribute_length;
    uint8_t* attribute_value;
    
    // read multiple characteristic values
    uint16_t    read_multiple_handle_count;
    uint16_t  * read_multiple_handles;

    uint16_t client_characteristic_configuration_handle;
    uint8_t  client_characteristic_configuration_value[2];
    
    uint8_t  filter_with_uuid;
    uint8_t  send_confirmation;

    int      le_device_index;
    uint8_t  cmac[8];

    btstack_timer_source_t gc_timeout;

    uint8_t  security_counter;
    uint8_t  wait_for_authentication_complete;
    uint8_t  pending_error_code;

    bool     reencryption_active;
    uint8_t  reencryption_result;

    gap_security_level_t security_level;

} gatt_client_t;

typedef struct gatt_client_notification {
    btstack_linked_item_t    item;
    btstack_packet_handler_t callback;
    hci_con_handle_t con_handle;
    uint16_t attribute_handle;
} gatt_client_notification_t;

/* API_START */

typedef struct {
    uint16_t start_group_handle;
    uint16_t end_group_handle;
    uint16_t uuid16;
    uint8_t  uuid128[16];
} gatt_client_service_t;

typedef struct {
    uint16_t start_handle;
    uint16_t value_handle;
    uint16_t end_handle;
    uint16_t properties;
    uint16_t uuid16;
    uint8_t  uuid128[16];
} gatt_client_characteristic_t;

typedef struct {
    uint16_t handle;
    uint16_t uuid16;
    uint8_t  uuid128[16];
} gatt_client_characteristic_descriptor_t;

/** 
 * @brief Set up GATT client.
 */
void gatt_client_init(void);

/**
 * @brief Set minimum required security level for GATT Client
 * @note  The Bluetooth specification makes the GATT Server responsible to check for security.
 *        This allows an attacker to spoof an existing device with a GATT Servers, but skip the authentication part.
 *        If your application is exchanging sensitive data with a remote device, you would need to manually check
 *        the security level before sending/receive such data.
 *        With level > 0, the GATT Client triggers authentication for all GATT Requests and defers any exchange
 *        until the required security level is established.
 *        gatt_client_request_can_write_without_response_event does not trigger authentication
 *        gatt_client_request_to_write_without_response does not trigger authentication
 *  @pram level, default LEVEL_0 (no encryption required)
 */
void gatt_client_set_required_security_level(gap_security_level_t level);

/**
 * @brief Connect to remote GATT Server over Classic (BR/EDR) Connection
 *        GATT_EVENT_CONNECTED with status and con_handle for other API functions
 *        is emitted on connection complete.
 * @note requires ENABLE_GATT_OVER_CLASSIC.
 * @param addr
 * @return status
 */
uint8_t gatt_client_classic_connect(btstack_packet_handler_t callback, bd_addr_t addr);

/**
 * @brief Disconnect o Classic (BR/EDR) connection to a remote GATT Server
 * @note requires ENABLE_GATT_OVER_CLASSIC
 * @param con_handle
 * @return status
 */
uint8_t gatt_client_classic_disconnect(btstack_packet_handler_t callback, hci_con_handle_t con_handle);

/**
 * @brief MTU is available after the first query has completed. If status is equal to ERROR_CODE_SUCCESS, it returns the real value, 
 * otherwise the default value ATT_DEFAULT_MTU (see bluetooth.h). 
 * @param  con_handle   
 * @param  mtu
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if MTU is not exchanged and MTU auto-exchange is disabled
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_get_mtu(hci_con_handle_t con_handle, uint16_t * mtu);

/**
 * @brief Sets whether a MTU Exchange Request shall be automatically send before the 
 * first attribute read request is send. Default is enabled.
 * @param enabled   
 */
void gatt_client_mtu_enable_auto_negotiation(uint8_t enabled);

/**
 * @brief Sends a MTU Exchange Request, this allows for the client to exchange MTU 
 * when gatt_client_mtu_enable_auto_negotiation is disabled.
 * @param  callback   
 * @param  con_handle
 */
void gatt_client_send_mtu_negotiation(btstack_packet_handler_t callback, hci_con_handle_t con_handle);

/** 
 * @brief Returns 1 if the GATT client is ready to receive a query. It is used with daemon. 
 * @param  con_handle
 * @return is_ready_status     0 - if no GATT client for con_handle is found, or is not ready, otherwise 1
 */
int gatt_client_is_ready(hci_con_handle_t con_handle);

/** 
 * @brief Discovers all primary services. 
 * For each found service a GATT_EVENT_SERVICE_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery. 
 * @param  callback   
 * @param  con_handle
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered
 */
uint8_t gatt_client_discover_primary_services(btstack_packet_handler_t callback, hci_con_handle_t con_handle);

/** 
 * @brief Discovers all secondary services. 
 * For each found service a GATT_EVENT_SERVICE_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery. 
 * @param  callback   
 * @param  con_handle
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered
 */
uint8_t gatt_client_discover_secondary_services(btstack_packet_handler_t callback, hci_con_handle_t con_handle);

/** 
 * @brief Discovers a specific primary service given its UUID. This service may exist multiple times. 
 * For each found service a GATT_EVENT_SERVICE_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery. 
 * @param callback   
 * @param con_handle
 * @param uuid16
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered  
 */
uint8_t gatt_client_discover_primary_services_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t uuid16);

/** 
 * @brief Discovers a specific primary service given its UUID. This service may exist multiple times. 
 * For each found service a GATT_EVENT_SERVICE_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery. 
 * @param  callback   
 * @param  con_handle
 * @param  uuid128
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered  
 */
uint8_t gatt_client_discover_primary_services_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, const uint8_t * uuid128);

/** 
 * @brief Finds included services within the specified service. 
 * For each found included service a GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT event will be emitted. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery. 
 * Information about included service type (primary/secondary) can be retrieved either by sending 
 * an ATT find information request for the returned start group handle 
 * (returning the handle and the UUID for primary or secondary service) or by comparing the service 
 * to the list of all primary services. 
 * @param  callback   
 * @param  con_handle
 * @param  service
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered  
 */
uint8_t gatt_client_find_included_services_for_service(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service);

/** 
 * @brief Discovers all characteristics within the specified service. 
 * For each found characteristic a GATT_EVENT_CHARACTERISTIC_QUERY_RESULT event will be emited. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery.
 * @param  callback   
 * @param  con_handle
 * @param  service
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_discover_characteristics_for_service(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service);

/** 
 * @brief The following four functions are used to discover all characteristics within 
 * the specified service or handle range, and return those that match the given UUID. 
 * 
 * For each found characteristic a GATT_EVENT_CHARACTERISTIC_QUERY_RESULT event will emitted. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery.
 * @param  callback   
 * @param  con_handle
 * @param  start_handle
 * @param  end_handle
 * @param  uuid16
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_discover_characteristics_for_handle_range_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16);

/** 
 * @brief The following four functions are used to discover all characteristics within the 
 * specified service or handle range, and return those that match the given UUID. 
 * For each found characteristic a GATT_EVENT_CHARACTERISTIC_QUERY_RESULT event will emitted. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery.
 * @param  callback   
 * @param  con_handle
 * @param  start_handle
 * @param  end_handle
 * @param  uuid128
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_discover_characteristics_for_handle_range_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, const uint8_t * uuid128);

/** 
 * @brief The following four functions are used to discover all characteristics within the 
 * specified service or handle range, and return those that match the given UUID. 
 * For each found characteristic a GATT_EVENT_CHARACTERISTIC_QUERY_RESULT event will emitted. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery.
 * @param  callback   
 * @param  con_handle
 * @param  service
 * @param  uuid16
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_discover_characteristics_for_service_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service, uint16_t uuid16);

/** 
 * @brief The following four functions are used to discover all characteristics within the 
 * specified service or handle range, and return those that match the given UUID. 
 * For each found characteristic a GATT_EVENT_CHARACTERISTIC_QUERY_RESULT event will emitted. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery.
 * @param  callback   
 * @param  con_handle
 * @param  service
 * @param  uuid128
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_discover_characteristics_for_service_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service, const uint8_t * uuid128);

/** 
 * @brief Discovers attribute handle and UUID of a characteristic descriptor within the specified characteristic. 
 * For each found descriptor a GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT event will be emitted.
 * 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of discovery.
 * @param  callback   
 * @param  con_handle
 * @param  characteristic 
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_discover_characteristic_descriptors(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic);

/** 
 * @brief Reads the characteristic value using the characteristic's value handle. 
 * If the characteristic value is found a GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  characteristic 
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_read_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic);

/** 
 * @brief Reads the characteristic value using the characteristic's value handle. 
 * If the characteristic value is found a GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  value_handle 
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_read_value_of_characteristic_using_value_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle);

/**
 * @brief Reads the characteric value of all characteristics with the uuid. 
 * For each characteristic value found a GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  start_handle
 * @param  end_handle
 * @param  uuid16
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_read_value_of_characteristics_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16);

/**
 * @brief Reads the characteric value of all characteristics with the uuid. 
 * For each characteristic value found a GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  start_handle
 * @param  end_handle
 * @param  uuid128
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_read_value_of_characteristics_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, const uint8_t * uuid128);

/** 
 * @brief Reads the long characteristic value using the characteristic's value handle. 
 * The value will be returned in several blobs. 
 * For each blob, a GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT event with updated value offset will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  characteristic 
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_read_long_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic);

/** 
 * @brief Reads the long characteristic value using the characteristic's value handle. 
 * The value will be returned in several blobs. 
 * For each blob, a GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT event with updated value offset will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  value_handle 
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_read_long_value_of_characteristic_using_value_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle);

/** 
 * @brief Reads the long characteristic value using the characteristic's value handle. 
 * The value will be returned in several blobs. 
 * For each blob, a GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT event with updated value offset will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  value_handle
 * @param  offset 
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t offset);

/*
 * @brief Read multiple characteristic values.
 * The all results are emitted via single GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT event, 
 * followed by the GATT_EVENT_QUERY_COMPLETE event, which marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  num_value_handles 
 * @param  value_handles list of handles 
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_read_multiple_characteristic_values(btstack_packet_handler_t callback, hci_con_handle_t con_handle, int num_value_handles, uint16_t * value_handles);

/** 
 * @brief Writes the characteristic value using the characteristic's value handle without 
 * an acknowledgment that the write was successfully performed.
 * @param  con_handle   
 * @param  value_handle
 * @param  value_length
 * @param  value is copied on success and does not need to be retained
 * @return status BTSTACK_MEMORY_ALLOC_FAILED, if no GATT client for con_handle is found 
 *                GATT_CLIENT_IN_WRONG_STATE , if GATT client is not ready
 *                BTSTACK_ACL_BUFFERS_FULL   , if L2CAP cannot send, there are no free ACL slots
 *                ERROR_CODE_SUCCESS         , if query is successfully registered 
 */
uint8_t gatt_client_write_value_of_characteristic_without_response(hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value);

/** 
 * @brief Writes the authenticated characteristic value using the characteristic's value handle 
 * without an acknowledgment that the write was successfully performed.
 * @note GATT_EVENT_QUERY_COMPLETE is emitted with ATT_ERROR_SUCCESS for success,
 * or ATT_ERROR_BONDING_INFORMATION_MISSING if there is no bonding information stored.
 * @param  callback   
 * @param  con_handle
 * @param  value_handle
 * @param  message_len
 * @param  message is not copied, make sure memory is accessible until write is done
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_signed_write_without_response(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t message_len, uint8_t * message);

/** 
 * @brief Writes the characteristic value using the characteristic's value handle. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of write. 
 * The write is successfully performed, if the event's att_status field is set to 
 * ATT_ERROR_SUCCESS (see bluetooth.h for ATT_ERROR codes).
 * @param  callback   
 * @param  con_handle
 * @param  value_handle
 * @param  value_length
 * @param  value is not copied, make sure memory is accessible until write is done, i.e. GATT_EVENT_QUERY_COMPLETE is received
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_write_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value);

/** 
 * @brief Writes the characteristic value using the characteristic's value handle. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of write. 
 * The write is successfully performed if the event's att_status field is set to ATT_ERROR_SUCCESS (see bluetooth.h for ATT_ERROR codes).
 * @param  callback   
 * @param  con_handle
 * @param  value_handle
 * @param  value_length
 * @param  value is not copied, make sure memory is accessible until write is done, i.e. GATT_EVENT_QUERY_COMPLETE is received
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_write_long_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value);

/** 
 * @brief Writes the characteristic value using the characteristic's value handle. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of write. 
 * The write is successfully performed if the event's att_status field is set to ATT_ERROR_SUCCESS (see bluetooth.h for ATT_ERROR codes).
 * @param  callback   
 * @param  con_handle
 * @param  value_handle
 * @param  offset of value
 * @param  value_length
 * @param  value is not copied, make sure memory is accessible until write is done, i.e. GATT_EVENT_QUERY_COMPLETE is received
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_write_long_value_of_characteristic_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t offset, uint16_t value_length, uint8_t * value);

/** 
 * @brief Writes of the long characteristic value using the characteristic's value handle. 
 * It uses server response to validate that the write was correctly received. 
 * The GATT_EVENT_QUERY_COMPLETE EVENT marks the end of write.
 * The write is successfully performed, if the event's att_status field is set to ATT_ERROR_SUCCESS (see bluetooth.h for ATT_ERROR codes). 
 * @param  callback   
 * @param  con_handle
 * @param  value_handle
 * @param  value_length
 * @param  value is not copied, make sure memory is accessible until write is done, i.e. GATT_EVENT_QUERY_COMPLETE is received
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_reliable_write_long_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value);

/** 
 * @brief Reads the characteristic descriptor using its handle. 
 * If the characteristic descriptor is found, a GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  descriptor
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_read_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor);

/** 
 * @brief Reads the characteristic descriptor using its handle. 
 * If the characteristic descriptor is found, a GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  descriptor
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_read_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle);

/** 
 * @brief Reads the long characteristic descriptor using its handle. It will be returned in several blobs. 
 * For each blob, a GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  descriptor
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_read_long_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor);

/** 
 * @brief Reads the long characteristic descriptor using its handle. It will be returned in several blobs. 
 * For each blob, a GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  descriptor_handle
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_read_long_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle);

/** 
 * @brief Reads the long characteristic descriptor using its handle. It will be returned in several blobs. 
 * For each blob, a GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT event will be emitted.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of read.
 * @param  callback   
 * @param  con_handle
 * @param  descriptor_handle
 * @param  offset
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t offset);

/** 
 * @brief Writes the characteristic descriptor using its handle. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of write. 
 * The write is successfully performed if the event's att_status field is set to ATT_ERROR_SUCCESS (see bluetooth.h for ATT_ERROR codes).
 * @param  callback   
 * @param  con_handle
 * @param  descriptor
 * @param  value_length
 * @param  value is not copied, make sure memory is accessible until write is done, i.e. GATT_EVENT_QUERY_COMPLETE is received
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_write_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor, uint16_t value_length, uint8_t * value);

/** 
 * @brief Writes the characteristic descriptor using its handle. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of write. 
 * The write is successfully performed if the event's att_status field is set to ATT_ERROR_SUCCESS (see bluetooth.h for ATT_ERROR codes).
 * @param  callback   
 * @param  con_handle
 * @param  descriptor_handle
 * @param  value_length
 * @param  value is not copied, make sure memory is accessible until write is done, i.e. GATT_EVENT_QUERY_COMPLETE is received
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_write_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t value_length, uint8_t * value);

/** 
 * @brief Writes the characteristic descriptor using its handle. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of write. 
 * The write is successfully performed if the event's att_status field is set to ATT_ERROR_SUCCESS (see bluetooth.h for ATT_ERROR codes).
 * @param  callback   
 * @param  con_handle
 * @param  descriptor
 * @param  value_length
 * @param  value is not copied, make sure memory is accessible until write is done, i.e. GATT_EVENT_QUERY_COMPLETE is received
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_write_long_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor, uint16_t value_length, uint8_t * value);

/** 
 * @brief Writes the characteristic descriptor using its handle. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of write. 
 * The write is successfully performed if the event's att_status field is set to ATT_ERROR_SUCCESS (see bluetooth.h for ATT_ERROR codes).
 * @param  callback   
 * @param  con_handle
 * @param  descriptor_handle
 * @param  value_length
 * @param  value is not copied, make sure memory is accessible until write is done, i.e. GATT_EVENT_QUERY_COMPLETE is received
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_write_long_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t value_length, uint8_t * value);

/** 
 * @brief Writes the characteristic descriptor using its handle. 
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of write. 
 * The write is successfully performed if the event's att_status field is set to ATT_ERROR_SUCCESS (see bluetooth.h for ATT_ERROR codes).
 * @param  callback   
 * @param  con_handle
 * @param  descriptor_handle
 * @param  offset of value
 * @param  value_length
 * @param  value is not copied, make sure memory is accessible until write is done, i.e. GATT_EVENT_QUERY_COMPLETE is received
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_write_long_characteristic_descriptor_using_descriptor_handle_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t offset, uint16_t value_length, uint8_t * value);

/** 
 * @brief Writes the client characteristic configuration of the specified characteristic. 
 * It is used to subscribe for notifications or indications of the characteristic value. 
 * For notifications or indications specify: GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION
 * resp. GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION as configuration value.
 * The GATT_EVENT_QUERY_COMPLETE event marks the end of write. 
 * The write is successfully performed if the event's att_status field is set to ATT_ERROR_SUCCESS (see bluetooth.h for ATT_ERROR codes).
 * @param  callback
 * @param  con_handle
 * @param  characteristic
 * @param  configuration                                                    GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION
 * @return status ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER                  if no HCI connection for con_handle is found 
 *                BTSTACK_MEMORY_ALLOC_FAILED                               if no GATT client for con_handle could be allocated 
 *                GATT_CLIENT_IN_WRONG_STATE                                if GATT client is not ready
 *                GATT_CLIENT_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED     if configuring notification, but characteristic has no notification property set
 *                GATT_CLIENT_CHARACTERISTIC_INDICATION_NOT_SUPPORTED       if configuring indication, but characteristic has no indication property set
 *                ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE         if configuration is invalid
 *                ERROR_CODE_SUCCESS                                        if query is successfully registered
 */
uint8_t gatt_client_write_client_characteristic_configuration(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic, uint16_t configuration);

/**
 * @brief Register for notifications and indications of a characteristic enabled by 
 * the gatt_client_write_client_characteristic_configuration function.
 * @param notification struct used to store registration
 * @param callback
 * @param con_handle or GATT_CLIENT_ANY_CONNECTION to receive updates from all connected devices
 * @param characteristic or NULL to receive updates for all characteristics
 */
void gatt_client_listen_for_characteristic_value_updates(gatt_client_notification_t * notification, btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic);

/**
 * @brief Stop listening to characteristic value updates registered with
 * the gatt_client_listen_for_characteristic_value_updates function.
 * @param notification struct used in gatt_client_listen_for_characteristic_value_updates
 */
void gatt_client_stop_listening_for_characteristic_value_updates(gatt_client_notification_t * notification);

/**
 * @brief Transactional write. It can be called as many times as it is needed to write the characteristics within the same transaction. 
 * Call the gatt_client_execute_write function to commit the transaction.
 * @param  callback   
 * @param  con_handle
 * @param  attribute_handle
 * @param  offset of value
 * @param  value_length
 * @param  value is not copied, make sure memory is accessible until write is done, i.e. GATT_EVENT_QUERY_COMPLETE is received
 */
uint8_t gatt_client_prepare_write(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint16_t value_length, uint8_t * value);

/**
 * @brief Commit transactional write. GATT_EVENT_QUERY_COMPLETE is received.
 * @param  callback   
 * @param  con_handle
 * @return status
 */
uint8_t gatt_client_execute_write(btstack_packet_handler_t callback, hci_con_handle_t con_handle);

/**
 * @brief Abort transactional write. GATT_EVENT_QUERY_COMPLETE is received.
 * @param  callback   
 * @param  con_handle
 * @return status
 */
uint8_t gatt_client_cancel_write(btstack_packet_handler_t callback, hci_con_handle_t con_handle);

/**
 * @brief Request callback when regular gatt query can be sent
 * @note callback might happen during call to this function
 * @param callback_registration to point to callback function and context information
 * @param con_handle
 * @return ERROR_CODE_SUCCESS if ok, ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if handle unknown, and ERROR_CODE_COMMAND_DISALLOWED if callback already registered
 */
uint8_t gatt_client_request_to_send_gatt_query(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle);

/**
 * @brief Request callback when writing characteristic value without response is possible
 * @note callback might happen during call to this function
 * @param callback_registration to point to callback function and context information
 * @param con_handle
 * @return ERROR_CODE_SUCCESS if ok, ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if handle unknown, and ERROR_CODE_COMMAND_DISALLOWED if callback already registered
 */
uint8_t gatt_client_request_to_write_without_response(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle);


// the following functions are marked as deprecated and will be removed eventually
/**
 * @brief Requests GATT_EVENT_CAN_WRITE_WITHOUT_RESPONSE that guarantees
 * a single successful gatt_client_write_value_of_characteristic_without_response call.
 * @deprecated please use gatt_client_request_to_write_without_response instead
 * @param  callback
 * @param  con_handle
 * @return status
 */
uint8_t gatt_client_request_can_write_without_response_event(btstack_packet_handler_t callback, hci_con_handle_t con_handle);


/* API_END */

// used by generated btstack_event.c

void gatt_client_deserialize_service(const uint8_t *packet, int offset, gatt_client_service_t * service);
void gatt_client_deserialize_characteristic(const uint8_t * packet, int offset, gatt_client_characteristic_t * characteristic);
void gatt_client_deserialize_characteristic_descriptor(const uint8_t * packet, int offset, gatt_client_characteristic_descriptor_t * descriptor);

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
void gatt_client_att_packet_handler_fuzz(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size);
uint8_t gatt_client_get_client(hci_con_handle_t con_handle, gatt_client_t ** gatt_client);
#endif

#if defined __cplusplus
}
#endif

#endif
