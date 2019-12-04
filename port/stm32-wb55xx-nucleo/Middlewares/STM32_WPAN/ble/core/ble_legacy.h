/*****************************************************************************
 * @file    ble_legacy.h
 * @author  MCD Application Team
 * @brief   This file contains legacy definitions used for BLE.
 *****************************************************************************
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

#ifndef BLE_LEGACY_H__
#define BLE_LEGACY_H__


/* ------------------------------------------------------------------------- */


/*
 * The event code in the @ref hci_event_pckt structure.
 * If event code is EVT_VENDOR,  application can use @ref evt_blue_aci
 * structure to parse the packet.
 */
#define EVT_VENDOR                      0xFF

#define EVT_CONN_COMPLETE               0x03
#define EVT_DISCONN_COMPLETE            0x05
#define EVT_LE_META_EVENT               0x3E
#define EVT_LE_CONN_UPDATE_COMPLETE     0x03
#define EVT_LE_CONN_COMPLETE            0x01
#define EVT_LE_ADVERTISING_REPORT       0x02
#define EVT_LE_PHY_UPDATE_COMPLETE      0x0C
#define EVT_LE_ENHANCED_CONN_COMPLETE   0x0A

typedef PACKED(struct) _hci_uart_pckt
{
  uint8_t type;
  uint8_t data[1];
} hci_uart_pckt;

typedef PACKED(struct) _hci_event_pckt
{
  uint8_t         evt;
  uint8_t         plen;
  uint8_t         data[1];
} hci_event_pckt;

typedef PACKED(struct) _evt_le_meta_event
{
  uint8_t         subevent;
  uint8_t         data[1];
} evt_le_meta_event;

/**
 * Vendor specific event for BLE core.
 */
typedef PACKED(struct) _evt_blue_aci
{
  uint16_t ecode; /**< One of the BLE core event codes. */
  uint8_t  data[1];
} evt_blue_aci;


/* BLE core event codes */
#define EVT_BLUE_GATT_ATTRIBUTE_MODIFIED          (0x0C01)
#define EVT_BLUE_GATT_PROCEDURE_TIMEOUT           (0x0C02)
#define EVT_BLUE_ATT_EXCHANGE_MTU_RESP            (0x0C03)
#define EVT_BLUE_ATT_FIND_INFORMATION_RESP        (0x0C04)
#define EVT_BLUE_ATT_FIND_BY_TYPE_VAL_RESP        (0x0C05)
#define EVT_BLUE_ATT_READ_BY_TYPE_RESP            (0x0C06)
#define EVT_BLUE_ATT_READ_RESP                    (0x0C07)
#define EVT_BLUE_ATT_READ_BLOB_RESP               (0x0C08)
#define EVT_BLUE_ATT_READ_MULTIPLE_RESP           (0x0C09)
#define EVT_BLUE_ATT_READ_BY_GROUP_TYPE_RESP      (0x0C0A)
#define EVT_BLUE_ATT_PREPARE_WRITE_RESP           (0x0C0C)
#define EVT_BLUE_ATT_EXEC_WRITE_RESP              (0x0C0D)
#define EVT_BLUE_GATT_INDICATION                  (0x0C0E)
#define EVT_BLUE_GATT_NOTIFICATION                (0x0C0F)
#define EVT_BLUE_GATT_PROCEDURE_COMPLETE          (0x0C10)
#define EVT_BLUE_GATT_ERROR_RESP                  (0x0C11)
#define EVT_BLUE_GATT_DISC_READ_CHAR_BY_UUID_RESP (0x0C12)
#define EVT_BLUE_GATT_WRITE_PERMIT_REQ            (0x0C13)
#define EVT_BLUE_GATT_READ_PERMIT_REQ             (0x0C14)
#define EVT_BLUE_GATT_READ_MULTI_PERMIT_REQ       (0x0C15)
#define EVT_BLUE_GATT_TX_POOL_AVAILABLE           (0x0C16)
#define EVT_BLUE_GATT_SERVER_CONFIRMATION_EVENT   (0x0C17)
#define EVT_BLUE_GATT_PREPARE_WRITE_PERMIT_REQ    (0x0C18)
#define EVT_BLUE_GAP_LIMITED_DISCOVERABLE         (0x0400)
#define EVT_BLUE_GAP_PAIRING_CMPLT                (0x0401)
#define EVT_BLUE_GAP_PASS_KEY_REQUEST             (0x0402)
#define EVT_BLUE_GAP_AUTHORIZATION_REQUEST        (0x0403)
#define EVT_BLUE_GAP_SLAVE_SECURITY_INITIATED     (0X0404)
#define EVT_BLUE_GAP_BOND_LOST                    (0X0405)
#define EVT_BLUE_GAP_DEVICE_FOUND                 (0x0406)
#define EVT_BLUE_GAP_PROCEDURE_COMPLETE           (0x0407)
#define EVT_BLUE_GAP_ADDR_NOT_RESOLVED            (0x0408)
#define EVT_BLUE_GAP_NUMERIC_COMPARISON_VALUE     (0x0409)
#define EVT_BLUE_GAP_KEYPRESS_NOTIFICATION        (0x040A)
#define EVT_BLUE_L2CAP_CONNECTION_UPDATE_REQ      (0x0802)
#define EVT_BLUE_L2CAP_CONNECTION_UPDATE_RESP     (0x0800)


/* ------------------------------------------------------------------------- */


/* Bluetooth 48 bit address (in little-endian order).
 */
typedef	uint8_t	tBDAddr[6];


/* ------------------------------------------------------------------------- */


/* Min. ATT MTU size
 */
#define ATT_MTU                               23


/* ------------------------------------------------------------------------- */


/* Error Codes as specified by the specification 
 */
#define ERR_CMD_SUCCESS                              0x00
#define ERR_UNKNOWN_HCI_COMMAND	                     0x01
#define ERR_UNKNOWN_CONN_IDENTIFIER                  0x02
#define ERR_AUTH_FAILURE                             0x05
#define ERR_PIN_OR_KEY_MISSING                       0x06
#define ERR_MEM_CAPACITY_EXCEEDED                    0x07
#define ERR_CONNECTION_TIMEOUT                       0x08
#define ERR_COMMAND_DISALLOWED                       0x0C
#define ERR_UNSUPPORTED_FEATURE                      0x11
#define ERR_INVALID_HCI_CMD_PARAMS                   0x12
#define ERR_RMT_USR_TERM_CONN                        0x13
#define ERR_RMT_DEV_TERM_CONN_LOW_RESRCES            0x14
#define ERR_RMT_DEV_TERM_CONN_POWER_OFF              0x15
#define ERR_LOCAL_HOST_TERM_CONN                     0x16
#define ERR_UNSUPP_RMT_FEATURE                       0x1A
#define ERR_INVALID_LMP_PARAM                        0x1E
#define ERR_UNSPECIFIED_ERROR                        0x1F
#define ERR_LL_RESP_TIMEOUT                          0x22
#define ERR_LMP_PDU_NOT_ALLOWED                      0x24
#define ERR_INSTANT_PASSED                           0x28
#define ERR_PAIR_UNIT_KEY_NOT_SUPP                   0x29
#define ERR_CONTROLLER_BUSY                          0x3A
#define ERR_DIRECTED_ADV_TIMEOUT                     0x3C
#define ERR_CONN_END_WITH_MIC_FAILURE                0x3D
#define ERR_CONN_FAILED_TO_ESTABLISH                 0x3E


/* ------------------------------------------------------------------------- */


#define FLASH_READ_FAILED                            0x49
#define FLASH_WRITE_FAILED                           0x4A
#define FLASH_ERASE_FAILED                           0x4B


/* ------------------------------------------------------------------------- */


#endif /* BLE_LEGACY_H__ */
