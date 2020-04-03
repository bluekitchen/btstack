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
/***********************************************************************************************************************
* File Name    : bsp_rom_registers.c
* Description  : Defines MCU registers that are in ROM (e.g. OFS) and must be set at compile-time. All registers
*                can be set using bsp_cfg.h.
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @ingroup BSP_MCU_S1JA
 * @defgroup BSP_MCU_ROM_REGISTER_S1JA ROM Registers
 *
 * Defines MCU registers that are in ROM (e.g. OFS) and must be set at compile-time. All registers can be set
 * using bsp_cfg.h.
 *
 * @{
 **********************************************************************************************************************/



/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "bsp_api.h"

#if defined(BSP_MCU_GROUP_S1JA)

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
/** OR in the HOCO frequency setting from bsp_clock_cfg.h with the OFS1 setting from bsp_cfg.h. */
#define BSP_ROM_REG_OFS1_SETTING        (((uint32_t)BSP_CFG_ROM_REG_OFS1 & 0xFFFF8FFFU) | ((uint32_t)BSP_CFG_HOCO_FREQUENCY << 12))

/** Build up SECMPUAC register based on MPU settings. */
#define BSP_ROM_REG_MPU_CONTROL_SETTING     ((0xFFFFFCF0U) | \
                                             ((uint32_t)BSP_CFG_ROM_REG_MPU_PC0_ENABLE << 8) | \
                                             ((uint32_t)BSP_CFG_ROM_REG_MPU_PC1_ENABLE << 9) | \
                                             ((uint32_t)BSP_CFG_ROM_REG_MPU_REGION0_ENABLE) | \
                                             ((uint32_t)BSP_CFG_ROM_REG_MPU_REGION1_ENABLE << 1) | \
                                             ((uint32_t)BSP_CFG_ROM_REG_MPU_REGION2_ENABLE << 2) | \
                                             ((uint32_t)BSP_CFG_ROM_REG_MPU_REGION3_ENABLE << 3))

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables (to be accessed by other files)
***********************************************************************************************************************/
 
/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/
/** ROM registers defined here. Some have masks to make sure reserved bits are set appropriately. */
/*LDRA_INSPECTED 57 D - This global is being initialized at it's declaration below. */
BSP_DONT_REMOVE static const uint32_t g_bsp_rom_registers[] BSP_PLACE_IN_SECTION_V2(BSP_SECTION_ROM_REGISTERS) =
{
    (uint32_t)BSP_CFG_ROM_REG_OFS0,
    (uint32_t)BSP_ROM_REG_OFS1_SETTING,
    ((uint32_t)BSP_CFG_ROM_REG_MPU_PC0_START      & 0xFFFFFFFCU),
    ((uint32_t)BSP_CFG_ROM_REG_MPU_PC0_END        | 0x00000003U),
    ((uint32_t)BSP_CFG_ROM_REG_MPU_PC1_START      & 0xFFFFFFFCU),
    ((uint32_t)BSP_CFG_ROM_REG_MPU_PC1_END        | 0x00000003U),
    ((uint32_t)BSP_CFG_ROM_REG_MPU_REGION0_START  & 0x000FFFFCU),
    (((uint32_t)BSP_CFG_ROM_REG_MPU_REGION0_END   & 0x000FFFFFU) | 0x00000003U),
    ((uint32_t)BSP_CFG_ROM_REG_MPU_REGION1_START  & 0xFFFFFFFCU),
    ((uint32_t)BSP_CFG_ROM_REG_MPU_REGION1_END    | 0x00000003U),
    (((uint32_t)BSP_CFG_ROM_REG_MPU_REGION2_START & 0x407FFFFCU) | 0x40000000U),
    (((uint32_t)BSP_CFG_ROM_REG_MPU_REGION2_END   & 0x407FFFFCU) | 0x40000003U),
    (((uint32_t)BSP_CFG_ROM_REG_MPU_REGION3_START & 0x407FFFFCU) | 0x40000000U),
    (((uint32_t)BSP_CFG_ROM_REG_MPU_REGION3_END   & 0x407FFFFCU) | 0x40000003U),
    (uint32_t)BSP_ROM_REG_MPU_CONTROL_SETTING
};

/** ID code definitions defined here. */
/*LDRA_INSPECTED 57 D - This global is being initialized at it's declaration below. */
BSP_DONT_REMOVE static const uint32_t g_bsp_id_code_1[] BSP_PLACE_IN_SECTION_V2(BSP_SECTION_ID_CODE_1) =
{
     BSP_CFG_ID_CODE_LONG_1         /* 0x01010018 */
};

/*LDRA_INSPECTED 57 D - This global is being initialized at it's declaration below. */
BSP_DONT_REMOVE static const uint32_t g_bsp_id_code_2[] BSP_PLACE_IN_SECTION_V2(BSP_SECTION_ID_CODE_2) =
{
     BSP_CFG_ID_CODE_LONG_2         /* 0x01010020 */
};

/*LDRA_INSPECTED 57 D - This global is being initialized at it's declaration below. */
BSP_DONT_REMOVE static const uint32_t g_bsp_id_code_3[] BSP_PLACE_IN_SECTION_V2(BSP_SECTION_ID_CODE_3) =
{
     BSP_CFG_ID_CODE_LONG_3         /* 0x01010028 */
};

/*LDRA_INSPECTED 57 D - This global is being initialized at it's declaration below. */
BSP_DONT_REMOVE static const uint32_t g_bsp_id_code_4[] BSP_PLACE_IN_SECTION_V2(BSP_SECTION_ID_CODE_4) =
{
     BSP_CFG_ID_CODE_LONG_4         /* 0x01010030 */
};

#endif


/** @} (end defgroup BSP_MCU_ROM_REGISTER_S1JA) */
