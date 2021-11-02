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

#define BTSTACK_FILE__ "btstack_port.c"

#define DEBUG

#include <string.h>

// to get cpu intrinsics, .e.g __dissble_irq(), before BTstack includes to avoid re-defining UNUSED
#include "stm32l4xx.h"

#include "controller.h"

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "hci_event.h"
#include "hci_transport.h"
#include "btstack_tlv.h"
#include "btstack_tlv_none.h"
#include "ble/le_device_db_tlv.h"

#ifdef ENABLE_SEGGER_RTT
#include "SEGGER_RTT.h"
#include "hci_dump_segger_rtt_stdout.h"
#else
#include "hci_dump_embedded_stdout.h"
#endif

void btstack_assert_failed(const char * file, uint16_t line_nr){
    printf("Assert: file %s, line %u\n", file, line_nr);
    while (1);
}

/** hal_time_ms.h */
#include "hal_time_ms.h"
uint32_t hal_time_ms(void){
    return HAL_GetTick();
}

/** hal_cpu.h */

// TODO: implement
void hal_cpu_disable_irqs(void){
    __disable_irq();
}

void hal_cpu_enable_irqs(void){
    __enable_irq();
}

void hal_cpu_enable_irqs_and_sleep(void){
    __enable_irq();
    // __asm__("wfe");	// go to sleep if event flag isn't set. if set, just clear it. IRQs set event flag
}

void btstack_main(void);
void btstack_port(void){

    // test code
    // lptim1_calibration();

    // Bring up BTstack
    printf("BlueKitchen Cinnamon Controller for Semtech SX1280\n");

    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    // initialize controller
    controller_init();

    // get virtual HCI transpoft
    const hci_transport_t * hci_transport = controller_get_hci_transport();

    // TODO: use flash storage

    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_none_init_instance();
    // setup global tlv
    btstack_tlv_set_instance(btstack_tlv_impl, NULL);

    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, NULL);

    // init HCI
    hci_init(hci_transport, NULL);

    // uncomment to enable packet logger
#ifdef ENABLE_SEGGER_RTT
    // hci_dump_init(hci_dump_segger_rtt_stdout_get_instance());
#else
    // hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif

    // hand over to btstack embedded code 
    btstack_main();

    // go
    btstack_run_loop_execute();

    while (1){};
}
