/******************************************************************************
 * @file    ble_hal_aci.h
 * @author  MCD Application Team
 * @date    23 May 2019
 * @brief   Header file for STM32WB (hal_aci)
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

#ifndef BLE_HAL_ACI_H__
#define BLE_HAL_ACI_H__


#include "ble_types.h"

/**
  * @brief This command returns the build number associated with the firmware version currently running
  * @param[out] Build_Number Build number of the firmware.
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_get_fw_build_number(uint16_t *Build_Number);

/**
  * @brief This command writes a value to a low level configure data structure. It is useful to setup
directly some low level parameters for the system in the runtime.
  * @param Offset Offset of the element in the configuration data structure
which has to be written. The valid offsets are:

- 0x00: Bluetooth public address, Value length to be written: 6 bytes
- 0x06: DIV used to derive CSRK, Value length to be written: 2 bytes
- 0x08: Encryption root key used to derive LTK and CSRK, Value length to be written: 16 bytes
- 0x18: Identity root key used to derive LTK and CSRK, Value length to be written: 16 bytes
- 0x2C: Link layer without host (for certification purposes), Value length to be written: 1 byte
- 0x2E: Static Random Address: 6 bytes
- 0x2F: Disable watchdog (1=disable, 0=enable). Value length to be written: 1 byte
  * Values:
  - 0x00: CONFIG_DATA_PUBADDR_OFFSET
  - 0x06: CONFIG_DATA_DIV_OFFSET
  - 0x08: CONFIG_DATA_ER_OFFSET
  - 0x18: CONFIG_DATA_IR_OFFSET
  - 0x2C: LL_WITHOUT_HOST
  - 0x2E: CONFIG_DATA_RANDOM_ADDRESS_WR
  - 0x2F: CONFIG_DATA_WATCHDOG_DISABLE
  * @param Length Length of data to be written
  * @param Value Data to be written
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_write_config_data(uint8_t Offset,
                                     uint8_t Length,
                                     uint8_t Value[]);

/**
  * @brief This command requests the value in the low level configure data structure.
The number of read bytes changes for different Offset. 
  * @param Offset Offset of the element in the configuration data structure
which has to be read. The valid offsets are:

* 0x00: Bluetooth public address, Value length returned: 6 bytes
* 0x06: DIV used to derive CSRK, Value length returned: 2 bytes
* 0x08: Encryption root key used to derive LTK and CSRK, Value length returned: 16 bytes
* 0x18: Identity root key used to derive LTK and CSRK, Value length returned: 16 bytes
* 0x2C: Link layer without host (for certification purposes), Value length returned: 1 byte
* 0x2F: Disable watchdog (1=disabled, 0=enable). Value length returned: 1 byte
* 0x80: Static random address. Value length returned: 6 bytes (read-only)
  * Values:
  - 0x00: CONFIG_DATA_PUBADDR_OFFSET
  - 0x06: CONFIG_DATA_DIV_OFFSET
  - 0x08: CONFIG_DATA_ER_OFFSET
  - 0x18: CONFIG_DATA_IR_OFFSET
  - 0x2C: LL_WITHOUT_HOST
  - 0x2F: CONFIG_DATA_WATCHDOG_DISABLE
  - 0x80: CONFIG_DATA_RANDOM_ADDRESS
  * @param[out] Data_Length Length of Data in octets
  * @param[out] Data Data field associated with Offset parameter
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_read_config_data(uint8_t Offset,
                                    uint8_t *Data_Length,
                                    uint8_t Data[]);

/**
  * @brief This command sets the TX power level of the device. By controlling the
PA_LEVEL, that determines the output power level (dBm) at the IC pin. 
When the system starts up or reboots, the default TX power level will be used, which is the
maximum value of 6 dBm. Once this command is given, the output power will be changed
instantly, regardless if there is Bluetooth communication going on or not. For example, for
debugging purpose, the device can be set to advertise all the time. And use this
command to observe the signal strength changing.
The system will keep the last received TX power level from the command, i.e. the 2nd
command overwrites the previous TX power level. The new TX power level remains until
another Set TX Power command, or the system reboots.
  * @param En_High_Power Enable High Power mode - Deprecated and ignored on STM32WB
  * Values:
  - 0x00: Standard Power
  - 0x01: High Power
  * @param PA_Level Power amplifier output level. Output power is indicative and it depends on the PCB layout and associated 
components.Here the values are given at the IC pin
  * Values:
  - 0x00: -40 dBm
  - 0x01: -20.85 dBm
  - 0x02: -19.75 dBm
  - 0x03: -18.85 dBm
  - 0x04: -17.6 dBm
  - 0x05: -16.5 dBm
  - 0x06: -15.25 dBm
  - 0x07: -14.1 dBm
  - 0x08: -13.15 dBm
  - 0x09: -12.05 dBm
  - 0x0A: -10.9 dBm
  - 0x0B: -9.9 dBm
  - 0x0C: -8.85 dBm
  - 0x0D: -7.8 dBm
  - 0x0E: -6.9 dBm
  - 0x0F: -5.9 dBm
  - 0x10: -4.95 dBm
  - 0x11: -4 dBm
  - 0x12: -3.15 dBm
  - 0x13: -2.45 dBm
  - 0x14: -1.8 dBm
  - 0x15: -1.3 dBm
  - 0x16: -0.85 dBm
  - 0x17: -0.5 dBm
  - 0x18: -0.15 dBm
  - 0x19: 0 dBm
  - 0x1A: +1 dBm
  - 0x1B: +2 dBm
  - 0x1C: +3 dBm
  - 0x1D: +4 dBm
  - 0x1E: +5 dBm
  - 0x1F: +6 dBm
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_set_tx_power_level(uint8_t En_High_Power,
                                      uint8_t PA_Level);

/**
  * @brief This command returns the number of packets sent in Direct Test Mode.
When the Direct TX test is started, a 32-bit counter is used to count how many packets have been transmitted. 
This command can be used to check how many packets have been sent during the Direct TX test.
The counter starts from 0 and counts upwards. The counter can wrap and start from 0 again. 
The counter is not cleared until the next Direct TX test starts. 
  * @param[out] Number_Of_Packets Number of packets sent during the last Direct TX test.
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_le_tx_test_packet_number(uint32_t *Number_Of_Packets);

/**
  * @brief This command starts a carrier frequency, i.e. a tone, on a specific channel. The frequency
sine wave at the specific channel may be used for debugging purpose only. The channel ID
is a parameter from 0x00 to 0x27 for the 40 BLE channels, e.g. 0x00 for 2.402 GHz, 0x01
for 2.404 GHz etc.
This command should not be used when normal Bluetooth activities are ongoing.
The tone should be stopped by @ref aci_hal_tone_stop command.
  * @param RF_Channel BLE Channel ID, from 0x00 to 0x27 meaning (2.402 + 2*0xXX) GHz
Device will continuously emit 0s, that means that the tone
will be at the channel center frequency less the maximum
frequency deviation (250kHz).
  * Values:
  - 0x00 ... 0x27
  * @param Freq_offset Frequency Offset for tone channel
  * Values:
  - 0x00 ... 0xFF
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_tone_start(uint8_t RF_Channel,
                              uint8_t Freq_offset);

/**
  * @brief This command is used to stop the previously started @ref aci_hal_tone_start command.
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_tone_stop(void);

/**
  * @brief This command returns the status of the 8 Bluetooth low energy links managed by the device
  * @param[out] Link_Status Array of link status (8 links). Each link status is 1 byte.
  * @param[out] Link_Connection_Handle Array of connection handles (2 bytes) for 8 links.
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_get_link_status(uint8_t Link_Status[8],
                                   uint16_t Link_Connection_Handle[16 / 2]);

/**
  * @brief This command set the bitmask associated to @ref aci_hal_end_of_radio_activity_event. 
Only the radio activities enabled in the mask will be reported to application by @ref aci_hal_end_of_radio_activity_event
  * @param Radio_Activity_Mask Bitmask of radio events
  * Flags:
  - 0x0001: Idle
  - 0x0002: Advertising
  - 0x0004: Connection event slave
  - 0x0008: Scanning
  - 0x0010: Connection request
  - 0x0020: Connection event master
  - 0x0040: TX test mode
  - 0x0080: RX test mode
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_set_radio_activity_mask(uint16_t Radio_Activity_Mask);

/**
  * @brief This command returns information about the Anchor Period to help application in selecting 
                      slot timings when operating in multi-link scenarios.
  * @param[out] Anchor_Period Current anchor period.
T = N * 0.625 ms.
  * @param[out] Max_Free_Slot Maximum available time that can be allocated for a new slot.
T = N * 0.625 ms.
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_get_anchor_period(uint32_t *Anchor_Period,
                                     uint32_t *Max_Free_Slot);

/**
  * @brief This command is used to enable/disable the generation of HAL events 
  * @param Event_Mask Mask to enable/disable generation of HAL events
  * Flags:
  - 0x00000000: No events specified (Default)
  - 0x00000001: ACI_HAL_SCAN_REQ_REPORT_EVENT
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_set_event_mask(uint32_t Event_Mask);

/**
  * @brief This command is used to provide a specific engineering setup to the Security Manager Protocol Layer. It may be used during development/debug only!
  * @param SMP_Config Mask to configure SMP engineering knobs
  * Flags:
  - 0x00000000: Default config (all reset)
  - 0x00000001: Cheat Level 1 ON
  - 0x00000002: RFU
  - 0x00000003: Cheat Level 3 ON
  - 0x00000004: RFU
  - 0x00000005: Cheat Level 5 ON
  - 0x00000006: Cheat Level 6 ON
  - 0x00000007: Cheat Level 7 ON
  - 0x00000010: DBG messages ON
  - 0x00000100: Debug Public Key ON
  - 0x00000107: Debug KEY On + DBG msg Off + CL=7
  - 0x00000117: Debug KEY On + DBG msg On + CL=7
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_set_smp_eng_config(uint32_t SMP_Config);

/**
  * @brief This command is used to retrieve TX, RX and total buffer count allocated for ACL packets.
  * @param[out] Allocated_For_TX MBlocks allocated for TXing
  * @param[out] Allocated_For_RX MBlocks allocated for RXing
  * @param[out] Allocated_MBlocks Overall allocated MBlocks
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_get_pm_debug_info(uint8_t *Allocated_For_TX,
                                     uint8_t *Allocated_For_RX,
                                     uint8_t *Allocated_MBlocks);

/**
  * @brief This command Reads Register value from the RF module 
  * @param Register_Address Address of the register to be read
  * @param[out] reg_val Register value
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_read_radio_reg(uint8_t Register_Address,
                                  uint8_t *reg_val);

/**
  * @brief This command writes Register value to the RF module 
  * @param Register_Address Address of the register to be written
  * @param Register_Value Value to be written
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_write_radio_reg(uint8_t Register_Address,
                                   uint8_t Register_Value);

/**
  * @brief This command returns the raw value of the RSSI 
  * @param[out] Value RAW RSSI value
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_read_raw_rssi(uint8_t Value[3]);

/**
  * @brief This command does set up the RF to listen to a specific RF channel 
  * @param RF_Channel BLE Channel ID, from 0x00 to 0x27 meaning (2.402 + 2*0xXX) GHz
Device will continuously emit 0s, that means that the tone
will be at the channel center frequency less the maximum
frequency deviation (250kHz).
  * Values:
  - 0x00 ... 0x27
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_rx_start(uint8_t RF_Channel);

/**
  * @brief This command stop a previous ACI_HAL_RX_START command 
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_rx_stop(void);

/**
  * @brief This command is equivalent to HCI_RESET but ensures the sleep mode is entered' immediately after its completion
  * @retval Value indicating success or error code.
*/
tBleStatus aci_hal_stack_reset(void);

/**
     * @}
     */
    
#endif /* ! BLE_HAL_ACI_H__ */
