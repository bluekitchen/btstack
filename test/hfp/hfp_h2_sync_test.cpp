#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_debug.h"
#include "classic/hfp.h"

// Test
static uint8_t test_data[HFP_H2_SYNC_FRAME_SIZE];
static uint8_t test_valid_sbc_frame[HFP_H2_SYNC_FRAME_SIZE];
static uint8_t test_invalid_sbc_frame[HFP_H2_SYNC_FRAME_SIZE];
static uint8_t test_invalid_frame[HFP_H2_SYNC_FRAME_SIZE];
static hfp_h2_sync_t test_hfp_h2_sync;
static uint8_t test_num_good_frames;
static uint8_t test_num_bad_frames;

static bool test_hfp_h2_sync_callback(bool bad_frame, const uint8_t * frame_data, uint16_t frame_len){
    btstack_assert(frame_len == HFP_H2_SYNC_FRAME_SIZE);
    if (bad_frame){
        test_num_bad_frames++;
        return false;
    } else {
        // mSBC frame ok <=> sync word = 0xAD
        if (frame_data[2] == 0xAD) {
            test_num_good_frames++;
            return true;
        }
    }
    return false;
}

TEST_GROUP(HFP_H2_SYNC){
    void setup(void){
        test_num_good_frames = 0;
        test_num_bad_frames = 0;
        test_valid_sbc_frame[0] = 0x01;
        test_valid_sbc_frame[1] = 0x08;
        test_valid_sbc_frame[2] = 0xAD;
        test_invalid_sbc_frame[0] = 0x01;
        test_invalid_sbc_frame[1] = 0x08;
        test_invalid_sbc_frame[2] = 0xFF;
        hfp_h2_sync_init(&test_hfp_h2_sync, &test_hfp_h2_sync_callback);
    }
    void teardown(void){
    }
};

// initial setting
TEST(HFP_H2_SYNC, ValidSBCFrame){
    hfp_h2_sync_process(&test_hfp_h2_sync, false, test_valid_sbc_frame, sizeof(test_valid_sbc_frame));
    CHECK_EQUAL(1, test_num_good_frames);
}

TEST(HFP_H2_SYNC, ValidSBCFramefter59){
    hfp_h2_sync_process(&test_hfp_h2_sync, false, test_invalid_frame, sizeof(test_invalid_frame)-1);
    hfp_h2_sync_process(&test_hfp_h2_sync, false, test_valid_sbc_frame, sizeof(test_valid_sbc_frame));
    CHECK_EQUAL(1, test_num_good_frames);
}

TEST(HFP_H2_SYNC, ValidSBCFrameAfter2){
    hfp_h2_sync_process(&test_hfp_h2_sync, false, test_invalid_frame, 2);
    hfp_h2_sync_process(&test_hfp_h2_sync, false, test_valid_sbc_frame, sizeof(test_valid_sbc_frame));
    CHECK_EQUAL(1, test_num_good_frames);
    CHECK_EQUAL(2, test_hfp_h2_sync.dropped_bytes);
}

TEST(HFP_H2_SYNC, BadAndGoodFrame){
    hfp_h2_sync_process(&test_hfp_h2_sync, false, test_invalid_frame, sizeof(test_invalid_frame));
    CHECK_EQUAL(0, test_num_good_frames);
    hfp_h2_sync_process(&test_hfp_h2_sync, false, test_valid_sbc_frame, sizeof(test_valid_sbc_frame));
    CHECK_EQUAL(1, test_num_bad_frames);
    CHECK_EQUAL(1, test_num_good_frames);
}

TEST(HFP_H2_SYNC, BadFrameFlagA){
    hfp_h2_sync_process(&test_hfp_h2_sync, false, test_valid_sbc_frame, sizeof(test_data) - 1);
    hfp_h2_sync_process(&test_hfp_h2_sync, true, test_valid_sbc_frame, 1);
    CHECK_EQUAL(1, test_num_bad_frames);
}

TEST(HFP_H2_SYNC, BadFrameFlagB){
    hfp_h2_sync_process(&test_hfp_h2_sync, true, test_valid_sbc_frame, sizeof(test_valid_sbc_frame));
    CHECK_EQUAL(1, test_num_bad_frames);
}

TEST(HFP_H2_SYNC, InvalidSBCFrame){
    hfp_h2_sync_process(&test_hfp_h2_sync, false, test_invalid_sbc_frame, sizeof(test_invalid_sbc_frame));
    hfp_h2_sync_process(&test_hfp_h2_sync, false, test_invalid_frame, 2);
    CHECK_EQUAL(1, test_num_bad_frames);
    CHECK_EQUAL(0, test_num_good_frames);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
