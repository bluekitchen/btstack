/** 
  ******************************************************************************
  * @file    stm32f4_discovery.h
  * @author  MCD Application Team
  * @version V2.1.2
  * @date    27-January-2017
  * @brief   This file contains definitions for STM32F4-Discovery Kit's Leds and 
  *          push-button hardware resources.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2017 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************  
  */ 
  
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F4_DISCOVERY_H
#define __STM32F4_DISCOVERY_H

#ifdef __cplusplus
 extern "C" {
#endif
                                              
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
   
/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup STM32F4_DISCOVERY
  * @{
  */ 
      
/** @addtogroup STM32F4_DISCOVERY_LOW_LEVEL
  * @{
  */ 

/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_Exported_Types STM32F4 DISCOVERY LOW LEVEL_Exported_Types
  * @{
  */
typedef enum 
{
  LED4 = 0,
  LED3 = 1,
  LED5 = 2,
  LED6 = 3
} Led_TypeDef;

typedef enum 
{  
  BUTTON_KEY = 0,
} Button_TypeDef;

typedef enum 
{  
  BUTTON_MODE_GPIO = 0,
  BUTTON_MODE_EXTI = 1
} ButtonMode_TypeDef;     
/**
  * @}
  */ 

/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_Exported_Constants STM32F4 DISCOVERY LOW LEVEL Exported Constants
  * @{
  */ 

/** 
* @brief  Define for STM32F4_DISCOVERY board  
*/ 
#if !defined (USE_STM32F4_DISCO)
 #define USE_STM32F4_DISCO
#endif

/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_LED STM32F4 DISCOVERY LOW LEVEL LED
  * @{
  */
#define LEDn                             4

#define LED4_PIN                         GPIO_PIN_12
#define LED4_GPIO_PORT                   GPIOD
#define LED4_GPIO_CLK_ENABLE()           __HAL_RCC_GPIOD_CLK_ENABLE()  
#define LED4_GPIO_CLK_DISABLE()          __HAL_RCC_GPIOD_CLK_DISABLE()  

#define LED3_PIN                         GPIO_PIN_13
#define LED3_GPIO_PORT                   GPIOD
#define LED3_GPIO_CLK_ENABLE()           __HAL_RCC_GPIOD_CLK_ENABLE()  
#define LED3_GPIO_CLK_DISABLE()          __HAL_RCC_GPIOD_CLK_DISABLE()  
  
#define LED5_PIN                         GPIO_PIN_14
#define LED5_GPIO_PORT                   GPIOD
#define LED5_GPIO_CLK_ENABLE()           __HAL_RCC_GPIOD_CLK_ENABLE()  
#define LED5_GPIO_CLK_DISABLE()          __HAL_RCC_GPIOD_CLK_DISABLE()  

#define LED6_PIN                         GPIO_PIN_15
#define LED6_GPIO_PORT                   GPIOD
#define LED6_GPIO_CLK_ENABLE()           __HAL_RCC_GPIOD_CLK_ENABLE()  
#define LED6_GPIO_CLK_DISABLE()          __HAL_RCC_GPIOD_CLK_DISABLE()  

#define LEDx_GPIO_CLK_ENABLE(__INDEX__) do{if((__INDEX__) == 0) LED4_GPIO_CLK_ENABLE(); else \
                                           if((__INDEX__) == 1) LED3_GPIO_CLK_ENABLE(); else \
                                           if((__INDEX__) == 2) LED5_GPIO_CLK_ENABLE(); else \
                                           if((__INDEX__) == 3) LED6_GPIO_CLK_ENABLE(); \
                                           }while(0)

#define LEDx_GPIO_CLK_DISABLE(__INDEX__) do{if((__INDEX__) == 0) LED4_GPIO_CLK_DISABLE(); else \
                                            if((__INDEX__) == 1) LED3_GPIO_CLK_DISABLE(); else \
                                            if((__INDEX__) == 2) LED5_GPIO_CLK_DISABLE(); else \
                                            if((__INDEX__) == 3) LED6_GPIO_CLK_DISABLE(); \
                                            }while(0)
/**
  * @}
  */ 
  
/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_BUTTON STM32F4 DISCOVERY LOW LEVEL BUTTON
  * @{
  */  
#define BUTTONn                          1 

/**
 * @brief Wakeup push-button
 */
#define KEY_BUTTON_PIN                GPIO_PIN_0
#define KEY_BUTTON_GPIO_PORT          GPIOA
#define KEY_BUTTON_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()
#define KEY_BUTTON_GPIO_CLK_DISABLE() __HAL_RCC_GPIOA_CLK_DISABLE()
#define KEY_BUTTON_EXTI_IRQn          EXTI0_IRQn 

#define BUTTONx_GPIO_CLK_ENABLE(__INDEX__)    do{if((__INDEX__) == 0) KEY_BUTTON_GPIO_CLK_ENABLE(); \
                                                }while(0)

#define BUTTONx_GPIO_CLK_DISABLE(__INDEX__)    do{if((__INDEX__) == 0) KEY_BUTTON_GPIO_CLK_DISABLE(); \
                                                 }while(0)
/**
  * @}
  */ 
  
/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_BUS STM32F4 DISCOVERY LOW LEVEL BUS
  * @{
  */  

/*############################### SPI1 #######################################*/
#define DISCOVERY_SPIx                              SPI1
#define DISCOVERY_SPIx_CLK_ENABLE()                 __HAL_RCC_SPI1_CLK_ENABLE()
#define DISCOVERY_SPIx_GPIO_PORT                    GPIOA                      /* GPIOA */
#define DISCOVERY_SPIx_AF                           GPIO_AF5_SPI1
#define DISCOVERY_SPIx_GPIO_CLK_ENABLE()            __HAL_RCC_GPIOA_CLK_ENABLE()
#define DISCOVERY_SPIx_GPIO_CLK_DISABLE()           __HAL_RCC_GPIOA_CLK_DISABLE()
#define DISCOVERY_SPIx_SCK_PIN                      GPIO_PIN_5                 /* PA.05 */
#define DISCOVERY_SPIx_MISO_PIN                     GPIO_PIN_6                 /* PA.06 */
#define DISCOVERY_SPIx_MOSI_PIN                     GPIO_PIN_7                 /* PA.07 */

/* Maximum Timeout values for flags waiting loops. These timeouts are not based
   on accurate values, they just guarantee that the application will not remain
   stuck if the SPI communication is corrupted.
   You may modify these timeout values depending on CPU frequency and application
   conditions (interrupts routines ...). */   
#define SPIx_TIMEOUT_MAX                            0x1000 /*<! The value of the maximal timeout for BUS waiting loops */


/*############################# I2C1 #########################################*/
/* I2C clock speed configuration (in Hz) */
#ifndef BSP_I2C_SPEED
 #define BSP_I2C_SPEED                            100000
#endif /* BSP_I2C_SPEED */

/* I2C peripheral configuration defines (control interface of the audio codec) */
#define DISCOVERY_I2Cx                            I2C1
#define DISCOVERY_I2Cx_CLK_ENABLE()               __HAL_RCC_I2C1_CLK_ENABLE()
#define DISCOVERY_I2Cx_SCL_SDA_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOB_CLK_ENABLE()
#define DISCOVERY_I2Cx_SCL_SDA_AF                 GPIO_AF4_I2C1
#define DISCOVERY_I2Cx_SCL_SDA_GPIO_PORT          GPIOB
#define DISCOVERY_I2Cx_SCL_PIN                    GPIO_PIN_6
#define DISCOVERY_I2Cx_SDA_PIN                    GPIO_PIN_9

#define DISCOVERY_I2Cx_FORCE_RESET()              __HAL_RCC_I2C1_FORCE_RESET()
#define DISCOVERY_I2Cx_RELEASE_RESET()            __HAL_RCC_I2C1_RELEASE_RESET()

/* I2C interrupt requests */                  
#define DISCOVERY_I2Cx_EV_IRQn                    I2C1_EV_IRQn
#define DISCOVERY_I2Cx_ER_IRQn                    I2C1_ER_IRQn

/* Maximum Timeout values for flags waiting loops. These timeouts are not based
   on accurate values, they just guarantee that the application will not remain
   stuck if the SPI communication is corrupted.
   You may modify these timeout values depending on CPU frequency and application
   conditions (interrupts routines ...). */   
#define I2Cx_TIMEOUT_MAX    0x1000 /*<! The value of the maximal timeout for BUS waiting loops */


/*############################# ACCELEROMETER ################################*/
/* Read/Write command */
#define READWRITE_CMD                     ((uint8_t)0x80) 
/* Multiple byte read/write command */ 
#define MULTIPLEBYTE_CMD                  ((uint8_t)0x40)
/* Dummy Byte Send by the SPI Master device in order to generate the Clock to the Slave device */
#define DUMMY_BYTE                        ((uint8_t)0x00)

/* Chip Select macro definition */
#define ACCELERO_CS_LOW()       HAL_GPIO_WritePin(ACCELERO_CS_GPIO_PORT, ACCELERO_CS_PIN, GPIO_PIN_RESET)
#define ACCELERO_CS_HIGH()      HAL_GPIO_WritePin(ACCELERO_CS_GPIO_PORT, ACCELERO_CS_PIN, GPIO_PIN_SET)

/**
  * @brief  ACCELEROMETER Interface pins
  */
#define ACCELERO_CS_PIN                        GPIO_PIN_3                 /* PE.03 */
#define ACCELERO_CS_GPIO_PORT                  GPIOE                      /* GPIOE */
#define ACCELERO_CS_GPIO_CLK_ENABLE()          __HAL_RCC_GPIOE_CLK_ENABLE()
#define ACCELERO_CS_GPIO_CLK_DISABLE()         __HAL_RCC_GPIOE_CLK_DISABLE()
#define ACCELERO_INT_GPIO_PORT                 GPIOE                      /* GPIOE */
#define ACCELERO_INT_GPIO_CLK_ENABLE()         __HAL_RCC_GPIOE_CLK_ENABLE()
#define ACCELERO_INT_GPIO_CLK_DISABLE()        __HAL_RCC_GPIOE_CLK_DISABLE()
#define ACCELERO_INT1_PIN                      GPIO_PIN_0                 /* PE.00 */
#define ACCELERO_INT1_EXTI_IRQn                EXTI0_IRQn 
#define ACCELERO_INT2_PIN                      GPIO_PIN_1                 /* PE.01 */
#define ACCELERO_INT2_EXTI_IRQn                EXTI1_IRQn 
/**
  * @}
  */ 


/*############################### AUDIO ######################################*/
/**
  * @brief  AUDIO I2C Interface pins
  */
#define AUDIO_I2C_ADDRESS                     0x94
  
  /* Audio Reset Pin definition */
#define AUDIO_RESET_GPIO_CLK_ENABLE()         __HAL_RCC_GPIOD_CLK_ENABLE()
#define AUDIO_RESET_PIN                       GPIO_PIN_4
#define AUDIO_RESET_GPIO                      GPIOD
/**
  * @}
  */  

/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_Exported_Macros STM32F4 DISCOVERY LOW LEVEL Exported Macros
  * @{
  */  
/**
  * @}
  */ 

/** @defgroup STM32F4_DISCOVERY_LOW_LEVEL_Exported_Functions STM32F4 DISCOVERY LOW LEVEL Exported Functions
  * @{
  */
uint32_t BSP_GetVersion(void);
void     BSP_LED_Init(Led_TypeDef Led);
void     BSP_LED_On(Led_TypeDef Led);
void     BSP_LED_Off(Led_TypeDef Led);
void     BSP_LED_Toggle(Led_TypeDef Led);
void     BSP_PB_Init(Button_TypeDef Button, ButtonMode_TypeDef Mode);
uint32_t BSP_PB_GetState(Button_TypeDef Button);

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */
  
#ifdef __cplusplus
}
#endif

#endif /* __STM32F4_DISCOVERY_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
