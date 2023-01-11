/***********************************************************************************************************************
 * Copyright [2020-2022] Renesas Electronics Corporation and/or its affiliates.  All Rights Reserved.
 *
 * This software and documentation are supplied by Renesas Electronics America Inc. and may only be used with products
 * of Renesas Electronics Corp. and its affiliates ("Renesas").  No other uses are authorized.  Renesas products are
 * sold pursuant to Renesas terms and conditions of sale.  Purchasers are solely responsible for the selection and use
 * of Renesas products and Renesas assumes no liability.  No license, express or implied, to any intellectual property
 * right is granted by Renesas. This software is protected under all applicable laws, including copyright laws. Renesas
 * reserves the right to change or discontinue this software and/or this documentation. THE SOFTWARE AND DOCUMENTATION
 * IS DELIVERED TO YOU "AS IS," AND RENESAS MAKES NO REPRESENTATIONS OR WARRANTIES, AND TO THE FULLEST EXTENT
 * PERMISSIBLE UNDER APPLICABLE LAW, DISCLAIMS ALL WARRANTIES, WHETHER EXPLICITLY OR IMPLICITLY, INCLUDING WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT, WITH RESPECT TO THE SOFTWARE OR
 * DOCUMENTATION.  RENESAS SHALL HAVE NO LIABILITY ARISING OUT OF ANY SECURITY VULNERABILITY OR BREACH.  TO THE MAXIMUM
 * EXTENT PERMITTED BY LAW, IN NO EVENT WILL RENESAS BE LIABLE TO YOU IN CONNECTION WITH THE SOFTWARE OR DOCUMENTATION
 * (OR ANY PERSON OR ENTITY CLAIMING RIGHTS DERIVED FROM YOU) FOR ANY LOSS, DAMAGES, OR CLAIMS WHATSOEVER, INCLUDING,
 * WITHOUT LIMITATION, ANY DIRECT, CONSEQUENTIAL, SPECIAL, INDIRECT, PUNITIVE, OR INCIDENTAL DAMAGES; ANY LOST PROFITS,
 * OTHER ECONOMIC DAMAGE, PROPERTY DAMAGE, OR PERSONAL INJURY; AND EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH LOSS, DAMAGES, CLAIMS OR COSTS.
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#ifndef R_FLASH_HP_H
#define R_FLASH_HP_H

#include "bsp_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

#include "r_flash_api.h"
#include "r_flash_hp_cfg.h"

/*******************************************************************************************************************//**
 * @ingroup HAL_Library
 * @addtogroup FLASH_HP
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* If Code Flash programming is enabled, then all API functions must execute out of RAM. */
#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)
 #if defined(__ICCARM__)
  #pragma section=".code_in_ram"
 #endif
 #if defined(__ARMCC_VERSION) || defined(__GNUC__)
  #define PLACE_IN_RAM_SECTION    __attribute__((noinline)) BSP_PLACE_IN_SECTION(".code_in_ram")
 #else
  #define PLACE_IN_RAM_SECTION    BSP_PLACE_IN_SECTION(".code_in_ram")
 #endif
#else
 #define PLACE_IN_RAM_SECTION
#endif

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Possible Flash operation states */
typedef enum e_flash_bgo_operation
{
    FLASH_OPERATION_NON_BGO,
    FLASH_OPERATION_DF_BGO_WRITE,
    FLASH_OPERATION_DF_BGO_ERASE,
    FLASH_OPERATION_DF_BGO_BLANKCHECK,
} flash_bgo_operation_t;

/** Flash HP instance control block. DO NOT INITIALIZE. */
typedef struct st_flash_hp_instance_ctrl
{
    uint32_t              opened;      ///< To check whether api has been opened or not.
    flash_cfg_t const   * p_cfg;
    uint32_t              timeout_write_cf;
    uint32_t              timeout_write_df;
    uint32_t              timeout_dbfull;
    uint32_t              timeout_blank_check;
    uint32_t              timeout_write_config;
    uint32_t              timeout_erase_cf_small_block;
    uint32_t              timeout_erase_cf_large_block;
    uint32_t              timeout_erase_df_block;
    uint32_t              source_start_address;
    uint32_t              dest_end_address;
    uint32_t              operations_remaining;
    flash_bgo_operation_t current_operation;      ///< Operation in progress, for example, FLASH_OPERATION_CF_ERASE

    void (* p_callback)(flash_callback_args_t *); // Pointer to callback
    flash_callback_args_t * p_callback_memory;    // Pointer to optional callback argument memory
    void const            * p_context;            // Pointer to context to be passed into callback function
} flash_hp_instance_ctrl_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const flash_api_t g_flash_on_flash_hp;

/** @endcond */

fsp_err_t R_FLASH_HP_Open(flash_ctrl_t * const p_api_ctrl, flash_cfg_t const * const p_cfg);

fsp_err_t R_FLASH_HP_Write(flash_ctrl_t * const p_api_ctrl,
                           uint32_t const       src_address,
                           uint32_t             flash_address,
                           uint32_t const       num_bytes);

fsp_err_t R_FLASH_HP_Erase(flash_ctrl_t * const p_api_ctrl, uint32_t const address, uint32_t const num_blocks);

fsp_err_t R_FLASH_HP_BlankCheck(flash_ctrl_t * const p_api_ctrl,
                                uint32_t const       address,
                                uint32_t             num_bytes,
                                flash_result_t     * blank_check_result);

fsp_err_t R_FLASH_HP_Close(flash_ctrl_t * const p_api_ctrl);

fsp_err_t R_FLASH_HP_StatusGet(flash_ctrl_t * const p_api_ctrl, flash_status_t * const p_status);

fsp_err_t R_FLASH_HP_AccessWindowSet(flash_ctrl_t * const p_api_ctrl, uint32_t const start_addr,
                                     uint32_t const end_addr);

fsp_err_t R_FLASH_HP_AccessWindowClear(flash_ctrl_t * const p_api_ctrl);

fsp_err_t R_FLASH_HP_IdCodeSet(flash_ctrl_t * const  p_api_ctrl,
                               uint8_t const * const p_id_code,
                               flash_id_code_mode_t  mode);

fsp_err_t R_FLASH_HP_Reset(flash_ctrl_t * const p_api_ctrl);

fsp_err_t R_FLASH_HP_UpdateFlashClockFreq(flash_ctrl_t * const p_api_ctrl);

fsp_err_t R_FLASH_HP_StartUpAreaSelect(flash_ctrl_t * const      p_api_ctrl,
                                       flash_startup_area_swap_t swap_type,
                                       bool                      is_temporary);
fsp_err_t R_FLASH_HP_CallbackSet(flash_ctrl_t * const          p_api_ctrl,
                                 void (                      * p_callback)(flash_callback_args_t *),
                                 void const * const            p_context,
                                 flash_callback_args_t * const p_callback_memory);
fsp_err_t R_FLASH_HP_BankSwap(flash_ctrl_t * const p_api_ctrl);
fsp_err_t R_FLASH_HP_InfoGet(flash_ctrl_t * const p_api_ctrl, flash_info_t * const p_info);

/*******************************************************************************************************************//**
 * @} (end defgroup FLASH_HP)
 **********************************************************************************************************************/
#if !BSP_FEATURE_FLASH_HP_VERSION
 #error "r_flash_hp is not a supported module for this board type."
#endif

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif
