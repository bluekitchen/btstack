/**
 ******************************************************************************
  * File Name          : hw_lpm.c
  * Description        : Hardware Low Power Mode source file for BLE 
  *                      middleWare.
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2019 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "app_conf.h"
#include "hw_conf.h"

/* Private const -----------------------------------------------------------*/
const uint32_t HW_LPM_STOP_MODE[3] = {LL_PWR_MODE_STOP0, LL_PWR_MODE_STOP1, LL_PWR_MODE_STOP2};
const uint32_t HW_LPM_OFF_MODE[2] = {LL_PWR_MODE_STANDBY, LL_PWR_MODE_SHUTDOWN};

void HW_LPM_SleepMode(void)
{
  LL_LPM_EnableSleep(); /**< Clear SLEEPDEEP bit of Cortex System Control Register */

  /**
   * This option is used to ensure that store operations are completed
   */
#if defined ( __CC_ARM)
  __force_stores();
#endif

  __WFI();

  return;
}

void HW_LPM_StopMode(HW_LPM_StopModeConf_t configuration)
{
  LL_PWR_SetPowerMode(HW_LPM_STOP_MODE[configuration]);

  LL_LPM_EnableDeepSleep(); /**< Set SLEEPDEEP bit of Cortex System Control Register */

  /**
   * This option is used to ensure that store operations are completed
   */
#if defined ( __CC_ARM)
  __force_stores();
#endif

  __WFI();

  return;
}

void HW_LPM_OffMode(HW_LPM_OffModeConf_t configuration)
{
    /*
     * There is no risk to clear all the WUF here because in the current implementation, this API is called
     * in critical section. If an interrupt occurs while in that critical section before that point,
     * the flag is set and will be cleared here but the system will not enter Off Mode
     * because an interrupt is pending in the NVIC. The ISR will be executed when moving out
     * of this critical section
     */
    LL_PWR_ClearFlag_WU();

    LL_PWR_SetPowerMode(HW_LPM_OFF_MODE[configuration]);

    LL_LPM_EnableDeepSleep(); /**< Set SLEEPDEEP bit of Cortex System Control Register */

    /**
     * This option is used to ensure that store operations are completed
     */
#if defined ( __CC_ARM)
    __force_stores();
#endif

    __WFI();

  return;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
