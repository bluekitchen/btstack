/*
 * Copyright (C) 2024 BlueKitchen GmbH
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
 *  @brief TODO
 */

#ifndef BROADCAST_AUDIO_URI_H
#define BROADCAST_AUDIO_URI_H

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    BROADCAST_AUDIO_URI_BROADCASTNAME,
    BROADCAST_AUDIO_URI_ADVERTISER_ADDRESS_TYPE,
    BROADCAST_AUDIO_URI_ADVERTISER_ADDRESS,
    BROADCAST_AUDIO_URI_BROADCAST_ID,
    BROADCAST_AUDIO_URI_BROADCAST_CODE,
    BROADCAST_AUDIO_URI_STANDARD_QUALITY,
    BROADCAST_AUDIO_URI_HIGH_QUALITY,
    BROADCAST_AUDIO_URI_VENDOR_SPECIFIC,
    BROADCAST_AUDIO_URI_ADVERTISING_SID,
    BROADCAST_AUDIO_URI_PA_INTERVAL,
    BROADCAST_AUDIO_URI_NUM_SUBGROUPS,
    BROADCAST_AUDIO_URI_BIS_SYNC,
    BROADCAST_AUDIO_URI_SG_NUMBER_OF_BISES,
    BROADCAST_AUDIO_URI_SG_METADATA,
    BROADCAST_AUDIO_URI_PUBLIC_BROADCAST,
    BROADCAST_AUDIO_URI_ANNOUNCEMENT,
    BROADCAST_AUDIO_URI_METADATA,
} broadcast_audio_uri_id_t;

#if defined __cplusplus
}
#endif
#endif // BROADCAST_AUDIO_URI_BUILDER_H
