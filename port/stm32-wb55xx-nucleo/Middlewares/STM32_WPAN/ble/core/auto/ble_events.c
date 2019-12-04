/******************************************************************************
 * @file    ble_events.c
 * @author  MCD Application Team
 * @date    19 July 2019
 * @brief   Source file for STM32WB (Event callbacks)
 *          Auto-generated file: do not edit!
 ******************************************************************************
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

#include "ble_events.h"

void hci_disconnection_complete_event_process(uint8_t *buffer_in);
void hci_encryption_change_event_process(uint8_t *buffer_in);
void hci_read_remote_version_information_complete_event_process(uint8_t *buffer_in);
void hci_hardware_error_event_process(uint8_t *buffer_in);
void hci_number_of_completed_packets_event_process(uint8_t *buffer_in);
void hci_data_buffer_overflow_event_process(uint8_t *buffer_in);
void hci_encryption_key_refresh_complete_event_process(uint8_t *buffer_in);
void aci_hal_end_of_radio_activity_event_process(uint8_t *buffer_in);
void aci_hal_scan_req_report_event_process(uint8_t *buffer_in);
void aci_hal_fw_error_event_process(uint8_t *buffer_in);
void aci_gap_limited_discoverable_event_process(uint8_t *buffer_in);
void aci_gap_pairing_complete_event_process(uint8_t *buffer_in);
void aci_gap_pass_key_req_event_process(uint8_t *buffer_in);
void aci_gap_authorization_req_event_process(uint8_t *buffer_in);
void aci_gap_slave_security_initiated_event_process(uint8_t *buffer_in);
void aci_gap_bond_lost_event_process(uint8_t *buffer_in);
void aci_gap_proc_complete_event_process(uint8_t *buffer_in);
void aci_gap_addr_not_resolved_event_process(uint8_t *buffer_in);
void aci_gap_numeric_comparison_value_event_process(uint8_t *buffer_in);
void aci_gap_keypress_notification_event_process(uint8_t *buffer_in);
void aci_l2cap_connection_update_resp_event_process(uint8_t *buffer_in);
void aci_l2cap_proc_timeout_event_process(uint8_t *buffer_in);
void aci_l2cap_connection_update_req_event_process(uint8_t *buffer_in);
void aci_l2cap_command_reject_event_process(uint8_t *buffer_in);
void aci_gatt_attribute_modified_event_process(uint8_t *buffer_in);
void aci_gatt_proc_timeout_event_process(uint8_t *buffer_in);
void aci_att_exchange_mtu_resp_event_process(uint8_t *buffer_in);
void aci_att_find_info_resp_event_process(uint8_t *buffer_in);
void aci_att_find_by_type_value_resp_event_process(uint8_t *buffer_in);
void aci_att_read_by_type_resp_event_process(uint8_t *buffer_in);
void aci_att_read_resp_event_process(uint8_t *buffer_in);
void aci_att_read_blob_resp_event_process(uint8_t *buffer_in);
void aci_att_read_multiple_resp_event_process(uint8_t *buffer_in);
void aci_att_read_by_group_type_resp_event_process(uint8_t *buffer_in);
void aci_att_prepare_write_resp_event_process(uint8_t *buffer_in);
void aci_att_exec_write_resp_event_process(uint8_t *buffer_in);
void aci_gatt_indication_event_process(uint8_t *buffer_in);
void aci_gatt_notification_event_process(uint8_t *buffer_in);
void aci_gatt_proc_complete_event_process(uint8_t *buffer_in);
void aci_gatt_error_resp_event_process(uint8_t *buffer_in);
void aci_gatt_disc_read_char_by_uuid_resp_event_process(uint8_t *buffer_in);
void aci_gatt_write_permit_req_event_process(uint8_t *buffer_in);
void aci_gatt_read_permit_req_event_process(uint8_t *buffer_in);
void aci_gatt_read_multi_permit_req_event_process(uint8_t *buffer_in);
void aci_gatt_tx_pool_available_event_process(uint8_t *buffer_in);
void aci_gatt_server_confirmation_event_process(uint8_t *buffer_in);
void aci_gatt_prepare_write_permit_req_event_process(uint8_t *buffer_in);
void aci_gatt_indication_ext_event_process(uint8_t *buffer_in);
void aci_gatt_notification_ext_event_process(uint8_t *buffer_in);
void hci_le_connection_complete_event_process(uint8_t *buffer_in);
void hci_le_advertising_report_event_process(uint8_t *buffer_in);
void hci_le_connection_update_complete_event_process(uint8_t *buffer_in);
void hci_le_read_remote_used_features_complete_event_process(uint8_t *buffer_in);
void hci_le_long_term_key_request_event_process(uint8_t *buffer_in);
void hci_le_data_length_change_event_process(uint8_t *buffer_in);
void hci_le_read_local_p256_public_key_complete_event_process(uint8_t *buffer_in);
void hci_le_generate_dhkey_complete_event_process(uint8_t *buffer_in);
void hci_le_enhanced_connection_complete_event_process(uint8_t *buffer_in);
void hci_le_direct_advertising_report_event_process(uint8_t *buffer_in);
void hci_le_phy_update_complete_event_process(uint8_t *buffer_in);

const hci_event_table_t hci_event_table[HCI_EVENT_TABLE_SIZE] = 
{
  /* hci_disconnection_complete_event */
  0x0005, hci_disconnection_complete_event_process,
  /* hci_encryption_change_event */
  0x0008, hci_encryption_change_event_process,
  /* hci_read_remote_version_information_complete_event */
  0x000c, hci_read_remote_version_information_complete_event_process,
  /* hci_hardware_error_event */
  0x0010, hci_hardware_error_event_process,
  /* hci_number_of_completed_packets_event */
  0x0013, hci_number_of_completed_packets_event_process,
  /* hci_data_buffer_overflow_event */
  0x001a, hci_data_buffer_overflow_event_process,
  /* hci_encryption_key_refresh_complete_event */
  0x0030, hci_encryption_key_refresh_complete_event_process,
};

const hci_event_table_t hci_le_meta_event_table[HCI_LE_META_EVENT_TABLE_SIZE] = 
{
  /* hci_le_connection_complete_event */
  0x0001, hci_le_connection_complete_event_process,
  /* hci_le_advertising_report_event */
  0x0002, hci_le_advertising_report_event_process,
  /* hci_le_connection_update_complete_event */
  0x0003, hci_le_connection_update_complete_event_process,
  /* hci_le_read_remote_used_features_complete_event */
  0x0004, hci_le_read_remote_used_features_complete_event_process,
  /* hci_le_long_term_key_request_event */
  0x0005, hci_le_long_term_key_request_event_process,
  /* hci_le_data_length_change_event */
  0x0007, hci_le_data_length_change_event_process,
  /* hci_le_read_local_p256_public_key_complete_event */
  0x0008, hci_le_read_local_p256_public_key_complete_event_process,
  /* hci_le_generate_dhkey_complete_event */
  0x0009, hci_le_generate_dhkey_complete_event_process,
  /* hci_le_enhanced_connection_complete_event */
  0x000a, hci_le_enhanced_connection_complete_event_process,
  /* hci_le_direct_advertising_report_event */
  0x000b, hci_le_direct_advertising_report_event_process,
  /* hci_le_phy_update_complete_event */
  0x000c, hci_le_phy_update_complete_event_process,
};

const hci_event_table_t hci_vendor_specific_event_table[HCI_VENDOR_SPECIFIC_EVENT_TABLE_SIZE] = 
{
  /* aci_hal_end_of_radio_activity_event */
  0x0004, aci_hal_end_of_radio_activity_event_process,
  /* aci_hal_scan_req_report_event */
  0x0005, aci_hal_scan_req_report_event_process,
  /* aci_hal_fw_error_event */
  0x0006, aci_hal_fw_error_event_process,
  /* aci_gap_limited_discoverable_event */
  0x0400, aci_gap_limited_discoverable_event_process,
  /* aci_gap_pairing_complete_event */
  0x0401, aci_gap_pairing_complete_event_process,
  /* aci_gap_pass_key_req_event */
  0x0402, aci_gap_pass_key_req_event_process,
  /* aci_gap_authorization_req_event */
  0x0403, aci_gap_authorization_req_event_process,
  /* aci_gap_slave_security_initiated_event */
  0x0404, aci_gap_slave_security_initiated_event_process,
  /* aci_gap_bond_lost_event */
  0x0405, aci_gap_bond_lost_event_process,
  /* aci_gap_proc_complete_event */
  0x0407, aci_gap_proc_complete_event_process,
  /* aci_gap_addr_not_resolved_event */
  0x0408, aci_gap_addr_not_resolved_event_process,
  /* aci_gap_numeric_comparison_value_event */
  0x0409, aci_gap_numeric_comparison_value_event_process,
  /* aci_gap_keypress_notification_event */
  0x040a, aci_gap_keypress_notification_event_process,
  /* aci_l2cap_connection_update_resp_event */
  0x0800, aci_l2cap_connection_update_resp_event_process,
  /* aci_l2cap_proc_timeout_event */
  0x0801, aci_l2cap_proc_timeout_event_process,
  /* aci_l2cap_connection_update_req_event */
  0x0802, aci_l2cap_connection_update_req_event_process,
  /* aci_l2cap_command_reject_event */
  0x080a, aci_l2cap_command_reject_event_process,
  /* aci_gatt_attribute_modified_event */
  0x0c01, aci_gatt_attribute_modified_event_process,
  /* aci_gatt_proc_timeout_event */
  0x0c02, aci_gatt_proc_timeout_event_process,
  /* aci_att_exchange_mtu_resp_event */
  0x0c03, aci_att_exchange_mtu_resp_event_process,
  /* aci_att_find_info_resp_event */
  0x0c04, aci_att_find_info_resp_event_process,
  /* aci_att_find_by_type_value_resp_event */
  0x0c05, aci_att_find_by_type_value_resp_event_process,
  /* aci_att_read_by_type_resp_event */
  0x0c06, aci_att_read_by_type_resp_event_process,
  /* aci_att_read_resp_event */
  0x0c07, aci_att_read_resp_event_process,
  /* aci_att_read_blob_resp_event */
  0x0c08, aci_att_read_blob_resp_event_process,
  /* aci_att_read_multiple_resp_event */
  0x0c09, aci_att_read_multiple_resp_event_process,
  /* aci_att_read_by_group_type_resp_event */
  0x0c0a, aci_att_read_by_group_type_resp_event_process,
  /* aci_att_prepare_write_resp_event */
  0x0c0c, aci_att_prepare_write_resp_event_process,
  /* aci_att_exec_write_resp_event */
  0x0c0d, aci_att_exec_write_resp_event_process,
  /* aci_gatt_indication_event */
  0x0c0e, aci_gatt_indication_event_process,
  /* aci_gatt_notification_event */
  0x0c0f, aci_gatt_notification_event_process,
  /* aci_gatt_proc_complete_event */
  0x0c10, aci_gatt_proc_complete_event_process,
  /* aci_gatt_error_resp_event */
  0x0c11, aci_gatt_error_resp_event_process,
  /* aci_gatt_disc_read_char_by_uuid_resp_event */
  0x0c12, aci_gatt_disc_read_char_by_uuid_resp_event_process,
  /* aci_gatt_write_permit_req_event */
  0x0c13, aci_gatt_write_permit_req_event_process,
  /* aci_gatt_read_permit_req_event */
  0x0c14, aci_gatt_read_permit_req_event_process,
  /* aci_gatt_read_multi_permit_req_event */
  0x0c15, aci_gatt_read_multi_permit_req_event_process,
  /* aci_gatt_tx_pool_available_event */
  0x0c16, aci_gatt_tx_pool_available_event_process,
  /* aci_gatt_server_confirmation_event */
  0x0c17, aci_gatt_server_confirmation_event_process,
  /* aci_gatt_prepare_write_permit_req_event */
  0x0c18, aci_gatt_prepare_write_permit_req_event_process,
  /* aci_gatt_indication_ext_event */
  0x0c1e, aci_gatt_indication_ext_event_process,
  /* aci_gatt_notification_ext_event */
  0x0c1f, aci_gatt_notification_ext_event_process,
};

/* hci_disconnection_complete_event */
/* Event len: 1 + 2 + 1 */
/**
  * @brief The Disconnection Complete event occurs when a connection is terminated.
The status parameter indicates if the disconnection was successful or not. The
reason parameter indicates the reason for the disconnection if the disconnection
was successful. If the disconnection was not successful, the value of the
reason parameter can be ignored by the Host. For example, this can be the
case if the Host has issued the Disconnect command and there was a parameter
error, or the command was not presently allowed, or a Connection_Handle
that didn't correspond to a connection was given.
  * @param Status Status error code.
  * @param Connection_Handle Connection_Handle which was disconnected.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Reason Reason for disconnection.
  * Values:
  - 0x00: Success
  - 0x01: Unknown HCI Command
  - 0x02: Unknown Connection Identifier
  - 0x03: Hardware Failure
  - 0x05: Authentication Failure
  - 0x06: PIN or Key Missing
  - 0x07: Memory Capacity Exceeded
  - 0x08: Connection Timeout
  - 0x09: Connection Limit Exceeded
  - 0x0B: ACL Connection Already Exists
  - 0x0C: Command Disallowed
  - 0x0D: Connection Rejected Due To Limited Resources
  - 0x0E: Connection Rejected Due To Security Reasons
  - 0x0F: Connection Rejected due to Unacceptable BD_ADDR
  - 0x10: Connection Accept Timeout Exceeded
  - 0x11: Unsupported Feature Or Parameter Value
  - 0x12: Invalid HCI Command Parameters
  - 0x13: Remote User Terminated Connection
  - 0x14: Remote Device Terminated Connection due to Low Resources
  - 0x15: Remote Device Terminated Connection due to Power Off
  - 0x16: Connection Terminated By Local Host
  - 0x17: Repeated Attempts
  - 0x18: Pairing Not Allowed
  - 0x19: Unknown LMP PDU
  - 0x1A: Unsupported Remote Feature / Unsupported LMP Feature
  - 0x1E: Invalid LMP Parameters
  - 0x1F: Unspecified Error
  - 0x20: Unsupported LMP Parameter Value
  - 0x21: Role Change Not Allowed
  - 0x22: LMP Response Timeout / LL Response Timeout
  - 0x23: LMP Error Transaction Collision
  - 0x24: LMP PDU Not Allowed
  - 0x25: Encryption Mode Not Acceptable
  - 0x26: Link Key cannot be Changed
  - 0x28: Instant Passed
  - 0x29: Pairing With Unit Key Not Supported
  - 0x2A: Different Transaction Collision
  - 0x2E: Channel Assessment Not Supported
  - 0x2F: Insufficient Security
  - 0x30: Parameter Out Of Mandatory Range
  - 0x32: Role Switch Pending
  - 0x34: Reserved Slot Violation
  - 0x35: Role Switch Failed
  - 0x37: Secure Simple Pairing Not Supported by Host
  - 0x38: Host Busy - Pairing
  - 0x39: Connection Rejected due to No Suitable Channel Found
  - 0x3A: Controller Busy
  - 0x3B: Unacceptable Connection Interval
  - 0x3C: Directed Advertising Timeout
  - 0x3D: Connection Terminated Due to MIC Failure
  - 0x3E: Connection Failed to be Established
  - 0x41: Failed
  - 0x42: Invalid parameters
  - 0x43: Busy
  - 0x44: Invalid length
  - 0x45: Pending
  - 0x46: Not allowed
  - 0x47: GATT error
  - 0x48: Address not resolved
  - 0x50: Invalid CID
  - 0x5A: CSRK not found
  - 0x5B: IRK not found
  - 0x5C: Device not found in DB
  - 0x5D: Security DB full
  - 0x5E: Device not bonded
  - 0x5F: Device in blacklist
  - 0x60: Invalid handle
  - 0x61: Invalid parameter
  - 0x62: Out of handles
  - 0x63: Invalid operation
  - 0x64: Insufficient resources
  - 0x65: Insufficient encryption key size
  - 0x66: Characteristic already exist
  - 0x82: No valid slot
  - 0x83: Short window
  - 0x84: New interval failed
  - 0x85: Too large interval
  - 0x86: Slot length failed
  * @retval None
*/

void hci_disconnection_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_disconnection_complete_event_rp0 *rp0 = (hci_disconnection_complete_event_rp0 *)buffer_in;
  hci_disconnection_complete_event(rp0->Status,
                                   rp0->Connection_Handle,
                                   rp0->Reason);
}

/* hci_encryption_change_event */
/* Event len: 1 + 2 + 1 */
/**
  * @brief The Encryption Change event is used to indicate that the change of the encryption
mode has been completed. The Connection_Handle will be a Connection_Handle
for an ACL connection. The Encryption_Enabled event parameter
specifies the new Encryption_Enabled parameter for the Connection_Handle
specified by the Connection_Handle event parameter. This event will occur on
both devices to notify the Hosts when Encryption has changed for the specified
Connection_Handle between two devices. Note: This event shall not be generated
if encryption is paused or resumed; during a role switch, for example.
The meaning of the Encryption_Enabled parameter depends on whether the
Host has indicated support for Secure Connections in the Secure_Connections_Host_Support
parameter. When Secure_Connections_Host_Support is
'disabled' or the Connection_Handle refers to an LE link, the Controller shall
only use Encryption_Enabled values 0x00 (OFF) and 0x01 (ON).
(See Bluetooth Specification v.5.0, Vol. 2, Part E, 7.7.8)
  * @param Status Status error code.
  * @param Connection_Handle Connection handle for which the command is given.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Encryption_Enabled Link Level Encryption.
  * Values:
  - 0x00: Link Level Encryption OFF
  - 0x01: Link Level Encryption is ON with AES-CCM
  * @retval None
*/

void hci_encryption_change_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_encryption_change_event_rp0 *rp0 = (hci_encryption_change_event_rp0 *)buffer_in;
  hci_encryption_change_event(rp0->Status,
                              rp0->Connection_Handle,
                              rp0->Encryption_Enabled);
}

/* hci_read_remote_version_information_complete_event */
/* Event len: 1 + 2 + 1 + 2 + 2 */
/**
  * @brief The Read Remote Version Information Complete event is used to indicate the
completion of the process obtaining the version information of the remote Controller
specified by the Connection_Handle event parameter. The Connection_Handle
shall be for an ACL connection.
The Version event parameter defines the specification version of the LE Controller.
The Manufacturer_Name event parameter indicates the manufacturer
of the remote Controller. The Subversion event parameter is controlled
by the manufacturer and is implementation dependent. The Subversion
event parameter defines the various revisions that each version of the Bluetooth
hardware will go through as design processes change and errors are
fixed. This allows the software to determine what Bluetooth hardware is being
used and, if necessary, to work around various bugs in the hardware.
When the Connection_Handle is associated with an LE-U logical link, the Version
event parameter shall be Link Layer VersNr parameter, the Manufacturer_Name
event parameter shall be the CompId parameter, and the Subversion
event parameter shall be the SubVersNr parameter.
(See Bluetooth Specification v.5.0, Vol. 2, Part E, 7.7.12)
  * @param Status Status error code.
  * @param Connection_Handle Connection handle for which the command is given.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Version Version of the Current LMP in the remote Controller
  * @param Manufacturer_Name Manufacturer Name of the remote Controller
  * @param Subversion Subversion of the LMP in the remote Controller
  * @retval None
*/

void hci_read_remote_version_information_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_read_remote_version_information_complete_event_rp0 *rp0 = (hci_read_remote_version_information_complete_event_rp0 *)buffer_in;
  hci_read_remote_version_information_complete_event(rp0->Status,
                                                     rp0->Connection_Handle,
                                                     rp0->Version,
                                                     rp0->Manufacturer_Name,
                                                     rp0->Subversion);
}

/* hci_hardware_error_event */
/* Event len: 1 */
/**
  * @brief The Hardware Error event is used to indicate some implementation specific type of hardware failure for the controller. This event is used to notify the Host that a hardware failure has occurred in the Controller.
  * @param Hardware_Code Hardware Error Event code.
Error code 0 is not used.
Error code 1 is bluecore act2 error detected.
Error code 2 is bluecore time overrun error detected.
Error code 3 is internal FIFO full.
  * Values:
  - 0x00: Not used
  - 0x01: event_act2 error
  - 0x02: event_time_overrun error
  - 0x03: event_fifo_full error
  * @retval None
*/

void hci_hardware_error_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_hardware_error_event_rp0 *rp0 = (hci_hardware_error_event_rp0 *)buffer_in;
  hci_hardware_error_event(rp0->Hardware_Code);
}

/* hci_number_of_completed_packets_event */
/* Event len: 1 + rp0->Number_of_Handles * (sizeof(Handle_Packets_Pair_Entry_t)) */
/**
  * @brief 'The Number Of Completed Packets event is used by the Controller to indicate
to the Host how many HCI Data Packets have been completed (transmitted or
flushed) for each Connection_Handle since the previous Number Of Completed
Packets event was sent to the Host. This means that the corresponding
buffer space has been freed in the Controller. Based on this information, and
the HC_Total_Num_ACL_Data_Packets and HC_Total_Num_Synchronous_-
Data_Packets return parameter of the Read_Buffer_Size command, the Host
can determine for which Connection_Handles the following HCI Data Packets
should be sent to the Controller. The Number Of Completed Packets event
must not be sent before the corresponding Connection Complete event. While
the Controller has HCI data packets in its buffer, it must keep sending the Number
Of Completed Packets event to the Host at least periodically, until it finally
reports that all the pending ACL Data Packets have been transmitted or
flushed.
  * @param Number_of_Handles The number of Connection_Handles and Num_HCI_Data_Packets parameters pairs contained in this event
  * @param Handle_Packets_Pair_Entry See @ref Handle_Packets_Pair_Entry_t
  * @retval None
*/

void hci_number_of_completed_packets_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_number_of_completed_packets_event_rp0 *rp0 = (hci_number_of_completed_packets_event_rp0 *)buffer_in;
  hci_number_of_completed_packets_event(rp0->Number_of_Handles,
                                        rp0->Handle_Packets_Pair_Entry);
}

/* hci_data_buffer_overflow_event */
/* Event len: 1 */
/**
  * @brief 'This event is used to indicate that the Controller's data buffers have been overflowed.
This can occur if the Host has sent more packets than allowed. The
Link_Type parameter is used to indicate that the overflow was caused by ACL data.
  * @param Link_Type On wich type of channel overflow has occurred.
  * Values:
  - 0x01: ACL Buffer Overflow
  * @retval None
*/

void hci_data_buffer_overflow_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_data_buffer_overflow_event_rp0 *rp0 = (hci_data_buffer_overflow_event_rp0 *)buffer_in;
  hci_data_buffer_overflow_event(rp0->Link_Type);
}

/* hci_encryption_key_refresh_complete_event */
/* Event len: 1 + 2 */
/**
  * @brief 'The Encryption Key Refresh Complete event is used to indicate to the Host
that the encryption key was refreshed on the given Connection_Handle any
time encryption is paused and then resumed.
If the Encryption Key Refresh Complete event was generated due to an
encryption pause and resume operation embedded within a change connection
link key procedure, the Encryption Key Refresh Complete event shall be sent
prior to the Change Connection Link Key Complete event.
If the Encryption Key Refresh Complete event was generated due to an
encryption pause and resume operation embedded within a role switch procedure,
the Encryption Key Refresh Complete event shall be sent prior to the
Role Change event.
  * @param Status Status error code.
  * @param Connection_Handle Connection handle for which the command is given.
  * Values:
  - 0x0000 ... 0x0EFF
  * @retval None
*/

void hci_encryption_key_refresh_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_encryption_key_refresh_complete_event_rp0 *rp0 = (hci_encryption_key_refresh_complete_event_rp0 *)buffer_in;
  hci_encryption_key_refresh_complete_event(rp0->Status,
                                            rp0->Connection_Handle);
}

/* aci_hal_end_of_radio_activity_event */
/* Event len: 1 + 1 + 4 */
/**
  * @brief 'This event is generated when the device completes a radio activity and provide information when a new radio acitivity will be performed.
Informtation provided includes type of radio activity and absolute time in system ticks when a new radio acitivity is schedule, if any. Application can use this information to schedule user activities synchronous to selected radio activitities. A command @ref aci_hal_set_radio_activity_mask is provided to enable radio activity events of user interests, by default no events are enabled.
User should take into account that enablinng radio events in application with intense radio activity could lead to a fairly high rate of events generated.
Application use cases includes synchronizing notification with connection interval, switiching antenna at the end of advertising or performing flash erase operation while radio is idle.
  * @param Last_State Completed radio events
  * Values:
  - 0x00: Idle
  - 0x01: Advertising
  - 0x02: Connection event slave
  - 0x03: Scanning
  - 0x04: Connection request
  - 0x05: Connection event slave
  - 0x06: TX test mode
  - 0x07: RX test mode
  * @param Next_State Incoming radio events
  * Values:
  - 0x00: Idle
  - 0x01: Advertising
  - 0x02: Connection event slave
  - 0x03: Scanning
  - 0x04: Connection request
  - 0x05: Connection event slave
  - 0x06: TX test mode
  - 0x07: RX test mode
  * @param Next_State_SysTime 32bit absolute current time expressed in internal time units.
  * @retval None
*/

void aci_hal_end_of_radio_activity_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_hal_end_of_radio_activity_event_rp0 *rp0 = (aci_hal_end_of_radio_activity_event_rp0 *)buffer_in;
  aci_hal_end_of_radio_activity_event(rp0->Last_State,
                                      rp0->Next_State,
                                      rp0->Next_State_SysTime);
}

/* aci_hal_scan_req_report_event */
/* Event len: 1 + 1 + 6 */
/**
  * @brief This event is reported to the application after a scan request is received and a scan reponse
is scheduled to be transmitted.
  * @param RSSI N Size: 1 Octet (signed integer)
Units: dBm
  * Values:
  - 127: RSSI not available
  - -127 ... 20
  * @param Peer_Address_Type 0x00 Public Device Address
0x01 Random Device Address
0x02 Public Identity Address (Corresponds to Resolved Private Address)
0x03 Random (Static) Identity Address (Corresponds to Resolved Private Address)
  * Values:
  - 0x00: Public Device Address
  - 0x01: Random Device Address
  - 0x02: Public Identity Address
  - 0x03: Random (Static) Identity Address
  * @param Peer_Address Public Device Address or Random Device Address of the peer device
  * @retval None
*/

void aci_hal_scan_req_report_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_hal_scan_req_report_event_rp0 *rp0 = (aci_hal_scan_req_report_event_rp0 *)buffer_in;
  aci_hal_scan_req_report_event(rp0->RSSI,
                                rp0->Peer_Address_Type,
                                rp0->Peer_Address);
}

/* aci_hal_fw_error_event */
/* Event len: 1 + 1 + rp0->Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated to report firmware error informations.
  * @param FW_Error_Type FW Error type
  * Values:
  - 0x01: L2CAP recombination failure
  - 0x02: GATT UNEXPECTED RESPONSE ERROR
  - 0x03: NVM LEVEL WARNING
  * @param Data_Length Length of Data in octets
  * @param Data The error event info
  * @retval None
*/

void aci_hal_fw_error_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_hal_fw_error_event_rp0 *rp0 = (aci_hal_fw_error_event_rp0 *)buffer_in;
  aci_hal_fw_error_event(rp0->FW_Error_Type,
                         rp0->Data_Length,
                         rp0->Data);
}

/* aci_gap_limited_discoverable_event */
/* Event len: 0 */
/**
  * @brief This event is generated by the controller when the limited discoverable mode ends due to
timeout. The timeout is 180 seconds.
  * @retval None
*/

void aci_gap_limited_discoverable_event_process(uint8_t *buffer_in)
{
  aci_gap_limited_discoverable_event();
}

/* aci_gap_pairing_complete_event */
/* Event len: 2 + 1 + 1 */
/**
  * @brief This event is generated when the pairing process has completed successfully or a pairing
procedure timeout has occurred or the pairing has failed. This is to notify the application that
we have paired with a remote device so that it can take further actions or to notify that a
timeout has occurred so that the upper layer can decide to disconnect the link.
  * @param Connection_Handle Connection handle on which the pairing procedure completed
  * @param Status Pairing status
  * Values:
  - 0x00: Success
  - 0x01: Timeout
  - 0x02: Failed
  * @param Reason Pairing reason error code
  * Values:
  - 0x00
  - 0x01: PASSKEY_ENTRY_FAILED
  - 0x02: OOB_NOT_AVAILABLE
  - 0x03: AUTH_REQ_CANNOT_BE_MET
  - 0x04: CONFIRM_VALUE_FAILED
  - 0x05: PAIRING_NOT_SUPPORTED
  - 0x06: INSUFF_ENCRYPTION_KEY_SIZE
  - 0x07: CMD_NOT_SUPPORTED
  - 0x08: UNSPECIFIED_REASON
  - 0x09: VERY_EARLY_NEXT_ATTEMPT
  - 0x0A: SM_INVALID_PARAMS
  - 0x0B: SMP_SC_DHKEY_CHECK_FAILED
  - 0x0C: SMP_SC_NUMCOMPARISON_FAILED
  * @retval None
*/

void aci_gap_pairing_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gap_pairing_complete_event_rp0 *rp0 = (aci_gap_pairing_complete_event_rp0 *)buffer_in;
  aci_gap_pairing_complete_event(rp0->Connection_Handle,
                                 rp0->Status,
                                 rp0->Reason);
}

/* aci_gap_pass_key_req_event */
/* Event len: 2 */
/**
  * @brief This event is generated by the Security manager to the application when a passkey is
required for pairing. When this event is received, the application has to respond with the
@ref aci_gap_pass_key_resp command.
  * @param Connection_Handle Connection handle for which the passkey has been requested.
  * @retval None
*/

void aci_gap_pass_key_req_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gap_pass_key_req_event_rp0 *rp0 = (aci_gap_pass_key_req_event_rp0 *)buffer_in;
  aci_gap_pass_key_req_event(rp0->Connection_Handle);
}

/* aci_gap_authorization_req_event */
/* Event len: 2 */
/**
  * @brief This event is generated by the Security manager to the application when the application has
set that authorization is required for reading/writing of attributes. This event will be
generated as soon as the pairing is complete. When this event is received,
@ref aci_gap_authorization_resp command should be used to respond by the application.
  * @param Connection_Handle Connection handle for which authorization has been requested.
  * @retval None
*/

void aci_gap_authorization_req_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gap_authorization_req_event_rp0 *rp0 = (aci_gap_authorization_req_event_rp0 *)buffer_in;
  aci_gap_authorization_req_event(rp0->Connection_Handle);
}

/* aci_gap_slave_security_initiated_event */
/* Event len: 0 */
/**
  * @brief This event is generated when the slave security request is successfully sent to the master.
  * @retval None
*/

void aci_gap_slave_security_initiated_event_process(uint8_t *buffer_in)
{
  aci_gap_slave_security_initiated_event();
}

/* aci_gap_bond_lost_event */
/* Event len: 0 */
/**
  * @brief This event is generated when a pairing request is issued in response to a slave security
request from a master which has previously bonded with the slave. When this event is
received, the upper layer has to issue the command @ref aci_gap_allow_rebond in order to
allow the slave to continue the pairing process with the master.
  * @retval None
*/

void aci_gap_bond_lost_event_process(uint8_t *buffer_in)
{
  aci_gap_bond_lost_event();
}

/* aci_gap_proc_complete_event */
/* Event len: 1 + 1 + 1 + rp0->Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is sent by the GAP to the upper layers when a procedure previously started has
been terminated by the upper layer or has completed for any other reason
  * @param Procedure_Code Terminated procedure.
  * Values:
  - 0x01: GAP_LIMITED_DISCOVERY_PROC
  - 0x02: GAP_GENERAL_DISCOVERY_PROC
  - 0x04: GAP_NAME_DISCOVERY_PROC
  - 0x08: GAP_AUTO_CONNECTION_ESTABLISHMENT_PROC
  - 0x10: GAP_GENERAL_CONNECTION_ESTABLISHMENT_PROC
  - 0x20: GAP_SELECTIVE_CONNECTION_ESTABLISHMENT_PROC
  - 0x40: GAP_DIRECT_CONNECTION_ESTABLISHMENT_PROC
  - 0x80: GAP_OBSERVATION_PROC
  * @param Status Status error code.
  * @param Data_Length Length of Data in octets
  * @param Data Procedure Specific Data:
- For Name Discovery Procedure: the name of the peer device if the procedure completed successfully.
  * @retval None
*/

void aci_gap_proc_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gap_proc_complete_event_rp0 *rp0 = (aci_gap_proc_complete_event_rp0 *)buffer_in;
  aci_gap_proc_complete_event(rp0->Procedure_Code,
                              rp0->Status,
                              rp0->Data_Length,
                              rp0->Data);
}

/* aci_gap_addr_not_resolved_event */
/* Event len: 2 */
/**
  * @brief This event is sent only by a privacy enabled Peripheral. The event is sent to the
upper layers when the peripheral is unsuccessful in resolving the resolvable
address of the peer device after connecting to it.
  * @param Connection_Handle Connection handle for which the private address could not be
resolved with any of the stored IRK's.
  * @retval None
*/

void aci_gap_addr_not_resolved_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gap_addr_not_resolved_event_rp0 *rp0 = (aci_gap_addr_not_resolved_event_rp0 *)buffer_in;
  aci_gap_addr_not_resolved_event(rp0->Connection_Handle);
}

/* aci_gap_numeric_comparison_value_event */
/* Event len: 2 + 4 */
/**
  * @brief This event is sent only during SC v.4.2 Pairing, when Numeric Comparison Association model is selected, in order to show the Numeric Value generated, and to ask for Confirmation to the User. When this event is received, the application has to respond with the
@ref aci_gap_numeric_comparison_resp command.
  * @param Connection_Handle Connection handle related to the underlying Pairing
  * @param Numeric_Value 
  * @retval None
*/

void aci_gap_numeric_comparison_value_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gap_numeric_comparison_value_event_rp0 *rp0 = (aci_gap_numeric_comparison_value_event_rp0 *)buffer_in;
  aci_gap_numeric_comparison_value_event(rp0->Connection_Handle,
                                         rp0->Numeric_Value);
}

/* aci_gap_keypress_notification_event */
/* Event len: 2 + 1 */
/**
  * @brief This event is sent only during SC v.4.2 Pairing, when Keypress Notifications are supported, in order to show the input type signalled by the peer device, having Keyboard only I/O capabilities. When this event is received, no action is required to the User.
  * @param Connection_Handle Connection handle related to the underlying Pairing
  * @param Notification_Type Type of Keypress input notified/signaled by peer device (having Keyboard only I/O capabilities
  * @retval None
*/

void aci_gap_keypress_notification_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gap_keypress_notification_event_rp0 *rp0 = (aci_gap_keypress_notification_event_rp0 *)buffer_in;
  aci_gap_keypress_notification_event(rp0->Connection_Handle,
                                      rp0->Notification_Type);
}

/* aci_l2cap_connection_update_resp_event */
/* Event len: 2 + 2 */
/**
  * @brief This event is generated when the master responds to the connection update request packet
with a connection update response packet.
  * @param Connection_Handle Connection handle referring to the COS Channel where the Disconnection has been received.
  * @param Result 
  * @retval None
*/

void aci_l2cap_connection_update_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_l2cap_connection_update_resp_event_rp0 *rp0 = (aci_l2cap_connection_update_resp_event_rp0 *)buffer_in;
  aci_l2cap_connection_update_resp_event(rp0->Connection_Handle,
                                         rp0->Result);
}

/* aci_l2cap_proc_timeout_event */
/* Event len: 2 + 1 + rp0->Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated when the master does not respond to the connection update
request packet with a connection update response packet or a command reject packet
within 30 seconds.
  * @param Connection_Handle Handle of the connection related to this L2CAP procedure.
  * @param Data_Length Length of following data
  * @param Data 
  * @retval None
*/

void aci_l2cap_proc_timeout_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_l2cap_proc_timeout_event_rp0 *rp0 = (aci_l2cap_proc_timeout_event_rp0 *)buffer_in;
  aci_l2cap_proc_timeout_event(rp0->Connection_Handle,
                               rp0->Data_Length,
                               rp0->Data);
}

/* aci_l2cap_connection_update_req_event */
/* Event len: 2 + 1 + 2 + 2 + 2 + 2 + 2 */
/**
  * @brief The event is given by the L2CAP layer when a connection update request is received from
the slave. The upper layer which receives this event has to respond by sending a
@ref aci_l2cap_connection_parameter_update_resp command.
  * @param Connection_Handle Handle of the connection related to this L2CAP procedure.
  * @param Identifier This is the identifier which associate the request to the response.
  * @param L2CAP_Length Length of the L2CAP connection update request.
  * @param Interval_Min Minimum value for the connection event interval. This shall be less
than or equal to Conn_Interval_Max.
Time = N * 1.25 msec.
  * Values:
  - 0x0006 (7.50 ms)  ... 0x0C80 (4000.00 ms) 
  * @param Interval_Max Maximum value for the connection event interval. This shall be
greater than or equal to Conn_Interval_Min.
Time = N * 1.25 msec.
  * Values:
  - 0x0006 (7.50 ms)  ... 0x0C80 (4000.00 ms) 
  * @param Slave_Latency Slave latency for the connection in number of connection events.
  * Values:
  - 0x0000 ... 0x01F3
  * @param Timeout_Multiplier Defines connection timeout parameter in the following manner: Timeout Multiplier * 10ms.
  * @retval None
*/

void aci_l2cap_connection_update_req_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_l2cap_connection_update_req_event_rp0 *rp0 = (aci_l2cap_connection_update_req_event_rp0 *)buffer_in;
  aci_l2cap_connection_update_req_event(rp0->Connection_Handle,
                                        rp0->Identifier,
                                        rp0->L2CAP_Length,
                                        rp0->Interval_Min,
                                        rp0->Interval_Max,
                                        rp0->Slave_Latency,
                                        rp0->Timeout_Multiplier);
}

/* aci_l2cap_command_reject_event */
/* Event len: 2 + 1 + 2 + 1 + rp0->Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated when the master responds to the connection update request packet
with a command reject packet.
  * @param Connection_Handle Connection handle referring to the COS Channel where the Disconnection has been received.
  * @param Identifier This is the identifier which associate the request to the response.
  * @param Reason Reason
  * @param Data_Length Length of following data
  * @param Data Data field associated with Reason
  * @retval None
*/

void aci_l2cap_command_reject_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_l2cap_command_reject_event_rp0 *rp0 = (aci_l2cap_command_reject_event_rp0 *)buffer_in;
  aci_l2cap_command_reject_event(rp0->Connection_Handle,
                                 rp0->Identifier,
                                 rp0->Reason,
                                 rp0->Data_Length,
                                 rp0->Data);
}

/* aci_gatt_attribute_modified_event */
/* Event len: 2 + 2 + 2 + 2 + rp0->Attr_Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated to the application by the GATT server when a client modifies any
attribute on the server, as consequence of one of the following GATT procedures:
- write without response
- signed write without response
- write characteristic value
- write long characteristic value
- reliable write.
  * @param Connection_Handle The connection handle which modified the attribute.
  * @param Attr_Handle Handle of the attribute that was modified.
  * @param Offset Bits 14-0: offset from which the write has been performed by the peer device. Bit 15 is used as flag: when set to 1 it indicates that more data are to come (fragmented event in case of long attribute data).
  * @param Attr_Data_Length Length of Attr_Data in octets
  * @param Attr_Data The modified value
  * @retval None
*/

void aci_gatt_attribute_modified_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_attribute_modified_event_rp0 *rp0 = (aci_gatt_attribute_modified_event_rp0 *)buffer_in;
  aci_gatt_attribute_modified_event(rp0->Connection_Handle,
                                    rp0->Attr_Handle,
                                    rp0->Offset,
                                    rp0->Attr_Data_Length,
                                    rp0->Attr_Data);
}

/* aci_gatt_proc_timeout_event */
/* Event len: 2 */
/**
  * @brief This event is generated by the client/server to the application on a GATT timeout (30
seconds). This is a critical event that should not happen during normal operating conditions. It is an indication of either a major disruption in the communication link or a mistake in the application which does not provide a reply to GATT procedures. After this event, the GATT channel is closed and no more GATT communication can be performed. The applications is exptected to issue an @ref aci_gap_terminate to disconnect from the peer device. It is important to leave an 100 ms blank window before sending the @ref aci_gap_terminate, since immediately after this event, system could save important information in non volatile memory.
  * @param Connection_Handle Connection handle on which the GATT procedure has timed out
  * @retval None
*/

void aci_gatt_proc_timeout_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_proc_timeout_event_rp0 *rp0 = (aci_gatt_proc_timeout_event_rp0 *)buffer_in;
  aci_gatt_proc_timeout_event(rp0->Connection_Handle);
}

/* aci_att_exchange_mtu_resp_event */
/* Event len: 2 + 2 */
/**
  * @brief This event is generated in response to an Exchange MTU request. See
@ref aci_gatt_exchange_config.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Server_RX_MTU Attribute server receive MTU size
  * @retval None
*/

void aci_att_exchange_mtu_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_att_exchange_mtu_resp_event_rp0 *rp0 = (aci_att_exchange_mtu_resp_event_rp0 *)buffer_in;
  aci_att_exchange_mtu_resp_event(rp0->Connection_Handle,
                                  rp0->Server_RX_MTU);
}

/* aci_att_find_info_resp_event */
/* Event len: 2 + 1 + 1 + rp0->Event_Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated in response to a Find Information Request. See
@ref aci_att_find_info_req and Find Information Response in Bluetooth Core v5.0
spec. This event is also generated in response to @ref aci_gatt_disc_all_char_desc
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Format Format of the hanndle-uuid pairs
  * @param Event_Data_Length Length of Handle_UUID_Pair in octets
  * @param Handle_UUID_Pair A sequence of handle-uuid pairs. if format=1, each pair is:[2 octets for handle, 2 octets for UUIDs], if format=2, each pair is:[2 octets for handle, 16 octets for UUIDs]
  * @retval None
*/

void aci_att_find_info_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_att_find_info_resp_event_rp0 *rp0 = (aci_att_find_info_resp_event_rp0 *)buffer_in;
  aci_att_find_info_resp_event(rp0->Connection_Handle,
                               rp0->Format,
                               rp0->Event_Data_Length,
                               rp0->Handle_UUID_Pair);
}

/* aci_att_find_by_type_value_resp_event */
/* Event len: 2 + 1 + rp0->Num_of_Handle_Pair * (sizeof(Attribute_Group_Handle_Pair_t)) */
/**
  * @brief This event is generated in response to a @ref aci_att_find_by_type_value_req
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Num_of_Handle_Pair Number of attribute, group handle pairs
  * @param Attribute_Group_Handle_Pair See @ref Attribute_Group_Handle_Pair_t
  * @retval None
*/

void aci_att_find_by_type_value_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_att_find_by_type_value_resp_event_rp0 *rp0 = (aci_att_find_by_type_value_resp_event_rp0 *)buffer_in;
  aci_att_find_by_type_value_resp_event(rp0->Connection_Handle,
                                        rp0->Num_of_Handle_Pair,
                                        rp0->Attribute_Group_Handle_Pair);
}

/* aci_att_read_by_type_resp_event */
/* Event len: 2 + 1 + 1 + rp0->Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated in response to a @ref aci_att_read_by_type_req. See
@ref aci_gatt_find_included_services and @ref aci_gatt_disc_all_char_desc.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Handle_Value_Pair_Length The size of each attribute handle-value pair
  * @param Data_Length Length of Handle_Value_Pair_Data in octets
  * @param Handle_Value_Pair_Data Attribute Data List as defined in Bluetooth Core v5.0 spec. A sequence of handle-value pairs: [2 octets for Attribute Handle, (Handle_Value_Pair_Length - 2 octets) for Attribute Value]
  * @retval None
*/

void aci_att_read_by_type_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_att_read_by_type_resp_event_rp0 *rp0 = (aci_att_read_by_type_resp_event_rp0 *)buffer_in;
  aci_att_read_by_type_resp_event(rp0->Connection_Handle,
                                  rp0->Handle_Value_Pair_Length,
                                  rp0->Data_Length,
                                  rp0->Handle_Value_Pair_Data);
}

/* aci_att_read_resp_event */
/* Event len: 2 + 1 + rp0->Event_Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated in response to a Read Request. See @ref aci_gatt_read_char_value.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Event_Data_Length Length of following data
  * @param Attribute_Value The value of the attribute.
  * @retval None
*/

void aci_att_read_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_att_read_resp_event_rp0 *rp0 = (aci_att_read_resp_event_rp0 *)buffer_in;
  aci_att_read_resp_event(rp0->Connection_Handle,
                          rp0->Event_Data_Length,
                          rp0->Attribute_Value);
}

/* aci_att_read_blob_resp_event */
/* Event len: 2 + 1 + rp0->Event_Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event can be generated during a read long characteristic value procedure. See @ref aci_gatt_read_long_char_value.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Event_Data_Length Length of following data
  * @param Attribute_Value Part of the attribute value.
  * @retval None
*/

void aci_att_read_blob_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_att_read_blob_resp_event_rp0 *rp0 = (aci_att_read_blob_resp_event_rp0 *)buffer_in;
  aci_att_read_blob_resp_event(rp0->Connection_Handle,
                               rp0->Event_Data_Length,
                               rp0->Attribute_Value);
}

/* aci_att_read_multiple_resp_event */
/* Event len: 2 + 1 + rp0->Event_Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated in response to a Read Multiple Request.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Event_Data_Length Length of following data
  * @param Set_Of_Values A set of two or more values.
A concatenation of attribute values for each of the attribute handles in the request in the order that they were requested.
  * @retval None
*/

void aci_att_read_multiple_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_att_read_multiple_resp_event_rp0 *rp0 = (aci_att_read_multiple_resp_event_rp0 *)buffer_in;
  aci_att_read_multiple_resp_event(rp0->Connection_Handle,
                                   rp0->Event_Data_Length,
                                   rp0->Set_Of_Values);
}

/* aci_att_read_by_group_type_resp_event */
/* Event len: 2 + 1 + 1 + rp0->Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated in response to a Read By Group Type Request. See
@ref aci_gatt_disc_all_primary_services.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Attribute_Data_Length The size of each attribute data
  * @param Data_Length Length of Attribute_Data_List in octets
  * @param Attribute_Data_List Attribute Data List as defined in Bluetooth Core v5.0 spec. A sequence of attribute handle, end group handle, attribute value tuples: [2 octets for Attribute Handle, 2 octets End Group Handle, (Attribute_Data_Length - 4 octets) for Attribute Value]
  * @retval None
*/

void aci_att_read_by_group_type_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_att_read_by_group_type_resp_event_rp0 *rp0 = (aci_att_read_by_group_type_resp_event_rp0 *)buffer_in;
  aci_att_read_by_group_type_resp_event(rp0->Connection_Handle,
                                        rp0->Attribute_Data_Length,
                                        rp0->Data_Length,
                                        rp0->Attribute_Data_List);
}

/* aci_att_prepare_write_resp_event */
/* Event len: 2 + 2 + 2 + 1 + rp0->Part_Attribute_Value_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated in response to a @ref aci_att_prepare_write_req.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Attribute_Handle The handle of the attribute to be written
  * @param Offset The offset of the first octet to be written.
  * @param Part_Attribute_Value_Length Length of Part_Attribute_Value in octets
  * @param Part_Attribute_Value The value of the attribute to be written
  * @retval None
*/

void aci_att_prepare_write_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_att_prepare_write_resp_event_rp0 *rp0 = (aci_att_prepare_write_resp_event_rp0 *)buffer_in;
  aci_att_prepare_write_resp_event(rp0->Connection_Handle,
                                   rp0->Attribute_Handle,
                                   rp0->Offset,
                                   rp0->Part_Attribute_Value_Length,
                                   rp0->Part_Attribute_Value);
}

/* aci_att_exec_write_resp_event */
/* Event len: 2 */
/**
  * @brief This event is generated in response to an Execute Write Request.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @retval None
*/

void aci_att_exec_write_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_att_exec_write_resp_event_rp0 *rp0 = (aci_att_exec_write_resp_event_rp0 *)buffer_in;
  aci_att_exec_write_resp_event(rp0->Connection_Handle);
}

/* aci_gatt_indication_event */
/* Event len: 2 + 2 + 1 + rp0->Attribute_Value_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated when an indication is received from the server.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Attribute_Handle The handle of the attribute
  * @param Attribute_Value_Length Length of Attribute_Value in octets
  * @param Attribute_Value The current value of the attribute
  * @retval None
*/

void aci_gatt_indication_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_indication_event_rp0 *rp0 = (aci_gatt_indication_event_rp0 *)buffer_in;
  aci_gatt_indication_event(rp0->Connection_Handle,
                            rp0->Attribute_Handle,
                            rp0->Attribute_Value_Length,
                            rp0->Attribute_Value);
}

/* aci_gatt_notification_event */
/* Event len: 2 + 2 + 1 + rp0->Attribute_Value_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is generated when a notification is received from the server.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Attribute_Handle The handle of the attribute
  * @param Attribute_Value_Length Length of Attribute_Value in octets
  * @param Attribute_Value The current value of the attribute
  * @retval None
*/

void aci_gatt_notification_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_notification_event_rp0 *rp0 = (aci_gatt_notification_event_rp0 *)buffer_in;
  aci_gatt_notification_event(rp0->Connection_Handle,
                              rp0->Attribute_Handle,
                              rp0->Attribute_Value_Length,
                              rp0->Attribute_Value);
}

/* aci_gatt_proc_complete_event */
/* Event len: 2 + 1 */
/**
  * @brief This event is generated when a GATT client procedure completes either with error or
successfully.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Error_Code Indicates whether the procedure completed with an error or was successful
  * Values:
  - 0x00: Success
  - 0x01: Unknown HCI Command
  - 0x02: Unknown Connection Identifier
  - 0x03: Hardware Failure
  - 0x05: Authentication Failure
  - 0x06: PIN or Key Missing
  - 0x07: Memory Capacity Exceeded
  - 0x08: Connection Timeout
  - 0x09: Connection Limit Exceeded
  - 0x0B: ACL Connection Already Exists
  - 0x0C: Command Disallowed
  - 0x0D: Connection Rejected Due To Limited Resources
  - 0x0E: Connection Rejected Due To Security Reasons
  - 0x0F: Connection Rejected due to Unacceptable BD_ADDR
  - 0x10: Connection Accept Timeout Exceeded
  - 0x11: Unsupported Feature Or Parameter Value
  - 0x12: Invalid HCI Command Parameters
  - 0x13: Remote User Terminated Connection
  - 0x14: Remote Device Terminated Connection due to Low Resources
  - 0x15: Remote Device Terminated Connection due to Power Off
  - 0x16: Connection Terminated By Local Host
  - 0x17: Repeated Attempts
  - 0x18: Pairing Not Allowed
  - 0x19: Unknown LMP PDU
  - 0x1A: Unsupported Remote Feature / Unsupported LMP Feature
  - 0x1E: Invalid LMP Parameters
  - 0x1F: Unspecified Error
  - 0x20: Unsupported LMP Parameter Value
  - 0x21: Role Change Not Allowed
  - 0x22: LMP Response Timeout / LL Response Timeout
  - 0x23: LMP Error Transaction Collision
  - 0x24: LMP PDU Not Allowed
  - 0x25: Encryption Mode Not Acceptable
  - 0x26: Link Key cannot be Changed
  - 0x28: Instant Passed
  - 0x29: Pairing With Unit Key Not Supported
  - 0x2A: Different Transaction Collision
  - 0x2E: Channel Assessment Not Supported
  - 0x2F: Insufficient Security
  - 0x30: Parameter Out Of Mandatory Range
  - 0x32: Role Switch Pending
  - 0x34: Reserved Slot Violation
  - 0x35: Role Switch Failed
  - 0x37: Secure Simple Pairing Not Supported by Host
  - 0x38: Host Busy - Pairing
  - 0x39: Connection Rejected due to No Suitable Channel Found
  - 0x3A: Controller Busy
  - 0x3B: Unacceptable Connection Interval
  - 0x3C: Directed Advertising Timeout
  - 0x3D: Connection Terminated Due to MIC Failure
  - 0x3E: Connection Failed to be Established
  - 0x41: Failed
  - 0x42: Invalid parameters
  - 0x43: Busy
  - 0x44: Invalid length
  - 0x45: Pending
  - 0x46: Not allowed
  - 0x47: GATT error
  - 0x48: Address not resolved
  - 0x50: Invalid CID
  - 0x5A: CSRK not found
  - 0x5B: IRK not found
  - 0x5C: Device not found in DB
  - 0x5D: Security DB full
  - 0x5E: Device not bonded
  - 0x5F: Device in blacklist
  - 0x60: Invalid handle
  - 0x61: Invalid parameter
  - 0x62: Out of handles
  - 0x63: Invalid operation
  - 0x64: Insufficient resources
  - 0x65: Insufficient encryption key size
  - 0x66: Characteristic already exist
  - 0x82: No valid slot
  - 0x83: Short window
  - 0x84: New interval failed
  - 0x85: Too large interval
  - 0x86: Slot length failed
  * @retval None
*/

void aci_gatt_proc_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_proc_complete_event_rp0 *rp0 = (aci_gatt_proc_complete_event_rp0 *)buffer_in;
  aci_gatt_proc_complete_event(rp0->Connection_Handle,
                               rp0->Error_Code);
}

/* aci_gatt_error_resp_event */
/* Event len: 2 + 1 + 2 + 1 */
/**
  * @brief This event is generated when an Error Response is received from the server. The error
response can be given by the server at the end of one of the GATT discovery procedures.
This does not mean that the procedure ended with an error, but this error event is part of the
procedure itself.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Req_Opcode The request that generated this error response
  * @param Attribute_Handle The attribute handle that generated this error response
  * @param Error_Code The reason why the request has generated an error response (ATT error codes)
  * Values:
  - 0x01: Invalid handle
  - 0x02: Read not permitted
  - 0x03: Write not permitted
  - 0x04: Invalid PDU
  - 0x05: Insufficient authentication
  - 0x06: Request not supported
  - 0x07: Invalid offset
  - 0x08: Insufficient authorization
  - 0x09: Prepare queue full
  - 0x0A: Attribute not found
  - 0x0B: Attribute not long
  - 0x0C: Insufficient encryption key size
  - 0x0D: Invalid attribute value length
  - 0x0E: Unlikely error
  - 0x0F: Insufficient encryption
  - 0x10: Unsupported group type
  - 0x11: Insufficient resources
  * @retval None
*/

void aci_gatt_error_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_error_resp_event_rp0 *rp0 = (aci_gatt_error_resp_event_rp0 *)buffer_in;
  aci_gatt_error_resp_event(rp0->Connection_Handle,
                            rp0->Req_Opcode,
                            rp0->Attribute_Handle,
                            rp0->Error_Code);
}

/* aci_gatt_disc_read_char_by_uuid_resp_event */
/* Event len: 2 + 2 + 1 + rp0->Attribute_Value_Length * (sizeof(uint8_t)) */
/**
  * @brief This event can be generated during a "Discover Characteristics By UUID" procedure or a
"Read using Characteristic UUID" procedure.
The attribute value will be a service declaration as defined in Bluetooth Core v5.0.spec
(vol.3, Part G, ch. 3.3.1), when a "Discover Characteristics By UUID" has been started. It will
be the value of the Characteristic if a* "Read using Characteristic UUID" has been
performed.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Attribute_Handle The handle of the attribute
  * @param Attribute_Value_Length Length of Attribute_Value in octets
  * @param Attribute_Value The attribute value will be a service declaration as defined in Bluetooth Core v5.0 spec
 (vol.3, Part G, ch. 3.3.1), when a "Discover Characteristics By UUID" has been started.
 It will be the value of the Characteristic if a "Read using Characteristic UUID" has been performed.
  * @retval None
*/

void aci_gatt_disc_read_char_by_uuid_resp_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_disc_read_char_by_uuid_resp_event_rp0 *rp0 = (aci_gatt_disc_read_char_by_uuid_resp_event_rp0 *)buffer_in;
  aci_gatt_disc_read_char_by_uuid_resp_event(rp0->Connection_Handle,
                                             rp0->Attribute_Handle,
                                             rp0->Attribute_Value_Length,
                                             rp0->Attribute_Value);
}

/* aci_gatt_write_permit_req_event */
/* Event len: 2 + 2 + 1 + rp0->Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is given to the application when a write request, write command or signed write
command is received by the server from the client. This event will be given to the application
only if the event bit for this event generation is set when the characteristic was added.
When this event is received, the application has to check whether the value being requested
for write can be allowed to be written and respond with the command @ref aci_gatt_write_resp.
The details of the parameters of the command can be found. Based on the response from
the application, the attribute value will be modified by the stack. If the write is rejected by the
application, then the value of the attribute will not be modified. In case of a write REQ, an
error response will be sent to the client, with the error code as specified by the application.
In case of write/signed write commands, no response is sent to the client but the attribute is
not modified.
  * @param Connection_Handle Handle of the connection on which there was the request to write the attribute
  * @param Attribute_Handle The handle of the attribute
  * @param Data_Length Length of Data field
  * @param Data The data that the client has requested to write
  * @retval None
*/

void aci_gatt_write_permit_req_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_write_permit_req_event_rp0 *rp0 = (aci_gatt_write_permit_req_event_rp0 *)buffer_in;
  aci_gatt_write_permit_req_event(rp0->Connection_Handle,
                                  rp0->Attribute_Handle,
                                  rp0->Data_Length,
                                  rp0->Data);
}

/* aci_gatt_read_permit_req_event */
/* Event len: 2 + 2 + 2 */
/**
  * @brief This event is given to the application when a read request or read blob request is received
by the server from the client. This event will be given to the application only if the event bit
for this event generation is set when the characteristic was added.
On receiving this event, the application can update the value of the handle if it desires and
when done, it has to send the @ref aci_gatt_allow_read command to indicate to the stack that it
can send the response to the client.
  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Attribute_Handle The handle of the attribute
  * @param Offset Contains the offset from which the read has been requested
  * @retval None
*/

void aci_gatt_read_permit_req_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_read_permit_req_event_rp0 *rp0 = (aci_gatt_read_permit_req_event_rp0 *)buffer_in;
  aci_gatt_read_permit_req_event(rp0->Connection_Handle,
                                 rp0->Attribute_Handle,
                                 rp0->Offset);
}

/* aci_gatt_read_multi_permit_req_event */
/* Event len: 2 + 1 + rp0->Number_of_Handles * (sizeof(Handle_Item_t)) */
/**
  * @brief This event is given to the application when a read multiple request or read by type request is
received by the server from the client. This event will be given to the application only if the
event bit for this event generation is set when the characteristic was added.
On receiving this event, the application can update the values of the handles if it desires and
when done, it has to send the @ref aci_gatt_allow_read command to indicate to the stack that it
can send the response to the client.
  * @param Connection_Handle Handle of the connection which requested to read the attribute
  * @param Number_of_Handles 
  * @param Handle_Item See @ref Handle_Item_t
  * @retval None
*/

void aci_gatt_read_multi_permit_req_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_read_multi_permit_req_event_rp0 *rp0 = (aci_gatt_read_multi_permit_req_event_rp0 *)buffer_in;
  aci_gatt_read_multi_permit_req_event(rp0->Connection_Handle,
                                       rp0->Number_of_Handles,
                                       rp0->Handle_Item);
}

/* aci_gatt_tx_pool_available_event */
/* Event len: 2 + 2 */
/**
  * @brief Each time BLE FW stack raises the error code
BLE_STATUS_INSUFFICIENT_RESOURCES (0x64), the
@ref aci_gatt_tx_pool_available_event event is generated as soon as there are at least two
buffers available for notifications or write commands.
  * @param Connection_Handle Connection handle related to the request
  * @param Available_Buffers Number of buffers available
  * @retval None
*/

void aci_gatt_tx_pool_available_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_tx_pool_available_event_rp0 *rp0 = (aci_gatt_tx_pool_available_event_rp0 *)buffer_in;
  aci_gatt_tx_pool_available_event(rp0->Connection_Handle,
                                   rp0->Available_Buffers);
}

/* aci_gatt_server_confirmation_event */
/* Event len: 2 */
/**
  * @brief This event is generated when the client has sent the confirmation to a previously sent indication
  * @param Connection_Handle Connection handle related to the event
  * @retval None
*/

void aci_gatt_server_confirmation_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_server_confirmation_event_rp0 *rp0 = (aci_gatt_server_confirmation_event_rp0 *)buffer_in;
  aci_gatt_server_confirmation_event(rp0->Connection_Handle);
}

/* aci_gatt_prepare_write_permit_req_event */
/* Event len: 2 + 2 + 2 + 1 + rp0->Data_Length * (sizeof(uint8_t)) */
/**
  * @brief This event is given to the application when a prepare write request
is received by the server from the client. This event will be given to the application
only if the event bit for this event generation is set when the characteristic was added.
When this event is received, the application has to check whether the value being requested
for write can be allowed to be written and respond with the command @ref aci_gatt_write_resp.
Based on the response from the application, the attribute value will be modified by the stack.
If the write is rejected by the application, then the value of the attribute will not be modified
and an error response will be sent to the client, with the error code as specified by the application.
  * @param Connection_Handle Handle of the connection on which there was the request to write the attribute
  * @param Attribute_Handle The handle of the attribute
  * @param Offset The offset from which the prepare write has been requested
  * @param Data_Length Length of Data field
  * @param Data The data that the client has requested to write
  * @retval None
*/

void aci_gatt_prepare_write_permit_req_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_prepare_write_permit_req_event_rp0 *rp0 = (aci_gatt_prepare_write_permit_req_event_rp0 *)buffer_in;
  aci_gatt_prepare_write_permit_req_event(rp0->Connection_Handle,
                                          rp0->Attribute_Handle,
                                          rp0->Offset,
                                          rp0->Data_Length,
                                          rp0->Data);
}

/* aci_gatt_indication_ext_event */
/* Event len: 2 + 2 + 2 + 2 + rp0->Attribute_Value_Length * (sizeof(uint8_t)) */
/**
  * @brief When it is enabled with ACI_GATT_SET_EVENT_MASK and when an indication is received from the server, this event is generated instead of ACI_GATT_INDICATION_EVENT.
This event should be used instead of ACI_GATT_INDICATION_EVENT when ATT_MTU > (BLE_EVT_MAX_PARAM_LEN - 4)
i.e. ATT_MTU > 251 for BLE_EVT_MAX_PARAM_LEN default value.

  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Attribute_Handle The handle of the attribute
  * @param Offset Bits 14-0: offset in octets from which Attribute_Value data starts. Bit 15 is used as flag: when set to 1 it indicates that more data are to come (fragmented event in case of long attribute data).
  * @param Attribute_Value_Length Length of Attribute_Value in octets
  * @param Attribute_Value The current value of the attribute
  * @retval None
*/

void aci_gatt_indication_ext_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_indication_ext_event_rp0 *rp0 = (aci_gatt_indication_ext_event_rp0 *)buffer_in;
  aci_gatt_indication_ext_event(rp0->Connection_Handle,
                                rp0->Attribute_Handle,
                                rp0->Offset,
                                rp0->Attribute_Value_Length,
                                rp0->Attribute_Value);
}

/* aci_gatt_notification_ext_event */
/* Event len: 2 + 2 + 2 + 2 + rp0->Attribute_Value_Length * (sizeof(uint8_t)) */
/**
  * @brief When it is enabled with ACI_GATT_SET_EVENT_MASK and when a notification is received from the server, this event is generated instead of ACI_GATT_NOTIFICATION_EVENT.
This event should be used instead of ACI_GATT_NOTIFICATION_EVENT when ATT_MTU > (BLE_EVT_MAX_PARAM_LEN - 4)
i.e. ATT_MTU > 251 for BLE_EVT_MAX_PARAM_LEN default value.

  * @param Connection_Handle Connection handle related to the response.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Attribute_Handle The handle of the attribute
  * @param Offset Bits 14-0: offset in octets from which Attribute_Value data starts. Bit 15 is used as flag: when set to 1 it indicates that more data are to come (fragmented event in case of long attribute data).
  * @param Attribute_Value_Length Length of Attribute_Value in octets
  * @param Attribute_Value The current value of the attribute
  * @retval None
*/

void aci_gatt_notification_ext_event_process(uint8_t *buffer_in)
{
  /* Input params */
  aci_gatt_notification_ext_event_rp0 *rp0 = (aci_gatt_notification_ext_event_rp0 *)buffer_in;
  aci_gatt_notification_ext_event(rp0->Connection_Handle,
                                  rp0->Attribute_Handle,
                                  rp0->Offset,
                                  rp0->Attribute_Value_Length,
                                  rp0->Attribute_Value);
}

/* hci_le_connection_complete_event */
/* Event len: 1 + 2 + 1 + 1 + 6 + 2 + 2 + 2 + 1 */
/**
  * @brief The LE Connection Complete event indicates to both of the Hosts forming the
connection that a new connection has been created. Upon the creation of the
connection a Connection_Handle shall be assigned by the Controller, and
passed to the Host in this event. If the connection establishment fails this event
shall be provided to the Host that had issued the LE_Create_Connection command.
This event indicates to the Host which issued a LE_Create_Connection
command and received a Command Status event if the connection
establishment failed or was successful.
The Master_Clock_Accuracy parameter is only valid for a slave. On a master,
this parameter shall be set to 0x00. See Bluetooth spec 5.0 vol 2 [part E] 7.7.65.1
  * @param Status Status error code.
  * @param Connection_Handle Connection handle to be used to identify the connection with the peer device.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Role Role of the local device in the connection.
  * Values:
  - 0x00: Master
  - 0x01: Slave
  * @param Peer_Address_Type The address type of the peer device.
  * Values:
  - 0x00: Public Device Address
  - 0x01: Random Device Address
  * @param Peer_Address Public Device Address or Random Device Address of the peer
device
  * @param Conn_Interval Connection interval used on this connection.
Time = N * 1.25 msec
  * Values:
  - 0x0006 (7.50 ms)  ... 0x0C80 (4000.00 ms) 
  * @param Conn_Latency Slave latency for the connection in number of connection events.
  * Values:
  - 0x0000 ... 0x01F3
  * @param Supervision_Timeout Supervision timeout for the LE Link.
It shall be a multiple of 10 ms and larger than (1 + connSlaveLatency) * connInterval * 2.
Time = N * 10 msec.
  * Values:
  - 0x000A (100 ms)  ... 0x0C80 (32000 ms) 
  * @param Master_Clock_Accuracy Master clock accuracy. Only valid for a slave.
  * Values:
  - 0x00: 500 ppm
  - 0x01: 250 ppm
  - 0x02: 150 ppm
  - 0x03: 100 ppm
  - 0x04: 75 ppm
  - 0x05: 50 ppm
  - 0x06: 30 ppm
  - 0x07: 20 ppm
  * @retval None
*/

void hci_le_connection_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_connection_complete_event_rp0 *rp0 = (hci_le_connection_complete_event_rp0 *)buffer_in;
  hci_le_connection_complete_event(rp0->Status,
                                   rp0->Connection_Handle,
                                   rp0->Role,
                                   rp0->Peer_Address_Type,
                                   rp0->Peer_Address,
                                   rp0->Conn_Interval,
                                   rp0->Conn_Latency,
                                   rp0->Supervision_Timeout,
                                   rp0->Master_Clock_Accuracy);
}

/* hci_le_advertising_report_event */
/* Event len: 1 + rp0->Num_Reports * (sizeof(Advertising_Report_t)) */
/**
  * @brief The LE Advertising Report event indicates that a Bluetooth device or multiple
Bluetooth devices have responded to an active scan or received some information
during a passive scan. The Controller may queue these advertising reports
and send information from multiple devices in one LE Advertising Report event. See Bluetooth spec 5.0 vol 2 [part E] 7.7.65.2
  * @param Num_Reports Number of responses in this event.
  * Values:
  - 0x01
  * @param Advertising_Report See @ref Advertising_Report_t
  * @retval None
*/

void hci_le_advertising_report_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_advertising_report_event_rp0 *rp0 = (hci_le_advertising_report_event_rp0 *)buffer_in;

  Advertising_Report_t Advertising_Report[1];
  int i;
  for (i = 0; i < rp0->Num_Reports; i++) 
  {
    buffer_in += 1;
    Osal_MemCpy( &Advertising_Report[0], buffer_in, 9 );
    Advertising_Report[0].Data = &buffer_in[9];
    buffer_in += 9 + buffer_in[8];
    Advertising_Report[0].RSSI = buffer_in[0];
    hci_le_advertising_report_event( 1, Advertising_Report );
  }
 
} 

/* hci_le_connection_update_complete_event */
/* Event len: 1 + 2 + 2 + 2 + 2 */
/**
  * @brief The LE Connection Update Complete event is used to indicate that the Controller
process to update the connection has completed.
On a slave, if no connection parameters are updated, then this event shall not be issued.
On a master, this event shall be issued if the Connection_Update command was sent. See Bluetooth spec 5.0 vol 2 [part E] 7.7.65.3
  * @param Status Status error code.
  * @param Connection_Handle Connection handle to be used to identify the connection with the peer device.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Conn_Interval Connection interval used on this connection.
Time = N * 1.25 msec
  * Values:
  - 0x0006 (7.50 ms)  ... 0x0C80 (4000.00 ms) 
  * @param Conn_Latency Slave latency for the connection in number of connection events.
  * Values:
  - 0x0000 ... 0x01F3
  * @param Supervision_Timeout Supervision timeout for the LE Link.
It shall be a multiple of 10 ms and larger than (1 + connSlaveLatency) * connInterval * 2.
Time = N * 10 msec.
  * Values:
  - 0x000A (100 ms)  ... 0x0C80 (32000 ms) 
  * @retval None
*/

void hci_le_connection_update_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_connection_update_complete_event_rp0 *rp0 = (hci_le_connection_update_complete_event_rp0 *)buffer_in;
  hci_le_connection_update_complete_event(rp0->Status,
                                          rp0->Connection_Handle,
                                          rp0->Conn_Interval,
                                          rp0->Conn_Latency,
                                          rp0->Supervision_Timeout);
}

/* hci_le_read_remote_used_features_complete_event */
/* Event len: 1 + 2 + 8 */
/**
  * @brief The LE Read Remote Used Features Complete event is used to indicate the
completion of the process of the Controller obtaining the used features of the
remote Bluetooth device specified by the Connection_Handle event parameter.See Bluetooth spec 5.0 vol 2 [part E] 7.7.65.4
  * @param Status Status error code.
  * @param Connection_Handle Connection handle to be used to identify the connection with the peer device.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param LE_Features Bit Mask List of used LE features. For details see LE Link Layer specification.
  * @retval None
*/

void hci_le_read_remote_used_features_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_read_remote_used_features_complete_event_rp0 *rp0 = (hci_le_read_remote_used_features_complete_event_rp0 *)buffer_in;
  hci_le_read_remote_used_features_complete_event(rp0->Status,
                                                  rp0->Connection_Handle,
                                                  rp0->LE_Features);
}

/* hci_le_long_term_key_request_event */
/* Event len: 2 + 8 + 2 */
/**
  * @brief The LE Long Term Key Request event indicates that the master device is
attempting to encrypt or re-encrypt the link and is requesting the Long Term
Key from the Host. (See [Vol 6] Part B, Section 5.1.3)and Bluetooth spec 5.0 vol 2 [part E] 7.7.65.5
  * @param Connection_Handle Connection handle to be used to identify the connection with the peer device.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Random_Number 64-bit random number
  * @param Encrypted_Diversifier 16-bit encrypted diversifier
  * @retval None
*/

void hci_le_long_term_key_request_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_long_term_key_request_event_rp0 *rp0 = (hci_le_long_term_key_request_event_rp0 *)buffer_in;
  hci_le_long_term_key_request_event(rp0->Connection_Handle,
                                     rp0->Random_Number,
                                     rp0->Encrypted_Diversifier);
}

/* hci_le_data_length_change_event */
/* Event len: 2 + 2 + 2 + 2 + 2 */
/**
  * @brief The LE Data Length Change event notifies the Host of a change to either the
maximum Payload length or the maximum transmission time of packets in
either direction. The values reported are the maximum that will actually be
used on the connection following the change, except that on the LE Coded
PHY a packet taking up to 2704 us to transmit may be sent even though the
corresponding parameter has a lower value.
 See Bluetooth spec 5.0 vol 2 [part E] 7.7.65.7
  * @param Connection_Handle Connection handle to be used to identify the connection with the peer device.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param MaxTxOctets The maximum number of payload octets in a Link Layer packet that the
local Controller will send on this connection(connEffectiveMaxTxOctets defined in [Vol 6] Part B, Section 4.5.10).
  * Values:
  - 0x001B ... 0x00FB
  * @param MaxTxTime The maximum time that the local Controller will take to send a Link
Layer packet on this connection (connEffectiveMaxTxTime defined in[Vol 6] Part B, Section 4.5.10).
  * Values:
  - 0x0148 ... 0x4290
  * @param MaxRxOctets The maximum number of payload octets in a Link Layer packet that the
local Controller expects to receive on this connection(connEffectiveMaxRxOctets defined in [Vol 6] Part B, Section 4.5.10).
  * Values:
  - 0x001B ... 0x00FB
  * @param MaxRxTime The maximum time that the local Controller expects to take to receive a
Link Layer packet on this connection (connEffectiveMaxRxTime defined in [Vol 6] Part B, Section 4.5.10).
  * Values:
  - 0x0148 ... 0x4290
  * @retval None
*/

void hci_le_data_length_change_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_data_length_change_event_rp0 *rp0 = (hci_le_data_length_change_event_rp0 *)buffer_in;
  hci_le_data_length_change_event(rp0->Connection_Handle,
                                  rp0->MaxTxOctets,
                                  rp0->MaxTxTime,
                                  rp0->MaxRxOctets,
                                  rp0->MaxRxTime);
}

/* hci_le_read_local_p256_public_key_complete_event */
/* Event len: 1 + 64 */
/**
  * @brief This event is generated when local P-256 key generation is complete. 
                                                                  See Bluetooth spec 5.0 vol 2 [part E] 7.7.65.8
  * @param Status Status error code.
  * @param Local_P256_Public_Key Local P-256 public key.
  * @retval None
*/

void hci_le_read_local_p256_public_key_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_read_local_p256_public_key_complete_event_rp0 *rp0 = (hci_le_read_local_p256_public_key_complete_event_rp0 *)buffer_in;
  hci_le_read_local_p256_public_key_complete_event(rp0->Status,
                                                   rp0->Local_P256_Public_Key);
}

/* hci_le_generate_dhkey_complete_event */
/* Event len: 1 + 32 */
/**
  * @brief This event indicates that LE Diffie Hellman key generation has been completed
by the Controller. See Bluetooth spec 5.0 vol 2 [part E] 7.7.65.9
  * @param Status Status error code.
  * @param DHKey Diffie Hellman Key
  * @retval None
*/

void hci_le_generate_dhkey_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_generate_dhkey_complete_event_rp0 *rp0 = (hci_le_generate_dhkey_complete_event_rp0 *)buffer_in;
  hci_le_generate_dhkey_complete_event(rp0->Status,
                                       rp0->DHKey);
}

/* hci_le_enhanced_connection_complete_event */
/* Event len: 1 + 2 + 1 + 1 + 6 + 6 + 6 + 2 + 2 + 2 + 1 */
/**
  * @brief The LE Enhanced Connection Complete event indicates to both of the Hosts
forming the connection that a new connection has been created. Upon the
creation of the connection a Connection_Handle shall be assigned by the
Controller, and passed to the Host in this event. If the connection establishment
fails, this event shall be provided to the Host that had issued the
LE_Create_Connection command.
If this event is unmasked and LE Connection Complete event is unmasked,
only the LE Enhanced Connection Complete event is sent when a new
connection has been completed.
This event indicates to the Host that issued a LE_Create_Connection
command and received a Command Status event if the connection
establishment failed or was successful.
The Master_Clock_Accuracy parameter is only valid for a slave. On a master,
this parameter shall be set to 0x00. See Bluetooth spec 5.0 vol 2 [part E] 7.7.65.10
  * @param Status Status error code.
  * @param Connection_Handle Connection handle to be used to identify the connection with the peer device.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param Role Role of the local device in the connection.
  * Values:
  - 0x00: Master
  - 0x01: Slave
  * @param Peer_Address_Type 0x00 Public Device Address
0x01 Random Device Address
0x02 Public Identity Address (Corresponds to Resolved Private Address)
0x03 Random (Static) Identity Address (Corresponds to Resolved Private Address)
  * Values:
  - 0x00: Public Device Address
  - 0x01: Random Device Address
  - 0x02: Public Identity Address
  - 0x03: Random (Static) Identity Address
  * @param Peer_Address Public Device Address, Random Device Address, Public Identity
Address or Random (static) Identity Address of the device to be connected.
  * @param Local_Resolvable_Private_Address Resolvable Private Address being used by the local device for this connection.
This is only valid when the Own_Address_Type is set to 0x02 or 0x03. For other Own_Address_Type values,
the Controller shall return all zeros.
  * @param Peer_Resolvable_Private_Address Resolvable Private Address being used by the peer device for this connection.
This is only valid for Peer_Address_Type 0x02 and 0x03. For
other Peer_Address_Type values, the Controller shall return all zeros.
  * @param Conn_Interval Connection interval used on this connection.
Time = N * 1.25 msec
  * Values:
  - 0x0006 (7.50 ms)  ... 0x0C80 (4000.00 ms) 
  * @param Conn_Latency Slave latency for the connection in number of connection events.
  * Values:
  - 0x0000 ... 0x01F3
  * @param Supervision_Timeout Supervision timeout for the LE Link.
It shall be a multiple of 10 ms and larger than (1 + connSlaveLatency) * connInterval * 2.
Time = N * 10 msec.
  * Values:
  - 0x000A (100 ms)  ... 0x0C80 (32000 ms) 
  * @param Master_Clock_Accuracy Master clock accuracy. Only valid for a slave.
  * Values:
  - 0x00: 500 ppm
  - 0x01: 250 ppm
  - 0x02: 150 ppm
  - 0x03: 100 ppm
  - 0x04: 75 ppm
  - 0x05: 50 ppm
  - 0x06: 30 ppm
  - 0x07: 20 ppm
  * @retval None
*/

void hci_le_enhanced_connection_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_enhanced_connection_complete_event_rp0 *rp0 = (hci_le_enhanced_connection_complete_event_rp0 *)buffer_in;
  hci_le_enhanced_connection_complete_event(rp0->Status,
                                            rp0->Connection_Handle,
                                            rp0->Role,
                                            rp0->Peer_Address_Type,
                                            rp0->Peer_Address,
                                            rp0->Local_Resolvable_Private_Address,
                                            rp0->Peer_Resolvable_Private_Address,
                                            rp0->Conn_Interval,
                                            rp0->Conn_Latency,
                                            rp0->Supervision_Timeout,
                                            rp0->Master_Clock_Accuracy);
}

/* hci_le_direct_advertising_report_event */
/* Event len: 1 + rp0->Num_Reports * (sizeof(Direct_Advertising_Report_t)) */
/**
  * @brief The LE Direct Advertising Report event indicates that directed advertisements
have been received where the advertiser is using a resolvable private address
for the InitA field in the ADV_DIRECT_IND PDU and the
Scanning_Filter_Policy is equal to 0x02 or 0x03, see HCI_LE_Set_Scan_Parameters.
Direct_Address_Type and Direct_Addres is the address the directed
advertisements are being directed to. Address_Type and Address is the
address of the advertiser sending the directed advertisements. See Bluetooth spec 5.0 vol 2 [part E] 7.7.65.11
  * @param Num_Reports Number of responses in this event.
  * Values:
  - 0x01
  * @param Direct_Advertising_Report See @ref Direct_Advertising_Report_t
  * @retval None
*/

void hci_le_direct_advertising_report_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_direct_advertising_report_event_rp0 *rp0 = (hci_le_direct_advertising_report_event_rp0 *)buffer_in;
  hci_le_direct_advertising_report_event(rp0->Num_Reports,
                                         rp0->Direct_Advertising_Report);
}

/* hci_le_phy_update_complete_event */
/* Event len: 1 + 2 + 1 + 1 */
/**
  * @brief The LE PHY Update Complete Event is used to indicate that the Controller has
changed the transmitter PHY or receiver PHY in use.
If the Controller changes the transmitter PHY, the receiver PHY, or both PHYs,
this event shall be issued.
If an LE_Set_PHY command was sent and the Controller determines that
neither PHY will change as a result, it issues this event immediately.
 See See Bluetooth spec 5.0 vol 2 [part E] 7.7.65.12
  * @param Status Status error code.
  * @param Connection_Handle Connection handle to be used to identify the connection with the peer device.
  * Values:
  - 0x0000 ... 0x0EFF
  * @param TX_PHY Transmitter PHY in use
  * Values:
  - 0x01: The transmitter PHY for the connection is LE 1M
  - 0x02: The transmitter PHY for the connection is LE 2M
  - 0x03: The transmitter PHY for the connection is LE Coded (Not Supported by STM32WB)
  * @param RX_PHY Receiver PHY in use
  * Values:
  - 0x01: The receiver PHY for the connection is LE 1M
  - 0x02: The receiver PHY for the connection is LE 2M
  - 0x03: The receiver PHY for the connection is LE Coded (Not Supported by STM32WB)
  * @retval None
*/

void hci_le_phy_update_complete_event_process(uint8_t *buffer_in)
{
  /* Input params */
  hci_le_phy_update_complete_event_rp0 *rp0 = (hci_le_phy_update_complete_event_rp0 *)buffer_in;
  hci_le_phy_update_complete_event(rp0->Status,
                                   rp0->Connection_Handle,
                                   rp0->TX_PHY,
                                   rp0->RX_PHY);
}

