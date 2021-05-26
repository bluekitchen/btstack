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

#define BTSTACK_FILE__ "hci_cmd.c"

/*
 *  hci_cmd.c
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#include "btstack_config.h"

#include "hci.h"
#include "hci_cmd.h"
#include "btstack_debug.h"

#include <string.h>

#ifdef ENABLE_SDP
#include "classic/sdp_util.h"
#endif

// calculate combined ogf/ocf value
#define OPCODE(ogf, ocf) ((ocf) | ((ogf) << 10))

/**
 * construct HCI Command based on template
 *
 * Format:
 *   1,2,3,4: one to four byte value
 *   H: HCI connection handle
 *   B: Bluetooth Baseband Address (BD_ADDR)
 *   D: 8 byte data block
 *   E: Extended Inquiry Result
 *   N: Name up to 248 chars, \0 terminated
 *   P: 16 byte data block. Pairing code, Simple Pairing Hash and Randomizer
 *   A: 31 bytes advertising data
 *   S: Service Record (Data Element Sequence)
 *   Q: 32 byte data block, e.g. for X and Y coordinates of P-256 public key
 */
uint16_t hci_cmd_create_from_template(uint8_t *hci_cmd_buffer, const hci_cmd_t *cmd, va_list argptr){
    
    hci_cmd_buffer[0] = cmd->opcode & 0xffu;
    hci_cmd_buffer[1] = cmd->opcode >> 8;
    int pos = 3;
    
    const char *format = cmd->format;
    uint16_t word;
    uint32_t longword;
    uint8_t * ptr;
    while (*format) {
        switch(*format) {
            case '1': //  8 bit value
            case '2': // 16 bit value
            case 'H': // hci_handle
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                hci_cmd_buffer[pos++] = word & 0xffu;
                if (*format == '2') {
                    hci_cmd_buffer[pos++] = word >> 8;
                } else if (*format == 'H') {
                    // TODO implement opaque client connection handles
                    //      pass module handle for now
                    hci_cmd_buffer[pos++] = word >> 8;
                } 
                break;
            case '3':
            case '4':
                longword = va_arg(argptr, uint32_t);
                // longword = va_arg(argptr, int);
                hci_cmd_buffer[pos++] = longword;
                hci_cmd_buffer[pos++] = longword >> 8;
                hci_cmd_buffer[pos++] = longword >> 16;
                if (*format == '4'){
                    hci_cmd_buffer[pos++] = longword >> 24;
                }
                break;
            case 'B': // bt-addr
                ptr = va_arg(argptr, uint8_t *);
                hci_cmd_buffer[pos++] = ptr[5];
                hci_cmd_buffer[pos++] = ptr[4];
                hci_cmd_buffer[pos++] = ptr[3];
                hci_cmd_buffer[pos++] = ptr[2];
                hci_cmd_buffer[pos++] = ptr[1];
                hci_cmd_buffer[pos++] = ptr[0];
                break;
            case 'D': // 8 byte data block
                ptr = va_arg(argptr, uint8_t *);
                (void)memcpy(&hci_cmd_buffer[pos], ptr, 8);
                pos += 8;
                break;
            case 'E': // Extended Inquiry Information 240 octets
                ptr = va_arg(argptr, uint8_t *);
                (void)memcpy(&hci_cmd_buffer[pos], ptr, 240);
                pos += 240;
                break;
            case 'N': { // UTF-8 string, null terminated
                ptr = va_arg(argptr, uint8_t *);
                uint16_t len = strlen((const char*) ptr);
                if (len > 248u) {
                    len = 248;
                }
                (void)memcpy(&hci_cmd_buffer[pos], ptr, len);
                if (len < 248u) {
                    // fill remaining space with zeroes
                    memset(&hci_cmd_buffer[pos+len], 0u, 248u-len);
                }
                pos += 248;
                break;
            }
            case 'P': // 16 byte PIN code or link key in little endian
                ptr = va_arg(argptr, uint8_t *);
                (void)memcpy(&hci_cmd_buffer[pos], ptr, 16);
                pos += 16;
                break;
#ifdef ENABLE_BLE
            case 'A': // 31 bytes advertising data
                ptr = va_arg(argptr, uint8_t *);
                (void)memcpy(&hci_cmd_buffer[pos], ptr, 31);
                pos += 31;
                break;
#endif
#ifdef ENABLE_SDP
            case 'S': { // Service Record (Data Element Sequence)
                ptr = va_arg(argptr, uint8_t *);
                uint16_t len = de_get_len(ptr);
                (void)memcpy(&hci_cmd_buffer[pos], ptr, len);
                pos += len;
                break;
            }
#endif
#ifdef ENABLE_LE_SECURE_CONNECTIONS
            case 'Q':
                ptr = va_arg(argptr, uint8_t *);
                reverse_bytes(ptr, &hci_cmd_buffer[pos], 32);
                pos += 32;
                break;
#endif
            case 'K':   // 16 byte OOB Data or Link Key in big endian
                ptr = va_arg(argptr, uint8_t *);
                reverse_bytes(ptr, &hci_cmd_buffer[pos], 16);
                pos += 16;
                break;
            default:
                break;
        }
        format++;
    };
    hci_cmd_buffer[2] = pos - 3;
    return pos;
}

/**
 *  Link Control Commands 
 */

/**
 * @param lap
 * @param inquiry_length
 * @param num_responses
 */
const hci_cmd_t hci_inquiry = {
    HCI_OPCODE_HCI_INQUIRY, "311"
};

/**
 */
const hci_cmd_t hci_inquiry_cancel = {
    HCI_OPCODE_HCI_INQUIRY_CANCEL, ""
};

/**
 * @param bd_addr
 * @param packet_type
 * @param page_scan_repetition_mode
 * @param reserved
 * @param clock_offset
 * @param allow_role_switch
 */
const hci_cmd_t hci_create_connection = {
    HCI_OPCODE_HCI_CREATE_CONNECTION, "B21121"
};

/**
 * @param handle 
 * @param reason (0x05, 0x13-0x15, 0x1a, 0x29, see Errors Codes in BT Spec Part D)
 */
const hci_cmd_t hci_disconnect = {
    HCI_OPCODE_HCI_DISCONNECT, "H1"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_create_connection_cancel = {
    HCI_OPCODE_HCI_CREATE_CONNECTION_CANCEL, "B"
};

/**
 * @param bd_addr
 * @param role (become master, stay slave)
 */
const hci_cmd_t hci_accept_connection_request = {
    HCI_OPCODE_HCI_ACCEPT_CONNECTION_REQUEST, "B1"
};

/**
 * @param bd_addr
 * @param reason (e.g. CONNECTION REJECTED DUE TO LIMITED RESOURCES (0x0d))
 */
const hci_cmd_t hci_reject_connection_request = {
    HCI_OPCODE_HCI_REJECT_CONNECTION_REQUEST, "B1"
};

/**
 * @param bd_addr
 * @param link_key
 */
const hci_cmd_t hci_link_key_request_reply = {
    HCI_OPCODE_HCI_LINK_KEY_REQUEST_REPLY, "BP"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_link_key_request_negative_reply = {
    HCI_OPCODE_HCI_LINK_KEY_REQUEST_NEGATIVE_REPLY, "B"
};

/**
 * @param bd_addr
 * @param pin_length
 * @param pin (c-string)
 */
const hci_cmd_t hci_pin_code_request_reply = {
    HCI_OPCODE_HCI_PIN_CODE_REQUEST_REPLY, "B1P"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_pin_code_request_negative_reply = {
    HCI_OPCODE_HCI_PIN_CODE_REQUEST_NEGATIVE_REPLY, "B"
};

/**
 * @param handle
 * @param packet_type
 */
const hci_cmd_t hci_change_connection_packet_type = {
    HCI_OPCODE_HCI_CHANGE_CONNECTION_PACKET_TYPE, "H2"
};

/**
 * @param handle
 */
const hci_cmd_t hci_authentication_requested = {
    HCI_OPCODE_HCI_AUTHENTICATION_REQUESTED, "H"
};

/**
 * @param handle
 * @param encryption_enable
 */
const hci_cmd_t hci_set_connection_encryption = {
    HCI_OPCODE_HCI_SET_CONNECTION_ENCRYPTION, "H1"
};

/**
 * @param handle
 */
const hci_cmd_t hci_change_connection_link_key = {
    HCI_OPCODE_HCI_CHANGE_CONNECTION_LINK_KEY, "H"
};

/**
 * @param bd_addr
 * @param page_scan_repetition_mode
 * @param reserved
 * @param clock_offset
 */
const hci_cmd_t hci_remote_name_request = {
    HCI_OPCODE_HCI_REMOTE_NAME_REQUEST, "B112"
};


/**
 * @param bd_addr
 */
const hci_cmd_t hci_remote_name_request_cancel = {
    HCI_OPCODE_HCI_REMOTE_NAME_REQUEST_CANCEL, "B"
};

 /**
 * @param handle
 */
const hci_cmd_t hci_read_remote_supported_features_command = {
    HCI_OPCODE_HCI_READ_REMOTE_SUPPORTED_FEATURES_COMMAND, "H"
};

/**
* @param handle
*/
const hci_cmd_t hci_read_remote_extended_features_command = {
    HCI_OPCODE_HCI_READ_REMOTE_EXTENDED_FEATURES_COMMAND, "H1"
};

/**
 * @param handle
 */
const hci_cmd_t hci_read_remote_version_information = {
    HCI_OPCODE_HCI_READ_REMOTE_VERSION_INFORMATION, "H"
};

/** 
 * @param handle
 * @param transmit_bandwidth 8000(64kbps)
 * @param receive_bandwidth  8000(64kbps)
 * @param max_latency        >= 7ms for eSCO, 0xFFFF do not care
 * @param voice_settings     e.g. CVSD, Input Coding: Linear, Input Data Format: 2’s complement, data 16bit: 00011000000 == 0x60
 * @param retransmission_effort  e.g. 0xFF do not care
 * @param packet_type        at least EV3 for eSCO
 */
const hci_cmd_t hci_setup_synchronous_connection = {
    HCI_OPCODE_HCI_SETUP_SYNCHRONOUS_CONNECTION, "H442212"
};

/**
 * @param bd_addr
 * @param transmit_bandwidth
 * @param receive_bandwidth
 * @param max_latency
 * @param voice_settings
 * @param retransmission_effort
 * @param packet_type
 */
const hci_cmd_t hci_accept_synchronous_connection = {
    HCI_OPCODE_HCI_ACCEPT_SYNCHRONOUS_CONNECTION, "B442212"
};

/**
 * @param bd_addr
 * @param IO_capability
 * @param OOB_data_present
 * @param authentication_requirements
 */
const hci_cmd_t hci_io_capability_request_reply = {
    HCI_OPCODE_HCI_IO_CAPABILITY_REQUEST_REPLY, "B111"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_user_confirmation_request_reply = {
    HCI_OPCODE_HCI_USER_CONFIRMATION_REQUEST_REPLY, "B"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_user_confirmation_request_negative_reply = {
    HCI_OPCODE_HCI_USER_CONFIRMATION_REQUEST_NEGATIVE_REPLY, "B"
};

/**
 * @param bd_addr
 * @param numeric_value
 */
const hci_cmd_t hci_user_passkey_request_reply = {
    HCI_OPCODE_HCI_USER_PASSKEY_REQUEST_REPLY, "B4"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_user_passkey_request_negative_reply = {
    HCI_OPCODE_HCI_USER_PASSKEY_REQUEST_NEGATIVE_REPLY, "B"
};

/**
 * @param bd_addr
 * @param c Simple Pairing Hash C
 * @param r Simple Pairing Randomizer R
 */
const hci_cmd_t hci_remote_oob_data_request_reply = {
    HCI_OPCODE_HCI_REMOTE_OOB_DATA_REQUEST_REPLY, "BKK"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_remote_oob_data_request_negative_reply = {
    HCI_OPCODE_HCI_REMOTE_OOB_DATA_REQUEST_NEGATIVE_REPLY, "B"
};

/**
 * @param bd_addr
 * @param reason (Part D, Error codes)
 */
const hci_cmd_t hci_io_capability_request_negative_reply = {
    HCI_OPCODE_HCI_IO_CAPABILITY_REQUEST_NEGATIVE_REPLY, "B1"
};

/**
 * @param handle
 * @param transmit_bandwidth
 * @param receive_bandwidth
 * @param transmit_coding_format_type
 * @param transmit_coding_format_company
 * @param transmit_coding_format_codec
 * @param receive_coding_format_type
 * @param receive_coding_format_company
 * @param receive_coding_format_codec
 * @param transmit_coding_frame_size
 * @param receive_coding_frame_size
 * @param input_bandwidth
 * @param output_bandwidth
 * @param input_coding_format_type
 * @param input_coding_format_company
 * @param input_coding_format_codec
 * @param output_coding_format_type
 * @param output_coding_format_company
 * @param output_coding_format_codec
 * @param input_coded_data_size
 * @param outupt_coded_data_size
 * @param input_pcm_data_format
 * @param output_pcm_data_format
 * @param input_pcm_sample_payload_msb_position
 * @param output_pcm_sample_payload_msb_position
 * @param input_data_path
 * @param output_data_path
 * @param input_transport_unit_size
 * @param output_transport_unit_size
 * @param max_latency
 * @param packet_type
 * @param retransmission_effort
 */
const hci_cmd_t hci_enhanced_setup_synchronous_connection = {
    HCI_OPCODE_HCI_ENHANCED_SETUP_SYNCHRONOUS_CONNECTION, "H4412212222441221222211111111221"
};

/**
 * @param bd_addr
 * @param transmit_bandwidth
 * @param receive_bandwidth
 * @param transmit_coding_format_type
 * @param transmit_coding_format_company
 * @param transmit_coding_format_codec
 * @param receive_coding_format_type
 * @param receive_coding_format_company
 * @param receive_coding_format_codec
 * @param transmit_coding_frame_size
 * @param receive_coding_frame_size
 * @param input_bandwidth
 * @param output_bandwidth
 * @param input_coding_format_type
 * @param input_coding_format_company
 * @param input_coding_format_codec
 * @param output_coding_format_type
 * @param output_coding_format_company
 * @param output_coding_format_codec
 * @param input_coded_data_size
 * @param outupt_coded_data_size
 * @param input_pcm_data_format
 * @param output_pcm_data_format
 * @param input_pcm_sample_payload_msb_position
 * @param output_pcm_sample_payload_msb_position
 * @param input_data_path
 * @param output_data_path
 * @param input_transport_unit_size
 * @param output_transport_unit_size
 * @param max_latency
 * @param packet_type
 * @param retransmission_effort
 */
const hci_cmd_t hci_enhanced_accept_synchronous_connection = {
    HCI_OPCODE_HCI_ENHANCED_ACCEPT_SYNCHRONOUS_CONNECTION, "B4412212222441221222211111111221"
};

/**
 * @param bd_addr
 * @param c_192 Simple Pairing Hash C derived from P-192 public key
 * @param r_192 Simple Pairing Randomizer derived from P-192 public key
 * @param c_256 Simple Pairing Hash C derived from P-256 public key
 * @param r_256 Simple Pairing Randomizer derived from P-256 public key
 */
const hci_cmd_t hci_remote_oob_extended_data_request_reply = {
        HCI_OPCODE_HCI_REMOTE_OOB_EXTENDED_DATA_REQUEST_REPLY, "BKKKK"
};

/**
 *  Link Policy Commands 
 */

/**
 * @param handle
 * @param sniff_max_interval
 * @param sniff_min_interval
 * @param sniff_attempt
 * @param sniff_timeout
 */
const hci_cmd_t hci_sniff_mode = {
    HCI_OPCODE_HCI_SNIFF_MODE, "H2222"
};

/**
 * @param handle
 */
const hci_cmd_t hci_exit_sniff_mode = {
    HCI_OPCODE_HCI_EXIT_SNIFF_MODE, "H"
};

/**
 * @param handle
 * @param flags
 * @param service_type
 * @param token_rate (bytes/s)
 * @param peak_bandwith (bytes/s)
 * @param latency (us)
 * @param delay_variation (us)
 */
const hci_cmd_t hci_qos_setup = {
    HCI_OPCODE_HCI_QOS_SETUP, "H114444"
};

/**
 * @param handle
 */
const hci_cmd_t hci_role_discovery = {
    HCI_OPCODE_HCI_ROLE_DISCOVERY, "H"
};

/**
 * @param bd_addr
 * @param role (0=master,1=slave)
 */
const hci_cmd_t hci_switch_role_command= {
    HCI_OPCODE_HCI_SWITCH_ROLE_COMMAND, "B1"
};

/**
 * @param handle
 */
const hci_cmd_t hci_read_link_policy_settings = {
    HCI_OPCODE_HCI_READ_LINK_POLICY_SETTINGS, "H"
};

/**
 * @param handle
 * @param settings
 */
const hci_cmd_t hci_write_link_policy_settings = {
    HCI_OPCODE_HCI_WRITE_LINK_POLICY_SETTINGS, "H2"
};

/**
 * @param handle
 * @param max_latency
 * @param min_remote_timeout
 * @param min_local_timeout
 */
const hci_cmd_t hci_sniff_subrating = {
        HCI_OPCODE_HCI_SNIFF_SUBRATING, "H222"
};

/**
 * @param policy
 */
const hci_cmd_t hci_write_default_link_policy_setting = {
    HCI_OPCODE_HCI_WRITE_DEFAULT_LINK_POLICY_SETTING, "2"
};

/**
 * @param handle
 * @param unused
 * @param flow_direction
 * @param service_type
 * @param token_rate
 * @param token_bucket_size
 * @param peak_bandwidth
 * @param access_latency
 */
const hci_cmd_t hci_flow_specification = {
        HCI_OPCODE_HCI_FLOW_SPECIFICATION, "H1114444"
};


/**
 *  Controller & Baseband Commands 
 */


/**
 * @param event_mask_lover_octets
 * @param event_mask_higher_octets
 */
const hci_cmd_t hci_set_event_mask = {
    HCI_OPCODE_HCI_SET_EVENT_MASK, "44"
};

/**
 */
const hci_cmd_t hci_reset = {
    HCI_OPCODE_HCI_RESET, ""
};

/**
 * @param handle
 */
const hci_cmd_t hci_flush = {
    HCI_OPCODE_HCI_FLUSH, "H"
};

/**
 * @param handle
 */
const hci_cmd_t hci_read_pin_type = {
    HCI_OPCODE_HCI_READ_PIN_TYPE, ""
};

/**
 * @param handle
 */
const hci_cmd_t hci_write_pin_type = {
    HCI_OPCODE_HCI_WRITE_PIN_TYPE, "1"
};

/**
 * @param bd_addr
 * @param delete_all_flags
 */
const hci_cmd_t hci_delete_stored_link_key = {
    HCI_OPCODE_HCI_DELETE_STORED_LINK_KEY, "B1"
};

#ifdef ENABLE_CLASSIC
/**
 * @param local_name (UTF-8, Null Terminated, max 248 octets)
 */
const hci_cmd_t hci_write_local_name = {
    HCI_OPCODE_HCI_WRITE_LOCAL_NAME, "N"
};
#endif

/**
 */
const hci_cmd_t hci_read_local_name = {
    HCI_OPCODE_HCI_READ_LOCAL_NAME, ""
};

/**
 */ 
const hci_cmd_t hci_read_page_timeout = {
    HCI_OPCODE_HCI_READ_PAGE_TIMEOUT, ""
};

/**
 * @param page_timeout (* 0.625 ms)
 */
const hci_cmd_t hci_write_page_timeout = {
    HCI_OPCODE_HCI_WRITE_PAGE_TIMEOUT, "2"
};

/**
 * @param scan_enable (no, inq, page, inq+page)
 */
const hci_cmd_t hci_write_scan_enable = {
    HCI_OPCODE_HCI_WRITE_SCAN_ENABLE, "1"
};

/**
 */ 
const hci_cmd_t hci_read_page_scan_activity = {
    HCI_OPCODE_HCI_READ_PAGE_SCAN_ACTIVITY, ""
};

/**
 * @param page_scan_interval (* 0.625 ms)
 * @param page_scan_window (* 0.625 ms, must be <= page_scan_interval)
 */ 
const hci_cmd_t hci_write_page_scan_activity = {
    HCI_OPCODE_HCI_WRITE_PAGE_SCAN_ACTIVITY, "22"
};

/**
 */ 
const hci_cmd_t hci_read_inquiry_scan_activity = {
    HCI_OPCODE_HCI_READ_INQUIRY_SCAN_ACTIVITY, ""
};

/**
 * @param inquiry_scan_interval (* 0.625 ms)
 * @param inquiry_scan_window (* 0.625 ms, must be <= inquiry_scan_interval)
 */ 
const hci_cmd_t hci_write_inquiry_scan_activity = {
    HCI_OPCODE_HCI_WRITE_INQUIRY_SCAN_ACTIVITY, "22"
};

/**
 * @param authentication_enable
 */
const hci_cmd_t hci_write_authentication_enable = {
    HCI_OPCODE_HCI_WRITE_AUTHENTICATION_ENABLE, "1"
};

/**
 * @param class_of_device
 */
const hci_cmd_t hci_write_class_of_device = {
    HCI_OPCODE_HCI_WRITE_CLASS_OF_DEVICE, "3"
};

/** 
 */
const hci_cmd_t hci_read_num_broadcast_retransmissions = {
    HCI_OPCODE_HCI_READ_NUM_BROADCAST_RETRANSMISSIONS, ""
};

/**
 * @param num_broadcast_retransmissions (e.g. 0 for a single broadcast)
 */
const hci_cmd_t hci_write_num_broadcast_retransmissions = {
    HCI_OPCODE_HCI_WRITE_NUM_BROADCAST_RETRANSMISSIONS, "1"
};

/**
 * @param connection_handle
 * @param type 0 = current transmit level, 1 = max transmit level
 */
const hci_cmd_t hci_read_transmit_power_level = {
    HCI_OPCODE_HCI_READ_TRANSMIT_POWER_LEVEL, "11"
};

/**
 * @param synchronous_flow_control_enable - if yes, num completed packet everts are sent for SCO packets
 */
const hci_cmd_t hci_write_synchronous_flow_control_enable = {
    HCI_OPCODE_HCI_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE, "1"
};

#ifdef ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL

/**
 * @param flow_control_enable - 0: off, 1: ACL only, 2: SCO only, 3: ACL + SCO
 */
const hci_cmd_t hci_set_controller_to_host_flow_control = {
    HCI_OPCODE_HCI_SET_CONTROLLER_TO_HOST_FLOW_CONTROL, "1"
};

/**
 * @param host_acl_data_packet_length
 * @param host_synchronous_data_packet_length
 * @param host_total_num_acl_data_packets
 * @param host_total_num_synchronous_data_packets
 */
const hci_cmd_t hci_host_buffer_size = {
    HCI_OPCODE_HCI_HOST_BUFFER_SIZE, "2122"
};


#if 0
//
// command sent manually sent by hci_host_num_completed_packets
//
/**
 * @note only single handle supported by BTstack command generator
 * @param number_of_handles must be 1
 * @param connection_handle
 * @param host_num_of_completed_packets for the given connection handle
 */
const hci_cmd_t hci_host_number_of_completed_packets = {
    HCI_OPCODE_HCI_HOST_NUMBER_OF_COMPLETED_PACKETS, "1H2"
};
#endif

#endif

/**
 * @param handle
 */
const hci_cmd_t hci_read_link_supervision_timeout = {
    HCI_OPCODE_HCI_READ_LINK_SUPERVISION_TIMEOUT, "H"
};

/**
 * @param handle
 * @param timeout (0x0001 - 0xFFFF Time -> Range: 0.625ms - 40.9 sec)
 */
const hci_cmd_t hci_write_link_supervision_timeout = {
    HCI_OPCODE_HCI_WRITE_LINK_SUPERVISION_TIMEOUT, "H2"
};

/**
 * @param num_current_iac must be 2
 * @param iac_lap1
 * @param iac_lap2
 */
const hci_cmd_t hci_write_current_iac_lap_two_iacs = {
    HCI_OPCODE_HCI_WRITE_CURRENT_IAC_LAP_TWO_IACS, "133"
};

/**
 * @param inquiry_scan_type (0x00 = standard, 0x01 = interlaced)
 */
const hci_cmd_t hci_write_inquiry_scan_type = {
    HCI_OPCODE_HCI_WRITE_INQUIRY_SCAN_TYPE,  "1"
};

/**
 * @param inquiry_mode (0x00 = standard, 0x01 = with RSSI, 0x02 = extended)
 */
const hci_cmd_t hci_write_inquiry_mode = {
    HCI_OPCODE_HCI_WRITE_INQUIRY_MODE, "1"
};

/**
 * @param page_scan_type (0x00 = standard, 0x01 = interlaced)
 */
const hci_cmd_t hci_write_page_scan_type = {
    HCI_OPCODE_HCI_WRITE_PAGE_SCAN_TYPE, "1"
};

/**
 * @param fec_required
 * @param exstended_inquiry_response
 */
const hci_cmd_t hci_write_extended_inquiry_response = {
    HCI_OPCODE_HCI_WRITE_EXTENDED_INQUIRY_RESPONSE, "1E"
};

/**
 * @param mode (0 = off, 1 = on)
 */
const hci_cmd_t hci_write_simple_pairing_mode = {
    HCI_OPCODE_HCI_WRITE_SIMPLE_PAIRING_MODE, "1"
};

/**
 */
const hci_cmd_t hci_read_local_oob_data = {
    HCI_OPCODE_HCI_READ_LOCAL_OOB_DATA, ""
    // return status, C, R
};

/**
 * @param mode (0 = off, 1 = on)
 */
const hci_cmd_t hci_write_default_erroneous_data_reporting = {
    HCI_OPCODE_HCI_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING, "1"
};

/**
 */
const hci_cmd_t hci_read_le_host_supported = {
    HCI_OPCODE_HCI_READ_LE_HOST_SUPPORTED, ""
    // return: status, le supported host, simultaneous le host
};

/**
 * @param le_supported_host
 * @param simultaneous_le_host
 */
const hci_cmd_t hci_write_le_host_supported = {
    HCI_OPCODE_HCI_WRITE_LE_HOST_SUPPORTED, "11"
    // return: status
};

/**
 * @param secure_connections_host_support
 */
const hci_cmd_t hci_write_secure_connections_host_support = {
    HCI_OPCODE_HCI_WRITE_SECURE_CONNECTIONS_HOST_SUPPORT, "1"
    // return: status
};

/**
 */
const hci_cmd_t hci_read_local_extended_oob_data = {
    HCI_OPCODE_HCI_READ_LOCAL_EXTENDED_OOB_DATA, ""
    // return status, C_192, R_192, R_256, C_256
};


/**
 * Testing Commands
 */


/**
 */
const hci_cmd_t hci_read_loopback_mode = {
    HCI_OPCODE_HCI_READ_LOOPBACK_MODE, ""
    // return: status, loopback mode (0 = off, 1 = local loopback, 2 = remote loopback)
};

/**
 * @param loopback_mode
 */
const hci_cmd_t hci_write_loopback_mode = {
    HCI_OPCODE_HCI_WRITE_LOOPBACK_MODE, "1"
    // return: status
};

/**
 */
const hci_cmd_t hci_enable_device_under_test_mode = {
    HCI_OPCODE_HCI_ENABLE_DEVICE_UNDER_TEST_MODE, ""
    // return: status
};

/**
 * @param simple_pairing_debug_mode
 */
const hci_cmd_t hci_write_simple_pairing_debug_mode = {
    HCI_OPCODE_HCI_WRITE_SIMPLE_PAIRING_DEBUG_MODE, "1"
    // return: status
};

/**
 * @param handle
 * @param dm1_acl_u_mode
 * @param esco_loopback_mode
 */
const hci_cmd_t hci_write_secure_connections_test_mode = {
    HCI_OPCODE_HCI_WRITE_SECURE_CONNECTIONS_TEST_MODE, "H11"
    // return: status
};


/**
 * Informational Parameters
 */

const hci_cmd_t hci_read_local_version_information = {
    HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION, ""
};
const hci_cmd_t hci_read_local_supported_commands = {
    HCI_OPCODE_HCI_READ_LOCAL_SUPPORTED_COMMANDS, ""
};
const hci_cmd_t hci_read_local_supported_features = {
    HCI_OPCODE_HCI_READ_LOCAL_SUPPORTED_FEATURES, ""
};
const hci_cmd_t hci_read_buffer_size = {
    HCI_OPCODE_HCI_READ_BUFFER_SIZE, ""
};
const hci_cmd_t hci_read_bd_addr = {
    HCI_OPCODE_HCI_READ_BD_ADDR, ""
};

/**
 * Status Paramters
 */

/**
 * @param handle
 */
const hci_cmd_t hci_read_rssi = {
    HCI_OPCODE_HCI_READ_RSSI, "H"
};

/**
 * @param handle
 */
const hci_cmd_t hci_read_encryption_key_size = {
    HCI_OPCODE_HCI_READ_ENCRYPTION_KEY_SIZE, "H"
};


#ifdef ENABLE_BLE
/**
 * Low Energy Commands
 */

/**
 * @param event_mask_lower_octets
 * @param event_mask_higher_octets
 */
const hci_cmd_t hci_le_set_event_mask = {
    HCI_OPCODE_HCI_LE_SET_EVENT_MASK, "44"
    // return: status
};

const hci_cmd_t hci_le_read_buffer_size = {
    HCI_OPCODE_HCI_LE_READ_BUFFER_SIZE, ""
    // return: status, le acl data packet len (16), total num le acl data packets(8)
};
const hci_cmd_t hci_le_read_supported_features = {
    HCI_OPCODE_HCI_LE_READ_SUPPORTED_FEATURES, ""
    // return: LE_Features See [Vol 6] Part B, Section 4.6
};

/**
 * @param random_bd_addr
 */
const hci_cmd_t hci_le_set_random_address = {
    HCI_OPCODE_HCI_LE_SET_RANDOM_ADDRESS, "B"
    // return: status
};

/**
 * @param advertising_interval_min ([0x0020,0x4000], default: 0x0800, unit: 0.625 msec)
 * @param advertising_interval_max ([0x0020,0x4000], default: 0x0800, unit: 0.625 msec)
 * @param advertising_type (enum from 0: ADV_IND, ADC_DIRECT_IND, ADV_SCAN_IND, ADV_NONCONN_IND)
 * @param own_address_type (enum from 0: public device address, random device address)
 * @param direct_address_type ()
 * @param direct_address (public or random address of device to be connecteed)
 * @param advertising_channel_map (flags: chan_37(1), chan_38(2), chan_39(4))
 * @param advertising_filter_policy (enum from 0: scan any conn any, scan whitelist, con any, scan any conn whitelist, scan whitelist, con whitelist)
 */
const hci_cmd_t hci_le_set_advertising_parameters = {
    HCI_OPCODE_HCI_LE_SET_ADVERTISING_PARAMETERS, "22111B11"
    // return: status
};

const hci_cmd_t hci_le_read_advertising_channel_tx_power = {
    HCI_OPCODE_HCI_LE_READ_ADVERTISING_CHANNEL_TX_POWER, ""
    // return: status, level [-20,10] signed int (8), units dBm
};

/**
 * @param advertising_data_length
 * @param advertising_data (31 bytes)
 */
const hci_cmd_t hci_le_set_advertising_data= {
    HCI_OPCODE_HCI_LE_SET_ADVERTISING_DATA, "1A"
    // return: status
};

/**
 * @param scan_response_data_length
 * @param scan_response_data (31 bytes)
 */
const hci_cmd_t hci_le_set_scan_response_data= {
    HCI_OPCODE_HCI_LE_SET_SCAN_RESPONSE_DATA, "1A"
    // return: status
};

/**
 * @param advertise_enable (off: 0, on: 1)
 */
const hci_cmd_t hci_le_set_advertise_enable = {
    HCI_OPCODE_HCI_LE_SET_ADVERTISE_ENABLE, "1"
    // return: status
};

/**
 * @param le_scan_type (passive (0), active (1))
 * @param le_scan_interval ([0x0004,0x4000], unit: 0.625 msec)
 * @param le_scan_window   ([0x0004,0x4000], unit: 0.625 msec)
 * @param own_address_type (public (0), random (1))
 * @param scanning_filter_policy (any (0), only whitelist (1))
 */
const hci_cmd_t hci_le_set_scan_parameters = {
    HCI_OPCODE_HCI_LE_SET_SCAN_PARAMETERS, "12211"
    // return: status
};

/**
 * @param le_scan_enable  (disabled (0), enabled (1))
 * @param filter_duplices (disabled (0), enabled (1))
 */
const hci_cmd_t hci_le_set_scan_enable = {
    HCI_OPCODE_HCI_LE_SET_SCAN_ENABLE, "11"
    // return: status
};

/**
 * @param le_scan_interval ([0x0004, 0x4000], unit: 0.625 msec)
 * @param le_scan_window ([0x0004, 0x4000], unit: 0.625 msec)
 * @param initiator_filter_policy (peer address type + peer address (0), whitelist (1))
 * @param peer_address_type (public (0), random (1))
 * @param peer_address
 * @param own_address_type (public (0), random (1))
 * @param conn_interval_min ([0x0006, 0x0c80], unit: 1.25 msec)
 * @param conn_interval_max ([0x0006, 0x0c80], unit: 1.25 msec)
 * @param conn_latency (number of connection events [0x0000, 0x01f4])
 * @param supervision_timeout ([0x000a, 0x0c80], unit: 10 msec)
 * @param minimum_CE_length ([0x0000, 0xffff], unit: 0.625 msec)
 * @param maximum_CE_length ([0x0000, 0xffff], unit: 0.625 msec)
 */
const hci_cmd_t hci_le_create_connection= {
    HCI_OPCODE_HCI_LE_CREATE_CONNECTION, "2211B1222222"
    // return: none -> le create connection complete event
};

const hci_cmd_t hci_le_create_connection_cancel = {
    HCI_OPCODE_HCI_LE_CREATE_CONNECTION_CANCEL, ""
    // return: status
};

const hci_cmd_t hci_le_read_white_list_size = {
    HCI_OPCODE_HCI_LE_READ_WHITE_LIST_SIZE, ""
    // return: status, number of entries in controller whitelist
};

const hci_cmd_t hci_le_clear_white_list = {
    HCI_OPCODE_HCI_LE_CLEAR_WHITE_LIST, ""
    // return: status
};

/**
 * @param address_type (public (0), random (1))
 * @param bd_addr
 */
const hci_cmd_t hci_le_add_device_to_white_list = {
    HCI_OPCODE_HCI_LE_ADD_DEVICE_TO_WHITE_LIST, "1B"
    // return: status
};

/**
 * @param address_type (public (0), random (1))
 * @param bd_addr
 */
const hci_cmd_t hci_le_remove_device_from_white_list = {
    HCI_OPCODE_HCI_LE_REMOVE_DEVICE_FROM_WHITE_LIST, "1B"
    // return: status
};

/**
 * @param conn_handle
 * @param conn_interval_min ([0x0006,0x0c80], unit: 1.25 msec)
 * @param conn_interval_max ([0x0006,0x0c80], unit: 1.25 msec)
 * @param conn_latency ([0x0000,0x03e8], number of connection events)
 * @param supervision_timeout ([0x000a,0x0c80], unit: 10 msec)
 * @param minimum_CE_length ([0x0000,0xffff], unit: 0.625 msec)
 * @param maximum_CE_length ([0x0000,0xffff], unit: 0.625 msec)
 */
const hci_cmd_t hci_le_connection_update = {
    HCI_OPCODE_HCI_LE_CONNECTION_UPDATE, "H222222"
    // return: none -> le connection update complete event
};

/**
 * @param channel_map_lower_32bits 
 * @param channel_map_higher_5bits
 */
const hci_cmd_t hci_le_set_host_channel_classification = {
    HCI_OPCODE_HCI_LE_SET_HOST_CHANNEL_CLASSIFICATION, "41"
    // return: status
};

/**
 * @param conn_handle
 */
const hci_cmd_t hci_le_read_channel_map = {
    HCI_OPCODE_HCI_LE_READ_CHANNEL_MAP, "H"
    // return: status, connection handle, channel map (5 bytes, 37 used)
};

/**
 * @param conn_handle
 */
const hci_cmd_t hci_le_read_remote_used_features = {
    HCI_OPCODE_HCI_LE_READ_REMOTE_USED_FEATURES, "H"
    // return: none -> le read remote used features complete event
};

/**
 * @param key ((128) for AES-128)
 * @param plain_text (128) 
 */
const hci_cmd_t hci_le_encrypt = {
    HCI_OPCODE_HCI_LE_ENCRYPT, "PP"
    // return: status, encrypted data (128)
};

const hci_cmd_t hci_le_rand = {
    HCI_OPCODE_HCI_LE_RAND, ""
    // return: status, random number (64)
};

/**
 * @param conn_handle
 * @param random_number_lower_32bits
 * @param random_number_higher_32bits
 * @param encryption_diversifier (16)
 * @param long_term_key (128)
 */
const hci_cmd_t hci_le_start_encryption = {
    HCI_OPCODE_HCI_LE_START_ENCRYPTION, "H442P"
    // return: none -> encryption changed or encryption key refresh complete event
};

/**
 * @param connection_handle
 * @param long_term_key (128)
 */
const hci_cmd_t hci_le_long_term_key_request_reply = {
    HCI_OPCODE_HCI_LE_LONG_TERM_KEY_REQUEST_REPLY, "HP"
    // return: status, connection handle
};

/**
 * @param conn_handle
 */
const hci_cmd_t hci_le_long_term_key_negative_reply = {
    HCI_OPCODE_HCI_LE_LONG_TERM_KEY_NEGATIVE_REPLY, "H"
    // return: status, connection handle
};

/**
 * @param conn_handle
 */
const hci_cmd_t hci_le_read_supported_states = {
    HCI_OPCODE_HCI_LE_READ_SUPPORTED_STATES, "H"
    // return: status, LE states (64)
};

/**
 * @param rx_frequency ([0x00 0x27], frequency (MHz): 2420 + N*2)
 */
const hci_cmd_t hci_le_receiver_test = {
    HCI_OPCODE_HCI_LE_RECEIVER_TEST, "1"
    // return: status
};

/**
 * @param tx_frequency ([0x00 0x27], frequency (MHz): 2420 + N*2)
 * @param test_payload_lengh ([0x00,0x25])
 * @param packet_payload ([0,7] different patterns)
 */
const hci_cmd_t hci_le_transmitter_test = {
    HCI_OPCODE_HCI_LE_TRANSMITTER_TEST, "111"
    // return: status
};

/**
 * @param end_test_cmd
 */
const hci_cmd_t hci_le_test_end = {
    HCI_OPCODE_HCI_LE_TEST_END, "1"
    // return: status, number of packets (8)
};

/**
 * @param conn_handle
 * @param conn_interval_min ([0x0006,0x0c80], unit: 1.25 msec)
 * @param conn_interval_max ([0x0006,0x0c80], unit: 1.25 msec)
 * @param conn_latency ([0x0000,0x03e8], number of connection events)
 * @param supervision_timeout ([0x000a,0x0c80], unit: 10 msec)
 * @param minimum_CE_length ([0x0000,0xffff], unit: 0.625 msec)
 * @param maximum_CE_length ([0x0000,0xffff], unit: 0.625 msec)
 */
const hci_cmd_t hci_le_remote_connection_parameter_request_reply = {
    HCI_OPCODE_HCI_LE_REMOTE_CONNECTION_PARAMETER_REQUEST_REPLY, "H222222"
    // return: status, connection handle
};

/**
 * @param con_handle
 * @param reason
 */
const hci_cmd_t hci_le_remote_connection_parameter_request_negative_reply = {
    HCI_OPCODE_HCI_LE_REMOTE_CONNECTION_PARAMETER_REQUEST_NEGATIVE_REPLY, "H1"
    // return: status, connection handle
};

/**
 * @param con_handle
 * @param tx_octets
 * @param tx_time
 */
const hci_cmd_t hci_le_set_data_length = {
    HCI_OPCODE_HCI_LE_SET_DATA_LENGTH, "H22"
    // return: status, connection handle
};

/**
 */
const hci_cmd_t hci_le_read_suggested_default_data_length = {
    HCI_OPCODE_HCI_LE_READ_SUGGESTED_DEFAULT_DATA_LENGTH, ""
    // return: status, suggested max tx octets, suggested max tx time
};

/**
 * @param suggested_max_tx_octets
 * @param suggested_max_tx_time
 */
const hci_cmd_t hci_le_write_suggested_default_data_length = {
    HCI_OPCODE_HCI_LE_WRITE_SUGGESTED_DEFAULT_DATA_LENGTH, "22"
    // return: status
};

/**
 */
const hci_cmd_t hci_le_read_local_p256_public_key = {
    HCI_OPCODE_HCI_LE_READ_LOCAL_P256_PUBLIC_KEY, ""
//  LE Read Local P-256 Public Key Complete is generated on completion
};

/**
 * @param public key
 * @param private key
 */
const hci_cmd_t hci_le_generate_dhkey = {
    HCI_OPCODE_HCI_LE_GENERATE_DHKEY, "QQ"
// LE Generate DHKey Complete is generated on completion
};

/**
 * @param Peer_Identity_Address_Type
 * @param Peer_Identity_Address
 * @param Peer_IRK
 * @param Local_IRK
 */
const hci_cmd_t hci_le_add_device_to_resolving_list = {
        HCI_OPCODE_HCI_LE_ADD_DEVICE_TO_RESOLVING_LIST, "1BPP"
};

/**
 * @param Peer_Identity_Address_Type
 * @param Peer_Identity_Address
 */
const hci_cmd_t hci_le_remove_device_from_resolving_list = {
        HCI_OPCODE_HCI_LE_REMOVE_DEVICE_FROM_RESOLVING_LIST, "1B"
};

/**
 */
const hci_cmd_t hci_le_clear_resolving_list = {
        HCI_OPCODE_HCI_LE_CLEAR_RESOLVING_LIST, ""
};

/**
 */
const hci_cmd_t hci_le_read_resolving_list_size = {
        HCI_OPCODE_HCI_LE_READ_RESOLVING_LIST_SIZE, ""
};

/**
 * @param Peer_Identity_Address_Type
 * @param Peer_Identity_Address
 */
const hci_cmd_t hci_le_read_peer_resolvable_address = {
        HCI_OPCODE_HCI_LE_READ_PEER_RESOLVABLE_ADDRESS, ""
};

/**
 * @param Peer_Identity_Address_Type
 * @param Peer_Identity_Address
 */
const hci_cmd_t hci_le_read_local_resolvable_address = {
        HCI_OPCODE_HCI_LE_READ_LOCAL_RESOLVABLE_ADDRESS, ""
};

/**
 * @param Address_Resolution_Enable
 */
const hci_cmd_t hci_le_set_address_resolution_enabled= {
        HCI_OPCODE_HCI_LE_SET_ADDRESS_RESOLUTION_ENABLED, "1"
};

/**
 * @param RPA_Timeout in seconds, range 0x0001 to 0x0E10, default: 900 s
 */
const hci_cmd_t hci_le_set_resolvable_private_address_timeout= {
        HCI_OPCODE_HCI_LE_SET_RESOLVABLE_PRIVATE_ADDRESS_TIMEOUT, "2"
};

/**
 */
const hci_cmd_t hci_le_read_maximum_data_length = {
    HCI_OPCODE_HCI_LE_READ_MAXIMUM_DATA_LENGTH, ""
    // return: status, supported max tx octets, supported max tx time, supported max rx octets, supported max rx time
};

/**
 * @param con_handle
 */
const hci_cmd_t hci_le_read_phy = {
    HCI_OPCODE_HCI_LE_READ_PHY, "H"
    // return: status, connection handler, tx phy, rx phy
};

/**
 * @param all_phys
 * @param tx_phys
 * @param rx_phys
 */
const hci_cmd_t hci_le_set_default_phy = {
    HCI_OPCODE_HCI_LE_SET_DEFAULT_PHY, "111"
    // return: status
};

/**
 * @param con_handle
 * @param all_phys
 * @param tx_phys
 * @param rx_phys
 * @param phy_options
 */
const hci_cmd_t hci_le_set_phy = {
    HCI_OPCODE_HCI_LE_SET_PHY, "H1111"
// LE PHY Update Complete is generated on completion
};


#endif

// Broadcom / Cypress specific HCI commands

/**
 * @brief Enable Wide-Band Speech / mSBC decoding for PCM
 * @param enable_wbs is 0 for disable, 1 for enable
 * @param uuid_wbs is 2 for EV2/EV3
 */
const hci_cmd_t hci_bcm_enable_wbs = {
        HCI_OPCODE_HCI_BCM_ENABLE_WBS, "12"
        // return: status
};

/**
 * @brief Configure SCO Routing (BCM)
 * @param sco_routing is 0 for PCM, 1 for Transport, 2 for Codec and 3 for I2S
 * @param pcm_interface_rate is 0 for 128KBps, 1 for 256 KBps, 2 for 512KBps, 3 for 1024KBps, and 4 for 2048Kbps
 * @param frame_type is 0 for short and 1 for long
 * @param sync_mode is 0 for slave and 1 for master
 * @param clock_mode is 0 for slabe and 1 for master
 */
const hci_cmd_t hci_bcm_write_sco_pcm_int = {
    HCI_OPCODE_HCI_BCM_WRITE_SCO_PCM_INT, "11111"
    // return: status
};

/**
 * @brief Configure the I2S/PCM interface (BCM)
 * @param i2s_enable is 0 for off, 1 for on
 * @param is_master is 0 for slave, is 1 for master
 * @param sample_rate is 0 for 8 kHz, 1 for 16 kHz, 2 for 4 kHz
 * @param clock_rate is 0 for 128 kz, 1 for 256 kHz, 2 for 512 khz, 3 for 1024 kHz, 4 for 2048 khz
 * @param clock_mode is 0 for slabe and 1 for master
 */
const hci_cmd_t hci_bcm_write_i2spcm_interface_param = {
        HCI_OPCODE_HCI_BCM_WRITE_I2SPCM_INTERFACE_PARAM, "1111"
        // return: status
};


/**
 * @brief Activates selected Sleep Mode
 * @param sleep_mode: 0=no sleep, 1=UART, 2=UART with Messaging, 3=USB, 4=H4IBSS, USB with Host Wake, 6=SDIO, 7=UART CS-N, 8=SPI, 9=H5, 10=H4DS, 12=UART with BREAK 
 * @param idle_threshold_host (modes 1,2,5,7) time until considered idle, unit roughly 300 ms
 * @param idle_threshold_controller (modes 1-7,9) time until considered idle, unit roughly 300 ms
 * @param bt_wake_active_mode (modes 1,2,7) 0 = BT_WAKE line is active high, 1 = BT_WAKE is active low
 * @param host_wake_active_mode (modes 1,2,5,7) 0 = HOST_WAKE line is active high, 1 = HOST_WAKE is active low
 * @param allow_host_sleep_during_sco (modes 1,2,3,5,7)
 * @param combine_sleep_mode_and_lpm  (modes 1,2,3,5,7)
 * @param enable_tristate_control_of_uart_tx_line (modes 1,2,7)
 * @param active_connection_handling_on_suspend (modes 3,5)
 * @param resume_timeout (modes 3,5)
 * @param enable_break_to_host (mode 12)
 * @param pulsed_host_wake (modes 1,12)
 */
const hci_cmd_t hci_bcm_set_sleep_mode = {
    HCI_OPCODE_HCI_BCM_SET_SLEEP_MODE, "111111111111"
};

/**
 * @brief Set TX Power Table
 * @param is_le 0=classic, 1=LE
 * @param chip_max_tx_pwr_db chip level max TX power in dBM 
 */
const hci_cmd_t hci_bcm_write_tx_power_table = {
    HCI_OPCODE_HCI_BCM_WRITE_TX_POWER_TABLE, "11"
};

const hci_cmd_t hci_bcm_set_tx_pwr = {
    HCI_OPCODE_HCI_BCM_SET_TX_PWR, "11H"
};

/**
 * @brief This command starts receiving packets using packet transmission parameters such as
 *        frequency channel, packet type, and packet length. It is used for Packet RX.
 * @see   https://processors.wiki.ti.com/index.php/CC256x_Testing_Guide#Continuous_RX
 * @param frequency
 * @param ADPLL loop mode
 */
const hci_cmd_t hci_ti_drpb_tester_con_rx = {
        0xFD17, "11"
};

/**
 *
 *
 * @brief This command tests the RF transceiver in continuous transmission mode.
 *        The transmitter is activated by configuring the transmission parameters such as pattern, 
 *        modulation, and frequency. 
 * @see   processors.wiki.ti.com/index.php/CC256x_VS_HCI_Commands#HCI_VS_DRPb_Tester_Con_TX.280xFD84.29
 * @param modulation
 * @param test_pattern
 * @param frequency
 * @param power_level
 * @param reserved1
 * @param reserved2
 */
const hci_cmd_t hci_ti_drpb_tester_con_tx = {
    0xFD84, "111144"
};

/**
 * @brief This command starts sending/receiving packets using packet transmission parameters such as 
 *        frequency channel, packet type, and packet length. It is used for Packet TX/RX.
 * @see   processors.wiki.ti.com/index.php/CC256x_VS_HCI_Commands#HCI_VS_DRPb_Tester_Packet_TX_RX_.280xFD85.29
 * @param frequency_mode
 * @param tx_single_frequency
 * @param rx_single_frequency
 * @param acl_packet_type
 * @paarm acl_packet_data_pattern
 * @param reserved
 * @param power_level
 * @param disable_whitening
 * @param prbs9_initialization_value
 */
const hci_cmd_t hci_ti_drpb_tester_packet_tx_rx = {
    0xFD85, "1111112112"
};


/**
 * @param best effort access percentage
 * @param guaranteed access percentage
 * @param poll period
 * @param slave burst after tx
 * @param slave master search count
 * @param master burst after tx enable
 * @param master burst after rx limit
 */
const hci_cmd_t hci_ti_configure_ddip = {
        HCI_OPCODE_HCI_TI_VS_CONFIGURE_DDIP, "1111111"
};

/**
 * @brief This command is used to associate the requested ACL handle with Wide Band Speech configuration.
 * @param enable 0=disable, 1=enable
 * @param a3dp_role (NL5500, WL128x only) 0=source,1=sink
 * @param code_upload (NL5500, WL128x only) 0=do not load a3dp code, 1=load a3dp code
 * @param reserved for future use
 */
const hci_cmd_t hci_ti_avrp_enable = {
        0xFD92, "1112"
};

/**
 * @brief This command is used to associate the requested ACL handle with Wide Band Speech configuration.
 * @param acl_con_handle
 */
const hci_cmd_t hci_ti_wbs_associate = {
        0xFD78, "H"
};

/**
 * @brief This command is used to disassociate Wide Band Speech configuration from any ACL handle.
 */
const hci_cmd_t hci_ti_wbs_disassociate = {
        0xFD79, ""
};

/**
 * @brief This command configures the codec interface parameters and the PCM clock rate, which is relevant when
          the Bluetooth core generates the clock. This command must be used by the host to use the PCM
          interface.
 * @param clock_rate in kHz
 * @param clock_direction 0=master/output, 1=slave/input
 * @param frame_sync_frequency in Hz
 * @param frame_sync_duty_cycle 0=50% (I2S Format), 0x0001-0xffff number of cycles of PCM clock
 * @param frame_sync_edge 0=driven/sampled at rising edge, 1=driven/sampled at falling edge of PCM clock
 * @param frame_sync_polariy 0=active high, 1=active low
 * @param reserved1
 * @param channel_1_data_out_size sample size in bits
 * @param channel_1_data_out_offset number of PCM clock cycles between rising of frame sync and data start
 * @param channel_1_data_out_edge 0=data driven at rising edge, 1=data driven at falling edge of PCM clock
 * @param channel_1_data_in_size sample size in bits
 * @param channel_1_data_in_offset number of PCM clock cycles between rising of frame sync and data start
 * @param channel_1_data_in_edge 0=data sampled at rising edge, 1=data sampled at falling edge of PCM clock
 * @param fsync_multiplier this field is only relevant to CC256XB from service pack 0.2 !!! -> use 0x00
 * @param channel_2_data_out_size sample size in bits
 * @param channel_2_data_out_offset number of PCM clock cycles between rising of frame sync and data start
 * @param channel_2_data_out_edge 0=data driven at rising edge, 1=data driven at falling edge of PCM clock
 * @param channel_2_data_in_size sample size in bits
 * @param channel_2_data_in_offset number of PCM clock cycles between rising of frame sync and data start
 * @param channel_2_data_in_edge 0=data sampled at rising edge, 1=data sampled at falling edge of PCM clock
 * @param reserved2
 *
 */
const hci_cmd_t hci_ti_write_codec_config = {
        0xFD06, "214211122122112212211"
};

/**
 * @brief This command is used only for internal testing.
 * @see   https://processors.wiki.ti.com/index.php/CC256x_Testing_Guide#Continuous_TX
 * @param frequency
 * @param ADPLL loop mode
 */
const hci_cmd_t hci_ti_drpb_enable_rf_calibration = {
        0xFD80, "141"
};

/**
 * @brief This command command is only required for the continuous TX test of modulated
 * (GFSK, π/4-DQPSK or 8DPSK) signal. This command should be skipped when performing continuous TX test for CW.
 * @see   https://processors.wiki.ti.com/index.php/CC256x_Testing_Guide#Continuous_RX
 * @param frequency
 * @param ADPLL loop mode
 */
const hci_cmd_t hci_ti_write_hardware_register = {
        0xFF01, "42"
};
