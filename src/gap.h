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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#ifndef GAP_H
#define GAP_H

#if defined __cplusplus
extern "C" {
#endif

#include "btstack_defines.h"
#include "btstack_util.h"
#include "classic/btstack_link_key_db.h"

typedef enum {

	// MITM protection not required
	// No encryption required
	// No user interaction required
	LEVEL_0 = 0,

	// MITM protection not required
	// No encryption required
	// Minimal user interaction desired
	LEVEL_1,

	// MITM protection not required
	// Encryption required
	LEVEL_2,

	// MITM protection required
	// Encryption required
	// User interaction acceptable
	LEVEL_3,

	// MITM protection required
	// Encryption required
	// 128-bit equivalent strength for link and encryption keys required (P-192 is not enough)
	// User interaction acceptable
	LEVEL_4,	
} gap_security_level_t;

typedef enum {
	GAP_SECURITY_NONE,
	GAP_SECUIRTY_ENCRYPTED,		// SSP: JUST WORKS
	GAP_SECURITY_AUTHENTICATED, // SSP: numeric comparison, passkey, OOB 
	// GAP_SECURITY_AUTHORIZED
} gap_security_state;

typedef enum {
	GAP_CONNECTION_INVALID,
	GAP_CONNECTION_ACL,
	GAP_CONNECTION_SCO,
	GAP_CONNECTION_LE
} gap_connection_type_t;

typedef struct le_connection_parameter_range{
    uint16_t le_conn_interval_min;
    uint16_t le_conn_interval_max;
    uint16_t le_conn_latency_min;
    uint16_t le_conn_latency_max;
    uint16_t le_supervision_timeout_min;
    uint16_t le_supervision_timeout_max;
} le_connection_parameter_range_t;

typedef enum {
    GAP_RANDOM_ADDRESS_TYPE_OFF = 0,
    GAP_RANDOM_ADDRESS_TYPE_STATIC,
    GAP_RANDOM_ADDRESS_NON_RESOLVABLE,
    GAP_RANDOM_ADDRESS_RESOLVABLE,
} gap_random_address_type_t;

// Authorization state
typedef enum {
    AUTHORIZATION_UNKNOWN,
    AUTHORIZATION_PENDING,
    AUTHORIZATION_DECLINED,
    AUTHORIZATION_GRANTED
} authorization_state_t;


/* API_START */

// Classic + LE

/**
 * @brief Disconnect connection with handle
 * @param handle
 */
uint8_t gap_disconnect(hci_con_handle_t handle);

/**
 * @brief Get connection type
 * @param con_handle
 * @result connection_type
 */
gap_connection_type_t gap_get_connection_type(hci_con_handle_t connection_handle);

// Classic

/** 
 * @brief Sets local name.
 * @note has to be done before stack starts up
 * @note Default name is 'BTstack 00:00:00:00:00:00'
 * @note '00:00:00:00:00:00' in local_name will be replaced with actual bd addr
 * @param name is not copied, make sure memory is accessible during stack startup
 */
void gap_set_local_name(const char * local_name);

/**
 * @brief Set Extended Inquiry Response data
 * @note has to be done before stack starts up
 * @note If not set, local name will be used for EIR data (see gap_set_local_name)
 * @note '00:00:00:00:00:00' in local_name will be replaced with actual bd addr
 * @param eir_data size 240 bytes, is not copied make sure memory is accessible during stack startup
 */
void gap_set_extended_inquiry_response(const uint8_t * data); 

/**
 * @brief Set class of device that will be set during Bluetooth init.
 * @note has to be done before stack starts up
 */
void gap_set_class_of_device(uint32_t class_of_device);

/**
 * @brief Set default link policy settings for all classic ACL links
 * @param default_link_policy_settings - see LM_LINK_POLICY_* in bluetooth.h
 * @note common value: LM_LINK_POLICY_ENABLE_ROLE_SWITCH | LM_LINK_POLICY_ENABLE_SNIFF_MODE to enable role switch and sniff mode
 * @note has to be done before stack starts up
 */
void gap_set_default_link_policy_settings(uint16_t default_link_policy_settings);

/**
 * @brief Enable/disable bonding. Default is enabled.
 * @param enabled
 */
void gap_set_bondable_mode(int enabled);

/**  
 * @brief Get bondable mode.
 * @return 1 if bondable
 */
int gap_get_bondable_mode(void);

/**
 * @brief Register filter for rejecting classic connections. Callback will return 1 accept connection, 0 on reject.
 */

void gap_register_classic_connection_filter(int (*accept_callback)(bd_addr_t addr));


/* Configure Secure Simple Pairing */

/**
 * @brief Enable will enable SSP during init.
 */
void gap_ssp_set_enable(int enable);

/**
 * @brief Set IO Capability. BTstack will return capability to SSP requests
 */
void gap_ssp_set_io_capability(int ssp_io_capability);

/**
 * @brief Set Authentication Requirements using during SSP
 */
void gap_ssp_set_authentication_requirement(int authentication_requirement);

/**
 * @brief If set, BTstack will confirm a numeric comparison and enter '000000' if requested.
 */
void gap_ssp_set_auto_accept(int auto_accept);

/**
 * @brief Set required encryption key size for GAP Levels 1-3 on ccassic connections. Default: 16 bytes
 * @param encryption_key_size in bytes. Valid 7..16
 */
void gap_set_required_encryption_key_size(uint8_t encryption_key_size);

/**
 * @brief Start dedicated bonding with device. Disconnect after bonding.
 * @param device
 * @param request MITM protection
 * @return error, if max num acl connections active
 * @result GAP_DEDICATED_BONDING_COMPLETE
 */
int gap_dedicated_bonding(bd_addr_t device, int mitm_protection_required);

gap_security_level_t gap_security_level_for_link_key_type(link_key_type_t link_key_type);

/**
 * @brief map link keys to secure connection yes/no
 */
int gap_secure_connection_for_link_key_type(link_key_type_t link_key_type);

/**
 * @brief map link keys to authenticated
 */
int gap_authenticated_for_link_key_type(link_key_type_t link_key_type);

gap_security_level_t gap_security_level(hci_con_handle_t con_handle);

void gap_request_security_level(hci_con_handle_t con_handle, gap_security_level_t level);

int  gap_mitm_protection_required_for_security_level(gap_security_level_t level);

// LE

/**
 * @brief Set parameters for LE Scan
 */
void gap_set_scan_parameters(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window);

/**
 * @brief Start LE Scan 
 */
void gap_start_scan(void);

/**
 * @brief Stop LE Scan
 */
void gap_stop_scan(void);

/**
 * @brief Enable privacy by using random addresses
 * @param random_address_type to use (incl. OFF)
 */
void gap_random_address_set_mode(gap_random_address_type_t random_address_type);

/**
 * @brief Get privacy mode
 */
gap_random_address_type_t gap_random_address_get_mode(void);

/**
 * @brief Sets update period for random address
 * @param period_ms in ms
 */
 void gap_random_address_set_update_period(int period_ms);

/** 
 * @brief Sets a fixed random address for advertising
 * @param addr
 * @note Sets random address mode to type off
 */
void gap_random_address_set(bd_addr_t addr);

/**
 * @brief Set Advertisement Data
 * @param advertising_data_length
 * @param advertising_data (max 31 octets)
 * @note data is not copied, pointer has to stay valid
 * @note '00:00:00:00:00:00' in advertising_data will be replaced with actual bd addr
 */
void gap_advertisements_set_data(uint8_t advertising_data_length, uint8_t * advertising_data);

/**
 * @brief Set Advertisement Paramters
 * @param adv_int_min
 * @param adv_int_max
 * @param adv_type
 * @param direct_address_type
 * @param direct_address
 * @param channel_map
 * @param filter_policy
 * @note own_address_type is used from gap_random_address_set_mode
 */
void gap_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
	uint8_t direct_address_typ, bd_addr_t direct_address, uint8_t channel_map, uint8_t filter_policy);

/** 
 * @brief Enable/Disable Advertisements. OFF by default.
 * @param enabled
 */
void gap_advertisements_enable(int enabled);

/** 
 * @brief Set Scan Response Data
 *
 * @note For scan response data, scannable undirected advertising (ADV_SCAN_IND) need to be used
 *
 * @param advertising_data_length
 * @param advertising_data (max 31 octets)
 * @note data is not copied, pointer has to stay valid
 * @note '00:00:00:00:00:00' in scan_response_data will be replaced with actual bd addr
 */
void gap_scan_response_set_data(uint8_t scan_response_data_length, uint8_t * scan_response_data);

/**
 * @brief Set connection parameters for outgoing connections
 * @param conn_scan_interval (unit: 0.625 msec), default: 60 ms
 * @param conn_scan_window (unit: 0.625 msec), default: 30 ms
 * @param conn_interval_min (unit: 1.25ms), default: 10 ms
 * @param conn_interval_max (unit: 1.25ms), default: 30 ms
 * @param conn_latency, default: 4
 * @param supervision_timeout (unit: 10ms), default: 720 ms
 * @param min_ce_length (unit: 0.625ms), default: 10 ms
 * @param max_ce_length (unit: 0.625ms), default: 30 ms
 */
void gap_set_connection_parameters(uint16_t conn_scan_interval, uint16_t conn_scan_window, 
    uint16_t conn_interval_min, uint16_t conn_interval_max, uint16_t conn_latency,
    uint16_t supervision_timeout, uint16_t min_ce_length, uint16_t max_ce_length);

/**
 * @brief Request an update of the connection parameter for a given LE connection
 * @param handle
 * @param conn_interval_min (unit: 1.25ms)
 * @param conn_interval_max (unit: 1.25ms)
 * @param conn_latency
 * @param supervision_timeout (unit: 10ms)
 * @returns 0 if ok
 */
int gap_request_connection_parameter_update(hci_con_handle_t con_handle, uint16_t conn_interval_min,
	uint16_t conn_interval_max, uint16_t conn_latency, uint16_t supervision_timeout);

/**
 * @brief Updates the connection parameters for a given LE connection
 * @param handle
 * @param conn_interval_min (unit: 1.25ms)
 * @param conn_interval_max (unit: 1.25ms)
 * @param conn_latency
 * @param supervision_timeout (unit: 10ms)
 * @returns 0 if ok
 */
int gap_update_connection_parameters(hci_con_handle_t con_handle, uint16_t conn_interval_min,
	uint16_t conn_interval_max, uint16_t conn_latency, uint16_t supervision_timeout);

/**
 * @brief Set accepted connection parameter range
 * @param range
 */
void gap_get_connection_parameter_range(le_connection_parameter_range_t * range);

/**
 * @brief Get accepted connection parameter range
 * @param range
 */
void gap_set_connection_parameter_range(le_connection_parameter_range_t * range);

/**
 * @brief Test if connection parameters are inside in existing rage
 * @param conn_interval_min (unit: 1.25ms)
 * @param conn_interval_max (unit: 1.25ms)
 * @param conn_latency
 * @param supervision_timeout (unit: 10ms)
 * @returns 1 if included
 */
int gap_connection_parameter_range_included(le_connection_parameter_range_t * existing_range, uint16_t le_conn_interval_min, uint16_t le_conn_interval_max, uint16_t le_conn_latency, uint16_t le_supervision_timeout);

/**
 * @brief Set max number of connections in LE Peripheral role (if Bluetooth Controller supports it)
 * @note: default: 1
 * @param max_peripheral_connections
 */
void gap_set_max_number_peripheral_connections(int max_peripheral_connections);

/**
 * @brief Connect to remote LE device
 */
uint8_t gap_connect(bd_addr_t addr, bd_addr_type_t addr_type);

/**
 * @brief Cancel connection process initiated by gap_connect
 */
uint8_t gap_connect_cancel(void);

/**
 * @brief Auto Connection Establishment - Start Connecting to device
 * @param address_typ
 * @param address
 * @returns 0 if ok
 */
int gap_auto_connection_start(bd_addr_type_t address_typ, bd_addr_t address);

/**
 * @brief Auto Connection Establishment - Stop Connecting to device
 * @param address_typ
 * @param address
 * @returns 0 if ok
 */
int gap_auto_connection_stop(bd_addr_type_t address_typ, bd_addr_t address);

/**
 * @brief Auto Connection Establishment - Stop everything
 * @note  Convenience function to stop all active auto connection attempts
 */
void gap_auto_connection_stop_all(void);

/**
 * @brief Set LE PHY
 * @param con_handle
 * @param all_phys 0 = set rx/tx, 1 = set only rx, 2 = set only tx
 * @param tx_phys 1 = 1M, 2 = 2M, 4 = Coded
 * @param rx_phys 1 = 1M, 2 = 2M, 4 = Coded
 * @param phy_options 0 = no preferred coding for Coded, 1 = S=2 coding (500 kbit), 2 = S=8 coding (125 kbit)
 * @returns 0 if ok
 */
uint8_t gap_le_set_phy(hci_con_handle_t con_handle, uint8_t all_phys, uint8_t tx_phys, uint8_t rx_phys, uint8_t phy_options);

/**
 * @brief Get connection interval
 * @return connection interval, otherwise 0 if error 
 */
uint16_t gap_le_connection_interval(hci_con_handle_t connection_handle);

/**
 *
 * @brief Get encryption key size.
 * @param con_handle
 * @return 0 if not encrypted, 7-16 otherwise
 */
int gap_encryption_key_size(hci_con_handle_t con_handle);

/**
 * @brief Get authentication property.
 * @param con_handle
 * @return 1 if bonded with OOB/Passkey (AND MITM protection)
 */
int gap_authenticated(hci_con_handle_t con_handle);

/**
 * @brief Get secure connection property
 * @param con_handle
 * @return 1 if bonded usiung LE Secure Connections
 */
int gap_secure_connection(hci_con_handle_t con_handle);

/**
 * @brief Queries authorization state.
 * @param con_handle
 * @return authorization_state for the current session
 */
authorization_state_t gap_authorization_state(hci_con_handle_t con_handle);

// Classic

/**
 * @brief Override page scan mode. Page scan mode enabled by l2cap when services are registered
 * @note Might be used to reduce power consumption while Bluetooth module stays powered but no (new)
 *       connections are expected
 */
void gap_connectable_control(uint8_t enable);

/**
 * @brief Allows to control if device is discoverable. OFF by default.
 */
void gap_discoverable_control(uint8_t enable);

/**
 * @brief Gets local address.
 */
void gap_local_bd_addr(bd_addr_t address_buffer);

/**
 * @brief Deletes link key for remote device with baseband address.
 * @param addr
 */
void gap_drop_link_key_for_bd_addr(bd_addr_t addr);

/**
 * @brief Delete all stored link keys
 */
void gap_delete_all_link_keys(void);

/** 
 * @brief Store link key for remote device with baseband address
 * @param addr
 * @param link_key
 * @param link_key_type
 */
void gap_store_link_key_for_bd_addr(bd_addr_t addr, link_key_t link_key, link_key_type_t type);

/**
 * @brief Setup Link Key iterator
 * @param it
 * @returns 1 on success
 */
int gap_link_key_iterator_init(btstack_link_key_iterator_t * it);

/**
 * @brief Get next Link Key
 * @param it
 * @brief addr
 * @brief link_key
 * @brief type of link key
 * @returns 1, if valid link key found
 */
int gap_link_key_iterator_get_next(btstack_link_key_iterator_t * it, bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * type);

/**
 * @brief Frees resources allocated by iterator_init
 * @note Must be called after iteration to free resources
 * @param it
 */
void gap_link_key_iterator_done(btstack_link_key_iterator_t * it);

/**
 * @brief Start GAP Classic Inquiry
 * @param duration in 1.28s units
 * @return 0 if ok
 * @events: GAP_EVENT_INQUIRY_RESULT, GAP_EVENT_INQUIRY_COMPLETE
 */
int gap_inquiry_start(uint8_t duration_in_1280ms_units);

/**
 * @brief Stop GAP Classic Inquiry
 * @brief Stop GAP Classic Inquiry
 * @returns 0 if ok
 * @events: GAP_EVENT_INQUIRY_COMPLETE
 */
int gap_inquiry_stop(void);

/**
 * @brief Remote Name Request
 * @param addr
 * @param page_scan_repetition_mode
 * @param clock_offset only used when bit 15 is set - pass 0 if not known
 * @events: HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE
 */
int gap_remote_name_request(bd_addr_t addr, uint8_t page_scan_repetition_mode, uint16_t clock_offset);

/**
 * @brief Read RSSI
 * @param con_handle
 * @events: GAP_EVENT_RSSI_MEASUREMENT
 */
int gap_read_rssi(hci_con_handle_t con_handle);

/**
 * @brief Legacy Pairing Pin Code Response
 * @param addr
 * @param pin
 * @return 0 if ok
 */
int gap_pin_code_response(bd_addr_t addr, const char * pin);

/**
 * @brief Abort Legacy Pairing
 * @param addr
 * @param pin
 * @return 0 if ok
 */
int gap_pin_code_negative(bd_addr_t addr);

/**
 * @brief SSP Passkey Response
 * @param addr
 * @param passkey [0..999999]
 * @return 0 if ok
 */
int gap_ssp_passkey_response(bd_addr_t addr, uint32_t passkey);

/**
 * @brief Abort SSP Passkey Entry/Pairing
 * @param addr
 * @param pin
 * @return 0 if ok
 */
int gap_ssp_passkey_negative(bd_addr_t addr);

/**
 * @brief Accept SSP Numeric Comparison
 * @param addr
 * @param passkey
 * @return 0 if ok
 */
int gap_ssp_confirmation_response(bd_addr_t addr);

/**
 * @brief Abort SSP Numeric Comparison/Pairing
 * @param addr
 * @param pin
 * @return 0 if ok
 */
int gap_ssp_confirmation_negative(bd_addr_t addr);

/**
 * @brief Enter Sniff mode
 * @param con_handle
 * @param sniff_min_interval range: 0x0002 to 0xFFFE; only even values are valid, Time = N * 0.625 ms
 * @param sniff_max_interval range: 0x0002 to 0xFFFE; only even values are valid, Time = N * 0.625 ms
 * @param sniff_attempt Number of Baseband receive slots for sniff attempt.
 * @param sniff_timeout Number of Baseband receive slots for sniff timeout.
 @ @return 0 if ok
 */
uint8_t gap_sniff_mode_enter(hci_con_handle_t con_handle, uint16_t sniff_min_interval, uint16_t sniff_max_interval, uint16_t sniff_attempt, uint16_t sniff_timeout);

/**
 * @brief Exit Sniff mode
 * @param con_handle
 @ @return 0 if ok
 */
uint8_t gap_sniff_mode_exit(hci_con_handle_t con_handle);

// LE

/**
 * @brief Get own addr type and address used for LE
 */
void gap_le_get_own_address(uint8_t * addr_type, bd_addr_t addr);


/**
 * @brief Get state of connection re-encryptiong for bonded devices when in central role
 * @note used by gatt_client and att_server to wait for re-encryption
 * @param con_handle
 * @return 1 if security setup is active
 */
int gap_reconnect_security_setup_active(hci_con_handle_t con_handle);


/* API_END*/

#if defined __cplusplus
}
#endif

#endif // GAP_H
