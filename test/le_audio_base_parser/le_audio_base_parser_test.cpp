//

#include <string.h>
#include <stdio.h>
#include <_inttypes.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "le-audio/le_audio_base_parser.h"

TEST_GROUP(BaseParserGroup) {

};

int main(int argc, const char ** argv){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

TEST(BaseParserGroup, SingleSubgroupWithSingleBis) {
    const uint8_t adv_data[] = {
        0x25, 0x16, 0x51, 0x18, 0x20, 0x4E, 0x00, 0x01, 0x01, 0x06, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x02, 0x01, 0x01,
        0x02, 0x02, 0x00, 0x03, 0x04, 0x1A, 0x00, 0x04, 0x03, 0x02, 0x04, 0x00, 0x01, 0x06, 0x05, 0x03, 0x01, 0x00,
        0x00, 0x00,
    };
    le_audio_base_parser_t parser;
    bool ok = le_audio_base_parser_init(&parser, adv_data, sizeof(adv_data));
    CHECK(ok);
    printf("BASE:\n");
    uint32_t presentation_delay = le_audio_base_parser_get_presentation_delay(&parser);
    CHECK_EQUAL(20000, presentation_delay);
    printf("- presentation delay: %" PRIu32 " us\n", presentation_delay);
    uint8_t num_subgroups = le_audio_base_parser_get_num_subgroups(&parser);
    CHECK_EQUAL(1,num_subgroups);
    printf("- num subgroups: %u\n", num_subgroups);
    uint8_t i;
    for (i=0;i<num_subgroups;i++) {
        // Level 2: Subgroup Level
        printf("  - Subgroup %u\n", i);
        uint8_t num_bis = le_audio_base_parser_subgroup_get_num_bis(&parser);
        printf("    - num bis[%u]: %u\n", i, num_bis);
        CHECK_EQUAL(1,num_bis);
    }
}
TEST(BaseParserGroup, TwoSubgroupWithSingleBis) {
    const uint8_t adv_data[] = {
        0x43, 0x16, 0x51, 0x18, 0x20, 0x4E, 0x00, 0x02, 0x01, 0x06, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x02, 0x01, 0x01,
        0x02, 0x02, 0x00, 0x03, 0x04, 0x1A, 0x00, 0x04, 0x03, 0x02, 0x04, 0x00, 0x01, 0x06, 0x05, 0x03, 0x01, 0x00,
        0x00, 0x00, 0x01, 0x06, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x02, 0x01, 0x01, 0x02, 0x02, 0x00, 0x03, 0x04, 0x1A,
        0x00, 0x04, 0x03, 0x02, 0x04, 0x00, 0x01, 0x06, 0x05, 0x03, 0x01, 0x00, 0x00, 0x00,
    };
    le_audio_base_parser_t parser;
    bool ok = le_audio_base_parser_init(&parser, adv_data, sizeof(adv_data));
    CHECK(ok);
    printf("BASE:\n");
    uint32_t presentation_delay = le_audio_base_parser_get_presentation_delay(&parser);
    CHECK_EQUAL(20000, presentation_delay);
    printf("- presentation delay: %" PRIu32 " us\n", presentation_delay);
    uint8_t num_subgroups = le_audio_base_parser_get_num_subgroups(&parser);
    CHECK_EQUAL(2,num_subgroups);
    printf("- num subgroups: %u\n", num_subgroups);
    uint8_t i;
    for (i=0;i<num_subgroups;i++) {
        // Level 2: Subgroup Level
        printf("  - Subgroup %u\n", i);
        uint8_t num_bis = le_audio_base_parser_subgroup_get_num_bis(&parser);
        printf("    - num bis[%u]: %u\n", i, num_bis);
        CHECK_EQUAL(1,num_bis);
    }
}

