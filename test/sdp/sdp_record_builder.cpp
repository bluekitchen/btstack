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
// test calculation of sdp records created by various profiles
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/avrcp_controller.h" 

#include "btstack_util.h"
#include "bluetooth.h"

#include "classic/device_id_server.h" 
#include "classic/avrcp_target.h" 
#include "classic/a2dp_source.h" 
#include "classic/a2dp_sink.h" 
#include "classic/hsp_hs.h" 
#include "classic/hsp_ag.h" 
#include "classic/hfp_hf.h" 
#include "classic/hfp_ag.h" 
#include "classic/hid_device.h" 
#include "classic/pan.h" 
#include "classic/spp_server.h" 
#include "classic/sdp_util.h"

static uint8_t service_buffer[300];
static const char * test_string = "test";
static const uint16_t uint16_list_empty[] = { 0 };
static const uint16_t uint16_list_single_element[] = { 0x1234, 0 };

TEST_GROUP(SDPRecordBuilder){
    void setup(void){
        memset(service_buffer, 0, sizeof(service_buffer));
    }
};

// a2dp sink

static const char * default_a2dp_sink_service_name = "BTstack A2DP Sink Service";
static const char * default_a2dp_sink_service_provider_name = "BTstack A2DP Sink Service Provider";

#define A2DP_SINK_RECORD_SIZE_MIN 84

static uint16_t a2dp_sink_record_size(const char * service_name, const char * service_provider_name){
    int service_name_len           = service_name           ? strlen(service_name)          : strlen(default_a2dp_sink_service_name);
    int service_provider_name_len  = service_provider_name  ? strlen(service_provider_name) : strlen(default_a2dp_sink_service_provider_name);
    return A2DP_SINK_RECORD_SIZE_MIN + service_name_len + service_provider_name_len;
}

TEST(SDPRecordBuilder, A2DP_SINK){
    const char * service_name;
    const char * service_provider_name;
    int expected_len;

    service_name = "";
    service_provider_name = "";
    expected_len = a2dp_sink_record_size(service_name, service_provider_name);
    a2dp_sink_create_sdp_record(service_buffer, 0, 0, service_name, service_provider_name);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

// a2dp source

static const char * default_a2dp_source_service_name = "BTstack A2DP Source Service";
static const char * default_a2dp_source_service_provider_name = "BTstack A2DP Source Service Provider";

#define A2DP_SOURCE_RECORD_SIZE_MIN 84

static uint16_t a2dp_source_record_size(const char * service_name, const char * service_provider_name){
    int service_name_len           = service_name           ? strlen(service_name)          : strlen(default_a2dp_source_service_name);
    int service_provider_name_len  = service_provider_name  ? strlen(service_provider_name) : strlen(default_a2dp_source_service_provider_name);
    return A2DP_SOURCE_RECORD_SIZE_MIN + service_name_len + service_provider_name_len;
}

TEST(SDPRecordBuilder, A2DP_SOURCE){
    const char * service_name;
    const char * service_provider_name;
    int expected_len;

    service_name = "";
    service_provider_name = "";
    expected_len = a2dp_source_record_size(service_name, service_provider_name);
    a2dp_source_create_sdp_record(service_buffer, 0, 0, service_name, service_provider_name);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    service_name = NULL;
    service_provider_name = "";
    expected_len = a2dp_source_record_size(service_name, service_provider_name);
    a2dp_source_create_sdp_record(service_buffer, 0, 0, service_name, service_provider_name);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    service_name = NULL;
    service_provider_name = NULL;
    expected_len = a2dp_source_record_size(service_name, service_provider_name);
    a2dp_source_create_sdp_record(service_buffer, 0, 0, service_name, service_provider_name);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

//
// avcrp target
//

static const char * default_avrcp_target_service_name = "BTstack AVRCP Target Service";
static const char * default_avrcp_target_service_provider_name = "BTstack AVRCP Target Service Provider";

#define AVRCP_TARGET_RECORD_SIZE_MIN 84

static uint16_t avrcp_target_record_size(const char * service_name, const char * service_provider_name){
    int service_name_len           = service_name           ? strlen(service_name)          : strlen(default_avrcp_target_service_name);
    int service_provider_name_len  = service_provider_name  ? strlen(service_provider_name) : strlen(default_avrcp_target_service_provider_name);
    return AVRCP_TARGET_RECORD_SIZE_MIN + service_name_len + service_provider_name_len;
}

TEST(SDPRecordBuilder, AVRCP_TARGET){
    const char * service_name;
    const char * service_provider_name;
    int expected_len;
    int descriptor_size;

    service_name = "";
    service_provider_name = "";
    descriptor_size = 0;
    expected_len = avrcp_target_record_size(service_name, service_provider_name);
    avrcp_target_create_sdp_record(service_buffer, 0, 0, service_name, service_provider_name);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

//
// avrcp controller
//

static const char * default_avrcp_controller_service_name = "BTstack AVRCP Controller Service";
static const char * default_avrcp_controller_service_provider_name = "BTstack AVRCP Controller Service Provider";

#define AVRCP_CONTROLLER_RECORD_SIZE_MIN 87

static uint16_t avrcp_controller_record_size(const char * service_name, const char * service_provider_name){
    int service_name_len           = service_name           ? strlen(service_name)          : strlen(default_avrcp_controller_service_name);
    int service_provider_name_len  = service_provider_name  ? strlen(service_provider_name) : strlen(default_avrcp_controller_service_provider_name);
    return AVRCP_CONTROLLER_RECORD_SIZE_MIN + service_name_len + service_provider_name_len;
}

TEST(SDPRecordBuilder, AVRCP_CONTROLLER){
    const char * service_name;
    const char * service_provider_name;
    int expected_len;
    int descriptor_size;

    service_name = "";
    service_provider_name = "";
    descriptor_size = 0;
    expected_len = avrcp_controller_record_size(service_name, service_provider_name);
    avrcp_controller_create_sdp_record(service_buffer, 0, 0, service_name, service_provider_name);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

//
// hid_device.h
//

#define HID_DEVICE_RECORD_SIZE_MIN 187

static uint16_t hid_device_record_size(uint16_t descriptor_size, const char * name){
    return HID_DEVICE_RECORD_SIZE_MIN + descriptor_size + strlen(name);
}

TEST(SDPRecordBuilder, HID_DEVICE){
    const char * name;
    int expected_len;
    int descriptor_size;

    name = "";
    descriptor_size = 0;
    expected_len = hid_device_record_size(descriptor_size, name);
    
    hid_sdp_record_t hid_params = {0, 0, 0, 0, 0, false, false, 0xFFFF, 0xFFFF, 3200, NULL, descriptor_size, name};
    hid_create_sdp_record(service_buffer, 0x0001, &hid_params);

    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

//
// hfp_hf
//

#define HFP_HF_RECORD_SIZE_MIN 78

static uint16_t hfp_hf_record_size(const char * name){
    return HFP_HF_RECORD_SIZE_MIN + strlen(name);
}

TEST(SDPRecordBuilder, HFP_HF){
    const char * name;
    int expected_len;

    name = "";
    expected_len = hfp_hf_record_size(name);
    hfp_hf_create_sdp_record(service_buffer, 0, 0, name, 0, 0);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    name =test_string;
    expected_len = hfp_hf_record_size(name);
    hfp_hf_create_sdp_record(service_buffer, 0, 0, name, 0, 0);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

//
// hfp_ag
//

#define HFP_AG_RECORD_SIZE_MIN 83

static uint16_t hfp_ag_record_size(const char * name){
    return HFP_AG_RECORD_SIZE_MIN + strlen(name);
}

TEST(SDPRecordBuilder, HFP_AG){
    const char * name;
    int expected_len;

    name = "";
    expected_len = hfp_ag_record_size(name);
    hfp_ag_create_sdp_record(service_buffer, 0, 0, name, 0, 0, 0);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    name =test_string;
    expected_len = hfp_ag_record_size(name);
    hfp_ag_create_sdp_record(service_buffer, 0, 0, name, 0, 0, 0);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}


//
// hsp_ag
//

#define HSP_AG_RECORD_SIZE_MIN 72

static uint16_t hsp_ag_record_size(const char * name){
    return HSP_AG_RECORD_SIZE_MIN + strlen(name);
}

TEST(SDPRecordBuilder, HSP_AG){
    const char * name;
    int expected_len;

    name = "";
    expected_len = hsp_ag_record_size(name);
    hsp_ag_create_sdp_record(service_buffer, 0, 0, name);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    name =test_string;
    expected_len = hsp_ag_record_size(name);
    hsp_ag_create_sdp_record(service_buffer, 0, 0, name);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

//
// hsp_hs
//

#define HSP_HS_RECORD_SIZE_MIN 80

static uint16_t hsp_hs_record_size(const char * name){
    return HSP_HS_RECORD_SIZE_MIN + strlen(name);
}

TEST(SDPRecordBuilder, HSP_HS){
    const char * name;
    int expected_len;

    name = "";
    expected_len = hsp_hs_record_size(name);
    hsp_hs_create_sdp_record(service_buffer, 0, 0, name, 0);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    name =test_string;
    expected_len = hsp_hs_record_size(name);
    hsp_hs_create_sdp_record(service_buffer, 0, 0, name, 0);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

//
// device id
//

#define DEVICE_ID_RECORD_SIZE 64

static uint16_t device_id_record_size(void){
    return DEVICE_ID_RECORD_SIZE;
}

TEST(SDPRecordBuilder, DeviceID){
    int expected_len;
    expected_len = device_id_record_size();
    device_id_create_sdp_record(service_buffer, 0, 0, 0, 0, 0);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}


//
// pan
//

#define PAN_RECORD_MIN_LEN 104

static const char default_panu_service_name[] = "Personal Ad-hoc User Service";
static const char default_panu_service_desc[] = "Personal Ad-hoc User Service";

static const char default_nap_service_name[] = "Network Access Point Service";
static const char default_nap_service_desc[] = "Personal Ad-hoc Network Service which provides access to a network";

static const char default_gn_service_name[] = "Group Ad-hoc Network Service";
static const char default_gn_service_desc[] = "Personal Group Ad-hoc Network Service";

// network_packet_types (X bytes x nr network packet types)
// name. default name or name
// ipv4 subnet, if yes + x
// ipv6 subnet, if yes + x

static uint16_t pan_sdp_record_size(uint16_t * network_packet_types, const char *name, const char *description, const char *IPv4Subnet,
    const char *IPv6Subnet, const char * default_name, const char * default_desc){
    int num_network_packet_types;
    for (num_network_packet_types=0; network_packet_types && *network_packet_types; num_network_packet_types++, network_packet_types++);
    int name_len = name        ? strlen(name)        : strlen(default_name);
    int desc_len = description ? strlen(description) : strlen(default_desc);
    int ipv4_len = IPv4Subnet  ? 5 + strlen(IPv4Subnet) : 0;
    int ipv6_len = IPv6Subnet  ? 5 + strlen(IPv6Subnet) : 0;
    uint16_t res = PAN_RECORD_MIN_LEN + name_len + desc_len + ipv4_len + ipv6_len + num_network_packet_types * 3;   // tyoe + uint16_t
    return res;
}

static uint16_t pan_panu_sdp_record_size(uint16_t * network_packet_types, const char *name, const char *description){
    return pan_sdp_record_size(network_packet_types, name, description, NULL, NULL, default_panu_service_name, default_panu_service_desc);
}

static uint16_t pan_gn_sdp_record_size(uint16_t * network_packet_types, const char *name, const char *description, const char *IPv4Subnet,
    const char *IPv6Subnet){
    return pan_sdp_record_size(network_packet_types, name, description, IPv4Subnet, IPv6Subnet, default_gn_service_name, default_gn_service_desc);
}

static uint16_t pan_nap_sdp_record_size(uint16_t * network_packet_types, const char *name, const char *description, const char *IPv4Subnet,
    const char *IPv6Subnet){
    return pan_sdp_record_size(network_packet_types, name, description, IPv4Subnet, IPv6Subnet, default_nap_service_name, default_nap_service_desc) + 14;
}

TEST(SDPRecordBuilder, PANU){
    int expected_len;
    const char * name;
    const char * desc;
    uint16_t * network_packet_types;

    // empty name
    name = "";
    desc = "";
    network_packet_types = NULL;
    expected_len = pan_panu_sdp_record_size(network_packet_types, name, desc);
    pan_create_panu_sdp_record(service_buffer, 0, network_packet_types, name, desc, BNEP_SECURITY_NONE);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    // default name
    name = NULL;
    desc = NULL;
    network_packet_types = NULL;
    expected_len = pan_panu_sdp_record_size(network_packet_types, name, desc);
    pan_create_panu_sdp_record(service_buffer, 0, network_packet_types, name, desc, BNEP_SECURITY_NONE);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    // empty list
    name = NULL;
    desc = NULL;
    network_packet_types = (uint16_t *) &uint16_list_empty;
    expected_len = pan_panu_sdp_record_size(network_packet_types, name, desc);
    pan_create_panu_sdp_record(service_buffer, 0, network_packet_types, name, desc, BNEP_SECURITY_NONE);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    // single element
    name = NULL;
    desc = NULL;
    network_packet_types = (uint16_t *) &uint16_list_single_element;
    expected_len = pan_panu_sdp_record_size(network_packet_types, name, desc);
    pan_create_panu_sdp_record(service_buffer, 0, network_packet_types, name, desc, BNEP_SECURITY_NONE);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

TEST(SDPRecordBuilder, GN){
    int expected_len;
    const char * name;
    const char * desc;
    uint16_t * network_packet_types;
    const char *IPv4Subnet;
    const char *IPv6Subnet;

    // empty name
    name = "";
    desc = "";
    network_packet_types = NULL;
    IPv4Subnet = NULL;
    IPv6Subnet = NULL;
    expected_len = pan_gn_sdp_record_size(network_packet_types, name, desc, IPv4Subnet, IPv6Subnet);
    pan_create_gn_sdp_record(service_buffer, 0, network_packet_types, name, desc, BNEP_SECURITY_NONE, IPv4Subnet, IPv6Subnet);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    // test ipv4 param
    name = "";
    desc = "";
    network_packet_types = NULL;
    IPv4Subnet = NULL;
    IPv6Subnet = "";
    expected_len = pan_gn_sdp_record_size(network_packet_types, name, desc, IPv4Subnet, IPv6Subnet);
    pan_create_gn_sdp_record(service_buffer, 0, network_packet_types, name, desc, BNEP_SECURITY_NONE, IPv4Subnet, IPv6Subnet);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    // test ipv6 param
    name = "";
    desc = "";
    network_packet_types = NULL;
    IPv4Subnet = "";
    IPv6Subnet = "";
    expected_len = pan_gn_sdp_record_size(network_packet_types, name, desc, IPv4Subnet, IPv6Subnet);
    pan_create_gn_sdp_record(service_buffer, 0, network_packet_types, name, desc, BNEP_SECURITY_NONE, IPv4Subnet, IPv6Subnet);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

TEST(SDPRecordBuilder, NAP){
    int expected_len;
    const char * name;
    const char * desc;
    uint16_t * network_packet_types;
    const char *IPv4Subnet;
    const char *IPv6Subnet;

    // empty name
    name = "";
    desc = "";
    network_packet_types = NULL;
    IPv4Subnet = NULL;
    IPv6Subnet = NULL;
    expected_len = pan_nap_sdp_record_size(network_packet_types, name, desc, IPv4Subnet, IPv6Subnet);
    pan_create_nap_sdp_record(service_buffer, 0, network_packet_types, name, desc, BNEP_SECURITY_NONE, PAN_NET_ACCESS_TYPE_PSTN, 0, IPv4Subnet, IPv6Subnet);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    // test ipv4 param
    name = "";
    desc = "";
    network_packet_types = NULL;
    IPv4Subnet = NULL;
    IPv6Subnet = "";
    expected_len = pan_nap_sdp_record_size(network_packet_types, name, desc, IPv4Subnet, IPv6Subnet);
    pan_create_nap_sdp_record(service_buffer, 0, network_packet_types, name, desc, BNEP_SECURITY_NONE, PAN_NET_ACCESS_TYPE_PSTN, 0, IPv4Subnet, IPv6Subnet);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);

    // test ipv6 param
    name = "";
    desc = "";
    network_packet_types = NULL;
    IPv4Subnet = "";
    IPv6Subnet = "";
    expected_len = pan_nap_sdp_record_size(network_packet_types, name, desc, IPv4Subnet, IPv6Subnet);
    pan_create_nap_sdp_record(service_buffer, 0, network_packet_types, name, desc, BNEP_SECURITY_NONE, PAN_NET_ACCESS_TYPE_PSTN, 0, IPv4Subnet, IPv6Subnet);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

//
// SPP: Record + strlen(service_name)
//

#define SPP_RECORD_MIN_LEN 84

static uint16_t spp_sdp_record_size_for_service_name(const char * service_name){
    return SPP_RECORD_MIN_LEN + strlen(service_name);
}

TEST(SDPRecordBuilder, SPP){
    int expected_len = spp_sdp_record_size_for_service_name(test_string);
    spp_create_sdp_record(service_buffer, 0, 0, test_string);
    CHECK_EQUAL(de_get_len(service_buffer), expected_len);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
