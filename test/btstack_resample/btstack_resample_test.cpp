#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include <stdint.h>
#include <string.h>

#include "btstack_resample.h"

#define MAX_INPUT_FRAMES  480
#define MAX_OUTPUT_FRAMES 600

static int16_t input_buffer[MAX_INPUT_FRAMES * BTSTACK_RESAMPLE_MAX_CHANNELS];
static int16_t output_buffer[MAX_OUTPUT_FRAMES * BTSTACK_RESAMPLE_MAX_CHANNELS];

TEST_GROUP(Resample){
    void setup(void){
        for (uint32_t i = 0; i < MAX_INPUT_FRAMES * BTSTACK_RESAMPLE_MAX_CHANNELS; i++){
            input_buffer[i] = (int16_t) i;
        }
        memset(output_buffer, 0, sizeof(output_buffer));
    }

    void check_capacity(uint32_t input_frames, uint32_t output_capacity_frames, int num_channels){
        btstack_resample_t context;
        btstack_resample_init(&context, num_channels);
        btstack_resample_set_factor(&context,
            btstack_resample_get_min_factor_for_output_capacity(input_frames, output_capacity_frames));

        uint16_t output_frames = btstack_resample_block(&context, input_buffer, input_frames, output_buffer);
        CHECK_TRUE(output_frames <= output_capacity_frames);
    }

    void check_capacity_with_bridge_frame(uint32_t input_frames, uint32_t output_capacity_frames, int num_channels){
        btstack_resample_t context;
        btstack_resample_init(&context, num_channels);
        btstack_resample_set_factor(&context,
            btstack_resample_get_min_factor_for_output_capacity(input_frames, output_capacity_frames));

        context.src_pos = 0xffff0000u;
        uint16_t output_frames = btstack_resample_block(&context, input_buffer, input_frames, output_buffer);
        CHECK_TRUE(output_frames <= output_capacity_frames);
    }
};

TEST(Resample, MinFactorKeepsOutputWithinCapacityMono){
    check_capacity(128, 128, 1);
    check_capacity(128, 144, 1);
    check_capacity(128, 160, 1);
    check_capacity(480, 480, 1);
    check_capacity(480, 528, 1);
}

TEST(Resample, MinFactorKeepsOutputWithinCapacityStereo){
    check_capacity(128, 128, 2);
    check_capacity(128, 144, 2);
    check_capacity(128, 160, 2);
    check_capacity(480, 480, 2);
    check_capacity(480, 528, 2);
}

TEST(Resample, MinFactorKeepsOutputWithinCapacityWithBridgeFrame){
    check_capacity_with_bridge_frame(128, 128, 1);
    check_capacity_with_bridge_frame(128, 144, 2);
    check_capacity_with_bridge_frame(480, 528, 1);
    check_capacity_with_bridge_frame(480, 528, 2);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
