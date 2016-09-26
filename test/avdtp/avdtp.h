/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

/*
 * avdtp.h
 * 
 * Audio/Video Distribution Transport Protocol
 *
 * This protocol defines A/V stream negotiation, establishment, and transmission 
 * procedures. Also specified are the message formats that are exchanged between 
 * such devices to transport their A/V streams in A/V distribution applications.
 *
 * Media packets are unidirectional, they travel downstream from AVDTP Source to AVDTP Sink. 
 */

#ifndef __AVDTP_H
#define __AVDTP_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

// protocols
#define PSM_AVCTP 0x0017
#define PSM_AVDTP 0x0019

// service classes
#define AUDIO_SOURCE_GROUP          0x110A
#define AUDIO_SINK_GROUP            0x110B
#define AV_REMOTE_CONTROL_TARGET    0X110C
#define ADVANCED_AUDIO_DISTRIBUTION 0X110D
#define AV_REMOTE_CONTROL           0X110E
#define AV_REMOTE_CONTROL_CONTROLER 0X110F

// Supported Features
#define AVDTP_SOURCE_SF_Player      0x0001
#define AVDTP_SOURCE_SF_Microphone  0x0002
#define AVDTP_SOURCE_SF_Tuner       0x0004
#define AVDTP_SOURCE_SF_Mixer       0x0008

#define AVDTP_SINK_SF_Headphone     0x0001
#define AVDTP_SINK_SF_Speaker       0x0002
#define AVDTP_SINK_SF_Recorder      0x0004
#define AVDTP_SINK_SF_Amplifier     0x0008

#if defined __cplusplus
}
#endif

#endif // __AVDTP_H