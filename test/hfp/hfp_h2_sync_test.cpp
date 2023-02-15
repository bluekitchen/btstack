#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "btstack_util.h"
#include "btstack_debug.h"

// HFP H2 Sync
#include <string.h>

#define HFP_H2_SYNC_FRAME_SIZE 60

typedef struct {
    // callback returns true if data was valid
    bool        (*callback)(bool bad_frame, const uint8_t * frame_data, uint16_t frame_len);
    uint8_t     frame_data[HFP_H2_SYNC_FRAME_SIZE];
    uint16_t    frame_len;
    uint16_t    dropped_bytes;
} hfp_h2_sync_t;

// find position of h2 sync header, returns -1 if not found, or h2 sync position
static int16_t hfp_h2_sync_find(const uint8_t * frame_data, uint16_t frame_len){
    uint16_t i;
    for (i=0;i<(frame_len - 1);i++){
        // check: first byte == 1
        uint8_t h2_first_byte = frame_data[i];
        if (h2_first_byte == 0x01) {
            uint8_t h2_second_byte = frame_data[i + 1];
            // check lower nibble of second byte == 0x08
            if ((h2_second_byte & 0x0F) == 8) {
                // check if bits 0+2 == bits 1+3
                uint8_t hn = h2_second_byte >> 4;
                if (((hn >> 1) & 0x05) == (hn & 0x05)) {
                    return (int16_t) i;
                }
            }
        }
    }
    return -1;
}

static void hfp_h2_sync_reset(hfp_h2_sync_t * hfp_h2_sync){
    hfp_h2_sync->frame_len = 0;
}

void hfp_h2_sync_init(hfp_h2_sync_t * hfp_h2_sync,
                      bool (*callback)(bool bad_frame, const uint8_t * frame_data, uint16_t frame_len)){
    hfp_h2_sync->callback = callback;
    hfp_h2_sync->dropped_bytes = 0;
    hfp_h2_sync_reset(hfp_h2_sync);
}

static void hfp_h2_report_bad_frames(hfp_h2_sync_t *hfp_h2_sync){
    // report bad frames
    while (hfp_h2_sync->dropped_bytes >= HFP_H2_SYNC_FRAME_SIZE){
        hfp_h2_sync->dropped_bytes -= HFP_H2_SYNC_FRAME_SIZE;
        (void)(*hfp_h2_sync->callback)(true,NULL, HFP_H2_SYNC_FRAME_SIZE);
    }
}

static void hfp_h2_sync_drop_bytes(hfp_h2_sync_t * hfp_h2_sync, uint16_t bytes_to_drop){
    btstack_assert(bytes_to_drop <= hfp_h2_sync->frame_len);
    memmove(hfp_h2_sync->frame_data, &hfp_h2_sync->frame_data[bytes_to_drop], hfp_h2_sync->frame_len - bytes_to_drop);
    hfp_h2_sync->dropped_bytes += bytes_to_drop;
    hfp_h2_sync->frame_len     -= bytes_to_drop;
    hfp_h2_report_bad_frames(hfp_h2_sync);
}

void hfp_h2_sync_process(hfp_h2_sync_t *hfp_h2_sync, bool bad_frame, const uint8_t *frame_data, uint16_t frame_len) {

    if (bad_frame){
        // drop all data
        hfp_h2_sync->dropped_bytes += hfp_h2_sync->frame_len;
        hfp_h2_sync->frame_len = 0;
        // all new data is bad, too
        hfp_h2_sync->dropped_bytes += frame_len;
        // report frames
        hfp_h2_report_bad_frames(hfp_h2_sync);
        return;
    }

    while (frame_len > 0){
        // Fill hfp_h2_sync->frame_buffer
        uint16_t bytes_free_in_frame_buffer = HFP_H2_SYNC_FRAME_SIZE - hfp_h2_sync->frame_len;
        uint16_t bytes_to_append = btstack_min(frame_len, bytes_free_in_frame_buffer);
        memcpy(&hfp_h2_sync->frame_data[hfp_h2_sync->frame_len], frame_data, bytes_to_append);
        frame_data             += bytes_to_append;
        frame_len              -= bytes_to_append;
        hfp_h2_sync->frame_len += bytes_to_append;
        // check complete frame for h2 sync
        if (hfp_h2_sync->frame_len == HFP_H2_SYNC_FRAME_SIZE){
            bool valid_frame = true;
            int16_t h2_pos = hfp_h2_sync_find(hfp_h2_sync->frame_data, hfp_h2_sync->frame_len);
            if (h2_pos < 0){
                // no h2 sync, no valid frame, keep last byte if it is 0x01
                if (hfp_h2_sync->frame_data[HFP_H2_SYNC_FRAME_SIZE-1] == 0x01){
                    hfp_h2_sync_drop_bytes(hfp_h2_sync, HFP_H2_SYNC_FRAME_SIZE - 1);
                } else {
                    hfp_h2_sync_drop_bytes(hfp_h2_sync, HFP_H2_SYNC_FRAME_SIZE);
                }
                valid_frame = false;
            }
            else if (h2_pos > 0){
                // drop data before h2 sync
                hfp_h2_sync_drop_bytes(hfp_h2_sync, h2_pos);
                valid_frame = false;
            }
            if (valid_frame) {
                // h2 sync at pos 0 and complete frame
                bool codec_ok = (*hfp_h2_sync->callback)(false, hfp_h2_sync->frame_data, hfp_h2_sync->frame_len);
                if (codec_ok){
                    hfp_h2_sync_reset(hfp_h2_sync);
                } else {
                    // drop first two bytes
                    hfp_h2_sync_drop_bytes(hfp_h2_sync, 2);
                }
            }
        }
    }
}

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
