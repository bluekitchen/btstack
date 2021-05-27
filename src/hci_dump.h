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
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/**
 * @title HCI Logging
 *
 * Dump HCI trace as BlueZ's hcidump format, Apple's PacketLogger, or stdout.
 *
 */

#ifndef HCI_DUMP_H
#define HCI_DUMP_H

#include <stdint.h>
#include <stdarg.h>       // for va_list
#include "btstack_bool.h"

#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

#if defined __cplusplus
extern "C" {
#endif

#define HCI_DUMP_LOG_LEVEL_DEBUG 0
#define HCI_DUMP_LOG_LEVEL_INFO  1
#define HCI_DUMP_LOG_LEVEL_ERROR 2

#define HCI_DUMP_HEADER_SIZE_PACKETLOGGER 13
#define HCI_DUMP_HEADER_SIZE_BLUEZ        13

/* API_START */

typedef enum {
    HCI_DUMP_INVALID = 0,
    HCI_DUMP_BLUEZ,
    HCI_DUMP_PACKETLOGGER,
} hci_dump_format_t;

typedef struct {
    // reset output, called if max packets is reached, to limit file size
    void (*reset)(void);
    // log packet
    void (*log_packet)(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
    // log message
    void (*log_message)(const char * format, va_list argptr);
#ifdef __AVR__ \
    // log message - AVR
    void (*log_message_P)(PGM_P * format, va_list argptr);
#endif
} hci_dump_t;

/**
 * @brief Init HCI Dump
 * @param hci_dump_impl - platform-specific implementation
 */
void hci_dump_init(const hci_dump_t * hci_dump_impl);

/**
 * @brief Enable packet logging
 * @param enabled default: true
 */
void hci_dump_enable_packet_log(bool enabled);

/**
 * @brief
 */
void hci_dump_enable_log_level(int log_level, int enable);

/*
 * @brief Set max number of packets - output file might be truncated
 */
void hci_dump_set_max_packets(int packets); // -1 for unlimited

/**
 * @brief Dump Packet
 * @param packet_type
 * @param in is 1 for incoming, 0 for outoing
 * @param packet
 * @param len
 */
void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);

/**
 * @brief Dump Message
 * @param log_level
 * @param format
 */
void hci_dump_log(int log_level, const char * format, ...)
#ifdef __GNUC__
__attribute__ ((format (__printf__, 2, 3)))
#endif
;

#ifdef __AVR__
/*
 * @brief Dump Message using format string stored in PGM memory (allows to save RAM)
 * @param log_level
 * @param format
 */
void hci_dump_log_P(int log_level, PGM_P format, ...)
#ifdef __GNUC__
__attribute__ ((format (__printf__, 2, 3)))
#endif
;
#endif

/**
 * @brief Setup header for PacketLogger format
 * @param buffer
 * @param tv_sec
 * @param tv_us
 * @param packet_type
 * @param in
 * @param len
 */
void hci_dump_setup_header_packetlogger(uint8_t * buffer, uint32_t tv_sec, uint32_t tv_us, uint8_t packet_type, uint8_t in, uint16_t len);
/**
 * @brief Setup header for BLUEZ (hcidump) format
 * @param buffer
 * @param tv_sec
 * @param tv_us
 * @param packet_type
 * @param in
 * @param len
 */
void hci_dump_setup_header_bluez(uint8_t * buffer, uint32_t tv_sec, uint32_t tv_us, uint8_t packet_type, uint8_t in, uint16_t len);

/* API_END */


#if defined __cplusplus
}
#endif
#endif // HCI_DUMP_H
