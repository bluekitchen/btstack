/** 
 * @file  hal_board.c
 * 
 * Copyright 2008 Texas Instruments, Inc.
******************************************************************************/
#include "hal_board.h"

#include "msp430.h"

#include "hal_compat.h"
// #include "hal_adc.h"
// #include "hal_usb.h"

static void halBoardSetVCoreUp(unsigned char level);
static void halBoardSetVCoreDown(unsigned char level);
static void halBoardGetSystemClockSettings(unsigned char systemClockSpeed, 
                                           unsigned char *setDcoRange,
                                           unsigned char *setVCore,
                                           unsigned int  *setMultiplier);

/************************************************************************
 * @brief  Increments the VCore setting.
 * 
 * @param  level The target VCore setting
 * 
 * @return none
 *************************************************************************/
static void halBoardSetVCoreUp (unsigned char level)
{
  // Open PMM module registers for write access 
  PMMCTL0_H = 0xA5;                         
  
  // Set SVS/M high side to new level
  SVSMHCTL = (SVSMHCTL & ~(SVSHRVL0*3 + SVSMHRRL0)) | \
             (SVSHE + SVSHRVL0 * level + SVMHE + SVSMHRRL0 * level); 
  
  // Set SVM new Level    
  SVSMLCTL = SVSLE + SVMLE + SVSMLRRL0 * level;       
  // Set SVS/M low side to new level
  SVSMLCTL = (SVSMLCTL & ~(SVSMLRRL_3)) | (SVMLE + SVSMLRRL0 * level);
   
  while ((PMMIFG & SVSMLDLYIFG) == 0);      // Wait till SVM is settled (Delay)
  PMMCTL0_L = PMMCOREV0 * level;            // Set VCore to x
  PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);        // Clear already set flags
  
  if ((PMMIFG & SVMLIFG))
    while ((PMMIFG & SVMLVLRIFG) == 0);     // Wait till level is reached
  
  // Set SVS/M Low side to new level
  SVSMLCTL = (SVSMLCTL & ~(SVSLRVL0*3 + SVSMLRRL_3)) | \
             (SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level);
  
  // Lock PMM module registers from write access
  PMMCTL0_H = 0x00;                         
}

/************************************************************************
 * @brief  Decrements the VCore setting.
 * 
 * @param  level The target VCore.  
 * 
 * @return none
 *************************************************************************/
static void halBoardSetVCoreDown(unsigned char level)
{
  // Open PMM module registers for write access
  PMMCTL0_H = 0xA5;                         
  
  // Set SVS/M low side to new level
  SVSMLCTL = (SVSMLCTL & ~(SVSLRVL0*3 + SVSMLRRL_3)) | \
             (SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level); 
  
  while ((PMMIFG & SVSMLDLYIFG) == 0);      // Wait till SVM is settled (Delay)
  PMMCTL0_L = (level * PMMCOREV0);          // Set VCore to new level 
  // Lock PMM module registers for write access
  
  PMMCTL0_H = 0x00;                         
}

/************************************************************************
 * @brief  Get function for the DCORSEL, VCORE, and DCO multiplier settings 
 *         that map to a given clock speed. 
 * 
 * @param  systemClockSpeed Target DCO frequency - SYSCLK_xxMHZ.
 * 
 * @param  setDcoRange      Pointer to the DCO range select bits.
 * 
 * @param  setVCore         Pointer to the VCore level bits. 
 * 
 * @param  setMultiplier    Pointer to the DCO multiplier bits. 
 *
 * @return none
 ************************************************************************/
static void halBoardGetSystemClockSettings(unsigned char systemClockSpeed, 
                                    unsigned char *setDcoRange,
                                    unsigned char *setVCore,
                                    unsigned int  *setMultiplier)
{
  switch (systemClockSpeed)
  {
  case SYSCLK_1MHZ: 
    *setDcoRange = DCORSEL_1MHZ;
    *setVCore = VCORE_1MHZ;
    *setMultiplier = DCO_MULT_1MHZ;
    break;
  case SYSCLK_4MHZ: 
    *setDcoRange = DCORSEL_4MHZ;
    *setVCore = VCORE_4MHZ;
    *setMultiplier = DCO_MULT_4MHZ;
    break;
  case SYSCLK_8MHZ: 
    *setDcoRange = DCORSEL_8MHZ;
    *setVCore = VCORE_8MHZ;
    *setMultiplier = DCO_MULT_8MHZ;
    break;
  case SYSCLK_12MHZ: 
    *setDcoRange = DCORSEL_12MHZ;
    *setVCore = VCORE_12MHZ;
    *setMultiplier = DCO_MULT_12MHZ;
    break;
  case SYSCLK_16MHZ: 
    *setDcoRange = DCORSEL_16MHZ;
    *setVCore = VCORE_16MHZ;
    *setMultiplier = DCO_MULT_16MHZ;
    break;     
  }	
}

/************************************************************************
 * @brief  Set function for the PMM core voltage (PMMCOREV) setting
 * 
 * @param  level Target VCore setting 
 * 
 * @return none
 *************************************************************************/
void halBoardSetVCore(unsigned char level)
{
  unsigned int currentVCore;

  currentVCore = PMMCTL0 & PMMCOREV_3;      // Get actual VCore  
  // Change VCore step by step 
  while (level != currentVCore) 
  {   
    if (level > currentVCore) 
      halBoardSetVCoreUp(++currentVCore);
    else
      halBoardSetVCoreDown(--currentVCore);
  }
}

/************************************************************************
 * @brief  Initialization routine for XT1. 
 * 
 * Sets the necessary internal capacitor values and loops until all 
 * ocillator fault flags remain cleared. 
 * 
 * @param  none
 * 
 * @return none
 *************************************************************************/
void halBoardStartXT1(void)
{
  /* LFXT can take up to 1000ms to start.
   * Go to the loop below 4 times for a total of 2 sec timout.
   * If a timeout happens due to no XTAL present or a faulty XTAL
   * the clock system will fall back to REFOCLK (~32kHz) */
  P5SEL |= BIT4 + BIT5;
  /* Set XTAL2 pins to output to reduce power consumption */
  P5DIR |= BIT2 + BIT3;
  /* Turn XT1 ON */
  UCSCTL6 &= ~(XT1OFF);
  /* Set XTAL CAPS to 12 pF */
  UCSCTL6 |= XCAP_3;

  do {
    /* Clear Oscillator fault flags */
    UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG);
    /* Clear the Oscillator fault interrupt flag */
    SFRIFG1 &= ~OFIFG;
    /* Test the fault flag */
    __delay_cycles(56250); 
  } while (SFRIFG1 & OFIFG);
  /* Reduse drive strength to reduce power consumption */
  UCSCTL6 &= ~(XT1DRIVE_3);
}

/************************************************************************
 * @brief  Set function for MCLK frequency.
 * 
 * @param  systemClockSpeed Intended frequency of operation - SYSCLK_xxMHZ.
 * 
 * @return none
 *************************************************************************/
void halBoardSetSystemClock(unsigned char systemClockSpeed)
{
  unsigned char setDcoRange = 0;
  unsigned char setVCore = 0;
  unsigned int  setMultiplier = 0;

  halBoardGetSystemClockSettings( systemClockSpeed, &setDcoRange,  \
                                  &setVCore, &setMultiplier);
  	
  if (setVCore > (PMMCTL0 & PMMCOREV_3))	// Only change VCore if necessary
    halBoardSetVCore( setVCore );   
    
  UCSCTL0 = 0x00;                           // Set lowest possible DCOx, MODx
  UCSCTL1 = setDcoRange;                    // Select suitable range
  
  UCSCTL2 = setMultiplier + FLLD_1;         // Set DCO Multiplier
  UCSCTL4 = SELA__XT1CLK | SELS__DCOCLKDIV  |  SELM__DCOCLKDIV ;
  
  // Worst-case settling time for the DCO when the DCO range bits have been 
  // changed is n x 32 x 32 x f_FLL_reference. See UCS chapter in 5xx UG 
  // for optimization.
  // 32 x 32 x / f_FLL_reference (32,768 Hz) = .03125 = t_DCO_settle
  // t_DCO_settle / (1 / 18 MHz) = 562500 = counts_DCO_settle
  
  // __delay_cycles(562500);  
  int i;
  for (i=0;i<10;i++){
    __delay_cycles(56250);  
  }
}

/************************************************************************
 * @brief  Initializes all GPIO configurations. 
 *         TI example did set all ports to OUTPUT, we don't.
 * @param  none
 * 
 * @return none
 *************************************************************************/
void halBoardInit(void)
{  
  //Tie unused ports
  PAOUT  = 0;
  PADIR  = 0;  
  PASEL  = 0;
  PBOUT  = 0;  
  PBDIR  = 0;
  PBSEL  = 0;
  PCOUT  = 0;    
  PCDIR  = 0;
  PCSEL  = 0;  
  PDOUT  = 0;  
  PDDIR  = 0;
  PDSEL  = 0;  
  PJOUT  = 0;    
  PJDIR  = 0;
     
  // AUDIO_PORT_OUT = AUDIO_OUT_PWR_PIN ;
  // USB_PORT_DIR &= ~USB_PIN_RXD;             // USB RX Pin, Input with 
  //                                           // ...pulled down Resistor
  // USB_PORT_OUT &= ~USB_PIN_RXD;
  // USB_PORT_REN |= USB_PIN_RXD;
}
