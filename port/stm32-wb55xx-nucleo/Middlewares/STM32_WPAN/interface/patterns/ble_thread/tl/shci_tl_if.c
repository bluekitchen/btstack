/**
 ******************************************************************************
 * @file    shci_tl_if.c
 * @author  MCD Application Team
 * @brief   Transport layer interface to the system channel
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
#include "shci_tl.h"
#include "tl.h"


void shci_register_io_bus(tSHciIO* fops)
{
  /* Register IO bus services */
  fops->Init    = TL_SYS_Init;
  fops->Send    = TL_SYS_SendCmd;

  return;
}

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
