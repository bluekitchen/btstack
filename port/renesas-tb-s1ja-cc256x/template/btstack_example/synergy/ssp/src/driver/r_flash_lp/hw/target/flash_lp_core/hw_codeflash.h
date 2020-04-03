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
 * File Name    : hw_codeflash.h
 * Description  : Code Flash Control processing for Low Power Flash
 **********************************************************************************************************************/

#ifndef R_CODEFLASH_H
#define R_CODEFLASH_H

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/******************************************************************************
 * Macro definitions
 ******************************************************************************/

/*  flash mode definition (FENTRYR Register setting)*/
#define FENTRYR_CODEFLASH_PE_MODE (0xAA01U)
#define FENTRYR_READ_MODE         (0xAA00U)

/*  flash mode definition (FPMCR Register setting)*/
#define DISCHARGE_1       (0x12U)
#define DISCHARGE_2       (0x92U)
#define CODEFLASH_PE_MODE (0x82U)
#define READ_MODE         (0x08U)
#define LVPE_MODE         (0x40U)

/*  operation definition (FCR Register setting)*/
#define FCR_WRITE             (0x81U)
#define FCR_ERASE             (0x84U)
#define FCR_BLANKCHECK        (0x83U)
#define FCR_CLEAR             (0x00U)

#define OPCCR_HIGH_SPEED_MODE (0x00U)

/******************************************************************************
 * Typedef definitions
 ******************************************************************************/
typedef struct R_CODEFLASH_DATA_T
{
    uint32_t  start_addr;   /* start address (Erase) or Ram Source for Write, Dest for read */
    uint32_t  end_addr;     /* end address (Erase), or Flash Start address which will be read/written */
    uint32_t  write_cnt;    /* units remaining to do */
} r_codeflash_data_t;

/******************************************************************************
 * Exported global variables
 ******************************************************************************/
/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
void      HW_FLASH_LP_codeflash_enter_pe_mode (flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_LP_codeflash_enter_read_mode (flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

void      HW_FLASH_LP_codeflash_erase (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t start_addr,
                                       const uint32_t num_blocks,
                                       uint32_t block_size) PLACE_IN_RAM_SECTION;

void      HW_FLASH_LP_codeflash_erase_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t start_addr,
                                                 uint32_t block_size) PLACE_IN_RAM_SECTION;

ssp_err_t HW_FLASH_LP_codeflash_erase_monitor (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t block_size) PLACE_IN_RAM_SECTION;

void      HW_FLASH_LP_codeflash_write (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const src_start_address,
                                       uint32_t       dest_start_address,
                                       uint32_t       num_bytes,
                                       uint32_t       min_program_size) PLACE_IN_RAM_SECTION;

void      HW_FLASH_LP_codeflash_write_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t psrc_addr,
                                                 const uint32_t dest_addr,
                                                 uint32_t       min_program_size) PLACE_IN_RAM_SECTION;

ssp_err_t HW_FLASH_LP_codeflash_write_monitor (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t min_program_size) PLACE_IN_RAM_SECTION;

ssp_err_t HW_FLASH_LP_codeflash_blank_check (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t start_addr, uint32_t num_bytes) PLACE_IN_RAM_SECTION;

void      HW_FLASH_LP_codeflash_blank_check_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t start_addr, uint32_t num_bytes) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_LP_codeflash_blank_check_monitor (flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
void      HW_FLASH_LP_code_flash_set_flash_settings (current_parameters_t * const p_current_parameters) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_LP_code_get_macro_info (uint32_t const address, uint32_t num_bytes, flash_lp_macro_info_t * const p_macro) PLACE_IN_RAM_SECTION;


/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_CODEFLASH_H */
