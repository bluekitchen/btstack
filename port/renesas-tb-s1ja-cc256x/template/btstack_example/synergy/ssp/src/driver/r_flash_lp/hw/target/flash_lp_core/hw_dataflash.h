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
 * File Name    : hw_dataflash.h
 * Description  : Data Flash Control processing for Low Power Flash
 **********************************************************************************************************************/

#ifndef R_DATAFLASH_H
#define R_DATAFLASH_H

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/******************************************************************************
 * Macro definitions
 ******************************************************************************/

/*  flash mode definition (FENTRYR Register setting)*/
#define FENTRYR_DATAFLASH_PE_MODE (0xAA80U)
#define FENTRYR_READ_MODE         (0xAA00U)

/*  flash mode definition (FPMCR Register setting)*/
#define DATAFLASH_PE_MODE (0x10U)
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

/**
 * @struct r_dataflash_data_t
 * DATAFLASH information values
 */
typedef struct R_DATAFLASH_DATA_T
{
    uint32_t  start_addr;   /* start address (Erase) or Ram Source for Write, Dest for read */
    uint32_t  end_addr;     /* end address (Erase), or Flash Start address which will be read/written */
    uint32_t  write_cnt;    /* bytes remaining to do */
} r_dataflash_data_t;

typedef struct R_DATAFLASH_ERASE_T
{
    uint32_t  start_addr;   /* start address (Erase) or Ram Source for Write, Dest for read */
    uint32_t  end_addr;     /* end address (Erase), or Flash Start address which will be read/written */
    uint32_t  write_cnt;    /* bytes remaining to do */
} r_dataflash_erase_t;

/******************************************************************************
 * Exported global variables
 ******************************************************************************/
void      HW_FLASH_LP_dataflash_enable (flash_lp_instance_ctrl_t * const p_ctrl);

void      HW_FLASH_LP_dataflash_enter_pe_mode (flash_lp_instance_ctrl_t * const p_ctrl);

ssp_err_t HW_FLASH_LP_dataflash_enter_read_mode (flash_lp_instance_ctrl_t * const p_ctrl);

void      HW_FLASH_LP_dataflash_write (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const src_start_address,
                                       uint32_t dest_start_address,
                                       uint32_t num_bytes,
                                       uint32_t min_program_size);

void      HW_FLASH_LP_dataflash_write_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t psrc_addr,  uint32_t dest_addr);

ssp_err_t HW_FLASH_LP_dataflash_write_monitor (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t min_program_size);

void      HW_FLASH_LP_dataflash_erase (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t start_addr, uint32_t num_blocks, uint32_t block_size);

void      HW_FLASH_LP_dataflash_erase_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t block_start_addr, uint32_t block_size);

ssp_err_t HW_FLASH_LP_dataflash_erase_monitor (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t block_size);

void      HW_FLASH_LP_dataflash_blank_check (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t start_addr, uint32_t end_addr);

ssp_err_t HW_FLASH_LP_dataflash_blank_check_monitor (flash_lp_instance_ctrl_t * const p_ctrl);

void HW_FLASH_LP_data_flash_set_flash_settings(current_parameters_t * const p_current_parameters);

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_DATAFLASH_H */
