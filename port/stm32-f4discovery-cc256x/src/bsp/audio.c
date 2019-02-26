#include "stm32f4_discovery_audio.h"

// support for I2S/I2C CS43L22 codec is provided by bsp/stm32f4_discovery_audio.c and not by STM32CubeMX
// we need to provide the missing IRQ handler somewhere

extern I2S_HandleTypeDef  hAudioOutI2s;
extern I2S_HandleTypeDef  hAudioInI2s;

/**
  * @brief  This function handles main I2S interrupt.
  * @param  None
  * @retval 0 if correct communication, else wrong communication
  */
void DMA1_Stream7_IRQHandler(void)
{
  HAL_DMA_IRQHandler(hAudioOutI2s.hdmatx);
}

/**
  * @brief  This function handles main I2S interrupt.
  * @param  None
  * @retval 0 if correct communication, else wrong communication
  */
void DMA1_Stream3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(hAudioInI2s.hdmarx);
}
