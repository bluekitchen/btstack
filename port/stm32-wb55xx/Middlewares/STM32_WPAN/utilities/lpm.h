/**
  ******************************************************************************
  * @file    lpm.h
  * @author  MCD Application Team
  * @brief   Header for stm32xx_lpm.c module
  ******************************************************************************
   * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics. 
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the 
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LPM__H
#define __LPM__H

#ifdef __cplusplus
extern "C" {
#endif

  /* Includes ------------------------------------------------------------------*/
  /* Exported types ------------------------------------------------------------*/
  /**
   * MCU Off Mode Selection
   */
  typedef enum
  {
    LPM_OffMode_Dis,  /** Restart from where it stops on wakeup */
    LPM_OffMode_En    /** Restart from reset on wakeup */
  } LPM_OffModeSel_t;

  /**
   * System clock Selection
   */
  typedef enum
  {
    LPM_StopMode_Dis,   /** Sysclock is kept enabled when in low power mode */
    LPM_StopMode_En,    /** Sysclock is disabled when in low power mode */
  } LPM_StopModeSel_t;

  /**
   * Low Power Mode selected
   */
  typedef enum
  {
    LPM_SleepMode,
    LPM_StopMode,
    LPM_OffMode,
  } LPM_ModeSelected_t;

  /**
   * Stop Mode configuration
   * The values of enum shall be kept unchanged
   */
  typedef enum
  {
    LPM_StopMode0,
    LPM_StopMode1,
    LPM_StopMode2,
  } LPM_StopModeConf_t;

  /**
   * Off Mode configuration
   * The values of enum shall be kept unchanged
   */
  typedef enum
  {
    LPM_Standby,
    LPM_Shutdown,
  } LPM_OffModeConf_t;

  typedef struct
  {
    LPM_StopModeConf_t Stop_Mode_Config;
    LPM_OffModeConf_t OFF_Mode_Config;
  } LPM_Conf_t;

  /* Exported constants --------------------------------------------------------*/
  /* External variables --------------------------------------------------------*/
  /* Exported macros -----------------------------------------------------------*/
  /* Exported functions ------------------------------------------------------- */
  /**
   * @brief  This API returns the Low Power Mode selected that will be applied when the system will enter low power mode
   *         if there is no update between the time the mode is read with this API and the time the system enters
   *         low power mode.
   * @param  None
   * @retval LPM_ModeSelected_t
   */
  LPM_ModeSelected_t LPM_ReadModeSel(void);

  /**
   * @brief  This API notifies the low power manager if the specified user allows the Off mode or not.
   *         When the application does not require the system clock, it enters Stop Mode if at least one user disallow
   *         Off Mode. Otherwise, it enters Off Mode.
   *         The default mode selection for all users is Off mode enabled
   * @param  id: process Id - value shall be between 0 and 31
   * @param  mode: mode selected
   * @retval None
   */
  void LPM_SetOffMode(uint32_t id, LPM_OffModeSel_t mode);

  /**
   * @brief  This API notifies the low power manager if the specified user allows the System clock to be switched Off
   *         or not when entering low power mode. When the System clock is requested by at least one user, the system
   *         enters Sleep mode. Otherwise, it enters either Stop Mode or Off Mode.
   *         The default System clock request for all users is set to System clock switched Off
   * @param  id: process Id - value shall be between 0 and 31
   * @param  mode: System Clock Activty
   * @retval None
   */
  void LPM_SetStopMode(uint32_t id, LPM_StopModeSel_t mode);

  /**
   * @brief  This API shall be used by the application when there is no more code to execute so that the system may
   *         enter low-power mode. The mode selected depends on the information received from LPM_SetOffMode() and
   *         LPM_SetStopMode()
   *         This function shall be called in critical section
   * @param  None
   * @retval None
   */
  void LPM_EnterModeSelected(void);

  /**
   * @brief  This API is called by the low power manager in a critical section (PRIMASK bit set) to allow the
   *         application to implement dedicated code before entering Sleep Mode
   * @param  None
   * @retval None
   */
  void LPM_EnterSleepMode(void);

  /**
   * @brief  This API is called by the low power manager in a critical section (PRIMASK bit set) to allow the
   *         application to implement dedicated code before getting out from Sleep Mode
   * @param  None
   * @retval None
   */
  void LPM_ExitSleepMode(void);

  /**
   * @brief  This API is called by the low power manager in a critical section (PRIMASK bit set) to allow the
   *         application to implement dedicated code before entering Stop Mode
   * @param  None
   * @retval None
   */
  void LPM_EnterStopMode(void);

  /**
   * @brief  This API is called by the low power manager in a critical section (PRIMASK bit set) to allow the
   *         application to implement dedicated code before getting out from Stop Mode. This is where the application
   *         should reconfigure the clock tree when needed
   * @param  None
   * @retval None
   */
  void LPM_ExitStopMode(void);

  /**
   * @brief  This API is called by the low power manager in a critical section (PRIMASK bit set) to allow the
   *         application to implement dedicated code before entering Off mode. This is where the application could save
   *         data in the retention memory as the RAM memory content will be lost
   * @param  None
   * @retval None
   */
  void LPM_EnterOffMode(void);


  /**
   * @brief  This API is called by the low power manager in a critical section (PRIMASK bit set) to allow the
   *         application to implement dedicated code before getting out from Off mode. This can only happen when the
   *         Off mode is finally not entered. In that case, the application may reverse some configurations done before
   *         entering Off mode. When Off mode is successful, the system is reset when getting out from this low-power mode
   * @param  None
   * @retval None
   */
  void LPM_ExitOffMode(void);

  /**
   * @brief  This API shall be called by the application.
   *         It sets the configuration mode requested to be applied in Stop Mode and Off Mode.
   * @param  p_conf: configuration
   * @retval None
   */
  void LPM_SetConf(LPM_Conf_t *p_conf);



#ifdef __cplusplus
}
#endif

#endif /*__LPM__H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
