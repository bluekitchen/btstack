/*******************************************************************************
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

#include <string.h>
#include <strings.h>
#include <assert.h>

#include "debug.h"
#include "hopping.h"

#define GET_BIT( b, n )  ((b[(n&UINT32_C(0xFFFFFFF8))>>3]>>(n&0x7))&1)

void hopping_init( hopping_t *c ) {
    assert( c != NULL );

    memset( c, 0, sizeof(hopping_t) );
}

void hopping_set_channel_map( hopping_t *c, const uint8_t *chm ) {
    assert( c != NULL );
    assert( chm != NULL );

    memcpy( c->chMap, chm, 5 );

    c->chCnt = 0;
    for(int i=0; i<37; ++i) {
        LOG_DBG("%d(%d)\n", i, (i&UINT32_C(0xFFFFFFF8))>>3 );
        LOG_DBG("%d(%d)\n", i, GET_BIT( c->chMap, i ) );
        if( 1 == GET_BIT( c->chMap, i ) ) {
            c->chRemap[c->chCnt++] = i;
        }
    }

//    for(int i=0; i<c->chCnt; ++i)
//        printf("%d\n", c->chRemap[i] );

//    printf("chCnt: %d\n", c->chCnt );
}

void hopping_csa1_set_hop_increment( hopping_t *c, uint8_t hopIncrement ){
    assert( c != NULL );

    c->hopIncrement = hopIncrement;
}

uint8_t hopping_csa1_get_next_channel( hopping_t *c ) {
    assert( c != NULL );

    c->currentCh = (c->currentCh + c->hopIncrement) % 37;
    if( 1 == GET_BIT( c->chMap, c->currentCh ) ) {
        return c->currentCh;
    } else {
        return c->chRemap[ c->currentCh % c->chCnt ];
    }
}

// channelIdentifier = (Access Address31-16) XOR (Access Address15-0)
void hopping_csa2_set_access_address( hopping_t *c, uint32_t accessAddress ){
    assert( c != NULL );

    c->channelIdentifier = (accessAddress >> 16) ^ (accessAddress & 0xffff);
}

// @TODO rbit intrinsic could be used
static uint16_t hopping_csa2_permutation(uint16_t v){
    v = ((v >> 1) & 0x5555) | ((v & 0x5555) << 1);
    v = ((v >> 2) & 0x3333) | ((v & 0x3333) << 2);
    v = ((v >> 4) & 0x0F0F) | ((v & 0x0F0F) << 4);
    return v;
}

static uint16_t hopping_csa2_mam(uint16_t a, uint16_t b){
    return (17 * a) + b;
}

static uint16_t hopping_csa2_prn_e(hopping_t *c, uint16_t counter){
    assert( c != NULL );

    // see Bluetooth Spec v5, 4.5.8.3.3 Unmapped Event Channel Selection
    uint16_t v = counter ^ c->channelIdentifier;
    v = hopping_csa2_permutation (v);
    v = hopping_csa2_mam(v, c->channelIdentifier);
    v = hopping_csa2_permutation (v);
    v = hopping_csa2_mam(v, c->channelIdentifier);
    v = hopping_csa2_permutation (v);
    v = hopping_csa2_mam(v, c->channelIdentifier);
    v = v ^ c->channelIdentifier;
    return v;
}

uint8_t hopping_csa2_get_channel_for_counter( hopping_t *c, uint16_t counter ){
    assert( c != NULL );

    uint16_t prn_e = hopping_csa2_prn_e( c, counter);
    uint16_t unmapped_channel = prn_e % 37;
    if ( 1 == GET_BIT( c->chMap, unmapped_channel)){
        return unmapped_channel;
    } else {
        uint16_t remappingIndex = (((uint32_t) c->chCnt) * prn_e) >> 16;
        return c->chRemap[ remappingIndex ];
    }
}





