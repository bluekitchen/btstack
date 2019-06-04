/**
 ******************************************************************************
 * @file    tl_thread_hci.c
 * @author  MCD Application Team
 * @brief   Function for managing HCI interface.
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
#include "stm32_wpan_common.h"
#include "hw.h"

#include "stm_list.h"

#include "tl.h"
#include "tl_thread_hci.h"


/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/



/* Private macros ------------------------------------------------------------*/

/* Public variables ---------------------------------------------------------*/



/* Private variables ---------------------------------------------------------*/


/* Private function prototypes -----------------------------------------------*/


/* Private functions ----------------------------------------------------------*/
__weak void Pre_OtCmdProcessing(void){return;}
__weak void Ot_Cmd_Transfer(void){return;}
__weak Thread_OT_Cmd_Request_t* THREAD_Get_OTCmdPayloadBuffer(void){return 0;}
__weak Thread_OT_Cmd_Request_t* THREAD_Get_OTCmdRspPayloadBuffer(void){return 0;}
__weak Thread_OT_Cmd_Request_t* THREAD_Get_NotificationPayloadBuffer(void){return 0;}


