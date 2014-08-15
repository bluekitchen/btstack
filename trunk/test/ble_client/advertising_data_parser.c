
//*****************************************************************************
//
// test rfcomm query tests
//
//*****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include <btstack/hci_cmds.h>

#include "btstack_memory.h"
#include "hci.h"
#include "ad_parser.h"
#include "l2cap.h"

void le_handle_advertisement_report(uint8_t *packet, int size);

typedef struct ad_event {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} ad_event_t;

static uint8_t ad_data[] =   {0x02, 0x01, 0x05, 0x03, 0x02, 0xF0, 0xFF};

static uint8_t mtk_adv_evt[] = {
    0x3e, 0x3b, 0x02, 0x03, 0x04, 0x01, 0x55, 0x05, 0x67, 0x5c, 0xc2, 0x4f, 0x00,
    0xb6, 0x00, 0x00, 0x87, 0x7b, 0x60, 0x70, 0xf3, 0x5c, 0x1b, 0x02, 0x01, 0x02,
    0x05, 0x09, 0x41, 0x4e, 0x43, 0x53, 0x11, 0x15, 0xd0, 0x00, 0x2d, 0x12, 0x1e,
    0x4b, 0x0f, 0xa4, 0x99, 0x4e, 0xce, 0xb5, 0x31, 0xf4, 0x05, 0x79, 0xbf, 0x04,
    0x00, 0x87, 0x7b, 0x60, 0x70, 0xf3, 0x5c, 0x00, 0xc0
};

static uint8_t adv_evt[] = {
    0x3E, 0x3B, 0x02, 0x03, 0x04, 0x00, 0x04, 0x01, 0x00, 0x00, 0x55, 0x05, 0x67,
    0x5C, 0xC2, 0x4F, 0x87, 0x7B, 0x60, 0x70, 0xF3, 0x5C, 0x87, 0x7B, 0x60, 0x70,
    0xF3, 0x5C, 0x00, 0x1B, 0x00, 0x02, 0x01, 0x02, 0x05, 0x09, 0x41, 0x4E, 0x43,
    0x53, 0x11, 0x15, 0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E,
    0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79, 0xb6, 0xbf, 0xc0
};

static uint8_t mtk_num_completed_evt[] ={
    0x13 ,0x09, 0x02 ,0x01, 0x02 ,0x01, 0x00 ,0x01, 0x02 ,0x01, 0x00
};

static uint8_t num_completed_evt[] ={
    0x13 ,0x09, 0x02 ,0x01, 0x02 ,0x01, 0x02, 0x01, 0x00, 0x01, 0x00
};


int dummy_callback(){
    return 0;
}

static hci_transport_t dummy_transport = {
  /*  .transport.open                          = */  NULL,
  /*  .transport.close                         = */  NULL,
  /*  .transport.send_packet                   = */  NULL,
  /*  .transport.register_packet_handler       = */  (void (*)(void (*)(uint8_t, uint8_t *, uint16_t))) dummy_callback,
  /*  .transport.get_transport_name            = */  NULL,
  /*  .transport.set_baudrate                  = */  NULL,
  /*  .transport.can_send_packet_now           = */  NULL,
};

static void dump_ad_event(ad_event_t * e){
    printf(" * adv. event: evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", e->event_type,
           e->address_type, bd_addr_to_str(e->address), e->rssi, e->length);
    hexdump(e->data, e->length);
    
}

void packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){

    ad_event_t ad_event;
    int pos = 2;
    ad_event.event_type = packet[pos++];
    ad_event.address_type = packet[pos++];
    memcpy(ad_event.address, &packet[pos], 6);
    
    pos += 6;
    ad_event.rssi = packet[pos++];
    ad_event.length = packet[pos++];
    ad_event.data = &packet[pos];
    pos += ad_event.length;
    dump_ad_event(&ad_event);

    printf("\ndata: \n");

    hexdump(packet, size);

    printf("\n");
}


TEST_GROUP(ADParser){
    void setup(){
        hci_init(&dummy_transport, NULL, NULL, NULL);
        hci_register_packet_handler(packet_handler);
    }
};


TEST(ADParser, TestDataParsing){
    ad_context_t context;
    uint8_t  expected_len[] = {1, 2};
    uint8_t  expected_type[] = {0x01, 0x02};
    uint8_t  expected_data[][2] = {{0x05, 0x00}, {0xF0, 0xFF}};

    int i = 0;
    uint8_t  ad_len = sizeof(ad_data);

    for (ad_iterator_init(&context, ad_len, ad_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len  = ad_iterator_get_data_len(&context);
        uint8_t * data    = ad_iterator_get_data(&context);
        
        CHECK_EQUAL(expected_len[i],  data_len);
        CHECK_EQUAL(expected_type[i], data_type);

        int j;
        for (j = 0; j < data_len; j++){
            CHECK_EQUAL(expected_data[i][j], data[j]);
        }
        i++; 
    }
}

TEST(ADParser, TestFixMtkAdvertisingReport){
    // fix_mtk_advertisement_report(mtk_adv_evt, sizeof(mtk_adv_evt));
    int j;
    for (j = 0; j < sizeof(mtk_adv_evt); j++){
        CHECK_EQUAL(mtk_adv_evt[j], adv_evt[j]);
    }
}

TEST(ADParser, TestFixMtkNumCompletedPackets){
    // fix_mtk_num_completed_packets(mtk_num_completed_evt, sizeof(mtk_num_completed_evt));
    int j;
    for (j = 0; j < sizeof(mtk_num_completed_evt); j++){
        CHECK_EQUAL(mtk_num_completed_evt[j], num_completed_evt[j]);
    }
}

TEST(ADParser, TestAdvertisementEventMultipleReports){
    le_handle_advertisement_report(adv_evt, sizeof(adv_evt));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}