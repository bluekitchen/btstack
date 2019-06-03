/**
 ******************************************************************************
 * @file    scheduler.h
 * @author  MCD Application Team
 * @brief   scheduler interface
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
#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/**
 * This should be used in the application
 * while(1)
 * {
 *    SCH_Run(SCH_DEFAULT);
 * }
 *
 * This informs the scheduler that all tasks registered shall be considered
 *
 */
#define SCH_DEFAULT         (~0)
/* Exported functions ------------------------------------------------------- */

/**
 * @brief This is called by the scheduler in critical section (PRIMASK bit) when
 *          - there is no more tasks to be executed
 *          AND
 *          - there is no pending event or the pending event is still not set
 *        The application should enter low power mode in this function
 *        When this function is not implemented by the application, the scheduler keeps running a while loop (RUN MODE)
 *
 * @param  None
 * @retval None
 */
void SCH_Idle( void );

/**
 * @brief  This requests the scheduler to execute all pending tasks using round robin mechanism.
 *         When no task are pending, it calls SCH_Idle();
 *         This function should be called in a while loop in the application
 *
 * @param  mask_bm: this is the list of task (bit mapping) that is be kept in the scheduler list
 * @retval None
 */
void SCH_Run( uint32_t mask_bm );

/**
 * @brief This registers a task in the scheduler.
 *
 * @param task_id: id of the task (It shall be any number from 0 to 31)
 * @param task: Reference of the function to be executed
 *
 * @retval None
 */
void SCH_RegTask( uint32_t task_id , void (*task)( void ) );

/**
 * @brief  Request a task to be executed
 *
 * @param  task_id_bm: The Id of the task
 *         It shall be (1<<task_id) where task_id is the number assigned when the task has been registered
 * @param  task_prio: The priority of the task
 *         It shall an number from  0 (high priority) to 31 (low priority)
 *         The priority is checked each time the scheduler needs to select a new task to execute
 *         It does not permit to preempt a running task with lower priority
 * @retval None
 */
void SCH_SetTask( uint32_t task_id_bm , uint32_t task_prio );

/**
 * @brief Prevents a task to be called by the scheduler even when set with SCH_SetTask()
 *        By default, all tasks are executed by the scheduler when set with SCH_SetTask()
 *        When a task is paused, it is moved out from the scheduler list
 *
 * @param  task_id_bm: The Id of the task
 *         It shall be (1<<task_id) where task_id is the number assigned when the task has been registered
 * @retval None
 */
void SCH_PauseTask( uint32_t task_id_bm );

/**
 * @brief Allows a task to be called by the scheduler if set with SCH_SetTask()
 *        By default, all tasks are executed by the scheduler when set with SCH_SetTask()
 *        This is used in relation with SCH_PauseTask() to get back in the scheduler list a task that has been
 *        moved out
 *
 * @param  task_id_bm: The Id of the task
 *         It shall be (1<<task_id) where task_id is the number assigned when the task has been registered
 * @retval None
 */
void SCH_ResumeTask( uint32_t task_id_bm );

/**
 * @brief It sets an event that is waited with SCH_WaitEvt()
 *
 * @param evt_id_bm
 *        It shall be a bit mapping where only 1 bit is set
 * @retval None
 */
void SCH_SetEvt( uint32_t evt_id_bm );

/**
 * @brief This API may be used to clear the event before calling SCH_WaitEvt()
 *        This API may be useful when the SCH_SetEvt() is called several time to notify the same event.
 *        Due to Software Architecture where the timings are hard to control, this may be an unwanted case.
 *
 * @param evt_id_bm
 *        It shall be a bit mapping where only 1 bit is set
 * @retval None
 */
void SCH_ClrEvt( uint32_t evt_id_bm );

/**
 * @brief It waits for a specific event to be set. The scheduler loops SCH_EvtIdle() until the event is set
 *        When called recursively, it acts as a First in / Last out mechanism. The scheduler waits for the
 *        last event requested to be set even though one of the already requested event has been set.
 *
 * @param evt_id_bm
 *        It shall be a bit mapping where only 1 bit is set
 * @retval None
 */
void SCH_WaitEvt( uint32_t evt_id_bm );

/**
 * @brief This API returns whether the waited event is pending or not
 *        It is useful only when the SCH_EvtIdle() is overloaded by the application. In that case, when the low
 *        power mode needs to be executed, the application shall first check whether the waited event is pending
 *        or not. Both the event checking and the low power mode processing should be done in critical section
 *
 * @param   None
 * @retval  0 when the waited event is not there or the evt_id when the waited event is pending
 */
uint32_t SCH_IsEvtPend( void );

/**
 * @brief The scheduler loops in that function until the waited event is set
 *        The application may either enter low power mode or call SCH_Run()
 *        When not implemented by the application, it calls SCH_Run(0) which means all tasks are removed from
 *        scheduler list and only SCH_Idle() is called. In that case, only low power mode is executed.
 *
 * @param  evt_waited: The event id that is waited.
 * @retval None
 */
void SCH_EvtIdle( uint32_t evt_waited_bm );

#ifdef __cplusplus
}
#endif

#endif /*__SCHEDULER_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
