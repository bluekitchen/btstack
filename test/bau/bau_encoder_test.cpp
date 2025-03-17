//

#include <string.h>
#include <stdio.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "le-audio/broadcast_audio_uri_builder.h"
#include "bluetooth.h"
#include "btstack_util.h"
#include "bluetooth_company_id.h"

TEST_GROUP(URIBuilderTestGroup) {

};

int main(int argc, const char ** argv){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

TEST(URIBuilderTestGroup, Example03) {
    static const char *expected_uri = "BLUETOOTH:UUID:184F;BN:Um9iaW7igJlzIFRW;AT:0;AD:AABBCC012345;BC:RmlsbUZsaWNzQTE=;SM:BARkZXUCBgE=;;";
    char uri_buffer[256];
    broadcast_audio_uri_builder_t uri;
    broadcast_audio_uri_builder_init(&uri, uri_buffer, sizeof(uri_buffer));
    broadcast_audio_uri_builder_append_broadcast_name(&uri, "Robin’s TV");
    broadcast_audio_uri_builder_append_advertiser_address_type(&uri, BD_ADDR_TYPE_LE_PUBLIC);
    broadcast_audio_uri_builder_append_advertiser_address(&uri, (uint8_t*)"\xAA\xBB\xCC\x01\x23\x45");
    broadcast_audio_uri_builder_append_broadcast_code_as_string(&uri, "FilmFlicsA1");
    broadcast_audio_uri_builder_sg_metadata_begin(&uri);
    broadcast_audio_uri_builder_sg_metadata_append_ltv(&uri, 3, 4, (uint8_t*)"deu");
    broadcast_audio_uri_builder_sg_metadata_append_ltv(&uri, 1, 6, (uint8_t*)"\x01");
    broadcast_audio_uri_builder_sg_metadata_end(&uri);
    broadcast_audio_uri_builder_finalize(&uri);
    STRCMP_EQUAL(expected_uri, uri_buffer);
}

TEST(URIBuilderTestGroup, Example02) {
    static const char *expected_uri = "BLUETOOTH:UUID:184F;BN:SG9ja2V5;SQ:1;AT:0;AD:AABBCC001122;AS:1;BI:DE51E9;PI:FFFF;NS:1;BS:1;;";
    char uri_buffer[256];
    broadcast_audio_uri_builder_t uri;
    broadcast_audio_uri_builder_init(&uri, uri_buffer, sizeof(uri_buffer));
    broadcast_audio_uri_builder_append_broadcast_name(&uri, "Hockey");
    broadcast_audio_uri_builder_append_standard_quality(&uri, true);
    broadcast_audio_uri_builder_append_advertiser_address_type(&uri, BD_ADDR_TYPE_LE_PUBLIC);
    broadcast_audio_uri_builder_append_advertiser_address(&uri, (uint8_t*)"\xAA\xBB\xCC\x00\x11\x22");
    broadcast_audio_uri_builder_append_advertising_sid(&uri, 1);
    broadcast_audio_uri_builder_append_broadcast_id(&uri, 0xDE51E9);
    broadcast_audio_uri_builder_append_pa_interval(&uri, 0xFFFF);
    broadcast_audio_uri_builder_append_num_subgroups(&uri, 1);
    broadcast_audio_uri_builder_append_bis_sync(&uri, 1);
    broadcast_audio_uri_builder_finalize(&uri);
    STRCMP_EQUAL( expected_uri, uri_buffer );
}

TEST(URIBuilderTestGroup, Example01) {
    static const char *expected_uri = "BLUETOOTH:UUID:184F;BN:QnJvYWRjYXN0IE5hbWU=;AT:0;AD:112233445566;BI:123456;BC:zMzMzMzMzMzMzMzMzMzMzA==;SQ:1;HQ:0;VS:SUQ6MzQ1NjtkYXRhMTtkYXRhMg==;AS:1;PI:1000;NS:1;BS:1;NB:1;SM:;;";
    bd_addr_t adv_addr;
    sscanf_bd_addr("11:22:33:44:55:66", adv_addr);
    uint8_t broadcast_code[16];
    memset(broadcast_code, 0xcc, sizeof(broadcast_code));

    broadcast_audio_uri_builder_t builder;
    char uri[256];
    memset(uri, 0, sizeof(uri));
    broadcast_audio_uri_builder_init(&builder, uri, sizeof(uri));
    //
    broadcast_audio_uri_builder_append_broadcast_name(&builder, "Broadcast Name");
    broadcast_audio_uri_builder_append_advertiser_address_type(&builder, BD_ADDR_TYPE_LE_PUBLIC);
    broadcast_audio_uri_builder_append_advertiser_address(&builder, adv_addr);
    broadcast_audio_uri_builder_append_broadcast_id(&builder, 0x123456);
    broadcast_audio_uri_builder_append_broadcast_code_as_bytes(&builder, broadcast_code);
    broadcast_audio_uri_builder_append_standard_quality(&builder, true);
    broadcast_audio_uri_builder_append_high_quality(&builder, false);
    static const char *atext = "data1;data2";
    broadcast_audio_uri_builder_append_vendor_specific(&builder, 0x3456, (uint8_t*)atext, strlen(atext));
    //
    broadcast_audio_uri_builder_append_advertising_sid(&builder, 0x01);
    broadcast_audio_uri_builder_append_pa_interval(&builder, 0x1000);
    broadcast_audio_uri_builder_append_num_subgroups(&builder, 1);
    broadcast_audio_uri_builder_append_bis_sync(&builder, 1);
    broadcast_audio_uri_builder_append_sg_number_of_bises(&builder, 1);
    broadcast_audio_uri_builder_sg_metadata_begin(&builder);
    broadcast_audio_uri_builder_sg_metadata_end(&builder);
    broadcast_audio_uri_builder_append_public_broadcast_announcement_metadata(&builder, NULL, 0);
    //
    broadcast_audio_uri_builder_finalize(&builder);
    STRCMP_EQUAL( expected_uri, uri );
}
