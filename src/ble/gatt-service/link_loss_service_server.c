/*
 * Copyright (C) 2021 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "link_loss_service_server.c"

/**
 * Implementation of the GATT Battery Service Server 
 * To use with your application, add '#import <link_loss_service.gatt' to your .gatt file
 */

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#include "btstack_defines.h"
#include "btstack_event.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "btstack_util.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"

#include "ble/gatt-service/link_loss_service_server.h"
#include "btstack_run_loop.h"
#include "hci.h"

static att_service_handler_t    link_loss_service;
static btstack_packet_handler_t lls_server_callback;
static btstack_packet_callback_registration_t lls_server_hci_event_callback_registration;

typedef enum {
    LLS_SERVER_ALERT_STATE_IDLE = 0,
    LLS_SERVER_ALERT_STATE_ALERTING
} lls_server_alert_state_t;

static uint16_t 				lls_server_alert_level_handle;
static lls_alert_level_t 		lls_server_alert_level;

static lls_server_alert_state_t lls_server_alert_state;
static uint32_t                 lls_server_alert_timeout_ms;
static btstack_timer_source_t   lls_server_alert_timer;


static void lls_server_emit_alarm_started(void){
    btstack_assert(lls_server_callback != NULL);
    
    uint8_t event[4];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_LLS_SERVER_START_ALERTING;
    event[pos++] = (uint8_t)lls_server_alert_level;
    (*lls_server_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void lls_server_emit_alarm_stopped(bool stopped_due_timeout){
    btstack_assert(lls_server_callback != NULL);
    
    uint8_t event[5];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_LLS_SERVER_STOP_ALERTING;
    event[pos++] = (uint8_t)lls_server_alert_level;
    event[pos++] = stopped_due_timeout ? 1u : 0u;
    
    (*lls_server_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void lls_server_stop_alerting(void){
    btstack_run_loop_remove_timer(&lls_server_alert_timer);

    if (lls_server_alert_state == LLS_SERVER_ALERT_STATE_ALERTING){
        lls_server_emit_alarm_stopped(false);
        lls_server_alert_state = LLS_SERVER_ALERT_STATE_IDLE;
    }
} 

static void lls_server_timer_timeout_handler(btstack_timer_source_t * timer){
    UNUSED(timer);
    lls_server_emit_alarm_stopped(true);
    lls_server_alert_state = LLS_SERVER_ALERT_STATE_IDLE;
}

static void lls_server_start_alerting(void){
    if (lls_server_alert_state == LLS_SERVER_ALERT_STATE_ALERTING){
        return;
    }
    if (lls_server_alert_level == LLS_ALERT_LEVEL_NO_ALERT){
        return;
    }

    lls_server_alert_state = LLS_SERVER_ALERT_STATE_ALERTING;
    lls_server_emit_alarm_started();
    
    if (lls_server_alert_timeout_ms == 0){
        return;
    }

    btstack_run_loop_set_timer_handler(&lls_server_alert_timer, lls_server_timer_timeout_handler);
    btstack_run_loop_set_timer(&lls_server_alert_timer, lls_server_alert_timeout_ms);
    btstack_run_loop_add_timer(&lls_server_alert_timer);
}

static uint16_t lls_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
	UNUSED(con_handle);

	if (attribute_handle == lls_server_alert_level_handle){
		return att_read_callback_handle_byte((uint8_t)lls_server_alert_level, offset, buffer, buffer_size);
	}
	return 0;
}

static int lls_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
	UNUSED(transaction_mode);
	UNUSED(offset);
	UNUSED(con_handle);


	if (attribute_handle == lls_server_alert_level_handle){
		if (buffer_size != 1){
			return ATT_ERROR_VALUE_NOT_ALLOWED;
		} 
		
		lls_alert_level_t alert_level = (lls_alert_level_t)buffer[0];
		switch (alert_level){
			case LLS_ALERT_LEVEL_NO_ALERT:
    		case LLS_ALERT_LEVEL_MILD_ALERT:
    		case LLS_ALERT_LEVEL_HIGH_ALERT:
    			lls_server_alert_level = alert_level;
				break;
			default:
				return ATT_ERROR_VALUE_NOT_ALLOWED;
		}
	}
	return 0;
}

static void lls_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(packet);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    uint8_t reason;

    switch (hci_event_packet_get_type(packet)){
    	case HCI_EVENT_CONNECTION_COMPLETE:
	        // ignore if connection failed
	        if (hci_event_connection_complete_get_status(packet) != ERROR_CODE_SUCCESS){
	        	return;
	        }
	        lls_server_stop_alerting();
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            reason = hci_event_disconnection_complete_get_reason(packet);
            if (reason == ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION){
                lls_server_stop_alerting();
            } else {
                lls_server_start_alerting();
            }
            break;
        default:
            break;
    }
}

void link_loss_service_server_init(void){
	// get service handle range
	uint16_t start_handle = 0;
	uint16_t end_handle   = 0xffff;
	int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_LINK_LOSS, &start_handle, &end_handle);
	btstack_assert(service_found != 0);
	UNUSED(service_found);

	lls_server_alert_level   = LLS_ALERT_LEVEL_NO_ALERT;
    lls_server_alert_timeout_ms = 0;
    lls_server_alert_state = LLS_SERVER_ALERT_STATE_IDLE;

	// get characteristic value handle and client configuration handle
	lls_server_alert_level_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_ALERT_LEVEL);

#ifdef ENABLE_TESTING_SUPPORT
    printf("LLS 0x%02x - 0x%02x \n", start_handle, end_handle);
    printf("    alert_level            0x%02x \n", lls_server_alert_level_handle);
#endif
	log_info("Link Loss Service Server 0x%02x-0x%02x", start_handle, end_handle);

    // register service with ATT Server
	link_loss_service.start_handle   = start_handle;
	link_loss_service.end_handle     = end_handle;
	link_loss_service.read_callback  = &lls_server_read_callback;
	link_loss_service.write_callback = &lls_server_write_callback;
	
	att_server_register_service_handler(&link_loss_service);

	lls_server_hci_event_callback_registration.callback = &lls_server_packet_handler;
    hci_add_event_handler(&lls_server_hci_event_callback_registration);
}

void link_loss_service_server_register_packet_handler(btstack_packet_handler_t packet_handler){
	btstack_assert(packet_handler != NULL);
	lls_server_callback = packet_handler;
}

uint8_t link_loss_service_server_set_alert_level(lls_alert_level_t alert_level){
    if (alert_level >= LLS_ALERT_LEVEL_RFU){
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    }
	lls_server_alert_level = alert_level;
    return ERROR_CODE_SUCCESS;
}

uint8_t link_loss_service_server_set_alert_timeout(uint32_t alert_timeout_ms){
    if (lls_server_alert_state != LLS_SERVER_ALERT_STATE_IDLE){
        return BTSTACK_BUSY;
    }
    lls_server_alert_timeout_ms = alert_timeout_ms;
    return ERROR_CODE_SUCCESS;
}

void link_loss_service_server_stop_alerting(void){
    lls_server_stop_alerting();
}
