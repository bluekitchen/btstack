/*******************************************************************************
    Filename: hal_adc.h

    Copyright 2008 Texas Instruments, Inc.
***************************************************************************/
#ifndef HAL_ADC_H
#define HAL_ADC_H

#define ACC_PWR_PIN       BIT0
#define ACC_PORT_DIR      P6DIR
#define ACC_PORT_OUT      P6OUT
#define ACC_PORT_SEL      P6SEL

#define ACC_X_PIN         BIT1
#define ACC_Y_PIN         BIT2
#define ACC_Z_PIN         BIT3
#define ACC_X_CHANNEL     ADC12INCH_1
#define ACC_Y_CHANNEL     ADC12INCH_2
#define ACC_Z_CHANNEL     ADC12INCH_3
#define TEMP_CHANNEL      ADC12INCH_10
#define VCC_CHANNEL       ADC12INCH_11

#define AUDIO_PORT_DIR    P6DIR
#define AUDIO_PORT_OUT    P6OUT
#define AUDIO_PORT_SEL    P6SEL

#define MIC_POWER_PIN     BIT4
#define MIC_INPUT_PIN     BIT5
#define MIC_INPUT_CHAN    ADC12INCH_5
#define AUDIO_OUT_PWR_PIN BIT6

#define AUDIO_OUT_DIR     P4DIR
#define AUDIO_OUT_OUT     P4OUT
#define AUDIO_OUT_SEL     P4SEL

#define AUDIO_OUT_PIN     BIT4

#define ACC_X_THRESHOLD   200
#define ACC_Y_THRESHOLD   200
#define ACC_X_MAX         1000
#define ACC_Y_MAX         1000
#define ACC_Z_MAX         1000

#define ACC_X_LOW_OFFSET  1950
#define ACC_X_HIGH_OFFSET 2150
#define ACC_Y_LOW_OFFSET  1950
#define ACC_Y_HIGH_OFFSET 2150
#define ACC_Z_LOW_OFFSET  1950
#define ACC_Z_HIGH_OFFSET 2150

#define CELSIUS 			0xFF
#define FAHRENHEIT 			0x00

#define CELSIUS_MUL			7040
#define CELSIUS_OFFSET		2620
#define FAHRENHEIT_MUL		12672
#define FAHRENHEIT_OFFSET	3780 
enum { ADC_OFF_MODE, ADC_ACC_MODE, ADC_TEMP_MODE};

/*-------------Accelerometer Functions----------------------------------------*/
void halAccelerometerInit(void);
void halAccelerometerCalibrate(void);
void halAccelerometerSetCalibratedOffset( int x, int y, int z );
void halAccelerometerGetCalibratedOffset(int *x, int *y, int*z);
void halAccelerometerRead(int* x, int* y, int* z);
void halAccelerometerReadWithOffset(int* x, int* y, int* z);
void halAccelerometerShutDown(void);

/*-------------Temperature & VCC Functions------------------------------------*/
void halAdcInitTempVcc(void);
void halAdcShutDownTempVcc(void);
void halAdcSetTempConversionType(unsigned char conversion);
void halAdcSetTempOffset(long offset);
int  halAdcGetTemp(void);
int  halAdcGetVcc(void);
void halAdcConvertTempVccFromADC(void);
void halAdcReadTempVcc(char *TemperatureStr, char *VccStr);

/*-------------Generic ADC12 Functions----------------------------------------*/
void halAdcStartRead(void);
void halAdcSetQuitFromISR(unsigned char quit);

#endif
