#include <stdio.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_util.h"
#include "bluetooth.h"
#include "mesh/mesh_configuration_client.h"

static uint8_t composition_data_valid_elements[] = {
    // header
    HCI_EVENT_MESH_META, 0x21, MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA, 0xff, 0xee, 0x00,
    // page
    0x00,
    // cid, pid, vid, crpl, feature
    0x11, 0x10, 0x22, 0x20, 0x33, 0x30, 0x44, 0x40, 0x55, 0x50, 

    // loc (2), num SIG models(1), num vendor models(1)
    0xBB, 0xAA, 0x03, 0x02,
    // SIG models
    0x10, 0x11, 0x20, 0x22, 0x30, 0x33,
    // Vendor models
    0xa2, 0xa1, 0xa4, 0xa3, 
    0xb2, 0xb1, 0xb4, 0xb3
};

static uint8_t composition_data_no_models[] = {
    // header
    HCI_EVENT_MESH_META, 0x21, MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA, 0xff, 0xee, 0x00,
    // page
    0x00,
    // cid, pid, vid, crpl, feature
    0x11, 0x10, 0x22, 0x20, 0x33, 0x30, 0x44, 0x40, 0x55, 0x50, 

    // loc (2), num SIG models(1), num vendor models(1)
    0xBB, 0xAA, 0x00, 0x00
};

static uint8_t composition_data_invalid[] = {
    // header
    HCI_EVENT_MESH_META, 0x21, MESH_SUBEVENT_CONFIGURATION_COMPOSITION_DATA, 0xff, 0xee, 0x00
};

TEST_GROUP(CompositionData){
    mesh_composite_data_iterator_t it;
    int expected_num_elements;
    int expected_num_sig_models;
    int expected_num_vendor_models;

    void setup(void){
    }

    void composition_data_validate(const uint8_t * elements, uint16_t elements_size){
        int num_elements = 0;
        int num_sig_models = 0;
        int num_vendor_models = 0;
        mesh_composition_data_iterator_init(&it, elements, elements_size);
        while (mesh_composition_data_iterator_has_next_element(&it)){
            num_elements++;
            mesh_composition_data_iterator_next_element(&it);
            CHECK_EQUAL(0xAABB, mesh_composition_data_iterator_element_loc(&it));

            while (mesh_composition_data_iterator_has_next_sig_model(&it)){
                num_sig_models++;
                mesh_composition_data_iterator_next_sig_model(&it);
            }
            while (mesh_composition_data_iterator_has_next_vendor_model(&it)){
                num_vendor_models++;
                mesh_composition_data_iterator_next_vendor_model(&it);
            }
        }

        CHECK_EQUAL(expected_num_elements, num_elements);
        CHECK_EQUAL(expected_num_sig_models, num_sig_models);
        CHECK_EQUAL(expected_num_vendor_models, num_vendor_models);  
    }
};

TEST(CompositionData, CompositionDataEventHeader){
    CHECK_EQUAL(0, mesh_subevent_configuration_composition_data_get_page(composition_data_valid_elements));
    CHECK_EQUAL(0x1011, mesh_subevent_configuration_composition_data_get_cid(composition_data_valid_elements));
    CHECK_EQUAL(0x2022, mesh_subevent_configuration_composition_data_get_pid(composition_data_valid_elements));
    CHECK_EQUAL(0x3033, mesh_subevent_configuration_composition_data_get_vid(composition_data_valid_elements));
    CHECK_EQUAL(0x4044, mesh_subevent_configuration_composition_data_get_crpl(composition_data_valid_elements));
    CHECK_EQUAL(0x5055, mesh_subevent_configuration_composition_data_get_features(composition_data_valid_elements));
}

TEST(CompositionData, CompositionDataElementsValid){
    expected_num_elements = 1;
    expected_num_sig_models = 3;
    expected_num_vendor_models = 2;
    composition_data_validate(composition_data_valid_elements, sizeof(composition_data_valid_elements));
}

TEST(CompositionData, CompositionDataElementsNoModels){
    expected_num_elements = 1;
    expected_num_sig_models = 0;
    expected_num_vendor_models = 0;
    composition_data_validate(composition_data_no_models, sizeof(composition_data_no_models));
}  

TEST(CompositionData, CompositionDataElementsInvalid){
    expected_num_elements = 0;
    expected_num_sig_models = 0;
    expected_num_vendor_models = 0;
    composition_data_validate(composition_data_invalid, sizeof(composition_data_invalid));
}  

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}