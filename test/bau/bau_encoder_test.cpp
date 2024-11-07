//

#include <string.h>
#include <stdio.h>

#include "le-audio/broadcast_audio_uri_builder.h"
#include "bluetooth.h"
#include "btstack_util.h"
#include "bluetooth_company_id.h"

int main(int argc, const char ** argv){
    (void) argc;
    (void) argv;

    bd_addr_t adv_addr;
    sscanf_bd_addr("11:22:33:44:55:66", adv_addr);
    uint8_t broadcast_code[16];
    memset(broadcast_code, 0xcc, sizeof(broadcast_code));

    broadcast_audio_uri_builder_t builder;
    uint8_t uri[256];
    memset(uri, 0, sizeof(uri));
    broadcast_audio_uri_builder_init(&builder, uri, sizeof(uri));
    broadcast_audio_uri_builder_append_string(&builder, "BLUETOOTH:UUID:184F;");
    //
    broadcast_audio_uri_builder_append_broadcast_name(&builder, "Broadcast Name");
    broadcast_audio_uri_builder_append_advertiser_address_type(&builder, BD_ADDR_TYPE_LE_PUBLIC);
    broadcast_audio_uri_builder_append_advertiser_address(&builder, adv_addr);
    broadcast_audio_uri_builder_append_broadcast_id(&builder, 0x123456);
    broadcast_audio_uri_builder_append_broadcast_code(&builder, broadcast_code);
    broadcast_audio_uri_builder_append_standard_quality(&builder, true);
    broadcast_audio_uri_builder_append_high_quality(&builder, false);
    broadcast_audio_uri_builder_append_vendor_specific(&builder, BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, NULL, 0);
    //
    broadcast_audio_uri_builder_append_advertising_sid(&builder, 0x01);
    broadcast_audio_uri_builder_append_pa_interval(&builder, 0x1000);
    broadcast_audio_uri_builder_append_num_subgroups(&builder, 1);
    broadcast_audio_uri_builder_append_bis_sync(&builder, 1);
    broadcast_audio_uri_builder_append_sg_number_of_bises(&builder, 1);
    broadcast_audio_uri_builder_append_sg_metadata(&builder, NULL, 0);
    broadcast_audio_uri_builder_append_public_broadcast_announcement_metadata(&builder, NULL, 0);
    //
    broadcast_audio_uri_builder_append_string(&builder, ";");
    puts((const char*)uri);
}