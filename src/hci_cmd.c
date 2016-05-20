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

/*
 *  hci_cmd.c
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#include "btstack_config.h"

#include "classic/sdp_util.h"
#include "hci.h"
#include "hci_cmd.h"

#include <string.h>

// calculate combined ogf/ocf value
#define OPCODE(ogf, ocf) (ocf | ogf << 10)

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
 *   P: 16 byte Pairing code
 *   A: 31 bytes advertising data
 *   S: Service Record (Data Element Sequence)
 */
uint16_t hci_cmd_create_from_template(uint8_t *hci_cmd_buffer, const hci_cmd_t *cmd, va_list argptr){
    
    hci_cmd_buffer[0] = cmd->opcode & 0xff;
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
                hci_cmd_buffer[pos++] = word & 0xff;
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
                memcpy(&hci_cmd_buffer[pos], ptr, 8);
                pos += 8;
                break;
            case 'E': // Extended Inquiry Information 240 octets
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 240);
                pos += 240;
                break;
            case 'N': { // UTF-8 string, null terminated
                ptr = va_arg(argptr, uint8_t *);
                uint16_t len = strlen((const char*) ptr);
                if (len > 248) {
                    len = 248;
                }
                memcpy(&hci_cmd_buffer[pos], ptr, len);
                if (len < 248) {
                    // fill remaining space with zeroes
                    memset(&hci_cmd_buffer[pos+len], 0, 248-len);
                }
                pos += 248;
                break;
            }
            case 'P': // 16 byte PIN code or link key
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 16);
                pos += 16;
                break;
#ifdef ENABLE_BLE
            case 'A': // 31 bytes advertising data
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 31);
                pos += 31;
                break;
#endif
#ifdef ENABLE_SDP
            case 'S': { // Service Record (Data Element Sequence)
                ptr = va_arg(argptr, uint8_t *);
                uint16_t len = de_get_len(ptr);
                memcpy(&hci_cmd_buffer[pos], ptr, len);
                pos += len;
                break;
            }
#endif
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
OPCODE(OGF_LINK_CONTROL, 0x01), "311"
};

/**
 */
const hci_cmd_t hci_inquiry_cancel = {
OPCODE(OGF_LINK_CONTROL, 0x02), ""
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
OPCODE(OGF_LINK_CONTROL, 0x05), "B21121"
};

/**
 * @param handle 
 * @param reason (0x05, 0x13-0x15, 0x1a, 0x29, see Errors Codes in BT Spec Part D)
 */
const hci_cmd_t hci_disconnect = {
OPCODE(OGF_LINK_CONTROL, 0x06), "H1"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_create_connection_cancel = {
OPCODE(OGF_LINK_CONTROL, 0x08), "B"
};

/**
 * @param bd_addr
 * @param role (become master, stay slave)
 */
const hci_cmd_t hci_accept_connection_request = {
OPCODE(OGF_LINK_CONTROL, 0x09), "B1"
};

/**
 * @param bd_addr
 * @param reason (e.g. CONNECTION REJECTED DUE TO LIMITED RESOURCES (0x0d))
 */
const hci_cmd_t hci_reject_connection_request = {
OPCODE(OGF_LINK_CONTROL, 0x0a), "B1"
};

/**
 * @param bd_addr
 * @param link_key
 */
const hci_cmd_t hci_link_key_request_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0b), "BP"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_link_key_request_negative_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0c), "B"
};

/**
 * @param bd_addr
 * @param pin_length
 * @param pin (c-string)
 */
const hci_cmd_t hci_pin_code_request_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0d), "B1P"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_pin_code_request_negative_reply = {
OPCODE(OGF_LINK_CONTROL, 0x0e), "B"
};

/**
 * @param handle
 * @param packet_type
 */
const hci_cmd_t hci_change_connection_packet_type = {
OPCODE(OGF_LINK_CONTROL, 0x0f), "H2"
};

/**
 * @param handle
 */
const hci_cmd_t hci_authentication_requested = {
OPCODE(OGF_LINK_CONTROL, 0x11), "H"
};

/**
 * @param handle
 * @param encryption_enable
 */
const hci_cmd_t hci_set_connection_encryption = {
OPCODE(OGF_LINK_CONTROL, 0x13), "H1"
};

/**
 * @param handle
 */
const hci_cmd_t hci_change_connection_link_key = {
OPCODE(OGF_LINK_CONTROL, 0x15), "H"
};

/**
 * @param bd_addr
 * @param page_scan_repetition_mode
 * @param reserved
 * @param clock_offset
 */
const hci_cmd_t hci_remote_name_request = {
OPCODE(OGF_LINK_CONTROL, 0x19), "B112"
};


/**
 * @param bd_addr
 */
const hci_cmd_t hci_remote_name_request_cancel = {
OPCODE(OGF_LINK_CONTROL, 0x1A), "B"
};

/**
 * @param handle
 */
const hci_cmd_t hci_read_remote_supported_features_command = {
OPCODE(OGF_LINK_CONTROL, 0x1B), "H"
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
OPCODE(OGF_LINK_CONTROL, 0x0028), "H442212"
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
OPCODE(OGF_LINK_CONTROL, 0x0029), "B442212"
};

/**
 * @param bd_addr
 * @param IO_capability
 * @param OOB_data_present
 * @param authentication_requirements
 */
const hci_cmd_t hci_io_capability_request_reply = {
OPCODE(OGF_LINK_CONTROL, 0x2b), "B111"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_user_confirmation_request_reply = {
OPCODE(OGF_LINK_CONTROL, 0x2c), "B"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_user_confirmation_request_negative_reply = {
OPCODE(OGF_LINK_CONTROL, 0x2d), "B"
};

/**
 * @param bd_addr
 * @param numeric_value
 */
const hci_cmd_t hci_user_passkey_request_reply = {
OPCODE(OGF_LINK_CONTROL, 0x2e), "B4"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_user_passkey_request_negative_reply = {
OPCODE(OGF_LINK_CONTROL, 0x2f), "B"
};

/**
 * @param bd_addr
 */
const hci_cmd_t hci_remote_oob_data_request_negative_reply = {
OPCODE(OGF_LINK_CONTROL, 0x33), "B"
};

/**
 * @param bd_addr
 * @param reason (Part D, Error codes)
 */
const hci_cmd_t hci_io_capability_request_negative_reply = {
OPCODE(OGF_LINK_CONTROL, 0x34), "B1"
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
OPCODE(OGF_LINK_CONTROL, 0x3d), "H4412212222441221222211111111221" 
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
OPCODE(OGF_LINK_CONTROL, 0x3e), "B4412212222441221222211111111221" 
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
OPCODE(OGF_LINK_POLICY, 0x03), "H2222"
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
OPCODE(OGF_LINK_POLICY, 0x07), "H114444"
};

/**
 * @param handle
 */
const hci_cmd_t hci_role_discovery = {
OPCODE(OGF_LINK_POLICY, 0x09), "H"
};

/**
 * @param bd_addr
 * @param role (0=master,1=slave)
 */
const hci_cmd_t hci_switch_role_command= {
OPCODE(OGF_LINK_POLICY, 0x0b), "B1"
};

/**
 * @param handle
 */
const hci_cmd_t hci_read_link_policy_settings = {
OPCODE(OGF_LINK_POLICY, 0x0c), "H"
};

/**
 * @param handle
 * @param settings
 */
const hci_cmd_t hci_write_link_policy_settings = {
OPCODE(OGF_LINK_POLICY, 0x0d), "H2"
};


/**
 *  Controller & Baseband Commands 
 */

/**
 * @param event_mask_lover_octets
 * @param event_mask_higher_octets
 */
const hci_cmd_t hci_set_event_mask = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x01), "44"
};

/**
 */
const hci_cmd_t hci_reset = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x03), ""
};

/**
 * @param bd_addr
 * @param delete_all_flags
 */
const hci_cmd_t hci_delete_stored_link_key = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x12), "B1"
};

/**
 * @param local_name (UTF-8, Null Terminated, max 248 octets)
 */
const hci_cmd_t hci_write_local_name = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x13), "N"
};

/**
 * @param page_timeout (* 0.625 ms)
 */
const hci_cmd_t hci_write_page_timeout = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x18), "2"
};

/**
 * @param scan_enable (no, inq, page, inq+page)
 */
const hci_cmd_t hci_write_scan_enable = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x1A), "1"
};

/**
 * @param authentication_enable
 */
const hci_cmd_t hci_write_authentication_enable = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x20), "1"
};

/**
 * @param class_of_device
 */
const hci_cmd_t hci_write_class_of_device = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x24), "3"
};

/** 
 */
const hci_cmd_t hci_read_num_broadcast_retransmissions = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x29), ""
};

/**
 * @param num_broadcast_retransmissions (e.g. 0 for a single broadcast)
 */
const hci_cmd_t hci_write_num_broadcast_retransmissions = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x2a), "1"
};

/**
 * @param synchronous_flow_control_enable - if yes, num completed packet everts are sent for SCO packets
 */
const hci_cmd_t hci_write_synchronous_flow_control_enable = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x2f), "1"
};

/**
 * @param host_acl_data_packet_length
 * @param host_synchronous_data_packet_length
 * @param host_total_num_acl_data_packets
 * @param host_total_num_synchronous_data_packets
 */
const hci_cmd_t hci_host_buffer_size = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x33), "2122"
};

/**
 * @param handle
 */
const hci_cmd_t hci_read_link_supervision_timeout = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x36), "H"
};

/**
 * @param handle
 * @param timeout (0x0001 - 0xFFFF Time -> Range: 0.625ms - 40.9 sec)
 */
const hci_cmd_t hci_write_link_supervision_timeout = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x37), "H2"
};

/**
 * @param inquiry_mode (0x00 = standard, 0x01 = with RSSI, 0x02 = extended)
 */
const hci_cmd_t hci_write_inquiry_mode = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x45), "1"
};

/**
 * @param fec_required
 * @param exstended_inquiry_response
 */
const hci_cmd_t hci_write_extended_inquiry_response = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x52), "1E"
};

/**
 * @param mode (0 = off, 1 = on)
 */
const hci_cmd_t hci_write_simple_pairing_mode = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x56), "1"
};


/**
 * @param mode (0 = off, 1 = on)
 */
const hci_cmd_t hci_write_default_erroneous_data_reporting = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x5B), "1"
};

/**
 */
const hci_cmd_t hci_read_le_host_supported = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x6c), ""
// return: status, le supported host, simultaneous le host
};

/**
 * @param le_supported_host
 * @param simultaneous_le_host
 */
const hci_cmd_t hci_write_le_host_supported = {
OPCODE(OGF_CONTROLLER_BASEBAND, 0x6d), "11"
// return: status
};

/**
 * Testing Commands
 */


/**
 */
const hci_cmd_t hci_read_loopback_mode = {
OPCODE(OGF_TESTING, 0x01), ""
// return: status, loopback mode (0 = off, 1 = local loopback, 2 = remote loopback)
};

/**
 * @param loopback_mode
 */
const hci_cmd_t hci_write_loopback_mode = {
OPCODE(OGF_TESTING, 0x02), "1"
// return: status
};


/**
 * Informational Parameters
 */

const hci_cmd_t hci_read_local_version_information = {
OPCODE(OGF_INFORMATIONAL_PARAMETERS, 0x01), ""
};
const hci_cmd_t hci_read_local_supported_commands = {
OPCODE(OGF_INFORMATIONAL_PARAMETERS, 0x02), ""
};
const hci_cmd_t hci_read_local_supported_features = {
OPCODE(OGF_INFORMATIONAL_PARAMETERS, 0x03), ""
};
const hci_cmd_t hci_read_buffer_size = {
OPCODE(OGF_INFORMATIONAL_PARAMETERS, 0x05), ""
};
const hci_cmd_t hci_read_bd_addr = {
OPCODE(OGF_INFORMATIONAL_PARAMETERS, 0x09), ""
};

/**
 * Status Paramters
 */

/**
 * @param handle
 */
const hci_cmd_t hci_read_rssi = {
OPCODE(OGF_STATUS_PARAMETERS, 0x05), "H"
// no params
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
OPCODE(OGF_LE_CONTROLLER, 0x01), "44"
// return: status
};

const hci_cmd_t hci_le_read_buffer_size = {
OPCODE(OGF_LE_CONTROLLER, 0x02), ""
// return: status, le acl data packet len (16), total num le acl data packets(8)
};
const hci_cmd_t hci_le_read_supported_features = {
OPCODE(OGF_LE_CONTROLLER, 0x03), ""
// return: LE_Features See [Vol 6] Part B, Section 4.6
};

/**
 * @param random_bd_addr
 */
const hci_cmd_t hci_le_set_random_address = {
OPCODE(OGF_LE_CONTROLLER, 0x05), "B"
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
OPCODE(OGF_LE_CONTROLLER, 0x06), "22111B11"
// return: status
};

const hci_cmd_t hci_le_read_advertising_channel_tx_power = {
OPCODE(OGF_LE_CONTROLLER, 0x07), ""
// return: status, level [-20,10] signed int (8), units dBm
};

/**
 * @param advertising_data_length
 * @param advertising_data (31 bytes)
 */
const hci_cmd_t hci_le_set_advertising_data= {
OPCODE(OGF_LE_CONTROLLER, 0x08), "1A"
// return: status
};

/**
 * @param scan_response_data_length
 * @param scan_response_data (31 bytes)
 */
const hci_cmd_t hci_le_set_scan_response_data= {
OPCODE(OGF_LE_CONTROLLER, 0x09), "1A"
// return: status
};

/**
 * @param advertise_enable (off: 0, on: 1)
 */
const hci_cmd_t hci_le_set_advertise_enable = {
OPCODE(OGF_LE_CONTROLLER, 0x0a), "1"
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
OPCODE(OGF_LE_CONTROLLER, 0x0b), "12211"
// return: status
};

/**
 * @param le_scan_enable  (disabled (0), enabled (1))
 * @param filter_duplices (disabled (0), enabled (1))
 */
const hci_cmd_t hci_le_set_scan_enable = {
OPCODE(OGF_LE_CONTROLLER, 0x0c), "11"
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
OPCODE(OGF_LE_CONTROLLER, 0x0d), "2211B1222222"
// return: none -> le create connection complete event
};

const hci_cmd_t hci_le_create_connection_cancel = {
OPCODE(OGF_LE_CONTROLLER, 0x0e), ""
// return: status
};

const hci_cmd_t hci_le_read_white_list_size = {
OPCODE(OGF_LE_CONTROLLER, 0x0f), ""
// return: status, number of entries in controller whitelist
};

const hci_cmd_t hci_le_clear_white_list = {
OPCODE(OGF_LE_CONTROLLER, 0x10), ""
// return: status
};

/**
 * @param address_type (public (0), random (1))
 * @param bd_addr
 */
const hci_cmd_t hci_le_add_device_to_white_list = {
OPCODE(OGF_LE_CONTROLLER, 0x11), "1B"
// return: status
};

/**
 * @param address_type (public (0), random (1))
 * @param bd_addr
 */
const hci_cmd_t hci_le_remove_device_from_white_list = {
OPCODE(OGF_LE_CONTROLLER, 0x12), "1B"
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
OPCODE(OGF_LE_CONTROLLER, 0x13), "H222222"
// return: none -> le connection update complete event
};

/**
 * @param channel_map_lower_32bits 
 * @param channel_map_higher_5bits
 */
const hci_cmd_t hci_le_set_host_channel_classification = {
OPCODE(OGF_LE_CONTROLLER, 0x14), "41"
// return: status
};

/**
 * @param conn_handle
 */
const hci_cmd_t hci_le_read_channel_map = {
OPCODE(OGF_LE_CONTROLLER, 0x15), "H"
// return: status, connection handle, channel map (5 bytes, 37 used)
};

/**
 * @param conn_handle
 */
const hci_cmd_t hci_le_read_remote_used_features = {
OPCODE(OGF_LE_CONTROLLER, 0x16), "H"
// return: none -> le read remote used features complete event
};

/**
 * @param key ((128) for AES-128)
 * @param plain_text (128) 
 */
const hci_cmd_t hci_le_encrypt = {
OPCODE(OGF_LE_CONTROLLER, 0x17), "PP"
// return: status, encrypted data (128)
};

const hci_cmd_t hci_le_rand = {
OPCODE(OGF_LE_CONTROLLER, 0x18), ""
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
OPCODE(OGF_LE_CONTROLLER, 0x19), "H442P"
// return: none -> encryption changed or encryption key refresh complete event
};

/**
 * @param connection_handle
 * @param long_term_key (128)
 */
const hci_cmd_t hci_le_long_term_key_request_reply = {
OPCODE(OGF_LE_CONTROLLER, 0x1a), "HP"
// return: status, connection handle
};

/**
 * @param conn_handle
 */
const hci_cmd_t hci_le_long_term_key_negative_reply = {
OPCODE(OGF_LE_CONTROLLER, 0x1b), "H"
// return: status, connection handle
};

/**
 * @param conn_handle
 */
const hci_cmd_t hci_le_read_supported_states = {
OPCODE(OGF_LE_CONTROLLER, 0x1c), "H"
// return: status, LE states (64)
};

/**
 * @param rx_frequency ([0x00 0x27], frequency (MHz): 2420 + N*2)
 */
const hci_cmd_t hci_le_receiver_test = {
OPCODE(OGF_LE_CONTROLLER, 0x1d), "1"
// return: status
};

/**
 * @param tx_frequency ([0x00 0x27], frequency (MHz): 2420 + N*2)
 * @param test_payload_lengh ([0x00,0x25])
 * @param packet_payload ([0,7] different patterns)
 */
const hci_cmd_t hci_le_transmitter_test = {
OPCODE(OGF_LE_CONTROLLER, 0x1e), "111"
// return: status
};

/**
 * @param end_test_cmd
 */
const hci_cmd_t hci_le_test_end = {
OPCODE(OGF_LE_CONTROLLER, 0x1f), "1"
// return: status, number of packets (8)
};
#endif
