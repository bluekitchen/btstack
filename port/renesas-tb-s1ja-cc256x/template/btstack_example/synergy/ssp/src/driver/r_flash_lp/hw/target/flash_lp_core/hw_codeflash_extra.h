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
 * File Name    : hw_codeflash_extra.h
 * Description  : Flash Control Access window and swap control processing for Low Power Flash.
 **********************************************************************************************************************/

#ifndef R_CODEFLASH_EXTRA_H
#define R_CODEFLASH_EXTRA_H

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/******************************************************************************
 * Macro definitions
 ******************************************************************************/
/*  operation definition (FEXCR Register setting)*/
#define FEXCR_STARTUP      (0x81U)
#define FEXCR_AW           (0x82U)
#define FEXCR_OCDID1       (0x83U)
#define FEXCR_OCDID2       (0x84U)
#define FEXCR_OCDID3       (0x85U)
#define FEXCR_OCDID4       (0x86U)
#define FEXCR_CLEAR        (0x00U)

#define DUMMY              (0xFFFFFFFFU)
#define DEFAULT_AREA       (1U)
#define DEFAULT_AREA_VALUE (0xFFFFU)
#define STARTUP_AREA_VALUE (0xFEFFU)

#define SAS_DEFAULT_AREA   0x02U                 ///< Bit value for FSUAC register specifying DEFAULT area
#define SAS_ALTERNATE_AREA 0x03U                 ///< Bit value for FSUAC register specifying ALTERNATE area
#define SAS_PER_BTFLG_AREA 0x00U                 ///< Bit value for FSUAC register specifying use BTFLG area

/******************************************************************************
 * Exported global functions (to be accessed by other files)
 ******************************************************************************/

/**   FLASH operation command values */
typedef enum e_flash_command
{
    FLASH_COMMAND_ACCESSWINDOW,               /**< Flash access window command */
    FLASH_COMMAND_STARTUPAREA                 /**< Flash change startup area command */
} r_flash_command_t;

ssp_err_t   HW_FLASH_LP_access_window_set (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const start_addr, uint32_t const end_addr) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t   HW_FLASH_LP_access_window_clear (flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t   HW_FLASH_LP_set_startup_area_temporary (flash_lp_instance_ctrl_t * const p_ctrl, flash_startup_area_swap_t swap_type) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t   HW_FLASH_LP_set_startup_area_boot (flash_lp_instance_ctrl_t * const p_ctrl, flash_startup_area_swap_t swap_type) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t   HW_FLASH_LP_set_id_code (flash_lp_instance_ctrl_t       * const p_ctrl,
                                     uint8_t                  const * const p_id_code,
                                     flash_id_code_mode_t                   mode) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
void HW_FLASH_LP_extra_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t    start_addr_startup_value,
                                         const uint32_t    end_addr,
                                         r_flash_command_t command) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_LP_extra_check (flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

ssp_err_t HW_FLASH_LP_set_startup_area (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t value) PLACE_IN_RAM_SECTION;

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_CODEFLASH_EXTRA_H */
