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

#define BTSTACK_FILE__ "sx1280_ble_demo.c"

// *****************************************************************************
/* EXAMPLE_START(sx1280_ble_demo): LE Peripheral - Control LED over GATT      */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stm32l4xx_hal.h"

#include "btstack.h"
#include "gatt_rgb.h"

#define LED_SMALL_PORT GPIOB
#define LED_SMALL_PIN  GPIO_PIN_8

#define LED_R_PORT GPIOH
#define LED_R_PIN  GPIO_PIN_0
#define LED_G_PORT GPIOH
#define LED_G_PIN  GPIO_PIN_1
#define LED_B_PORT GPIOC
#define LED_B_PIN  GPIO_PIN_1

#define HEARTBEAT_PERIOD_MS 2000
#define HEARTBEAT_BLINK_MS   200

static btstack_timer_source_t heartbeat;

static void  heartbeat_handler_led_on(struct btstack_timer_source *ts);
static void  heartbeat_handler_led_off(struct btstack_timer_source *ts);

const uint8_t adv_data[] = {
        // Flags general discoverable, BR/EDR not supported
        0x02, 0x01, 0x06,
        // Service 16-bit: FFF0
        0x03, 0x02, 0xF0, 0xFF,
        // Manufacturer specific data (aa bb - address 78:A5:04:82:1B:A4)
        0x09, 0xFF, 0x04, 0x1E, 0x78, 0xA5, 0x04, 0x82, 0x1A, 0xAA,
        // Name
        0x08, 0x09, 'D', 'E', 'L', 'I', 'G', 'H', 'T',
};
const uint8_t adv_data_len = sizeof(adv_data);

static void heartbeat_handler_led_off(struct btstack_timer_source *ts){
    HAL_GPIO_WritePin(LED_SMALL_PORT, LED_SMALL_PIN, GPIO_PIN_SET);
    heartbeat.process = &heartbeat_handler_led_on;
    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS - HEARTBEAT_BLINK_MS);
    btstack_run_loop_add_timer(ts);
}

static void heartbeat_handler_led_on(struct btstack_timer_source *ts){
    HAL_GPIO_WritePin(LED_SMALL_PORT, LED_SMALL_PIN, GPIO_PIN_RESET);
    printf("BLINK\n");
    heartbeat.process = &heartbeat_handler_led_off;
    btstack_run_loop_set_timer(ts, HEARTBEAT_BLINK_MS);
    btstack_run_loop_add_timer(ts);
} 

static void set_rgb(uint8_t r, uint8_t g, uint8_t b){
    printf("R: %u,  G: %u, B: %u\n", r, g, b);
    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, r > 128 ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, g > 128 ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_B_PORT, LED_B_PIN, b > 128 ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)){
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            printf("Connected\n");
                            btstack_run_loop_remove_timer(&heartbeat);
                            HAL_GPIO_WritePin(LED_SMALL_PORT, LED_SMALL_PIN, GPIO_PIN_RESET);
                            break;
                        default:
                            break;
                    }
                    break;
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    printf("Disconnected\n");
                    // restart heartbeat
                    set_rgb(0,0,0);
                    heartbeat_handler_led_on(&heartbeat);
                    break;
            }
            break;
    }
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer_size);

    if (att_handle == ATT_CHARACTERISTIC_FFF3_01_VALUE_HANDLE) {
        set_rgb(buffer[4], buffer[5], buffer[6]);
    }
    return 0;
}

int btstack_main(void);
int btstack_main(void)
{
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);
    
    // register for ATT event
    att_server_register_packet_handler(packet_handler);

    // set one-shot timer
    heartbeat.process = &heartbeat_handler_led_on;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);

    // LEDs init
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // enable GPIO for Port H
    __HAL_RCC_GPIOH_CLK_ENABLE();

    GPIO_InitStruct.Pin = LED_SMALL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_SMALL_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_R_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_R_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_G_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_G_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_B_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_B_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(LED_SMALL_PORT, LED_SMALL_PIN, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LED_R_PORT, LED_R_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_G_PORT, LED_G_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_B_PORT, LED_B_PIN, GPIO_PIN_SET);


    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */
