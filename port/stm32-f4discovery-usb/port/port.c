/*
 * Copyright (C) 2020 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "port.c"

// include STM32 first to avoid warning about redefinition of UNUSED
#include "stm32f4xx_hal.h"
#include "main.h"

#include "port.h"

#include <stdio.h>
#include <stddef.h>

#include "port.h"
#include "btstack.h"
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop_embedded.h"
#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hal_flash_bank_stm32.h"
#include "hci_transport.h"
#include "hci_transport_h2_stm32.h"

#ifdef ENABLE_SEGGER_RTT
#include "SEGGER_RTT.h"
#include "hci_dump_segger_rtt_stdout.h"
#else
#include "hci_dump_embedded_stdout.h"
#endif

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;
static hal_flash_bank_stm32_t   hal_flash_bank_context;

// hal_time_ms.h
#include "hal_time_ms.h"
uint32_t hal_time_ms(void){
    return HAL_GetTick();
}

// hal_cpu.h implementation
#include "hal_cpu.h"

void hal_cpu_disable_irqs(void){
    __disable_irq();
}

void hal_cpu_enable_irqs(void){
    __enable_irq();
}

void hal_cpu_enable_irqs_and_sleep(void){
    __enable_irq();
#if 0
    // temp disable until effect on RTT is clear
    // go to sleep if event flag isn't set. if set, just clear it. IRQs set event flag
    //  __asm__("wfe");
#endif
}

#define HAL_FLASH_BANK_SIZE (128 * 1024)
#define HAL_FLASH_BANK_0_ADDR 0x080C0000
#define HAL_FLASH_BANK_1_ADDR 0x080E0000
#define HAL_FLASH_BANK_0_SECTOR FLASH_SECTOR_10
#define HAL_FLASH_BANK_1_SECTOR FLASH_SECTOR_11

int btstack_main(int argc, char ** argv);

// main.c
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            gap_local_bd_addr(local_addr);
            printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
            break;
        default:
            break;
    }
}

void btstack_assert_failed(const char * file, uint16_t line_nr){
    printf("ASSERT in %s, line %u failed - HALT\n", file, line_nr);
    while(1);
}

void port_main(void){

    printf("BTstack on STM32 F4 Discovery with USB support starting...\n");

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // uncomment to enable packet logger
#ifdef ENABLE_SEGGER_RTT
    // hci_dump_init(hci_dump_segger_rtt_stdout_get_instance());
#else
    // hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif

    // init HCI
    hci_init(hci_transport_h2_stm32_instance(), NULL);

    // setup TLV Flash Sector implementation
    const hal_flash_bank_t * hal_flash_bank_impl = hal_flash_bank_stm32_init_instance(
            &hal_flash_bank_context,
            HAL_FLASH_BANK_SIZE,
            HAL_FLASH_BANK_0_SECTOR,
            HAL_FLASH_BANK_1_SECTOR,
            HAL_FLASH_BANK_0_ADDR,
            HAL_FLASH_BANK_1_ADDR);
    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(
            &btstack_tlv_flash_bank_context,
            hal_flash_bank_impl,
            &hal_flash_bank_context);

    // setup global tlv
    btstack_tlv_set_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);

    // setup Link Key DB using TLV
    const btstack_link_key_db_t * btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
    hci_set_link_key_db(btstack_link_key_db);

    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, &btstack_tlv_flash_bank_context);

#ifdef HAVE_HAL_AUDIO
    // setup audio
   	btstack_audio_sink_set_instance(btstack_audio_embedded_sink_get_instance());
    btstack_audio_source_set_instance(btstack_audio_embedded_source_get_instance());
#endif

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // hand over to btstack embedded code
    btstack_main(0, NULL);

    // go
    btstack_run_loop_execute();
}
