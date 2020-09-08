#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/hfp.h"

#include "mock.h"

TEST_GROUP(HFPLinkSettings){
        void setup(void){
        }
        void teardown(void){
        }
};

TEST(HFPLinkSettings, NONE){
    CHECK_EQUAL(HFP_LINK_SETTINGS_NONE, hfp_next_link_setting(HFP_LINK_SETTINGS_D1, false, true, true, HFP_CODEC_MSBC));
}

TEST(HFPLinkSettings, D1){
    CHECK_EQUAL(HFP_LINK_SETTINGS_D1, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, false, true, true, HFP_CODEC_CVSD));
    CHECK_EQUAL(HFP_LINK_SETTINGS_D1, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, true, false, true, HFP_CODEC_CVSD));
}

TEST(HFPLinkSettings, S3){
    CHECK_EQUAL(HFP_LINK_SETTINGS_S3, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, true, true, false, HFP_CODEC_CVSD));
}

TEST(HFPLinkSettings, S4){
    CHECK_EQUAL(HFP_LINK_SETTINGS_S4, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, true, true, true, HFP_CODEC_CVSD));
}

TEST(HFPLinkSettings, T2){
    CHECK_EQUAL(HFP_LINK_SETTINGS_T2, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, true, true, true, HFP_CODEC_MSBC));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
