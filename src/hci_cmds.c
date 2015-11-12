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
 *  hci_cmds.c
 *
 *  Created by Matthias Ringwald on 7/23/09.
 */

#include <btstack/hci_cmds.h>

#include <string.h>

#include <btstack/sdp_util.h>
#include "btstack-config.h"
#include "hci.h"

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
uint16_t hci_create_cmd_internal(uint8_t *hci_cmd_buffer, const hci_cmd_t *cmd, va_list argptr){
    
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
#ifdef HAVE_BLE
            case 'A': // 31 bytes advertising data
                ptr = va_arg(argptr, uint8_t *);
                memcpy(&hci_cmd_buffer[pos], ptr, 31);
                pos += 31;
                break;
#endif
#ifdef HAVE_SDP
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
 * construct HCI Command based on template
 *
 * mainly calls hci_create_cmd_internal
 */
uint16_t hci_create_cmd(uint8_t *hci_cmd_buffer, hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_create_cmd_internal(hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    return len;
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



#ifdef HAVE_BLE
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


// BTstack commands
const hci_cmd_t btstack_get_state = {
OPCODE(OGF_BTSTACK, BTSTACK_GET_STATE), ""
};

/**
 * @param power_mode (0 = off, 1 = on)
 */
const hci_cmd_t btstack_set_power_mode = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_POWER_MODE), "1"
};


/**
 * @param acl_capture_mode (0 = off, 1 = on)
 */
const hci_cmd_t btstack_set_acl_capture_mode = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_ACL_CAPTURE_MODE), "1"
};

const hci_cmd_t btstack_get_version = {
OPCODE(OGF_BTSTACK, BTSTACK_GET_VERSION), ""
};

const hci_cmd_t btstack_get_system_bluetooth_enabled = {
OPCODE(OGF_BTSTACK, BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED), ""
};

/**
 * @param bluetooth_enabled_flag (0 = off, 1 = on, only used by btstack config)
 */
const hci_cmd_t btstack_set_system_bluetooth_enabled = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED), "1"
};

/**
 * @param discoverable_flag (0 = off, 1 = on)
 */
const hci_cmd_t btstack_set_discoverable = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_DISCOVERABLE), "1"
};

/**
 * @param bluetooth_enabled_flag (0 = off, 1 = on, only used by btstack config)
 */
const hci_cmd_t btstack_set_bluetooth_enabled = {
OPCODE(OGF_BTSTACK, BTSTACK_SET_BLUETOOTH_ENABLED), "1"
};

/**
 * @param bd_addr (48)
 * @param psm (16)
 */
const hci_cmd_t l2cap_create_channel = {
OPCODE(OGF_BTSTACK, L2CAP_CREATE_CHANNEL), "B2"
};

/**
 * @param bd_addr (48)
 * @param psm (16)
 * @param mtu (16)
 */
const hci_cmd_t l2cap_create_channel_mtu = {
OPCODE(OGF_BTSTACK, L2CAP_CREATE_CHANNEL_MTU), "B22"
// @param bd_addr(48), psm (16), mtu (16)
};

/**
 * @param channel (16)
 * @param reason (16)
 */
const hci_cmd_t l2cap_disconnect = {
OPCODE(OGF_BTSTACK, L2CAP_DISCONNECT), "21"
};

/**
 * @param psm (16)
 * @param mtu (16)
 */
const hci_cmd_t l2cap_register_service = {
OPCODE(OGF_BTSTACK, L2CAP_REGISTER_SERVICE), "22"
};

/**
 * @param psm (16)
 */
const hci_cmd_t l2cap_unregister_service = {
OPCODE(OGF_BTSTACK, L2CAP_UNREGISTER_SERVICE), "2"
};

/**
 * @param source_cid (16)
 */
const hci_cmd_t l2cap_accept_connection = {
OPCODE(OGF_BTSTACK, L2CAP_ACCEPT_CONNECTION), "2"
};

/**
 * @param source_cid (16)
 * @param reason
 */
const hci_cmd_t l2cap_decline_connection = {
OPCODE(OGF_BTSTACK, L2CAP_DECLINE_CONNECTION), "21"
};


/**
 * @param service_record
 */
const hci_cmd_t sdp_register_service_record = {
OPCODE(OGF_BTSTACK, SDP_REGISTER_SERVICE_RECORD), "S"
};

/**
 * @param service_record_handle
 */
const hci_cmd_t sdp_unregister_service_record = {
OPCODE(OGF_BTSTACK, SDP_UNREGISTER_SERVICE_RECORD), "4"
};

/**
 * @param bd_addr
 * @param service_search_pattern
 */
const hci_cmd_t sdp_client_query_rfcomm_services = {
OPCODE(OGF_BTSTACK, SDP_CLIENT_QUERY_RFCOMM_SERVICES), "BS"
};

/**
 * @param bd_addr
 * @param service_search_pattern
 * @param attribute_ID_list
 */
const hci_cmd_t sdp_client_query_services = {
OPCODE(OGF_BTSTACK, SDP_CLIENT_QUERY_SERVICES), "BSS"
};

/**
 * @param bd_addr
 * @param server_channel
 */
const hci_cmd_t rfcomm_create_channel = {
OPCODE(OGF_BTSTACK, RFCOMM_CREATE_CHANNEL), "B1"
};

/**
 * @param bd_addr
 * @param server_channel
 * @param mtu
 * @param credits
 */
const hci_cmd_t rfcomm_create_channel_with_initial_credits = {
OPCODE(OGF_BTSTACK, RFCOMM_CREATE_CHANNEL_WITH_CREDITS), "B121"
};

/**
 * @param rfcomm_cid
 * @param credits
 */
const hci_cmd_t rfcomm_grants_credits= {
OPCODE(OGF_BTSTACK, RFCOMM_GRANT_CREDITS), "21"
};

/**
 * @param rfcomm_cid
 * @param reason
 */
const hci_cmd_t rfcomm_disconnect = {
OPCODE(OGF_BTSTACK, RFCOMM_DISCONNECT), "21"
};

/**
 * @param server_channel
 * @param mtu
 */
const hci_cmd_t rfcomm_register_service = {
OPCODE(OGF_BTSTACK, RFCOMM_REGISTER_SERVICE), "12"
};

/**
 * @param server_channel
 * @param mtu
 * @param initial_credits
 */
const hci_cmd_t rfcomm_register_service_with_initial_credits = {
OPCODE(OGF_BTSTACK, RFCOMM_REGISTER_SERVICE_WITH_CREDITS), "121"
};

/**
 * @param service_channel
 */
const hci_cmd_t rfcomm_unregister_service = {
OPCODE(OGF_BTSTACK, RFCOMM_UNREGISTER_SERVICE), "2"
};

/**
 * @param source_cid
 */
const hci_cmd_t rfcomm_accept_connection = {
OPCODE(OGF_BTSTACK, RFCOMM_ACCEPT_CONNECTION), "2"
};


/**
 * @param source_cid
 * @param reason
 */
const hci_cmd_t rfcomm_decline_connection = {
OPCODE(OGF_BTSTACK, RFCOMM_DECLINE_CONNECTION), "21"
};

// request persistent rfcomm channel number for named service
/**
 * @param named_service
 */
const hci_cmd_t rfcomm_persistent_channel_for_service = {
OPCODE(OGF_BTSTACK, RFCOMM_PERSISTENT_CHANNEL), "N"
};

/**
 * @param handle
 */
const hci_cmd_t gap_disconnect_cmd = {
OPCODE(OGF_BTSTACK, GAP_DISCONNECT), "H"
};

/**
 */
const hci_cmd_t gap_le_scan_start = {
OPCODE(OGF_BTSTACK, GAP_LE_SCAN_START), ""
};

/**
 */
const hci_cmd_t gap_le_scan_stop = {
OPCODE(OGF_BTSTACK, GAP_LE_SCAN_STOP), ""
};

/**
 * @param scan_type
 * @param scan_interval
 * @param scan_window
 */
const hci_cmd_t gap_le_set_scan_parameters = {
OPCODE(OGF_BTSTACK, GAP_LE_SET_SCAN_PARAMETERS), "122"
};

/**
 * @param peer_address_type
 * @param peer_address
 */
const hci_cmd_t gap_le_connect_cmd = {
OPCODE(OGF_BTSTACK, GAP_LE_CONNECT), "1B"
};

/**
 * @param peer_address_type
 * @param peer_address
 */
const hci_cmd_t gap_le_connect_cancel_cmd = {
OPCODE(OGF_BTSTACK, GAP_LE_CONNECT_CANCEL), ""
};

/**
 * @param handle
 */
const hci_cmd_t gatt_discover_primary_services_cmd = {
OPCODE(OGF_BTSTACK, GATT_DISCOVER_ALL_PRIMARY_SERVICES), "H"
};

/**
 * @param handle
 * @param uuid16
 */
const hci_cmd_t gatt_discover_primary_services_by_uuid16_cmd = {
    OPCODE(OGF_BTSTACK, GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID16), "H2"
};

/**
 * @param handle
 * @param uuid128
 */
const hci_cmd_t gatt_discover_primary_services_by_uuid128_cmd = {
    OPCODE(OGF_BTSTACK, GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID128), "HU"
};

/**
 * @param handle
 * @param service
 */
const hci_cmd_t gatt_find_included_services_for_service_cmd = {
    OPCODE(OGF_BTSTACK, GATT_FIND_INCLUDED_SERVICES_FOR_SERVICE), "HX"
};

/**
 * @param handle
 * @param service
 */
const hci_cmd_t gatt_discover_characteristics_for_service_cmd = {
    OPCODE(OGF_BTSTACK, GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE), "HX"
};

/**
 * @param handle
 * @param service
 * @param uuid128
 */
const hci_cmd_t gatt_discover_characteristics_for_service_by_uuid128_cmd = {
    OPCODE(OGF_BTSTACK, GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID128), "HXU"
};

/**
 * @param handle
 * @param characteristic
 */
const hci_cmd_t gatt_discover_characteristic_descriptors_cmd = {
    OPCODE(OGF_BTSTACK, GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS), "HY"
};

/**
 * @param handle
 * @param characteristic
 */
const hci_cmd_t gatt_read_value_of_characteristic_cmd = {
    OPCODE(OGF_BTSTACK, GATT_READ_VALUE_OF_CHARACTERISTIC), "HY"
};

/**
 * @param handle
 * @param characteristic
 */
const hci_cmd_t gatt_read_long_value_of_characteristic_cmd = {
    OPCODE(OGF_BTSTACK, GATT_READ_LONG_VALUE_OF_CHARACTERISTIC), "HY"
};

/**
 * @param handle
 * @param characteristic
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_write_value_of_characteristic_without_response_cmd = {
    OPCODE(OGF_BTSTACK, GATT_WRITE_VALUE_OF_CHARACTERISTIC_WITHOUT_RESPONSE), "HYLV"
};

/**
 * @param handle
 * @param characteristic
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_write_value_of_characteristic_cmd = {
    OPCODE(OGF_BTSTACK, GATT_WRITE_VALUE_OF_CHARACTERISTIC), "HYLV"
};

/**
 * @param handle
 * @param characteristic
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_write_long_value_of_characteristic_cmd = {
    OPCODE(OGF_BTSTACK, GATT_WRITE_LONG_VALUE_OF_CHARACTERISTIC), "HYLV"
};

/**
 * @param handle
 * @param characteristic
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_reliable_write_long_value_of_characteristic_cmd = {
    OPCODE(OGF_BTSTACK, GATT_RELIABLE_WRITE_LONG_VALUE_OF_CHARACTERISTIC), "HYLV"
};

/**
 * @param handle
 * @param descriptor
 */
const hci_cmd_t gatt_read_characteristic_descriptor_cmd = {
    OPCODE(OGF_BTSTACK, GATT_READ_CHARACTERISTIC_DESCRIPTOR), "HZ"
};

/**
 * @param handle
 * @param descriptor
 */
const hci_cmd_t gatt_read_long_characteristic_descriptor_cmd = {
    OPCODE(OGF_BTSTACK, GATT_READ_LONG_CHARACTERISTIC_DESCRIPTOR), "HZ"
};

/**
 * @param handle
 * @param descriptor
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_write_characteristic_descriptor_cmd = {
    OPCODE(OGF_BTSTACK, GATT_WRITE_CHARACTERISTIC_DESCRIPTOR), "HZLV"
};

/**
 * @param handle
 * @param descriptor
 * @param data_length
 * @param data
 */
const hci_cmd_t gatt_write_long_characteristic_descriptor_cmd = {
    OPCODE(OGF_BTSTACK, GATT_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR), "HZLV"
};

/**
 * @param handle
 * @param characteristic
 * @param configuration
 */
const hci_cmd_t gatt_write_client_characteristic_configuration_cmd = {
    OPCODE(OGF_BTSTACK, GATT_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION), "HY2"
};

/**
 * @param handle
 */
const hci_cmd_t gatt_get_mtu = {
    OPCODE(OGF_BTSTACK, GATT_GET_MTU), "H"
};
