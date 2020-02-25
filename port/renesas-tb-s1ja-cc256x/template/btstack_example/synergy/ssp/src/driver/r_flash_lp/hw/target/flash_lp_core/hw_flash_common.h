/***********************************************************************************************************************
 * Copyright [2015-2017] Renesas Electronics Corporation and/or its licensors. All Rights Reserved.
 * 
 * This file is part of Renesas SynergyTM Software Package (SSP)
 *
 * The contents of this file (the "contents") are proprietary and confidential to Renesas Electronics Corporation
 * and/or its licensors ("Renesas") and subject to statutory and contractual protections.
 *
 * This file is subject to a Renesas SSP license agreement. Unless otherwise agreed in an SSP license agreement with
 * Renesas: 1) you may not use, copy, modify, distribute, display, or perform the contents; 2) you may not use any name
 * or mark of Renesas for advertising or publicity purposes or in connection with your use of the contents; 3) RENESAS
 * MAKES NO WARRANTY OR REPRESENTATIONS ABOUT THE SUITABILITY OF THE CONTENTS FOR ANY PURPOSE; THE CONTENTS ARE PROVIDED
 * "AS IS" WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, AND NON-INFRINGEMENT; AND 4) RENESAS SHALL NOT BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, OR
 * CONSEQUENTIAL DAMAGES, INCLUDING DAMAGES RESULTING FROM LOSS OF USE, DATA, OR PROJECTS, WHETHER IN AN ACTION OF
 * CONTRACT OR TORT, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE CONTENTS. Third-party contents
 * included in this file may be subject to different terms.
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * File Name    : hw_flash_common.h
 * Description  : Common functions used by both the Code and Data Low Power Flash modules
 **********************************************************************************************************************/

#ifndef R_FLASH_COMMON_H_
#define R_FLASH_COMMON_H_

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/******************************************************************************
 * Includes <System Includes> , "Project Includes"
 ******************************************************************************/

/******************************************************************************
 * Typedef definitions
 ******************************************************************************/
/* Wait Process definition */
#define WAIT_TDIS     (3U)
#define WAIT_TMS_MID  (4U)
#define WAIT_TMS_HIGH (6U)
#define WAIT_TDSTOP   (6U)

#define MHZ           (1000000U)
#define KHZ           (1000U)

/* Flash information */

/* Used for DataFlash */
#define DATAFLASH_READ_BASE_ADDR  (0x40100000U)
#define DATAFLASH_WRITE_BASE_ADDR (0xFE000000U)
#define DATAFLASH_ADDR_OFFSET     (DATAFLASH_WRITE_BASE_ADDR - DATAFLASH_READ_BASE_ADDR)

void HW_FLASH_LP_delay_us (uint32_t us, uint32_t mhz) PLACE_IN_RAM_SECTION;

void HW_FLASH_LP_set_flash_settings (R_FACI_Type * p_faci_reg, current_parameters_t * p_current_parameters);

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_FLASH_COMMON_H_ */

/* End of File */
