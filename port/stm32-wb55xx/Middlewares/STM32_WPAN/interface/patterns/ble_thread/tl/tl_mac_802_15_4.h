/**
 ******************************************************************************
 * @file    tl_mac_802_15_4.h
 * @author  MCD Application Team
 * @brief   Constants and functions for managing MAC 802.15.4 TL
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


#ifndef __TL_MAC_802_15_4_H
#define __TL_MAC_802_15_4_H

/* Includes ------------------------------------------------------------------*/

#include "tl.h"
#include <string.h>

#include "802_15_4_mac_core.h"

/* Exported functions  ------------------------------------------------------------*/
void Mac_802_15_4_PreCmdProcessing(void);
void Mac_802_15_4_CmdTransfer(void);

TL_CmdPacket_t* MAC_802_15_4_GetCmdBuffer(void);
TL_Evt_t* MAC_802_15_4_GetRspPayEvt(void);

TL_Evt_t* MAC_802_15_4_GetNotificationBuffer(void);
MAC_802_15_4_Notification_t* MAC_802_15_4_GetNotificationPayloadBuffer(void);

/* Exported defines -----------------------------------------------------------*/


#endif /* __TL_MAC_802_15_4_H_ */
