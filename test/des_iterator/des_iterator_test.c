
// *****************************************************************************
//
// des iterator tests
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/sdp_util.h>
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"


static uint8_t  des_list[] = { 
    0x35, 0x1E, 0x35, 0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x0F, 0x35, 0x14, 0x19, 0x00, 0x0F, 0x09, 
    0x01, 0x00, 0x35, 0x0C, 0x09, 0x08, 0x00, 0x09, 0x08, 0x06, 0x09, 0x86, 0xDD, 0x09, 0x88, 0x0B
};

static uint16_t expected_values[] = {
    0x0100, 0xf, 0x000f, 0x0100, 0x800, 0x806, 0x86dd, 0x880b
};


TEST_GROUP(DESParser){
    int value_index;
    des_iterator_t des_list_it;

    void setup(void){
        value_index = 0;
    }
    
    void CHECK_EQUAL_UINT16(des_iterator_t *it){
        uint16_t value;
        uint8_t * element = des_iterator_get_element(it);
        if (des_iterator_get_type(it) == DE_UUID){
            value = de_get_uuid32(element);
        } else {
            value = 0xffff;
            de_element_get_uint16(element, &value);
        }
        CHECK_EQUAL(expected_values[value_index], value);
        value_index++;
    }

    void iter(des_iterator_t * des_list_it){
        CHECK_EQUAL(des_iterator_get_type(des_list_it), DE_DES);
        des_iterator_t prot_it;
        des_iterator_t packet_it;
        uint8_t * des_element;
        uint8_t * packet_des;
        des_element = des_iterator_get_element(des_list_it);
        for (des_iterator_init(&prot_it, des_element) ; des_iterator_has_more(&prot_it) ; des_iterator_next(&prot_it) ){
            if (des_iterator_get_type(&prot_it) != DE_DES){
                CHECK_EQUAL_UINT16(&prot_it);
                continue;
            }
            packet_des = des_iterator_get_element(&prot_it);
            for (des_iterator_init(&packet_it, packet_des) ; des_iterator_has_more(&packet_it) ; des_iterator_next(&packet_it)){
                CHECK_EQUAL_UINT16(&packet_it);
            }
        }
    }
};

// DES { DES{0x0100, 0xf}, DES{0x000f, 0x0100, DES{0x800, 0x806, 0x86dd, 0x880b}}}
TEST(DESParser, DESIterator){
    for (des_iterator_init(&des_list_it, des_list); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)){
        iter(&des_list_it);
    }
    CHECK_EQUAL(des_iterator_has_more(&des_list_it), 0);
    CHECK_EQUAL(value_index, 8);
}

TEST(DESParser, DESIterator2){
    uint16_t l2cap_psm = 0;
    uint16_t bnep_version = 0;
    for (des_iterator_init(&des_list_it, des_list); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)){
        CHECK_EQUAL(des_iterator_get_type(&des_list_it), DE_DES);
        des_iterator_t prot_it;
        uint8_t * des_element = des_iterator_get_element(&des_list_it);
        des_iterator_init(&prot_it, des_element);
        uint8_t * element = des_iterator_get_element(&prot_it);
        CHECK_EQUAL(de_get_element_type(element), DE_UUID);
        uint32_t uuid = de_get_uuid32(element);
        switch (uuid){
            case SDP_L2CAPProtocol:
                CHECK_EQUAL(des_iterator_has_more(&prot_it), 1);
                des_iterator_next(&prot_it);
                de_element_get_uint16(des_iterator_get_element(&prot_it), &l2cap_psm);
                break;
            case SDP_BNEPProtocol:
                CHECK_EQUAL(des_iterator_has_more(&prot_it), 1);
                des_iterator_next(&prot_it);
                de_element_get_uint16(des_iterator_get_element(&prot_it), &bnep_version);
                break;
            default:
                break;
        }
    }
    printf("l2cap_psm 0x%04x, bnep_version 0x%04x\n", l2cap_psm, bnep_version);
    CHECK_EQUAL(l2cap_psm, expected_values[1]);
    CHECK_EQUAL(bnep_version, expected_values[3]);
    CHECK_EQUAL(des_iterator_has_more(&des_list_it), 0);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
