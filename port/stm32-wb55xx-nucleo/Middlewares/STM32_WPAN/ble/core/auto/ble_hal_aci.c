/******************************************************************************
 * @file    ble_hal_aci.c
 * @author  MCD Application Team
 * @date    23 May 2019
 * @brief   Source file for ble api STM32WB (hal_aci)
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

#include "ble_hal_aci.h"

tBleStatus aci_hal_get_fw_build_number(uint16_t *Build_Number)
{
  struct hci_request rq;
  aci_hal_get_fw_build_number_rp0 resp;
  Osal_MemSet(&resp, 0, sizeof(resp));
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x000;
  rq.rparam = &resp;
  rq.rlen = sizeof(resp);
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (resp.Status) 
  {
    return resp.Status;
  }
  *Build_Number = btoh(resp.Build_Number, 2);
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_write_config_data(uint8_t Offset,
                                     uint8_t Length,
                                     uint8_t Value[])
{
  struct hci_request rq;
  uint8_t cmd_buffer[BLE_CMD_MAX_PARAM_LEN];
  aci_hal_write_config_data_cp0 *cp0 = (aci_hal_write_config_data_cp0*)(cmd_buffer);
  tBleStatus status = 0;
  int index_input = 0;
  cp0->Offset = htob(Offset, 1);
  index_input += 1;
  cp0->Length = htob(Length, 1);
  index_input += 1;
  Osal_MemCpy((void *) &cp0->Value, (const void *) Value, Length);
  index_input += Length;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x00c;
  rq.cparam = cmd_buffer;
  rq.clen = index_input;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_read_config_data(uint8_t Offset,
                                    uint8_t *Data_Length,
                                    uint8_t Data[])
{
  struct hci_request rq;
  uint8_t cmd_buffer[BLE_CMD_MAX_PARAM_LEN];
  aci_hal_read_config_data_cp0 *cp0 = (aci_hal_read_config_data_cp0*)(cmd_buffer);
  aci_hal_read_config_data_rp0 resp;
  Osal_MemSet(&resp, 0, sizeof(resp));
  int index_input = 0;
  cp0->Offset = htob(Offset, 1);
  index_input += 1;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x00d;
  rq.cparam = cmd_buffer;
  rq.clen = index_input;
  rq.rparam = &resp;
  rq.rlen = sizeof(resp);
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (resp.Status) 
  {
    return resp.Status;
  }
  *Data_Length = btoh(resp.Data_Length, 1);
  Osal_MemCpy((void *) Data, (const void *) resp.Data, *Data_Length);
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_set_tx_power_level(uint8_t En_High_Power,
                                      uint8_t PA_Level)
{
  struct hci_request rq;
  uint8_t cmd_buffer[BLE_CMD_MAX_PARAM_LEN];
  aci_hal_set_tx_power_level_cp0 *cp0 = (aci_hal_set_tx_power_level_cp0*)(cmd_buffer);
  tBleStatus status = 0;
  int index_input = 0;
  cp0->En_High_Power = htob(En_High_Power, 1);
  index_input += 1;
  cp0->PA_Level = htob(PA_Level, 1);
  index_input += 1;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x00f;
  rq.cparam = cmd_buffer;
  rq.clen = index_input;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_le_tx_test_packet_number(uint32_t *Number_Of_Packets)
{
  struct hci_request rq;
  aci_hal_le_tx_test_packet_number_rp0 resp;
  Osal_MemSet(&resp, 0, sizeof(resp));
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x014;
  rq.rparam = &resp;
  rq.rlen = sizeof(resp);
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (resp.Status) 
  {
    return resp.Status;
  }
  *Number_Of_Packets = btoh(resp.Number_Of_Packets, 4);
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_tone_start(uint8_t RF_Channel,
                              uint8_t Freq_offset)
{
  struct hci_request rq;
  uint8_t cmd_buffer[BLE_CMD_MAX_PARAM_LEN];
  aci_hal_tone_start_cp0 *cp0 = (aci_hal_tone_start_cp0*)(cmd_buffer);
  tBleStatus status = 0;
  int index_input = 0;
  cp0->RF_Channel = htob(RF_Channel, 1);
  index_input += 1;
  cp0->Freq_offset = htob(Freq_offset, 1);
  index_input += 1;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x015;
  rq.cparam = cmd_buffer;
  rq.clen = index_input;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_tone_stop(void)
{
  struct hci_request rq;
  tBleStatus status = 0;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x016;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_get_link_status(uint8_t Link_Status[8],
                                   uint16_t Link_Connection_Handle[16 / 2])
{
  struct hci_request rq;
  aci_hal_get_link_status_rp0 resp;
  Osal_MemSet(&resp, 0, sizeof(resp));
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x017;
  rq.rparam = &resp;
  rq.rlen = sizeof(resp);
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (resp.Status) 
  {
    return resp.Status;
  }
  Osal_MemCpy((void *) Link_Status, (const void *) resp.Link_Status, 8);
  Osal_MemCpy((void *) Link_Connection_Handle, (const void *) resp.Link_Connection_Handle, 16);
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_set_radio_activity_mask(uint16_t Radio_Activity_Mask)
{
  struct hci_request rq;
  uint8_t cmd_buffer[BLE_CMD_MAX_PARAM_LEN];
  aci_hal_set_radio_activity_mask_cp0 *cp0 = (aci_hal_set_radio_activity_mask_cp0*)(cmd_buffer);
  tBleStatus status = 0;
  int index_input = 0;
  cp0->Radio_Activity_Mask = htob(Radio_Activity_Mask, 2);
  index_input += 2;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x018;
  rq.cparam = cmd_buffer;
  rq.clen = index_input;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_get_anchor_period(uint32_t *Anchor_Period,
                                     uint32_t *Max_Free_Slot)
{
  struct hci_request rq;
  aci_hal_get_anchor_period_rp0 resp;
  Osal_MemSet(&resp, 0, sizeof(resp));
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x019;
  rq.rparam = &resp;
  rq.rlen = sizeof(resp);
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (resp.Status) 
  {
    return resp.Status;
  }
  *Anchor_Period = btoh(resp.Anchor_Period, 4);
  *Max_Free_Slot = btoh(resp.Max_Free_Slot, 4);
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_set_event_mask(uint32_t Event_Mask)
{
  struct hci_request rq;
  uint8_t cmd_buffer[BLE_CMD_MAX_PARAM_LEN];
  aci_hal_set_event_mask_cp0 *cp0 = (aci_hal_set_event_mask_cp0*)(cmd_buffer);
  tBleStatus status = 0;
  int index_input = 0;
  cp0->Event_Mask = htob(Event_Mask, 4);
  index_input += 4;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x01a;
  rq.cparam = cmd_buffer;
  rq.clen = index_input;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_set_smp_eng_config(uint32_t SMP_Config)
{
  struct hci_request rq;
  uint8_t cmd_buffer[BLE_CMD_MAX_PARAM_LEN];
  aci_hal_set_smp_eng_config_cp0 *cp0 = (aci_hal_set_smp_eng_config_cp0*)(cmd_buffer);
  tBleStatus status = 0;
  int index_input = 0;
  cp0->SMP_Config = htob(SMP_Config, 4);
  index_input += 4;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x01b;
  rq.cparam = cmd_buffer;
  rq.clen = index_input;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_get_pm_debug_info(uint8_t *Allocated_For_TX,
                                     uint8_t *Allocated_For_RX,
                                     uint8_t *Allocated_MBlocks)
{
  struct hci_request rq;
  aci_hal_get_pm_debug_info_rp0 resp;
  Osal_MemSet(&resp, 0, sizeof(resp));
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x01c;
  rq.rparam = &resp;
  rq.rlen = sizeof(resp);
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (resp.Status) 
  {
    return resp.Status;
  }
  *Allocated_For_TX = btoh(resp.Allocated_For_TX, 1);
  *Allocated_For_RX = btoh(resp.Allocated_For_RX, 1);
  *Allocated_MBlocks = btoh(resp.Allocated_MBlocks, 1);
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_read_radio_reg(uint8_t Register_Address,
                                  uint8_t *reg_val)
{
  struct hci_request rq;
  uint8_t cmd_buffer[BLE_CMD_MAX_PARAM_LEN];
  aci_hal_read_radio_reg_cp0 *cp0 = (aci_hal_read_radio_reg_cp0*)(cmd_buffer);
  aci_hal_read_radio_reg_rp0 resp;
  Osal_MemSet(&resp, 0, sizeof(resp));
  int index_input = 0;
  cp0->Register_Address = htob(Register_Address, 1);
  index_input += 1;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x030;
  rq.cparam = cmd_buffer;
  rq.clen = index_input;
  rq.rparam = &resp;
  rq.rlen = sizeof(resp);
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (resp.Status) 
  {
    return resp.Status;
  }
  *reg_val = btoh(resp.reg_val, 1);
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_write_radio_reg(uint8_t Register_Address,
                                   uint8_t Register_Value)
{
  struct hci_request rq;
  uint8_t cmd_buffer[BLE_CMD_MAX_PARAM_LEN];
  aci_hal_write_radio_reg_cp0 *cp0 = (aci_hal_write_radio_reg_cp0*)(cmd_buffer);
  tBleStatus status = 0;
  int index_input = 0;
  cp0->Register_Address = htob(Register_Address, 1);
  index_input += 1;
  cp0->Register_Value = htob(Register_Value, 1);
  index_input += 1;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x031;
  rq.cparam = cmd_buffer;
  rq.clen = index_input;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_read_raw_rssi(uint8_t Value[3])
{
  struct hci_request rq;
  aci_hal_read_raw_rssi_rp0 resp;
  Osal_MemSet(&resp, 0, sizeof(resp));
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x032;
  rq.rparam = &resp;
  rq.rlen = sizeof(resp);
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (resp.Status) 
  {
    return resp.Status;
  }
  Osal_MemCpy((void *) Value, (const void *) resp.Value, 3);
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_rx_start(uint8_t RF_Channel)
{
  struct hci_request rq;
  uint8_t cmd_buffer[BLE_CMD_MAX_PARAM_LEN];
  aci_hal_rx_start_cp0 *cp0 = (aci_hal_rx_start_cp0*)(cmd_buffer);
  tBleStatus status = 0;
  int index_input = 0;
  cp0->RF_Channel = htob(RF_Channel, 1);
  index_input += 1;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x033;
  rq.cparam = cmd_buffer;
  rq.clen = index_input;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_rx_stop(void)
{
  struct hci_request rq;
  tBleStatus status = 0;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x034;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

tBleStatus aci_hal_stack_reset(void)
{
  struct hci_request rq;
  tBleStatus status = 0;
  Osal_MemSet(&rq, 0, sizeof(rq));
  rq.ogf = 0x3f;
  rq.ocf = 0x03b;
  rq.rparam = &status;
  rq.rlen = 1;
  if (hci_send_req(&rq, FALSE) < 0)
    return BLE_STATUS_TIMEOUT;
  if (status) 
  {
    return status;
  }
  return BLE_STATUS_SUCCESS;
}

