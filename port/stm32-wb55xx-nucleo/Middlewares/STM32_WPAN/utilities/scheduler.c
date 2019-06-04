/**
 ******************************************************************************
 * @file    scheduler.c
 * @author  MCD Application Team
 * @brief   Simple scheduler implementation
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


/* Includes ------------------------------------------------------------------*/
#include "utilities_common.h"

#include "scheduler.h"

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
uint32_t priority;
uint32_t round_robin;
} SCH_Priority_t;

/* Private defines -----------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/

#if( __CORTEX_M != 0 )
#define COUNT_LEAD_ZERO   __CLZ
#else
#define COUNT_LEAD_ZERO   CountLeadZero
#endif

/* Private variables ---------------------------------------------------------*/
#if( __CORTEX_M == 0 )
static const uint8_t clz_table_4bit[16] =
{ 4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
#endif
static uint32_t TaskSet = 0;
static uint32_t TaskMask = (~0);
static uint32_t SuperMask = (~0);
static uint32_t EvtSet = 0;
static uint32_t EvtWaited = 0;
static void (*TaskCb[SCH_CONF_TASK_NBR])( void );
static SCH_Priority_t TaskPrio[SCH_CONF_PRIO_NBR] = { 0 };

/* Global variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
#if( __CORTEX_M == 0)
static uint32_t CountLeadZero(uint32_t value);
#endif

/* Functions Definition ------------------------------------------------------*/

/**
 * This function can be nested.
 * That is the reason why many variables that are used only in that function are declared static.
 * Note: These variables could have been declared static in the function.
 */
void SCH_Run( uint32_t mask_bm )
{
  uint32_t bit_nbr;
  uint32_t counter;
  uint32_t current_task_set;
  uint32_t super_mask_backup;

  BACKUP_PRIMASK();

  /**
   *  When this function is nested, the mask to be applied cannot be larger than the first call
   *  The mask is always getting smaller and smaller
   *  A copy is made of the mask set by SCH_Run() in case it is called again in the task
   */
  super_mask_backup = SuperMask;
  SuperMask &= mask_bm;

  /**
   * There are two independent mask to check:
   * TaskMask that comes from SCH_PauseTask() / SCH_ResumeTask
   * SuperMask that comes from SCH_Run
   */
  while(TaskSet & TaskMask & SuperMask)
  {
    counter = 0;
    /**
     * When a flag is set, the associated bit is set in TaskPrio[counter].priority mask depending
     * on the priority parameter given from SCH_SetTask()
     * The while loop is looking for a flag set from the highest priority maskr to the lower
     */
    while(!(TaskPrio[counter].priority & TaskMask & SuperMask))
    {
      counter++;
    }

    current_task_set = TaskPrio[counter].priority & TaskMask & SuperMask;

    /**
     * The round_robin register is a mask of allowed flags to be evaluated.
     * The concept is to make sure that on each round on SCH_Run(), if two same flags are always set,
     * the scheduler does not run always only the first one.
     * When a task has been executed, The flag is removed from the round_robin mask.
     * If on the next SCH_RUN(), the two same flags are set again, the round_robin mask will mask out the first flag
     * so that the second one can be executed.
     * Note that the first flag is not removed from the list of pending task but just masked by the round_robin mask
     *
     * In the check below, the round_robin mask is reinitialize in case all pending tasks haven been executed at least once
     */
    if (!(TaskPrio[counter].round_robin & current_task_set))
    {
      TaskPrio[counter].round_robin = (~0);
    }

    /** read the flag index of the task to be executed */
    bit_nbr = COUNT_LEAD_ZERO(current_task_set & TaskPrio[counter].round_robin);

    /** remove from the roun_robin mask the task that has been selected to be executed */
    TaskPrio[counter].round_robin &= ~(1 << (31 - bit_nbr));

    DISABLE_IRQ();
    /** remove from the list or pending task the one that has been selected to be executed */
    TaskSet &= ~(1 << (31 - bit_nbr));
    /** remove from all priority mask the task that has been selected to be executed */
    for (counter = SCH_CONF_PRIO_NBR; counter; counter--)
    {
      TaskPrio[counter - 1].priority &= ~(1 << (31 - bit_nbr));
    }
    RESTORE_PRIMASK();
    /** Execute the task */
    TaskCb[31 - bit_nbr]();
  }

  DISABLE_IRQ();
  if (!((TaskSet & TaskMask & SuperMask) || (EvtSet & EvtWaited)))
  {
    SCH_Idle();
  }
  RESTORE_PRIMASK();

  /** restore the mask from SCH_Run() */
  SuperMask = super_mask_backup;

  return;
}

/**
 *  this function can be nested
 */
void SCH_RegTask( uint32_t task_id , void (*task)( void ) )
{
  BACKUP_PRIMASK();

  DISABLE_IRQ();

  TaskCb[task_id] = task;

  RESTORE_PRIMASK();

  return;
}

/**
 *  this function can be nested
 */
void SCH_SetTask( uint32_t task_id_bm , uint32_t task_prio )
{
  BACKUP_PRIMASK();

  DISABLE_IRQ();

  TaskSet |= task_id_bm;
  TaskPrio[task_prio].priority |= task_id_bm;

  RESTORE_PRIMASK();

  return;
}

/**
 *  this function can be nested
 */
void SCH_PauseTask( uint32_t task_id_bm )
{
  BACKUP_PRIMASK();

  DISABLE_IRQ();

  TaskMask &= (~task_id_bm);

  RESTORE_PRIMASK();

  return;
}

/**
 *  this function can be nested
 */
void SCH_ResumeTask( uint32_t task_id_bm )
{
  BACKUP_PRIMASK();

  DISABLE_IRQ();

  TaskMask |= task_id_bm;

  RESTORE_PRIMASK();

  return;
}

/**
 *  this function can be nested
 */
void SCH_SetEvt( uint32_t evt_id_bm )
{
  BACKUP_PRIMASK();

  DISABLE_IRQ();

  EvtSet |= evt_id_bm;

  RESTORE_PRIMASK();

  return;
}

/**
 *  this function can be nested
 */
void SCH_ClrEvt( uint32_t evt_id_bm )
{
  BACKUP_PRIMASK();

  DISABLE_IRQ();

  EvtSet &= (~evt_id_bm);

  RESTORE_PRIMASK();

  return;
}

/**
 *  this function can be nested
 */
void SCH_WaitEvt( uint32_t evt_id_bm )
{
  uint32_t event_waited_id_backup;

  /** backup the event id that was currently waited */
  event_waited_id_backup = EvtWaited;
  EvtWaited = evt_id_bm;
  /**
   *  wait for the new event
   *  note: that means that if the previous waited event occurs, it will not exit
   *  the while loop below.
   *  The system is waiting only for the last waited event.
   *  When it will go out, it will wait again fro the previous one.
   *  It case it occurs while waiting for the second one, the while loop will exit immediately
   */
  while((EvtSet & EvtWaited) == 0)
  {
    SCH_EvtIdle(EvtWaited);
  }
  EvtSet &= (~EvtWaited);
  EvtWaited = event_waited_id_backup;

  return;
}

/**
 *  this function can be nested
 */
uint32_t SCH_IsEvtPend( void )
{
  return (EvtSet & EvtWaited);
}

__weak void SCH_EvtIdle( uint32_t evt_waited_bm )
{
  /**
   * Execute scheduler if not implemented by the application
   * By default, all tasks are moved out of the scheduler
   * Only SCH_Idle() will be called in critical section
   */
  SCH_Run(0);

  return;
}

__weak void SCH_Idle( void )
{
  /**
   * Stay in run mode if not implemented by the application
   */
  return;
}

#if( __CORTEX_M == 0)
static uint32_t CountLeadZero(uint32_t value)
{
  uint32_t n;

  if ((value & 0xFFFF0000) == 0)
  {
    n = 16;
    value <<= 16;
  }
  else
  {
    n = 0;
  }

  if ((value & 0xFF000000) == 0)
  {
    n += 8;
    value <<= 8;
  }

  if ((value & 0xF0000000) == 0)
  {
    n += 4;
    value <<= 4;
  }

  n += (uint32_t)clz_table_4bit[value >> (32-4)];

  return n;
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
