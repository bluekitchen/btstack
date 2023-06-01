#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/hfp.h"

#include "mock.h"

TEST_GROUP(HFPLinkSettings){
        void setup(void){
            hfp_set_sco_packet_types(SCO_PACKET_TYPES_ALL);
        }
        void teardown(void){
        }
};

// initial setting
TEST(HFPLinkSettings, NONE){
    CHECK_EQUAL(HFP_LINK_SETTINGS_NONE, hfp_next_link_setting(HFP_LINK_SETTINGS_D1, SCO_PACKET_TYPES_SCO, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_MSBC));
}

TEST(HFPLinkSettings, D1){
    CHECK_EQUAL(HFP_LINK_SETTINGS_D1, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_SCO, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_CVSD));
    CHECK_EQUAL(HFP_LINK_SETTINGS_D1, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_SCO, true, HFP_CODEC_CVSD));
}

TEST(HFPLinkSettings, S3){
    CHECK_EQUAL(HFP_LINK_SETTINGS_S3, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, false, HFP_CODEC_CVSD));
}

TEST(HFPLinkSettings, S4){
    CHECK_EQUAL(HFP_LINK_SETTINGS_S4, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_CVSD));
}

TEST(HFPLinkSettings, T2){
    CHECK_EQUAL(HFP_LINK_SETTINGS_T2, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_MSBC));
}

// regular transition
TEST(HFPLinkSettings, T2_T1){
    CHECK_EQUAL(HFP_LINK_SETTINGS_T1, hfp_next_link_setting(HFP_LINK_SETTINGS_T2, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_MSBC));
}
TEST(HFPLinkSettings, T1_NONE){
    CHECK_EQUAL(HFP_LINK_SETTINGS_NONE, hfp_next_link_setting(HFP_LINK_SETTINGS_T1, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_MSBC));
}
TEST(HFPLinkSettings, S4_S3){
    CHECK_EQUAL(HFP_LINK_SETTINGS_S3, hfp_next_link_setting(HFP_LINK_SETTINGS_S4, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_CVSD));
}
TEST(HFPLinkSettings, S3_S2){
    CHECK_EQUAL(HFP_LINK_SETTINGS_S2, hfp_next_link_setting(HFP_LINK_SETTINGS_S3, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_CVSD));
}
TEST(HFPLinkSettings, S2_S1){
    CHECK_EQUAL(HFP_LINK_SETTINGS_S1, hfp_next_link_setting(HFP_LINK_SETTINGS_S2, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_CVSD));
}
TEST(HFPLinkSettings, D1_D0){
    CHECK_EQUAL(HFP_LINK_SETTINGS_D0, hfp_next_link_setting(HFP_LINK_SETTINGS_D1, SCO_PACKET_TYPES_SCO, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_CVSD));
    CHECK_EQUAL(HFP_LINK_SETTINGS_D0, hfp_next_link_setting(HFP_LINK_SETTINGS_D1, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_SCO, true, HFP_CODEC_CVSD));
}

// initial settings based on packet types
TEST(HFPLinkSettings, HV1){
    hfp_set_sco_packet_types(SCO_PACKET_TYPES_HV1);
    CHECK_EQUAL(HFP_LINK_SETTINGS_D0, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_SCO, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_CVSD));
    CHECK_EQUAL(HFP_LINK_SETTINGS_D0, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_SCO, true, HFP_CODEC_CVSD));
}

TEST(HFPLinkSettings, HV3){
    hfp_set_sco_packet_types(SCO_PACKET_TYPES_HV3);
    CHECK_EQUAL(HFP_LINK_SETTINGS_D1, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, false, HFP_CODEC_CVSD));
}

TEST(HFPLinkSettings, EV3){
    hfp_set_sco_packet_types(SCO_PACKET_TYPES_EV3);
    CHECK_EQUAL(HFP_LINK_SETTINGS_S1, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_CVSD));
    CHECK_EQUAL(HFP_LINK_SETTINGS_T1, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true, HFP_CODEC_MSBC));
}

TEST(HFPLinkSettings, 2EV3){
    hfp_set_sco_packet_types(SCO_PACKET_TYPES_2EV3);
    CHECK_EQUAL(HFP_LINK_SETTINGS_S3, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, false, HFP_CODEC_CVSD));
    CHECK_EQUAL(HFP_LINK_SETTINGS_S4, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true,  HFP_CODEC_CVSD));
    CHECK_EQUAL(HFP_LINK_SETTINGS_T2, hfp_next_link_setting(HFP_LINK_SETTINGS_NONE, SCO_PACKET_TYPES_ALL, SCO_PACKET_TYPES_ALL, true,  HFP_CODEC_MSBC));
}

TEST(HFPLinkSettings, Safe_CVSD_SCO_NOSC){
    CHECK_EQUAL(HFP_LINK_SETTINGS_D1, hfp_safe_settings_for_context(false, HFP_CODEC_CVSD, false));
}

TEST(HFPLinkSettings, Safe_CVSD_eSCO_NOSC){
    CHECK_EQUAL(HFP_LINK_SETTINGS_S1, hfp_safe_settings_for_context(true, HFP_CODEC_CVSD, false));
}

TEST(HFPLinkSettings, Safe_CVSD_eSCO_SC){
    CHECK_EQUAL(HFP_LINK_SETTINGS_S4, hfp_safe_settings_for_context(true, HFP_CODEC_CVSD, true));
}

TEST(HFPLinkSettings, Safe_MSBC_eSCO_NOSC){
    CHECK_EQUAL(HFP_LINK_SETTINGS_T1, hfp_safe_settings_for_context(true, HFP_CODEC_MSBC, false));
}

TEST(HFPLinkSettings, Safe_MSBC_eSCO_SC){
    CHECK_EQUAL(HFP_LINK_SETTINGS_T2, hfp_safe_settings_for_context(true, HFP_CODEC_MSBC, true));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
