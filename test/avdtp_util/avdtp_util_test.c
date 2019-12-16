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
 
// *****************************************************************************
//
// test rfcomm query tests
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/avdtp_util.h"
#include "btstack.h"
#include "classic/avdtp.h"
#include "classic/avdtp_util.h"


// mock start
extern "C" uint16_t l2cap_get_remote_mtu_for_local_cid(uint16_t local_cid){
    return 1024;
}
extern "C" void l2cap_request_can_send_now_event(uint16_t local_cid){}
// mock end

static avdtp_connection_t connection;
static avdtp_capabilities_t caps;

TEST_GROUP(AvdtpUtil){
};

// typedef enum {
//     AVDTP_SERVICE_CATEGORY_INVALID_0 = 0x00,
//     AVDTP_MEDIA_TRANSPORT = 0X01,
//     AVDTP_REPORTING,
//     AVDTP_RECOVERY,
//     AVDTP_CONTENT_PROTECTION, //4
//     AVDTP_HEADER_COMPRESSION, //5
//     AVDTP_MULTIPLEXING,       //6
//     AVDTP_MEDIA_CODEC,        //7
//     AVDTP_DELAY_REPORTING,    //8
//     AVDTP_SERVICE_CATEGORY_INVALID_FF = 0xFF
// } avdtp_service_category_t;

TEST(AvdtpUtil, avdtp_unpack_service_capabilities_test){
    uint8_t packet[] = {
        0x01, 0x00,
        0x07, 0x06, 0x00, 0x00, 0xff, 0xff, 0x02, 0x35,
        0x08, 0x00 
    };
    uint8_t expected_sbc_codec_capabilities[] = {
        0xFF, // (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
        0xFF, //(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
        2, 53
    };

    uint16_t expected_categories = (1 << AVDTP_MEDIA_TRANSPORT) | (1 << AVDTP_MEDIA_CODEC) | (1 << AVDTP_DELAY_REPORTING);
    CHECK_EQUAL(expected_categories, avdtp_unpack_service_capabilities(&connection, AVDTP_SI_GET_CONFIGURATION, &caps, &packet[0], sizeof(packet)));

    // media codec:
    CHECK_EQUAL(AVDTP_CODEC_SBC, caps.media_codec.media_codec_type);
    CHECK_EQUAL(4,  caps.media_codec.media_codec_information_len);
    int i;
    for (i = 0; i < caps.media_codec.media_codec_information_len; i++){
        CHECK_EQUAL(expected_sbc_codec_capabilities[i], caps.media_codec.media_codec_information[i]);
    }
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
