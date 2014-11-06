/** 
 * @file  hal_adc.c
 * 
 *  Copyright 2008 Texas Instruments, Inc.
***************************************************************************/

#include "hal_adc.h"

#include <msp430x54x.h>
#include "hal_compat.h"

static int SavedADC12MEM0 = 0, SavedADC12MEM1 = 0, SavedADC12MEM2 = 0;
static int Acc_x = 0, Acc_y = 0, Acc_z = 0;
static int Acc_x_offset = 0, Acc_y_offset = 0, Acc_z_offset = 0;
static long int Vcc = 0, Temperature = 0;
static long int temperatureOffset = CELSIUS_OFFSET;
static unsigned char conversionType = CELSIUS, adcMode = ADC_OFF_MODE;
static unsigned char exit_active_from_ADC12 = 0;

/************************************************************************
 * @brief  Turns on and initializes ADC12, accelerometer in order to 
 *         sample x, y, z-axis inputs.
 * 
 * @param  none
 * 
 * @return none
 *************************************************************************/
void halAccelerometerInit(void)
{
  adcMode = ADC_ACC_MODE;
  ACC_PORT_SEL |= ACC_X_PIN + ACC_Y_PIN;    //Enable A/D channel inputs
  ACC_PORT_DIR &= ~(ACC_X_PIN + ACC_Y_PIN + ACC_Z_PIN);
  ACC_PORT_DIR |= ACC_PWR_PIN;              //Enable ACC_POWER
  ACC_PORT_OUT |= ACC_PWR_PIN;

  //Sequence of channels, once, ACLK  
  ADC12CTL0 = ADC12ON + ADC12SHT02 + ADC12MSC;  
  ADC12CTL1 = ADC12SHP + ADC12CONSEQ_1 + ADC12SSEL_0;  
  ADC12CTL2 = ADC12RES_2;    
  ADC12MCTL0 = ACC_X_CHANNEL;
  ADC12MCTL1 = ACC_Y_CHANNEL;
  ADC12MCTL2 = ACC_Z_CHANNEL + ADC12EOS;
  
  // Allow the accelerometer to settle before sampling any data

  // 4.5.3-20110706-2 doesn't allow for 32-bit delay cycles
  int i;
  for (i=0;i<10;i++){
      __delay_cycles(20000);                     
  }
  UCSCTL8 |= MODOSCREQEN;                   
}

/************************************************************************
 * @brief  Calibrates the offset values for x, y, and z axes.
 * 
 * @param  none
 * 
 * @return none
 *************************************************************************/
void halAccelerometerCalibrate(void)
{
  unsigned char tempQuit;
  
  tempQuit = exit_active_from_ADC12;
  halAdcSetQuitFromISR( 1 );
  halAdcStartRead();
  
  __bis_SR_register(LPM3_bits + GIE);   
  __no_operation(); 
  
  halAccelerometerReadWithOffset(&Acc_x_offset, &Acc_y_offset, &Acc_z_offset);
  halAdcSetQuitFromISR( tempQuit );    
}

/************************************************************************
 * @brief  Set function for the calibrated offsets for the x, y, and z axes.
 * 
 * @param  x Calibrated offset for the x-axis
 * 
 * @param  y Calibrated offset for the y-axis
 * 
 * @param  z Calibrated offset for the z-axis
 * 
 * @return none
 *************************************************************************/
void halAccelerometerSetCalibratedOffset( int x, int y, int z )
{
  Acc_x_offset = x;
  Acc_y_offset = y;
  Acc_z_offset = z;
}

/************************************************************************
 * @brief  Get function for the x, y, and z axes calibrated offsets
 * 
 * @param  x Pointer to the calibrated offset for the x-axis
 * 
 * @param  y Pointer to the calibrated offset for the y-axis
 * 
 * @param  z Pointer to the calibrated offset for the z-axis
 * 
 * @return none 
 *************************************************************************/
void halAccelerometerGetCalibratedOffset(int *x, int *y, int *z)
{
  *x = Acc_x_offset;
  *y = Acc_y_offset;
  *z = Acc_y_offset;
}

/************************************************************************
 * @brief  Get function for the x, y, and z accelerometer samples, 
 *         including the calibrated offsets.
 * 
 * @param  x Pointer to the accelerometer reading (x-axis)
 * 
 * @param  y Pointer to the accelerometer reading (y-axis)
 * 
 * @param  z Pointer to the accelerometer reading (z-axis)
 * 
 * @return none
 *************************************************************************/
void halAccelerometerRead(int *x, int *y, int *z)
{
  Acc_x = SavedADC12MEM0;
  Acc_y = SavedADC12MEM1;
  Acc_z = SavedADC12MEM2;
  
  *x = Acc_x - Acc_x_offset;
  *y = Acc_y - Acc_y_offset;
  *z = Acc_z - Acc_z_offset;  
}

/************************************************************************
 * @brief  Get function for the x, y, and z accelerometer samples, 
 *         excluding the calibrated offsets.
 * 
 * @param  x Pointer to the accelerometer reading (x-axis)
 * 
 * @param  y Pointer to the accelerometer reading (y-axis)
 * 
 * @param  z Pointer to the accelerometer reading (z-axis)
 * 
 * @return none
 *************************************************************************/
void halAccelerometerReadWithOffset(int *x, int *y, int *z)
{
  *x = SavedADC12MEM0;
  *y = SavedADC12MEM1;    
  *z = SavedADC12MEM2;
}

/************************************************************************
 * @brief  Disables the ADC12, accelerometer that sampled x, y, z-axis inputs.
 * 
 * @param  none
 * 
 * @return none
 *************************************************************************/
void halAccelerometerShutDown(void)
{
  //Turn off ADC Module
  ADC12CTL0 &= ~( ADC12ON + ADC12ENC );  
  ACC_PORT_OUT &= ~ACC_PWR_PIN;             //Disable ACC_POWER

  //Disable A/D channel inputs
  ACC_PORT_SEL &= ~(ACC_X_PIN + ACC_Y_PIN + ACC_Z_PIN); 
  ACC_PORT_DIR |= (ACC_X_PIN + ACC_Y_PIN + ACC_Z_PIN + ACC_PWR_PIN);
  ACC_PORT_OUT &= ~(ACC_X_PIN + ACC_Y_PIN + ACC_Z_PIN + ACC_PWR_PIN);
  
  adcMode = ADC_OFF_MODE;
}

/*----------------------------------------------------------------------------*/
/************************************************************************
 * @brief  Intializes the ADC12 to sample Temperature and Vcc. 
 * 
 * @param  none
 * 
 * @return none
 *************************************************************************/
void halAdcInitTempVcc(void)
{
  //Sequence of channels, once,   
  adcMode = ADC_TEMP_MODE;
  UCSCTL8 |= MODOSCREQEN;
  ADC12CTL0 = ADC12ON + ADC12SHT0_15 + ADC12MSC + + ADC12REFON + ADC12REF2_5V;  
  ADC12CTL1 = ADC12SHP + ADC12CONSEQ_1 + ADC12SSEL_0;  
  ADC12CTL2 = ADC12RES_2;    
  
  ADC12MCTL0 = ADC12SREF_1 + TEMP_CHANNEL;
  ADC12MCTL1 = ADC12SREF_1 + VCC_CHANNEL + ADC12EOS;                            
}

/************************************************************************
 * @brief  Turns off / disable the ADC12. 
 * 
 * @param  none
 * 
 * @return none
 *************************************************************************/
void halAdcShutDownTempVcc(void)
{
  ADC12CTL0 &= ~ ( ADC12ON + ADC12ENC + ADC12REFON );
  adcMode = ADC_OFF_MODE;      
}

/************************************************************************
 * @brief  Sets the conversion type to either Farenheit (F) or Celsius (C).
 * 
 * @param  conversion The #define constant CELSIUS or FAHRENHEIT. 
 * 
 * @return none
 *************************************************************************/
void halAdcSetTempConversionType(unsigned char conversion)
{
  conversionType = conversion;
}

/************************************************************************
 * @brief  Set function for the calibrated temperature offset.
 * 
 * @param  offset The temperature offset.
 * 
 * @return none
 *************************************************************************/
void halAdcSetTempOffset(long offset)
{
  temperatureOffset = offset;
}

/************************************************************************
 * @brief  Get function for the current temperature value. 
 * 
 * @param  none
 * 
 * @return The current temperature value. 
 *************************************************************************/
int halAdcGetTemp(void)
{
  return Temperature; 
}

/************************************************************************
 * @brief  Get function for the current Vcc value. 
 * 
 * @param  none
 * 
 * @return The current Vcc value.  
 *************************************************************************/
int halAdcGetVcc(void)
{
  return Vcc;
}

/************************************************************************
 * @brief  Converts the Vcc and Temp readings from the ADC to BCD format. 
 * 
 * @param  none
 * 
 * @return none
 *************************************************************************/
void halAdcConvertTempVccFromADC(void)
{ 	
  long multiplier, offset;

  // Convert Vcc
  Vcc = SavedADC12MEM1;
  Vcc = Vcc * 50;
  Vcc = Vcc / 4096;
    
  // Convert Temperature  
  if (conversionType == CELSIUS)
  {
    multiplier = CELSIUS_MUL;
    offset = temperatureOffset;
  }  
  else
  {
    multiplier = (long) CELSIUS_MUL * 9 /5 ;
    offset = (long) temperatureOffset * 9 / 5 - 320;    
  }   
  Temperature = (long) SavedADC12MEM0 * multiplier/4096 - offset;   	
}

/************************************************************************
 * @brief  Get function for the temperature and Vcc samples in "xxx^C/F" and 
 *         "x.xV" format.
 * 
 * @param  TemperatureStr The string that holds the temperature reading
 * 
 * @param  Vcc            The string that holds the Vcc reading
 * 
 * @return none
 *************************************************************************/
void halAdcReadTempVcc(char *TemperatureStr, char *VccStr)
{    
  unsigned char i, leadingZero = 0;
  long int dummyTemperature, dummyVcc;
	
  halAdcConvertTempVccFromADC(); 
  dummyTemperature = Temperature;
  dummyVcc = Vcc;	
  for (i = 0; i < 6; i++)   	
	  TemperatureStr[i] = '\0';	 
	i=0;
  //Check for negative  	   	
  if (Temperature < 0)
  {
  	TemperatureStr[i++]='-';
  	Temperature = -Temperature;  	
  }
  TemperatureStr[i] ='0';
  if (Temperature >= 1000)
  {
  	TemperatureStr[i]='1';
  	Temperature -=1000;
  	leadingZero = 1;
  }
  if (leadingZero == 1)
    i++;
  //100s digit
  TemperatureStr[i] = '0';
  if  (Temperature >= 100)  
  {
    do
	{
	  TemperatureStr[i]++;
	  Temperature -=100;
	}
	while (Temperature >=100);	  
	leadingZero =  1;
  }
  if (leadingZero == 1)
    i++;
  //10s digit
  TemperatureStr[i] = '0';
  if (Temperature >=10)
  {
	  do
	  {
	    TemperatureStr[i]++;
	    Temperature -=10;
	  }
	  while (Temperature >=10);	  
  }
  
  TemperatureStr[++i] = '^';
  if (conversionType == CELSIUS)
  	TemperatureStr[++i]='C';
  else
  	TemperatureStr[++i]='F';
  
  VccStr[0] = '0';
  VccStr[2] = '0';
  while (Vcc >= 10)
  {
  	VccStr[0]++;
  	Vcc -= 10;  	
  }
  VccStr[2] += Vcc;  
  Temperature = dummyTemperature;
  Vcc = dummyVcc;
}

/*----------------------------------------------------------------------------*/
/************************************************************************
 * @brief  Starts the ADC conversion.
 * 
 * @param  none
 * 
 * @return none
 *************************************************************************/
void halAdcStartRead(void)
{
  ADC12IFG &= ~(BIT1+BIT0);                 // Clear any pending flags 
  
  if (adcMode == ADC_ACC_MODE)
  {
  	ADC12CTL0 |=  ADC12ENC | ADC12SC ;   
  	ADC12IE |= BIT2;
  }
  else
  {
    ADC12CTL0 |= ADC12REFON;                // Turn on ADC12 reference
    
    // Delay to stabilize ADC12 reference assuming the fastest MCLK of 18 MHz. 
    // 35 us = 1 / 18 MHz * 630 
    __delay_cycles(630); 
  	
	ADC12IE |= BIT1;                        // Enable interrupt
  	ADC12CTL0 |=  ADC12ENC | ADC12SC;  
  }
}

/************************************************************************
 * @brief  Sets the flag that causes an exit into active CPU mode from 
 *         the ADC12 ISR.
 * 
 * @param  quit 
 * 
 * - 1 - Exit active from ADC12 ISR
 * - 0 - Remain in LPMx on exit from ADC12ISR
 * 
 * @return none
 *************************************************************************/
void halAdcSetQuitFromISR(unsigned char quit)
{
  exit_active_from_ADC12 = quit;  
}

/*----------------------------------------------------------------------------*/

#ifdef __GNUC__
__attribute__((interrupt(ADC12_VECTOR)))
#endif
#ifdef __IAR_SYSTEMS_ICC__
#pragma vector=ADC12_VECTOR
__interrupt
#endif
void ADC12_ISR(void)
{
    SavedADC12MEM0 = ADC12MEM0;             // Store the sampled data 
	SavedADC12MEM1 = ADC12MEM1;
	SavedADC12MEM2 = ADC12MEM2;
    ADC12IFG = 0;                           // Clear the interrupt flags
	ADC12CTL0 &= ~( ADC12ENC | ADC12SC | ADC12REFON);    
    if (exit_active_from_ADC12) __bic_SR_register_on_exit(LPM3_bits);    
}
