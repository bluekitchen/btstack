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
 * File Name    : hw_flash_lp_private.h
 * Description  : FLASH LLD header file for Low power Flash.
 **********************************************************************************************************************/
#include "r_flash_lp.h"

#ifndef HW_FLASH_LP_PRIVATE_H
#define HW_FLASH_LP_PRIVATE_H

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/**********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/** The maximum timeout for commands is 100usec when FCLK is 16 MHz i.e. 1600 FCLK cycles.
 * Assuming worst case of ICLK at 240 MHz and FCLK at 4 MHz, and optimization set to max such that
 * each count decrement loop takes only 5 cycles, then ((240/4)*1600)/5 = 19200 */
#define FLASH_FRDY_CMD_TIMEOUT (19200)

/** Time that it would take for the Data Buffer to be empty (DBFULL Flag) is 90 FCLK cycles.
 * Assuming worst case of ICLK at 240 MHz and FCLK at 4 MHz, and optimization set to max such that
 * each count decrement loop takes only 5 cycles, then ((240/4)*90)/5 = 1080 */
#define FLASH_DBFULL_TIMEOUT (1080)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** Flash software lock states */
typedef enum e_flash_states_t
{
    FLASH_STATE_UNINITIALIZED = 0,
    FLASH_STATE_INITIALIZATION,
    FLASH_STATE_READY,
    FLASH_STATE_WRITING,
    FLASH_STATE_ERASING,
    FLASH_STATE_BLANK_CHECK,
    FLASH_STATE_ACCESS_WINDOW,
    FLASH_STATE_STARTUP_AREA,
    FLASH_STATE_ID_CODE,
    FLASH_UPDATE_FCLK
} flash_states_t;

/** Flash types */
typedef enum e_flash_type
{
    FLASH_TYPE_CODE_FLASH = 0,      ///< Operation is referencing Code Flash
    FLASH_TYPE_DATA_FLASH           ///< Operation is referencing Data Flash
} flash_type_t;

/** Possible Flash operation states */
typedef enum e_flash_operation
{
    FLASH_OPERATION_INVALID,
    FLASH_OPERATION_IDLE,
    FLASH_OPERATION_CF_ERASE,
    FLASH_OPERATION_CF_WRITE,
    FLASH_OPERATION_DF_ERASE,
    FLASH_OPERATION_DF_WRITE,
    FLASH_OPERATION_DF_BLANKCHECK,
    FLASH_OPERATION_CF_BLANKCHECK,
    FLASH_OPERATION_DF_BGO_WRITE,
    FLASH_OPERATION_DF_BGO_ERASE,
    FLASH_OPERATION_DF_BGO_BLANKCHECK,
    FLASH_OPERATION_FCU_INIT,
    FLASH_OPERATION_FCU_RESET
} flash_operation_t;

/** Parameters reflecting current in progress Flash operation */
typedef struct st_current_parameters
{
    uint32_t           src_addr;                      ///< Source address for operation in progress
    uint32_t           dest_addr;                     ///< Destination address for operation in progress
    uint32_t           total_count;                   ///< total number of bytes to write/erase
    uint32_t           current_count;                 ///< bytes of total completed
    flash_operation_t  current_operation;             ///< Operation in progress, ie. FLASH_OPERATION_CF_ERASE
    bool               bgo_enabled_df;                ///< Data Flash BGO operation enable/disable
    uint32_t           min_pgm_size;                  ///< Minimum # of bytes that can be programmed, as defined for the
                                                      ///< part, usually differs for CF and DF
    uint32_t           block_size;                    ///< Minimum # of bytes that can be read or erased
    uint32_t           cf_memory_st_addr;             ///< Starting memory address for Code Flash
    uint32_t           df_memory_st_addr;             ///< Starting memory address for Data Flash
    uint8_t            flash_cf_macros;               ///< The number of implemented HW Code Flash macros.
    uint32_t           cf_macro_size;                 ///< The size of the implemented Code Flash macro
    uint32_t           wait_cnt;                      ///< Worst case wait time for operation completion
    uint32_t           wait_max_erase_df_block;       ///< Worst case erase time for DF block
    uint32_t           wait_max_erase_cf_block;       ///< Worst case erase time for large CF block
    uint32_t           wait_max_blank_check;          ///< Worst case blankcheck time for minimum blank check size
    uint32_t           wait_max_write_df;             ///< Worst case write time for DF minimum write size
    uint32_t           wait_max_write_cf;             ///< Worst case write time for CF minimum write size
    uint32_t           flash_clock_freq;              ///< Flash clock frequency in Hz
    uint32_t           system_clock_freq;             ///< System clock frequency in Hz
    flash_result_t     bc_result;                     ///< Overall result of possible multi operation blankcheck.
} current_parameters_t;

/** Flash block details */
typedef struct st_flash_block_info
{
    uint32_t  block_section_st_addr;       ///< starting address for this block section (blocks of this size)
    uint32_t  block_section_end_addr;      ///< ending address for this block section (blocks of this size)
    uint32_t  memory_end_addr;             ///< ending address for the memory itself (code or data)
    uint32_t  block_size;                  ///< Size of this block
    uint32_t  min_program_size;            ///< minimum program size (bytes)
    uint32_t  num_blocks;                  ///< number of blocks at this size
    uint32_t  total_blocks;                ///< total number of blocks of this type
    uint32_t  this_block_st_addr;          ///< starting address for this block (for queried address)
    uint32_t  this_block_end_addr;         ///< ending address for this block (blocks of this size)
    uint32_t  this_block_number;           ///< zero based block number for this address
    bool      is_code_flash_addr;          ///< true if queried address is a Code Flash address
    flash_type_t flash_type;               ///< target operation is for FLASH_TYPE_CODE_FLASH or FLASH_TYPE_DATA_FLASH
} flash_block_info_t;

/** Flash macro information */
typedef struct st_flash_lp_macro_info
{
    uint32_t  macro_st_addr;       ///< starting address for this macro
    uint32_t  macro_end_addr;      ///< ending address for this macro
    uint32_t  macro_size;          ///< number of bytes in this macro
    uint32_t  total_macros;        ///< total number of macros for this device and flash type (Code or Data)
    uint32_t  allowed_bytes;       ///< number of the requested bytes that may be provided to blancheck for this macro
} flash_lp_macro_info_t;

/**********************************************************************************************************************
 * Public Functions
 **********************************************************************************************************************/

/** NOTE: The keyword 'PLACE_FLASH_HERE' is defined in r_flash.h and in conjunction with the
 * FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE config setting, determines if sections of the Flash API will be copied
 * to
 * and executed from RAM. This is required for Data Flash BGO (Background) operation.
 */
/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_LP_irq_cfg (bool state, flash_cfg_t const * const p_cfg, IRQn_Type irq) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
void      HW_FLASH_LP_init (flash_lp_instance_ctrl_t * const p_ctrl, current_parameters_t * p_current_parameters) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_LP_pe_mode_enter (flash_lp_instance_ctrl_t * const p_ctrl, flash_type_t flash_type) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_LP_pe_mode_exit (flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_LP_erase (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t block_address, uint32_t num_blocks, uint32_t block_size) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_LP_write (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const src_start_address,
                             uint32_t dest_start_address,
                             uint32_t num_bytes,
                             uint32_t min_program_size) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_LP_blankcheck (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t start_address, uint32_t num_bytes,
                                  flash_result_t * result) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
void      HW_FLASH_LP_reset (flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
ssp_err_t HW_FLASH_HP_stop (void) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
void      flash_ReleaseState (void) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
bool      flash_get_block_info (uint32_t addr, flash_block_info_t * p_block_info) PLACE_IN_RAM_SECTION;

/** FRDY ISR is only used for DataFlash operations. For Code flash operations are blocking. Therefore ISR does
 * not need to be located in RAM.
 */
void fcu_frdyi_isr (void);

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER
#endif /* HW_FLASH_LP_PRIVATE_H */
