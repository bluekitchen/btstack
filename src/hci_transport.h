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

/**
 * @title HCI Transport
 *
 * The API allows BTstack to use different transport interfaces.
 *
 */

#ifndef HCI_TRANSPORT_H
#define HCI_TRANSPORT_H

#include "btstack_config.h"
#include "btstack_defines.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

    
/* API_START */

/* HCI packet types */
typedef struct {
    /**
     * transport name 
     */
    const char * name;

    /**
     * init transport
     * @param transport_config
     */
    void   (*init) (const void *transport_config);

    /**
     * open transport connection
     */
    int    (*open)(void);

    /**
     * close transport connection
     */
    int    (*close)(void);

    /**
     * register packet handler for HCI packets: ACL, SCO, and Events
     */
    void   (*register_packet_handler)(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size));

    /**
     * support async transport layers, e.g. IRQ driven without buffers
     */
    int    (*can_send_packet_now)(uint8_t packet_type);

    /**
     * send packet
     */
    int    (*send_packet)(uint8_t packet_type, uint8_t *packet, int size);

    /**
     * extension for UART transport implementations
     */
    int    (*set_baudrate)(uint32_t baudrate);

    /**
     * extension for UART H5 on CSR: reset BCSP/H5 Link
     */
    void   (*reset_link)(void);

    /**
     * extension for USB transport implementations: config SCO connections
     */
    void   (*set_sco_config)(uint16_t voice_setting, int num_connections);

} hci_transport_t;

typedef enum {
    HCI_TRANSPORT_CONFIG_UART,
    HCI_TRANSPORT_CONFIG_USB
} hci_transport_config_type_t;

typedef struct {
    hci_transport_config_type_t type;
} hci_transport_config_t;

typedef struct {
    hci_transport_config_type_t type; // == HCI_TRANSPORT_CONFIG_UART
    uint32_t   baudrate_init; // initial baud rate
    uint32_t   baudrate_main; // = 0: same as initial baudrate
    int        flowcontrol;   // 
    const char *device_name;
    int        parity;        // see btstack_uart.h BTSTACK_UART_PARITY
} hci_transport_config_uart_t;

/* API_END */
    
#if defined __cplusplus
}
#endif

#endif // HCI_TRANSPORT_H
