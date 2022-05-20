#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hci.h"
#include "l2cap.h"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_debug.h"
#include "bluetooth.h"

#include "mock_att_server.h"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

static att_service_handler_t * service;
static btstack_context_callback_registration_t * mock_callback_registration;

att_service_handler_t * mock_att_server_get_service(void){
    return service;
}

void mock_deinit(void){
    mock().clear();
    mock_callback_registration = NULL;
    service = NULL;
}

uint16_t mock_att_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    btstack_assert(service != NULL);
    btstack_assert(service->read_callback != NULL);
    return (service->read_callback)(con_handle, attribute_handle, offset, buffer, buffer_size);
}

uint16_t mock_att_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, const uint8_t *buffer, uint16_t buffer_size){
    btstack_assert(service != NULL);
    btstack_assert(service->write_callback != NULL);
    return (service->write_callback)(con_handle, attribute_handle, transaction_mode, offset, (uint8_t *) buffer, buffer_size);
}

void att_server_register_service_handler(att_service_handler_t * handler){
    service = handler;
}

void mock_att_service_trigger_can_send_now(void){
    btstack_assert(mock_callback_registration != NULL);
    (*mock_callback_registration->callback)(mock_callback_registration->context);
}

uint16_t att_server_get_mtu(hci_con_handle_t con_handle){
    UNUSED(con_handle);
    mock().actualCall("att_server_get_mtu");
    return ATT_DEFAULT_MTU;
}

uint8_t att_server_indicate(hci_con_handle_t con_handle, uint16_t attribute_handle, const uint8_t *value, uint16_t value_len){
    UNUSED(con_handle);
    UNUSED(attribute_handle);
    UNUSED(value);
    UNUSED(value_len);
    mock().actualCall("att_server_indicate");
    return ERROR_CODE_SUCCESS;
}

uint8_t att_server_notify(hci_con_handle_t con_handle, uint16_t attribute_handle, const uint8_t *value, uint16_t value_len){
    UNUSED(con_handle);
    UNUSED(attribute_handle);
    UNUSED(value);
    UNUSED(value_len);
    mock().actualCall("att_server_notify");
    return ERROR_CODE_SUCCESS;
}

uint8_t att_server_register_can_send_now_callback(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle){
    UNUSED(callback_registration);
    UNUSED(con_handle);
    mock_callback_registration = callback_registration;

    mock().actualCall("att_server_register_can_send_now_callback");
    return ERROR_CODE_SUCCESS;
}

uint8_t att_server_request_to_send_notification(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle){
    UNUSED(callback_registration);
    UNUSED(con_handle);
    mock().actualCall("att_server_request_to_send_notification");
    return ERROR_CODE_SUCCESS;
}

void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    UNUSED(callback_handler);
}

void l2cap_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    UNUSED(callback_handler);
}

uint16_t l2cap_max_mtu(void){
    return HCI_ACL_PAYLOAD_SIZE - L2CAP_HEADER_SIZE;
}

void l2cap_register_packet_handler(void (*handler)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
}