/**
 * Implementation of the nordic DFU ble port
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "btstack_defines.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "nrf_log.h"
#include "nrf_dfu_ble.h"
#include "nrf_dfu_utils.h"
#include "nrf_dfu_req_handler.h"
#include "nrf_dfu_handling_error.h"
#include "nrf_assert.h"
#include "nrf_dfu_validation.h"

typedef struct {
	uint16_t att_handle;
	uint8_t is_notify_en;
    hci_con_handle_t con_handle;
	uint8_t data[64];
	uint32_t size;
} nrf_dfu_notify_request_context_t;

typedef struct {
	uint16_t att_handle;
	uint8_t is_indicate_en;
    hci_con_handle_t con_handle;
	uint8_t data[64];
	uint32_t size;
} nrf_dfu_indicate_request_context_t;

static nrf_dfu_observer_t m_dfu_observer;
static att_service_handler_t  nrf_dfu_service;
static btstack_context_callback_registration_t send_request;

static nrf_dfu_ble_t dfu_ble;
static uint16_t m_pkt_notif_target;            /**< Number of packets of firmware data to be received before transmitting the next Packet Receipt Notification to the DFU Controller. */
static uint16_t m_pkt_notif_target_cnt;        /**< Number of packets of firmware data received after sending last Packet Receipt Notification or since the receipt of a @ref BLE_DFU_PKT_RCPT_NOTIF_ENABLED event from the DFU service, which ever occurs later.*/
static uint8_t  dfu_is_ctrl_pt_notify_enabled;
static uint8_t  dfu_is_buttonless_indicate_enabled;

static uint8_t  dfu_ctrl_point_value[32]  = {0};
static uint8_t  dfu_data_point_value[256] = {0};
static uint8_t  dfu_buttonless_value[16]  = {0};
static uint8_t  dfu_buttonless_bootloader_name[8];

static uint32_t on_ctrl_pt_write(nrf_dfu_ble_t * p_dfu, uint8_t *data, uint32_t size);
static void on_packet_write(nrf_dfu_ble_t * p_dfu, uint8_t *data, uint32_t size);
static void on_buttonless_write(nrf_dfu_ble_t * p_dfu, uint8_t *data, uint32_t size);

static uint16_t nrf_dfu_ble_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	UNUSED(con_handle);
	UNUSED(offset);
	UNUSED(buffer_size);

	if (attribute_handle == dfu_ble.dfu_ctrl_pt_handles.value_handle) {
        return att_read_callback_handle_blob((const uint8_t *)dfu_ctrl_point_value, sizeof(dfu_ctrl_point_value), offset, buffer, buffer_size);
	} else if (attribute_handle == dfu_ble.dfu_ctrl_pt_handles.cccd_handle) {
        return 0;
	} else if (attribute_handle == dfu_ble.dfu_pkt_handles.value_handle) {
        return att_read_callback_handle_blob((const uint8_t *)dfu_data_point_value, sizeof(dfu_data_point_value), offset, buffer, buffer_size);
	} else if (attribute_handle == dfu_ble.dfu_buttonless_handles.value_handle) {
        return att_read_callback_handle_blob((const uint8_t *)dfu_buttonless_value, sizeof(dfu_buttonless_value), offset, buffer, buffer_size);
	} else if (attribute_handle == dfu_ble.dfu_buttonless_handles.cccd_handle) {
        return 0;
	}
	return 0;
}

static int nrf_dfu_ble_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	UNUSED(transaction_mode);
	UNUSED(offset);
	UNUSED(buffer_size);

    dfu_ble.con_handle = con_handle;

	if (attribute_handle == dfu_ble.dfu_ctrl_pt_handles.value_handle) {
        on_ctrl_pt_write(&dfu_ble, buffer, buffer_size);
	} else if (attribute_handle == dfu_ble.dfu_ctrl_pt_handles.cccd_handle) {
		dfu_is_ctrl_pt_notify_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
	} else if (attribute_handle == dfu_ble.dfu_pkt_handles.value_handle) {
        on_packet_write(&dfu_ble, buffer, buffer_size);
	} else if (attribute_handle == dfu_ble.dfu_buttonless_handles.value_handle) {
		on_buttonless_write(&dfu_ble, buffer, buffer_size);
	} else if (attribute_handle == dfu_ble.dfu_buttonless_handles.cccd_handle) {
		dfu_is_buttonless_indicate_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION;
	}
	return 0;
}

/** 
 * @brief Queue send request. When called, one packet can be send via nrf_dfu_ble_send below
 * @param request
 * @param con_handle
 */
static void nrf_dfu_ble_notify_request(void (*cb)(void *context), void *context, hci_con_handle_t con_handle) {
	send_request.callback = cb;
	send_request.context = context;
	att_server_request_to_send_notification(&send_request, con_handle);
}

/** 
 * @brief Queue send request. When called, one packet can be send via nrf_dfu_ble_send below
 * @param request
 * @param con_handle
 */
static void nrf_dfu_ble_indicate_request(void (*cb)(void *context), void *context, hci_con_handle_t con_handle) {
	send_request.callback = cb;
	send_request.context = context;
	att_server_request_to_send_indication(&send_request, con_handle);
}

/**
 * @brief Init nrf DFU Service Server with ATT DB
 * @param callback for events and data from dfu controller
 */
uint32_t nrf_dfu_ble_init(nrf_dfu_observer_t observer) {
	static const uint16_t dfu_profile_uuid16      = 0xFE59;
	static const uint8_t dfu_ctrl_point_uuid128[] = { 0x8E, 0xC9, 0x00, 0x01, 0xF3, 0x15, 0x4F, 0x60, 0x9F, 0xB8, 0x83, 0x88, 0x30, 0xDA, 0xEA, 0x50 };
	static const uint8_t dfU_data_point_uuid128[] = { 0x8E, 0xC9, 0x00, 0x02, 0xF3, 0x15, 0x4F, 0x60, 0x9F, 0xB8, 0x83, 0x88, 0x30, 0xDA, 0xEA, 0x50 };
	static const uint8_t dfu_buttonless_uuid128[] = { 0x8E, 0xC9, 0x00, 0x03, 0xF3, 0x15, 0x4F, 0x60, 0x9F, 0xB8, 0x83, 0x88, 0x30, 0xDA, 0xEA, 0x50 };

	m_dfu_observer = observer;

	// get service handle range
	uint16_t start_handle = 0;
	uint16_t end_handle   = 0xffff;
	int service_found = gatt_server_get_handle_range_for_service_with_uuid16(dfu_profile_uuid16, &start_handle, &end_handle);
	ASSERT(service_found != 0);
	UNUSED(service_found);

	// get characteristic value handle and client configuration handle
	dfu_ble.dfu_ctrl_pt_handles.value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(start_handle, end_handle, dfu_ctrl_point_uuid128);
	dfu_ble.dfu_ctrl_pt_handles.cccd_handle  = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(start_handle, end_handle, dfu_ctrl_point_uuid128);
	dfu_ble.dfu_pkt_handles.value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(start_handle, end_handle, dfU_data_point_uuid128);
	dfu_ble.dfu_buttonless_handles.value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid128(start_handle, end_handle, dfu_buttonless_uuid128);
	dfu_ble.dfu_buttonless_handles.cccd_handle  = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(start_handle, end_handle, dfu_buttonless_uuid128);

	NRF_LOG_INFO("dfu_ctrl_point_value_handle 	0x%02x", dfu_ble.dfu_ctrl_pt_handles.value_handle);
	NRF_LOG_INFO("dfu_ctrl_point_cccd_handle 	0x%02x", dfu_ble.dfu_ctrl_pt_handles.cccd_handle);
	NRF_LOG_INFO("dfu_data_point_value_handle 	0x%02x", dfu_ble.dfu_pkt_handles.value_handle);
	NRF_LOG_INFO("dfu_buttonless_value_handle 	0x%02x", dfu_ble.dfu_buttonless_handles.value_handle);
	NRF_LOG_INFO("dfu_buttonless_cccd_handle 	0x%02x", dfu_ble.dfu_buttonless_handles.cccd_handle);
	
	// register service with ATT Server
	nrf_dfu_service.start_handle   = start_handle;
	nrf_dfu_service.end_handle     = end_handle;
	nrf_dfu_service.read_callback  = &nrf_dfu_ble_read_callback;
	nrf_dfu_service.write_callback = &nrf_dfu_ble_write_callback;
	att_server_register_service_handler(&nrf_dfu_service);

    return NRF_SUCCESS;
}

/**@brief Function for encoding the beginning of a response.
 *
 * @param[inout] p_buffer  The buffer to encode into.
 * @param[in]    op_code   The opcode of the response.
 * @param[in]    result    The result of the operation.
 *
 * @return The length added to the buffer.
 */
static uint32_t response_prepare(uint8_t * p_buffer, uint8_t op_code, uint8_t result)
{
    ASSERT(p_buffer);
    p_buffer[0] = NRF_DFU_OP_RESPONSE;
    p_buffer[1] = op_code;
    p_buffer[2] = result;
    return RESPONSE_HEADER_LEN;
}

/**@brief Function for encoding a select object response into a buffer.
 *
 * The select object response consists of a maximum object size, a firmware offset, and a CRC value.
 *
 * @param[inout] p_buffer   The buffer to encode the response into.
 * @param[in]    max_size   The maximum object size value to encode.
 * @param[in]    fw_offset  The firmware offset value to encode.
 * @param[in]    crc        The CRC value to encode.
 *
 * @return The length added to the buffer.
 */
static uint32_t response_select_obj_add(uint8_t  * p_buffer,
                                        uint32_t   max_size,
                                        uint32_t   fw_offset,
                                        uint32_t   crc)
{
    uint16_t offset = uint32_encode(max_size,  &p_buffer[RESPONSE_HEADER_LEN]);
    offset         += uint32_encode(fw_offset, &p_buffer[RESPONSE_HEADER_LEN + offset]);
    offset         += uint32_encode(crc,       &p_buffer[RESPONSE_HEADER_LEN + offset]);
    return offset;
}

/**@brief Function for encoding a CRC response into a buffer.
 *
 * The CRC response consists of a firmware offset and a CRC value.
 *
 * @param[inout] p_buffer   The buffer to encode the response into.
 * @param[in]    fw_offset  The firmware offset value to encode.
 * @param[in]    crc        The CRC value to encode.
 *
 * @return The length added to the buffer.
 */
static uint32_t response_crc_add(uint8_t * p_buffer, uint32_t fw_offset, uint32_t crc)
{
    uint16_t offset = uint32_encode(fw_offset, &p_buffer[RESPONSE_HEADER_LEN]);
    offset         += uint32_encode(crc,       &p_buffer[RESPONSE_HEADER_LEN + offset]);
    return offset;
}

/**@brief Function for appending an extended error code to the response buffer.
 *
 * @param[inout] p_buffer    The buffer to append the extended error code to.
 * @param[in]    result      The error code to append.
 * @param[in]    buf_offset  The current length of the buffer.
 *
 * @return The length added to the buffer.
 */
static uint32_t response_ext_err_payload_add(uint8_t * p_buffer, uint8_t result, uint32_t buf_offset)
{
    p_buffer[buf_offset] = ext_error_get();
    (void) ext_error_set(NRF_DFU_EXT_ERROR_NO_ERROR);
    return 1;
}

static void nrf_dfu_req_response_send_cb(void *context) {
	if (NULL == context)
		return;

	nrf_dfu_notify_request_context_t *req_context = (nrf_dfu_notify_request_context_t *)context;
	if (req_context->is_notify_en)
		att_server_notify(req_context->con_handle, req_context->att_handle, req_context->data, req_context->size);

    NRF_LOG_DEBUG("Freeing req context:%p", req_context);
    free((void *)req_context);
}

static ret_code_t nrf_dfu_req_response_send(uint8_t * p_buf, uint16_t len)
{
    nrf_dfu_notify_request_context_t *context;

    context = (nrf_dfu_notify_request_context_t *)malloc(sizeof(nrf_dfu_notify_request_context_t));
    memset(context, 0, sizeof(nrf_dfu_notify_request_context_t));
    context->att_handle = dfu_ble.dfu_ctrl_pt_handles.value_handle;
    context->is_notify_en = dfu_is_ctrl_pt_notify_enabled;
    memcpy(context->data, p_buf, len);
    context->size = len;
    context->con_handle = dfu_ble.con_handle;
    nrf_dfu_ble_notify_request(nrf_dfu_req_response_send_cb, context, context->con_handle);
    return 0;
}

static void nrf_dfu_req_handler_callback(nrf_dfu_response_t * p_res, void * p_context)
{
    ASSERT(p_res);
    ASSERT(p_context);

    uint8_t len = 0;
    uint8_t buffer[MAX_RESPONSE_LEN] = {0};

    if (p_res->request == NRF_DFU_OP_OBJECT_WRITE)
    {
        --m_pkt_notif_target_cnt;
        if ((m_pkt_notif_target == 0) || (m_pkt_notif_target_cnt && m_pkt_notif_target > 0))
        {
            return;
        }

        /* Reply with a CRC message and reset the packet counter. */
        m_pkt_notif_target_cnt = m_pkt_notif_target;

        p_res->request = NRF_DFU_OP_CRC_GET;
    }

    len += response_prepare(buffer, p_res->request, p_res->result);

    if (p_res->result != NRF_DFU_RES_CODE_SUCCESS)
    {
        NRF_LOG_WARNING("DFU request %d failed with error: 0x%x", p_res->request, p_res->result);

        if (p_res->result == NRF_DFU_RES_CODE_EXT_ERROR)
        {
            len += response_ext_err_payload_add(buffer, p_res->result, len);
        }

        (void) nrf_dfu_req_response_send(buffer, len);
        return;
    }

    switch (p_res->request)
    {
        case NRF_DFU_OP_OBJECT_CREATE:
        case NRF_DFU_OP_OBJECT_EXECUTE:
            break;

        case NRF_DFU_OP_OBJECT_SELECT:
        {
            len += response_select_obj_add(buffer,
                                           p_res->select.max_size,
                                           p_res->select.offset,
                                           p_res->select.crc);
        } break;

        case NRF_DFU_OP_OBJECT_WRITE:
        {
            len += response_crc_add(buffer, p_res->write.offset, p_res->write.crc);
        } break;

        case NRF_DFU_OP_CRC_GET:
        {
            len += response_crc_add(buffer, p_res->crc.offset, p_res->crc.crc);
        } break;

        default:
        {
            // No action.
        } break;
    }

    (void) nrf_dfu_req_response_send(buffer, len);
}

static void on_flash_write(void * p_buf)
{
    NRF_LOG_DEBUG("flash operation complete! freeing buffer %p", p_buf);
    free(p_buf);
}

static uint32_t on_ctrl_pt_write(nrf_dfu_ble_t * p_dfu, uint8_t *data, uint32_t size)
{
    //lint -save -e415 -e416 : Out-of-bounds access on p_ble_write_evt->data
    nrf_dfu_request_t request =
    {
        .request           = data[0],
        .p_context         = p_dfu,
        .callback.response = nrf_dfu_req_handler_callback,
    };

    switch (request.request)
    {
        case NRF_DFU_OP_OBJECT_SELECT:
        {
            /* Set object type to read info about */
            request.select.object_type = data[1];
        } break;

        case NRF_DFU_OP_OBJECT_CREATE:
        {
            /* Reset the packet receipt notification on create object */
            m_pkt_notif_target_cnt = m_pkt_notif_target;

            request.create.object_type = data[1];
            request.create.object_size = uint32_decode(&data[2]);

            if (request.create.object_type == NRF_DFU_OBJ_TYPE_COMMAND)
            {
                /* Activity on the current transport. Close all except the current one. */
                //(void) nrf_dfu_transports_close(&ble_dfu_transport);
            }
        } break;

        case NRF_DFU_OP_RECEIPT_NOTIF_SET:
        {
            NRF_LOG_DEBUG("Set receipt notif");

            m_pkt_notif_target     = uint16_decode(&data[1]);
            m_pkt_notif_target_cnt = m_pkt_notif_target;
        } break;

        default:
            break;
    }
    //lint -restore : Out-of-bounds access

    return nrf_dfu_req_handler_on_req(&request);
}

static void on_packet_write(nrf_dfu_ble_t * p_dfu, uint8_t *data, uint32_t size)
{
    /* Allocate a buffer to receive data. */
    uint8_t * p_balloc_buf = (uint8_t *)malloc(size);
    if (p_balloc_buf == NULL)
    {
        /* Operations are retried by the host; do not give up here. */
        NRF_LOG_WARNING("cannot allocate memory buffer!");
        return;
    }

    NRF_LOG_DEBUG("Buffer %p acquired, len %d (%d)",
                  p_balloc_buf, size, MAX_DFU_PKT_LEN);

    /* Copy payload into buffer. */
    memcpy(p_balloc_buf, data, size);

    /* Set up the request. */
    nrf_dfu_request_t request =
    {
        .request      = NRF_DFU_OP_OBJECT_WRITE,
        .p_context    = p_dfu,
        .callback     =
        {
            .response = nrf_dfu_req_handler_callback,
            .write    = on_flash_write,
        }
    };

    /* Set up the request buffer. */
    request.write.p_data   = p_balloc_buf;
    request.write.len      = size;

    /* Schedule handling of the request. */
    ret_code_t rc = nrf_dfu_req_handler_on_req(&request);
    if (rc != NRF_SUCCESS)
    {
        /* The error is logged in nrf_dfu_req_handler_on_req().
         * Free the buffer.
         */
        (void) free(p_balloc_buf);
    }
}

static void nrf_dfu_buttonless_response_send_cb(void *context) {
    if (NULL == context)
        return;

    nrf_dfu_indicate_request_context_t *req_context = (nrf_dfu_indicate_request_context_t *)context;
    if (req_context->is_indicate_en) {
        att_server_indicate(req_context->con_handle, req_context->att_handle, req_context->data, req_context->size);
        if (req_context->data[1] == NRF_DFU_BUT_CMD_CHANGE_BOOTLOADER_NAME) {
            m_dfu_observer(NRF_DFU_EVT_CHANGE_BOOTLOADER_NAME, dfu_buttonless_bootloader_name, sizeof(dfu_buttonless_bootloader_name));
        } else if (req_context->data[1] == NRF_DFU_BUT_CMD_ENTR_BOOTLOADER) {
            m_dfu_observer(NRF_DFU_EVT_ENTER_BOOTLOADER_MODE, NULL, 0);
        }
    }
    NRF_LOG_DEBUG("Freeing req context:%p", req_context);
    free((void *)req_context);
}

static void nrf_dfu_buttonless_response_send(uint8_t * p_buf, uint16_t len)
{
    nrf_dfu_indicate_request_context_t *context;

    context = (nrf_dfu_indicate_request_context_t *)malloc(sizeof(nrf_dfu_indicate_request_context_t));
    memset(context, 0, sizeof(nrf_dfu_indicate_request_context_t));
    context->att_handle = dfu_ble.dfu_buttonless_handles.value_handle;
    context->is_indicate_en = dfu_is_buttonless_indicate_enabled;
    memcpy(context->data, p_buf, len);
    context->size = len;
    context->con_handle = dfu_ble.con_handle;
    nrf_dfu_ble_indicate_request(nrf_dfu_buttonless_response_send_cb, context, context->con_handle);
}

static void on_buttonless_write(nrf_dfu_ble_t * p_dfu, uint8_t *data, uint32_t size)
{
    nrf_dfu_buttonless_rsp_t rsp = {
        .rsp_code = NRF_DFU_BUTTONLESS_RSP,
        .cmd_code = 0,
        .result = NRF_DFU_RES_CODE_SUCCESS
    };

    UNUSED(p_dfu);
    if (NULL == data || 0 == size) {
        NRF_LOG_ERROR("Invalid param");
        return;
    }

    if (data[0] == NRF_DFU_BUT_CMD_ENTR_BOOTLOADER) {
        rsp.cmd_code = NRF_DFU_BUT_CMD_ENTR_BOOTLOADER;
    } else if (data[0] == NRF_DFU_BUT_CMD_CHANGE_BOOTLOADER_NAME) {
        memcpy(dfu_buttonless_bootloader_name, &data[2], data[1]);
        rsp.cmd_code = NRF_DFU_BUT_CMD_CHANGE_BOOTLOADER_NAME;
    }
    nrf_dfu_buttonless_response_send((uint8_t *)&rsp, sizeof(nrf_dfu_buttonless_rsp_t));
}