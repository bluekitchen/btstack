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
//  HFP Hands-Free (HF) unit and Audio-Gateway Commons
//
// *****************************************************************************


#ifndef btstack_hfp_h
#define btstack_hfp_h

#include "hci.h"
#include "sdp_query_rfcomm.h"

#if defined __cplusplus
extern "C" {
#endif

#define HFP_DEFAULT_HF_SUPPORTED_FEATURES 0x0000
#define HFP_DEFAULT_AG_SUPPORTED_FEATURES 0x0009

#define HFP_MAX_NUM_CODECS 20
#define HFP_MAX_NUM_INDICATORS 20

/* AT+BRSF Result: 
0: EC and/or NR function
1: Three-way calling
2: CLI presentation capability
3: Voice recognition activation
4: Remote volume control
5: Enhanced call status
6: Enhanced call control
7: Codec negotiation
8: HF Indicators
9: eSCO S4 (and T2) Settings Supported
10-31: Reserved for future definition
*/
/* +BRSF Result:
0: Three-way calling
1: EC and/or NR function
2: Voice recognition function
3: In-band ring tone capability
4: Attach a number to a voice tag
5: Ability to reject a call
6: Enhanced call status
7: Enhanced call control
8: Extended Error Result Codes
9: Codec negotiation
10: HF Indicators
11: eSCO S4 (and T2) Settings Supported
12-31: Reserved for future definition
*/
#define HFP_SUPPORTED_FEATURES "+BRSF"
#define HFP_AVAILABLE_CODECS "+BAC"
#define HFP_INDICATOR "+CIND"
#define HFP_ENABLE_INDICATOR_STATUS_UPDATE "+CMER"
#define HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES "+CHLD"
#define HFP_GENERIC_STATUS_INDICATOR "+BIND"
#define HFP_OK "OK"

// Codecs 
#define HFP_CODEC_CVSD 0x01
#define HFP_CODEC_MSBC 0x02


typedef enum {
    HFP_IDLE = 0, //50
    HFP_SDP_QUERY_RFCOMM_CHANNEL,
    HFP_W4_SDP_QUERY_COMPLETE,
    HFP_W4_RFCOMM_CONNECTED,
    
    HFP_EXCHANGE_SUPPORTED_FEATURES,
    HFP_W4_EXCHANGE_SUPPORTED_FEATURES, // 5
    
    HFP_NOTIFY_ON_CODECS,
    HFP_W4_NOTIFY_ON_CODECS,
    
    HFP_RETRIEVE_INDICATORS,
    HFP_W4_RETRIEVE_INDICATORS,
    
    HFP_RETRIEVE_INDICATORS_STATUS, // 10
    HFP_W4_RETRIEVE_INDICATORS_STATUS,
    
    HFP_ENABLE_INDICATORS_STATUS_UPDATE,
    HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE,
    
    HFP_RETRIEVE_CAN_HOLD_CALL,
    HFP_W4_RETRIEVE_CAN_HOLD_CALL, // 15
    
    HFP_LIST_GENERIC_STATUS_INDICATORS,
    HFP_W4_LIST_GENERIC_STATUS_INDICATORS,
    
    HFP_RETRIEVE_GENERIC_STATUS_INDICATORS,
    HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS,
    
    HFP_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS, // 20
    HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS,
    
    HFP_ACTIVE,
    HFP_W2_DISCONNECT_RFCOMM,
    HFP_W4_RFCOMM_DISCONNECTED, 
    HFP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN
} hfp_state_t;

typedef enum {
    HFP_AG_SERVICE,   /*    <value>=0 implies no service. No Home/Roam network available.
                            <value>=1 implies presence of service. Home/Roam network available.
                       */

    HFP_AG_CALL,      /*    <value>=0 means there are no calls in progress
                            <value>=1 means at least one call is in progress
                       */

    HFP_AG_CALLSETUP, /*    <value>=0 means not currently in call set up.
                            <value>=1 means an incoming call process ongoing.
                            <value>=2 means an outgoing call set up is ongoing.
                            <value>=3 means remote party being alerted in an outgoing call.
                       */
    HFP_AG_CALLHELD,   /*   0 = No calls held
                            1 = Call is placed on hold or active/held calls swapped
                                    (The AG has both an active AND a held call) 
                            2 = Call on hold, no active call
                        */
    HFP_AG_SIGNAL,      /*  ranges from 0 to 5, Signal Strength indicator */
    HFP_AG_ROAM,        /*  <value>=0 means roaming is not active
                            <value>=1 means a roaming is active
                        */
    HFP_AG_BATTCHG      /* ranges from 0 to 5, Battery Charge */

} hfp_ag_indicators_t;

typedef void (*hfp_callback_t)(uint8_t * event, uint16_t event_size);


typedef struct hfp_connection {
    linked_item_t    item;
    hfp_state_t state;

    uint32_t line_size;
    uint8_t  line_buffer[200];

    bd_addr_t remote_addr;
    uint16_t con_handle;
    uint16_t rfcomm_channel_nr;
    uint16_t rfcomm_cid;

    uint8_t  wait_ok;
    uint8_t  negotiated_codec;

    uint32_t remote_supported_features;
    uint8_t  remote_indicators_update_enabled;
    uint8_t  remote_indicators_nr;
    uint16_t remote_indicators[20];
    uint32_t remote_indicators_status;
    
    uint8_t  remote_hf_indicators_nr;
    uint16_t remote_hf_indicators[20];
    uint32_t remote_hf_indicators_status;

    hfp_callback_t callback;
} hfp_connection_t;


void hfp_create_service(uint8_t * service, uint16_t service_uuid, int rfcomm_channel_nr, const char * name, uint16_t supported_features);
void hfp_register_packet_handler(hfp_callback_t callback);
hfp_connection_t * hfp_handle_hci_event(uint8_t packet_type, uint8_t *packet, uint16_t size);
void hfp_init(uint16_t rfcomm_channel_nr);
void hfp_connect(bd_addr_t bd_addr, uint16_t service_uuid);

hfp_connection_t * provide_hfp_connection_context_for_rfcomm_cid(uint16_t cid);
linked_list_t * hfp_get_connections();

// TODO: move to utils
int send_str_over_rfcomm(uint16_t cid, char * command);
void join(char * buffer, int buffer_size, uint8_t * values, int values_nr);

const char * hfp_hf_feature(int index);
const char * hfp_ag_feature(int index);

#if defined __cplusplus
}
#endif

#endif