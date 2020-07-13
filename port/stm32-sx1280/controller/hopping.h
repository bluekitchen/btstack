/*******************************************************************************
 *
 *      queue
 *
 *      Copyright (c) 2018, Raccon BLE Sniffer
 *      All rights reserved.
 *
 *      Redistribution and use in source and binary forms, with or without
 *      modification, are permitted provided that the following conditions are
 *      met:
 *      
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following disclaimer
 *        in the documentation and/or other materials provided with the
 *        distribution.
 *      * Neither the name of "btlejack2" nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *      
 *      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *      "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *      LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *      A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *      OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *      SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *      LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *      DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *      THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *      (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *      OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************/

#include <stdint.h>


typedef struct {
    uint8_t chMap[5];
    uint8_t hopIncrement;
    uint8_t chRemap[37];
    uint8_t chCnt;
    // csa1
    uint8_t currentCh;
    // csa2
    uint16_t channelIdentifier;
} hopping_t;


/**
 * @brief init hopping instance
 */
void hopping_init( hopping_t *c );

/*
 * @brief set list of used channels
 * @param channel_map
 */
void hopping_set_channel_map( hopping_t *c, const uint8_t *chm);

// Channel Selection Algorithm #1

/* 
 * @brief set hop increment
 */
void hopping_csa1_set_hop_increment( hopping_t *c, uint8_t hopIncrement );

/**
 * @brief get next channel
 */
uint8_t hopping_csa1_get_next_channel( hopping_t *c );

// Channel Selection Algorithm #2

/* 
 * @brief init csa 2 algorithm
 */
void hopping_csa2_set_access_address( hopping_t *c, uint32_t accessAddress );

/**
 * @brief get next channel
 */
uint8_t hopping_csa2_get_channel_for_counter( hopping_t *c, uint16_t counter );

