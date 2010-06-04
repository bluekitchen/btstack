/*
 * Copyright (C) 2009 by Matthias Ringwald
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
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *  hci_transport.h
 *
 *  HCI Transport API -- allows BT Daemon to use different transport protcols 
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */
#pragma once

#include <stdint.h>
#include <btstack/run_loop.h>

/* HCI packet types */
typedef struct {
    int    (*open)(void *transport_config);
    int    (*close)();
    int    (*send_cmd_packet)(uint8_t *packet, int size);
    int    (*send_acl_packet)(uint8_t *packet, int size);
    void   (*register_event_packet_handler)(void (*handler)(uint8_t *packet, int size));
    void   (*register_acl_packet_handler)  (void (*handler)(uint8_t *packet, int size));
    const char * (*get_transport_name)();
} hci_transport_t;

typedef struct {
    const char *device_name;
    uint32_t   baudrate;
    int   flowcontrol; // 
} hci_uart_config_t;

typedef struct {
    // unique usb device identifier
} hci_libusb_config_t;

// inline various hci_transport_X.h files
extern hci_transport_t * hci_transport_h4_instance();
extern hci_transport_t * hci_transport_h5_instance();
extern hci_transport_t * hci_transport_usb_instance();


