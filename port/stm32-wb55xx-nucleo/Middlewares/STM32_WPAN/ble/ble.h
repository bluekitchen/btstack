/**
  ******************************************************************************
  * @file    ble.h
  * @author  MCD Application Team
  * @brief   BLE interface
  ******************************************************************************
  * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BLE_H
#define __BLE_H

#ifdef __cplusplus
extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#include "ble_conf.h"
#include "ble_dbg_conf.h"

/**< core */
#include "core/ble_core.h"
#include "core/ble_bufsize.h"
#include "core/ble_defs.h"
#include "core/ble_legacy.h"
#include "core/ble_std.h"

/**< blesvc */
#include "svc/Inc/bls.h"
#include "svc/Inc/crs_stm.h"
#include "svc/Inc/dis.h"
#include "svc/Inc/eds_stm.h"
#include "svc/Inc/hids.h"
#include "svc/Inc/hrs.h"
#include "svc/Inc/hts.h"
#include "svc/Inc/ias.h"
#include "svc/Inc/lls.h"
#include "svc/Inc/tps.h"
#include "svc/Inc/motenv_stm.h"
#include "svc/Inc/p2p_stm.h"
#include "svc/Inc/otas_stm.h"
#include "svc/Inc/mesh.h"  
#include "svc/Inc/template_stm.h"  
  
#include "svc/Inc/svc_ctl.h"

#include "svc/Inc/uuid.h"


/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /*__BLE_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
