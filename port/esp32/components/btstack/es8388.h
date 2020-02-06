#ifndef ES8388_H

#define ES8388_H

#include "esp_types.h"
#include "driver/i2c.h"

/* ES8388 address */
#define ES8388_ADDR 0x20  // 0x22:CE=1;0x20:CE=0

/* ES8388 register */
#define ES8388_CONTROL1         0x00
#define ES8388_CONTROL2         0x01

#define ES8388_CHIPPOWER        0x02

#define ES8388_ADCPOWER         0x03
#define ES8388_DACPOWER         0x04

#define ES8388_CHIPLOPOW1       0x05
#define ES8388_CHIPLOPOW2       0x06

#define ES8388_ANAVOLMANAG      0x07

#define ES8388_MASTERMODE       0x08
/* ADC */
#define ES8388_ADCCONTROL1      0x09
#define ES8388_ADCCONTROL2      0x0a
#define ES8388_ADCCONTROL3      0x0b
#define ES8388_ADCCONTROL4      0x0c
#define ES8388_ADCCONTROL5      0x0d
#define ES8388_ADCCONTROL6      0x0e
#define ES8388_ADCCONTROL7      0x0f
#define ES8388_ADCCONTROL8      0x10
#define ES8388_ADCCONTROL9      0x11
#define ES8388_ADCCONTROL10     0x12
#define ES8388_ADCCONTROL11     0x13
#define ES8388_ADCCONTROL12     0x14
#define ES8388_ADCCONTROL13     0x15
#define ES8388_ADCCONTROL14     0x16
/* DAC */
#define ES8388_DACCONTROL1      0x17
#define ES8388_DACCONTROL2      0x18
#define ES8388_DACCONTROL3      0x19
#define ES8388_DACCONTROL4      0x1a
#define ES8388_DACCONTROL5      0x1b
#define ES8388_DACCONTROL6      0x1c
#define ES8388_DACCONTROL7      0x1d
#define ES8388_DACCONTROL8      0x1e
#define ES8388_DACCONTROL9      0x1f
#define ES8388_DACCONTROL10     0x20
#define ES8388_DACCONTROL11     0x21
#define ES8388_DACCONTROL12     0x22
#define ES8388_DACCONTROL13     0x23
#define ES8388_DACCONTROL14     0x24
#define ES8388_DACCONTROL15     0x25
#define ES8388_DACCONTROL16     0x26
#define ES8388_DACCONTROL17     0x27
#define ES8388_DACCONTROL18     0x28
#define ES8388_DACCONTROL19     0x29
#define ES8388_DACCONTROL20     0x2a
#define ES8388_DACCONTROL21     0x2b
#define ES8388_DACCONTROL22     0x2c
#define ES8388_DACCONTROL23     0x2d
#define ES8388_DACCONTROL24     0x2e
#define ES8388_DACCONTROL25     0x2f
#define ES8388_DACCONTROL26     0x30
#define ES8388_DACCONTROL27     0x31
#define ES8388_DACCONTROL28     0x32
#define ES8388_DACCONTROL29     0x33
#define ES8388_DACCONTROL30     0x34

typedef enum {
    ES_MODULE_MIN = -1,
    ES_MODULE_ADC = 0x01,
    ES_MODULE_DAC = 0x02,
    ES_MODULE_ADC_DAC = 0x03,
    ES_MODULE_LINE = 0x04,
    ES_MODULE_MAX
} es_codec_module_t;

typedef enum {
    ES_ = -1,
    ES_I2S_NORMAL = 0,
    ES_I2S_LEFT = 1,
    ES_I2S_RIGHT = 2,
    ES_I2S_DSP = 3,
    ES_I2S_MAX
} es_codec_i2s_fmt_t;

typedef enum {
    MclkDiv_MIN = -1,
    MclkDiv_1 = 1,
    MclkDiv_2 = 2,
    MclkDiv_3 = 3,
    MclkDiv_4 = 4,
    MclkDiv_6 = 5,
    MclkDiv_8 = 6,
    MclkDiv_9 = 7,
    MclkDiv_11 = 8,
    MclkDiv_12 = 9,
    MclkDiv_16 = 10,
    MclkDiv_18 = 11,
    MclkDiv_22 = 12,
    MclkDiv_24 = 13,
    MclkDiv_33 = 14,
    MclkDiv_36 = 15,
    MclkDiv_44 = 16,
    MclkDiv_48 = 17,
    MclkDiv_66 = 18,
    MclkDiv_72 = 19,
    MclkDiv_5 = 20,
    MclkDiv_10 = 21,
    MclkDiv_15 = 22,
    MclkDiv_17 = 23,
    MclkDiv_20 = 24,
    MclkDiv_25 = 25,
    MclkDiv_30 = 26,
    MclkDiv_32 = 27,
    MclkDiv_34 = 28,
    MclkDiv_7  = 29,
    MclkDiv_13 = 30,
    MclkDiv_14 = 31,
    MclkDiv_MAX,
} sclk_div_t;

typedef enum {
    LclkDiv_MIN = -1,
    LclkDiv_128 = 0,
    LclkDiv_192 = 1,
    LclkDiv_256 = 2,
    LclkDiv_384 = 3,
    LclkDiv_512 = 4,
    LclkDiv_576 = 5,
    LclkDiv_768 = 6,
    LclkDiv_1024 = 7,
    LclkDiv_1152 = 8,
    LclkDiv_1408 = 9,
    LclkDiv_1536 = 10,
    LclkDiv_2112 = 11,
    LclkDiv_2304 = 12,

    LclkDiv_125 = 16,
    LclkDiv_136 = 17,
    LclkDiv_250 = 18,
    LclkDiv_272 = 19,
    LclkDiv_375 = 20,
    LclkDiv_500 = 21,
    LclkDiv_544 = 22,
    LclkDiv_750 = 23,
    LclkDiv_1000 = 24,
    LclkDiv_1088 = 25,
    LclkDiv_1496 = 26,
    LclkDiv_1500 = 27,
    LclkDiv_MAX,
} lclk_div_t;


typedef struct {
    sclk_div_t sclk_div;
    lclk_div_t lclk_div;
} es_codec_i2s_clock_t;

typedef enum {
    BIT_LENGTH_MIN = -1,
    BIT_LENGTH_16BITS = 0x03,
    BIT_LENGTH_18BITS = 0x02,
    BIT_LENGTH_20BITS = 0x01,
    BIT_LENGTH_24BITS = 0x00,
    BIT_LENGTH_32BITS = 0x04,
    BIT_LENGTH_MAX,
} es_codec_bits_len_t;

typedef enum {
    MIC_GAIN_MIN = -1,
    MIC_GAIN_0DB = 0,
    MIC_GAIN_3DB = 3,
    MIC_GAIN_6DB = 6,
    MIC_GAIN_9DB = 9,
    MIC_GAIN_12DB = 12,
    MIC_GAIN_15DB = 15,
    MIC_GAIN_18DB = 18,
    MIC_GAIN_21DB = 21,
    MIC_GAIN_24DB = 24,
    MIC_GAIN_MAX,
} es_codec_mic_gain_t;



typedef enum {
    DAC_OUTPUT_MIN = -1,
    DAC_OUTPUT_LOUT1 = 0x04,
    DAC_OUTPUT_LOUT2 = 0x08,
    DAC_OUTPUT_SPK   = 0x09,
    DAC_OUTPUT_ROUT1 = 0x10,
    DAC_OUTPUT_ROUT2 = 0x20,
    DAC_OUTPUT_ALL = 0x3c,
    DAC_OUTPUT_MAX,
} es_codec_dac_output_t;

typedef enum {
    ADC_INPUT_MIN = -1,
    ADC_INPUT_LINPUT1_RINPUT1 = 0x00,
    ADC_INPUT_MIC1	= 0x05,
    ADC_INPUT_MIC2	= 0x06,
    ADC_INPUT_LINPUT2_RINPUT2 = 0x50,
    ADC_INPUT_DIFFERENCE = 0xf0,
    ADC_INPUT_MAX,
} es_codec_adc_input_t;


typedef enum {
    ES_MODE_MIN = -1,
    ES_MODE_SLAVE = 0x00,
    ES_MODE_MASTER = 0x01,
    ES_MODE_MAX,
} es_codec_mode_t;

typedef struct {
    es_codec_mode_t es_mode;
    i2c_port_t i2c_port_num;
    i2c_config_t i2c_cfg;
    es_codec_dac_output_t dac_output;
    es_codec_adc_input_t adc_input;
} es8388_config_t; 


#define AUDIO_CODEC_ES8388_DEFAULT(){ \
    .es_mode = ES_MODE_SLAVE, \
    .i2c_port_num = I2C_NUM_0, \
    .i2c_cfg = { \
        .mode = I2C_MODE_MASTER, \
        .sda_io_num = IIC_DATA, \
        .scl_io_num = IIC_CLK, \
        .sda_pullup_en = GPIO_PULLUP_ENABLE,\
        .scl_pullup_en = GPIO_PULLUP_ENABLE,\
        .master.clk_speed = 100000\
    }, \
    .adc_input= ADC_INPUT_LINPUT1_RINPUT1,\
    .dac_output = DAC_OUTPUT_LOUT1 | DAC_OUTPUT_LOUT2 | DAC_OUTPUT_ROUT1 | DAC_OUTPUT_ROUT2,\
};

int es8388_init(es8388_config_t *cfg);
void es8388_uninit();

int es8388_config_fmt(es_codec_module_t mode, es_codec_i2s_fmt_t fmt);

int es8388_i2s_config_clock(es_codec_i2s_clock_t cfg);
int es8388_set_bits_per_sample(es_codec_module_t mode, es_codec_bits_len_t bits);

int es8388_start(es_codec_module_t mode);
int es8388_stop(es_codec_module_t mode);

int es8388_set_volume(int volume);
int es8388_get_volume(int *volume);

int es8388_set_mute(int enable);
int es8388_get_mute(int *enable);
int es8388_set_mic_gain(es_codec_mic_gain_t gain);

int es8388_set_adc_input(es_codec_adc_input_t input);
int es8388_set_dac_ouput(es_codec_dac_output_t output);
void es8388_pa_power(bool enable);


#endif
