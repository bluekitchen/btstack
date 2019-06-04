/*
 * stm32wbxx IT
 * System interruptions implementations
 */

#include "stm32wbxx.h"
#include "stm32wbxx_hal.h"

#include "stm32wbxx_hal_tim.h"

#include "hw.h"

extern TIM_HandleTypeDef htim17;

/**
* @brief This function handles Non maskable interrupt.
*/
void NMI_Handler                (void)
{

}

/**
* @brief This function handles Hard fault interrupt.
*/
void HardFault_Handler          (void)
{
  for(;;);
}

/**
* @brief This function handles Memory management fault.
*/
void MemManage_Handler          (void)
{
  while (1)
  {

  }
}

/**
* @brief This function handles Prefetch fault, memory access fault.
*/
void BusFault_Handler           (void)
{
  while (1)
  {

  }
}

/**
* @brief This function handles Undefined instruction or illegal state.
*/
void UsageFault_Handler         (void)
{
  while (1)
  {

  }
}

/**
* @brief This function handles Debug monitor.
*/
void DebugMon_Handler           (void)
{
}

/************************************************************************************
 *                  STM32L4xx Peripheral Interrupt Handlers
 ************************************************************************************/
//void WWDG_IRQHandler                   (void){for(;;);} //Not used
//void PVD_PVM_IRQHandler                (void){for(;;);} //Not used
void TAMP_STAMP_LSECSS_IRQHandler      (void){for(;;);} //Not used
void RTC_WKUP_IRQHandler               (void){for(;;);} //Not used
void FLASH_IRQHandler                  (void){for(;;);} //Not used
void RCC_IRQHandler                    (void){for(;;);} //Not used
void EXTI0_IRQHandler                  (void){for(;;);} //Not used
void EXTI1_IRQHandler                  (void){for(;;);} //Not used
void EXTI2_IRQHandler                  (void){for(;;);} //Not used
void EXTI3_IRQHandler                  (void){for(;;);} //Not used
void EXTI4_IRQHandler                  (void){for(;;);} //Not used
void DMA1_Channel1_IRQHandler          (void){for(;;);} //Not used
void DMA1_Channel2_IRQHandler          (void){for(;;);} //Not used
void DMA1_Channel3_IRQHandler          (void){for(;;);} //Not used
void DMA1_Channel4_IRQHandler          (void){for(;;);} //Not used
void DMA1_Channel5_IRQHandler          (void){for(;;);} //Not used
void DMA1_Channel6_IRQHandler          (void){for(;;);} //Not used
void DMA1_Channel7_IRQHandler          (void){for(;;);} //Not used
void ADC1_IRQHandler                   (void){for(;;);} //Not used
void USB_HP_IRQHandler                 (void){for(;;);} //Not used
void USB_LP_IRQHandler                 (void){for(;;);} //Not used
void C2SEV_PWR_C2H_IRQHandler          (void){for(;;);} //Not used
void COMP_IRQHandler                   (void){for(;;);} //Not used
void EXTI9_5_IRQHandler                (void){for(;;);} //Not used
void TIM1_BRK_IRQHandler               (void){for(;;);} //Not used
void TIM1_UP_TIM16_IRQHandler          (void){for(;;);} //Not used
//void TIM1_TRG_COM_TIM17_IRQHandler     (void){for(;;);} //Not used
void TIM1_CC_IRQHandler                (void){for(;;);} //Not used
void TIM2_IRQHandler                   (void){for(;;);} //Not used
void PKA_IRQHandler                    (void){for(;;);} //Not used
void I2C1_EV_IRQHandler                (void){for(;;);} //Not used
void I2C1_ER_IRQHandler                (void){for(;;);} //Not used
void I2C3_EV_IRQHandler                (void){for(;;);} //Not used
void I2C3_ER_IRQHandler                (void){for(;;);} //Not used
void SPI1_IRQHandler                   (void){for(;;);} //Not used
void SPI2_IRQHandler                   (void){for(;;);} //Not used
void USART1_IRQHandler                 (void){for(;;);} //Not used
void LPUART1_IRQHandler                (void){for(;;);} //Not used
void SAI1_IRQHandler                   (void){for(;;);} //Not used
void TSC_IRQHandler                    (void){for(;;);} //Not used
void EXTI15_10_IRQHandler              (void){for(;;);} //Not used
void RTC_Alarm_IRQHandler              (void){for(;;);} //Not used
void CRS_IRQHandler                    (void){for(;;);} //Not used
void PWR_SOTF_BLEACT_802ACT_RFPHASE_IRQHandler (void){for(;;);} //Not used
//void IPCC_C1_RX_IRQHandler             (void){for(;;);} //Not used
//void IPCC_C1_TX_IRQHandler             (void){for(;;);} //Not used
void HSEM_IRQHandler                   (void){for(;;);} //Not used
void LPTIM1_IRQHandler                 (void){for(;;);} //Not used
void LPTIM2_IRQHandler                 (void){for(;;);} //Not used
void LCD_IRQHandler                    (void){for(;;);} //Not used
void QUADSPI_IRQHandler                (void){for(;;);} //Not used
void AES1_IRQHandler                   (void){for(;;);} //Not used
void AES2_IRQHandler                   (void){for(;;);} //Not used
void RNG_IRQHandler                    (void){for(;;);} //Not used
void FPU_IRQHandler                    (void){for(;;);} //Not used
void DMA2_Channel1_IRQHandler          (void){for(;;);} //Not used
void DMA2_Channel2_IRQHandler          (void){for(;;);} //Not used
void DMA2_Channel3_IRQHandler          (void){for(;;);} //Not used
void DMA2_Channel4_IRQHandler          (void){for(;;);} //Not used
void DMA2_Channel5_IRQHandler          (void){for(;;);} //Not used
void DMA2_Channel6_IRQHandler          (void){for(;;);} //Not used
void DMA2_Channel7_IRQHandler          (void){for(;;);} //Not used
void DMAMUX1_OVR_IRQHandler            (void){for(;;);} //Not used

/**
* @brief This function handles TIM1 update interrupt and TIM16 global interrupt.
*/
void TIM1_TRG_COM_TIM17_IRQHandler   (void)
{
  HAL_TIM_IRQHandler(&htim17);
}

void IPCC_C1_TX_IRQHandler(void)
{
  HW_IPCC_Tx_Handler();

  return;
}

void IPCC_C1_RX_IRQHandler(void)
{
  HW_IPCC_Rx_Handler();
  return;
}

/************************************************************************************
 *        STM32L4xx Callbacks due to HAL redispatching (dirty code)
 ************************************************************************************/

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM17) {
        HAL_IncTick();
    }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
