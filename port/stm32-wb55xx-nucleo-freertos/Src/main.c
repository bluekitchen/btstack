/*
 * Copyright (C) 2019 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/*
 *  Made for BlueKitchen by OneWave with <3
 *      Author: ftrefou@onewave.io
 */

#define BTSTACK_FILE__ "main.c"

#include <stdio.h>

#include "main.h"
#include "otp.h"
#include "app_conf.h"

#include "FreeRTOS.h"
#include "task.h"

UART_HandleTypeDef hTuart     = { 0 };
static RTC_HandleTypeDef hrtc = { 0 };

TaskHandle_t hbtstack_task;

static void SystemClock_Config(void);
static void PeriphClock_Config(void);
static void Tune_HSE( void );
static void Init_Exti( void );
static void Init_UART( void );
static void Init_RTC( void );


static void Error_Handler(void)
{
    for(;;);
}

static void Reset_BackupDomain( void )
{
    if ((LL_RCC_IsActiveFlag_PINRST() != 0) && (LL_RCC_IsActiveFlag_SFTRST() == 0))
    {
        HAL_PWR_EnableBkUpAccess(); /**< Enable access to the RTC registers */

        /**
         *  Write twice the value to flush the APB-AHB bridge
         *  This bit shall be written in the register before writing the next one
         */
        HAL_PWR_EnableBkUpAccess();

        __HAL_RCC_BACKUPRESET_FORCE();
        __HAL_RCC_BACKUPRESET_RELEASE();
    }

    return;
}


/**
  * @brief  The application entry point.
  * @retval int
  */
void port_thread(void* args);
int main(void)
{
	/* Reset of all peripherals, initializes the Systick. */
    HAL_Init();

    Reset_BackupDomain();
    
    Tune_HSE();

    SystemClock_Config();
    PeriphClock_Config();

    /* Init debug */
    Init_UART();

    Init_Exti();

    Init_RTC();

    xTaskCreate(port_thread, "btstack_thread", 2048, NULL, 1, &hbtstack_task);
    
    vTaskStartScheduler();

    /* We should never get here as control is now taken by the scheduler */
    for(;;);
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure LSE Drive Capability 
  */
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI1
                              |RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK4|RCC_CLOCKTYPE_HCLK2
                              |RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the peripherals clocks
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS|RCC_PERIPHCLK_RFWAKEUP
                              |RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_USART1
                              |RCC_PERIPHCLK_LPUART1;
  PeriphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInitStruct.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  PeriphClkInitStruct.RFWakeUpClockSelection = RCC_RFWKPCLKSOURCE_LSI;
  PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSE;
  PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE0;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripheral Clock Configuration
  * @retval None
  */
void PeriphClock_Config(void)
{
    /**
     * Select LSE clock
     * on wb series LSI is not enough accurate to maintain connection
     */
    LL_RCC_LSE_Enable();
    while(!LL_RCC_LSE_IsReady());

    /**
     * Select wakeup source of BLE RF
     */
    LL_RCC_SetRFWKPClockSource(LL_RCC_RFWKP_CLKSOURCE_LSE);

    /**
     * Switch OFF LSI
     */
    LL_RCC_LSI1_Disable();


    /**
     * Set RNG on HSI48
     */
    LL_RCC_HSI48_Enable();
    while(!LL_RCC_HSI48_IsReady());
    LL_RCC_SetCLK48ClockSource(LL_RCC_CLK48_CLKSOURCE_HSI48);

    return;
}

void Tune_HSE( void )
{
  OTP_ID0_t * p_otp;

  /**
    * Read HSE_Tuning from OTP
    */
  p_otp = (OTP_ID0_t *) OTP_Read(0);
  if (p_otp)
  {
    LL_RCC_HSE_SetCapacitorTuning(p_otp->hse_tuning);
  }
}

static void Init_UART( void )
{
    GPIO_InitTypeDef GPIO_InitStruct;

    /* Peripheral clock enable */
    DEBUG_USART_CLK_ENABLE();
    DEBUG_USART_PORT_CLK_ENABLE();

    /**USART GPIO Configuration */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = DEBUG_GPIO_AF;
    /* DEBUG_USART_TX */
    GPIO_InitStruct.Pin = DEBUG_USART_TX_Pin;
    HAL_GPIO_Init(DEBUG_USART_TX_GPIO_Port, &GPIO_InitStruct);
    /* DEBUG_USART_RX */
    GPIO_InitStruct.Pin = DEBUG_USART_RX_Pin;
    HAL_GPIO_Init(DEBUG_USART_RX_GPIO_Port, &GPIO_InitStruct);

    /* USART Configuration */
    hTuart.Instance = DEBUG_USART;
    hTuart.Init.BaudRate = 115200;
    hTuart.Init.WordLength = UART_WORDLENGTH_8B;
    hTuart.Init.StopBits = UART_STOPBITS_1;
    hTuart.Init.Parity = UART_PARITY_NONE;
    hTuart.Init.Mode = UART_MODE_TX_RX;
    hTuart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    hTuart.Init.OverSampling = UART_OVERSAMPLING_16;
    hTuart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    hTuart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&hTuart) != HAL_OK){
        Error_Handler();
    }
    return;
}

static void Init_Exti( void )
{
  /**< Disable all wakeup interrupt on CPU1  except IPCC(36), HSEM(38) */
  LL_EXTI_DisableIT_0_31(~0);
  LL_EXTI_DisableIT_32_63( (~0) & (~(LL_EXTI_LINE_36 | LL_EXTI_LINE_38)) );

  HAL_NVIC_SetPriority(IPCC_C1_RX_IRQn,5,0);
  HAL_NVIC_SetPriority(IPCC_C1_TX_IRQn,5,0);

  return;
}

static void Init_RTC( void )
{
  HAL_PWR_EnableBkUpAccess(); /**< Enable access to the RTC registers */

  /**
   *  Write twice the value to flush the APB-AHB bridge
   *  This bit shall be written in the register before writing the next one
   */
  HAL_PWR_EnableBkUpAccess();

  __HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSE); /**< Select LSI as RTC Input */

  __HAL_RCC_RTC_ENABLE(); /**< Enable RTC */

  hrtc.Instance = RTC; /**< Define instance */

  /**
   * Set the Asynchronous prescaler
   */
  hrtc.Init.AsynchPrediv = CFG_RTC_ASYNCH_PRESCALER;
  hrtc.Init.SynchPrediv = CFG_RTC_SYNCH_PRESCALER;
  HAL_RTC_Init(&hrtc);

  MODIFY_REG(RTC->CR, RTC_CR_WUCKSEL, CFG_RTC_WUCKSEL_DIVIDER);
  
    /* RTC interrupt Init */
    HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);

  return;
}

void PreSleepProcessing(uint32_t ulExpectedIdleTime)
{
  /* Called by the kernel before it places the MCU into a sleep mode because
  configPRE_SLEEP_PROCESSING() is #defined to PreSleepProcessing().

  NOTE:  Additional actions can be taken here to get the power consumption
  even lower.  For example, peripherals can be turned off here, and then back
  on again in the post sleep processing function.  For maximum power saving
  ensure all unused pins are in their lowest power state. */

  /* 
    (*ulExpectedIdleTime) is set to 0 to indicate that PreSleepProcessing contains
    its own wait for interrupt or wait for event instruction and so the kernel vPortSuppressTicksAndSleep 
    function does not need to execute the wfi instruction  
  */
  ulExpectedIdleTime = 0;
  
  /*Enter to sleep Mode using the HAL function HAL_PWR_EnterSLEEPMode with WFI instruction*/
  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);  
}


void PostSleepProcessing(uint32_t ulExpectedIdleTime)
{
  /* Called by the kernel when the MCU exits a sleep mode because
  configPOST_SLEEP_PROCESSING is #defined to PostSleepProcessing(). */

  /* Avoid compiler warnings about the unused parameter. */
  (void) ulExpectedIdleTime;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask,
        signed char *pcTaskName)
{
    printf("stack overflow in task %s!\r\n", pcTaskName);
    while (1);
}