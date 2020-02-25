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
 * File Name    : hw_codeflash_extra.c
 * Description  : Flash Control Access window and swap control processing for Low Power Flash
 **********************************************************************************************************************/

/******************************************************************************
 * Includes   <System Includes> , “Project Includes”
 ******************************************************************************/
#include "bsp_api.h"
#include "r_flash_lp.h"
#include "../hw_flash_lp_private.h"
#include "r_flash_cfg.h"
#include "hw_flash_common.h"
#include "hw_dataflash.h"
#include "hw_codeflash.h"
#include "hw_codeflash_extra.h"

/*******************************************************************************************************************//**
 * @addtogroup FLASH
 * @{
 **********************************************************************************************************************/
/*******************************************************************************************************************//**
 * @} (end FLASH)
 **********************************************************************************************************************/

/******************************************************************************
 * Private global variables and functions
 ******************************************************************************/

/*******************************************************************************************************************//**
 * @brief Temporarily sets the startup area to use the DEFAULT or ALTERNATE area as requested.
 * On the next subsequent reset, the startup area will be determined by the state of the BTFLG.
 * This command does NOT modify the configuration via The Configuration Set command, hence these changes are
 * only in effect until the next reset.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @param[in]  swap_type -  specifies the startup area swap being requested.
 *
 * @retval SSP_SUCCESS                StartUp area temporarily modified.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_set_startup_area_temporary (flash_lp_instance_ctrl_t * const p_ctrl, flash_startup_area_swap_t swap_type)
{
    ssp_err_t err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** Set the Flash initial setting startup area select bit as requested. */
    if (FLASH_STARTUP_AREA_BLOCK0 == swap_type)
    {
        p_faci_reg->FISR_b.SAS =  SAS_DEFAULT_AREA;
    }
    else if (FLASH_STARTUP_AREA_BLOCK1 == swap_type)
    {
        p_faci_reg->FISR_b.SAS =  SAS_ALTERNATE_AREA;
    }
    else
    {
        p_faci_reg->FISR_b.SAS =  (uint32_t)SAS_PER_BTFLG_AREA;
    }

    return err;
}

/*******************************************************************************************************************//**
 * @brief  Configure an access window for the Code Flash memory using the provided start and end address. An access
 *         window defines a contiguous area in Code Flash for which programming/erase is enabled. This area is
 *         on block boundaries.
 *         The block containing start_addr is the first block. The block containing end_addr is the last block.
 *         The access window then becomes first block - last block inclusive. Anything outside this range
 *         of Code Flash is then write protected.
 *         This command DOES modify the configuration via The Configuration Set command to update the FAWS and FAWE.
 *
 * @param[in]  p_ctrl     Pointer to the Flash control block
 * @param[in]  start_addr Determines the Starting block for the Code Flash access window.
 * @param[in]  end_addr   Determines the Ending block for the Code Flash access window.
 *
 * @retval SSP_SUCCESS                Access window successfully configured.
 * @retval SSP_ERR_ERASE_FAILED        Status is indicating an erase error. Possibly from a prior operation.
 * @retval SSP_ERR_CMD_LOCKED   FCU is in locked state, typically as a result of having received an illegal
 * command.
 * @retval SSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation.
 * @retval SSP_ERR_TIMEOUT Timed out waiting for the FCU to become ready.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_access_window_set (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const start_addr, uint32_t const end_addr)
{
    ssp_err_t err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    uint32_t  start_addr_idx;
    uint32_t  end_addr_idx;

    start_addr_idx = start_addr;
    end_addr_idx   = end_addr;

    /** Select The Extra Area */
    p_faci_reg->FASR_b.EXS = 1U;

    /** Set the access window. */
    HW_FLASH_LP_extra_operation(p_ctrl, start_addr_idx, end_addr_idx, FLASH_COMMAND_ACCESSWINDOW);

    /** Wait for the operation to complete or error. */
    do
    {
        err = HW_FLASH_LP_extra_check(p_ctrl);
    } while (SSP_ERR_IN_USE == err);

    /** Select User Area */
    p_faci_reg->FASR_b.EXS = 0U;

    /** Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * @brief  Remove any access window that is currently configured in the Code Flash.
 *         This command DOES modify the configuration via The Configuration Set command to update the FAWS and FAWE.
 *         Subsequent to this call all Code Flash is writable.
 *
 * @retval SSP_SUCCESS              Access window successfully removed.
 * @retval SSP_ERR_ERASE_FAILED      Status is indicating an erase error. Possibly from a prior operation.
 * @retval SSP_ERR_CMD_LOCKED   FCU is in locked state, typically as a result of having received an illegal
 * command.
 * @retval SSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation.
 * @retval SSP_ERR_TIMEOUT      Timed out waiting for the FCU to become ready.
 * @param[in]  p_ctrl           Pointer to the Flash control block
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_access_window_clear (flash_lp_instance_ctrl_t * const p_ctrl)
{
    ssp_err_t err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** Select Extra Area */
    p_faci_reg->FASR_b.EXS = 1U;

    // Setting the accesswindow to 0,0 will clear any configured access window.
    /** Call extra operation to clear the access window. */
    HW_FLASH_LP_extra_operation(p_ctrl, 0, 0, FLASH_COMMAND_ACCESSWINDOW);

    /** Wait until the operation is complete or an error. */
    do
    {
        err = HW_FLASH_LP_extra_check(p_ctrl);
    } while (SSP_ERR_IN_USE == err);

    /** Select User Area */
    p_faci_reg->FASR_b.EXS = 0U;

    return err;
}

/*******************************************************************************************************************//**
 * @brief      Set the id code
 *
 * @param      p_ctrl                The p control
 * @param      p_id_code             Pointer to the code to be written
 * @param[in]  mode                  The id code mode
 *
 * @retval     SSP_SUCCESS           The id code have been written.
 * @retval     SSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation.
 * @retval     SSP_ERR_TIMEOUT       Timed out waiting for completion of extra command.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_set_id_code (flash_lp_instance_ctrl_t       * const p_ctrl,
                                   uint8_t                  const * const p_id_code,
                                   flash_id_code_mode_t                   mode)
{
    ssp_err_t err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;
    uint32_t fexcr_command = FEXCR_OCDID1;

    uint16_t mode_mask = 0U;

    if(FLASH_ID_CODE_MODE_LOCKED_WITH_ALL_ERASE_SUPPORT == mode)
    {
        mode_mask = 0xC000U;
    }

    if(FLASH_ID_CODE_MODE_LOCKED == mode)
    {
        mode_mask = 0x8000U;
    }

    /* Timeout counter. */
    volatile uint32_t wait_cnt = FLASH_FRDY_CMD_TIMEOUT;

    /** For each ID byte register */
    for(uint32_t i = 0U; i < 16U; i+=4U)
    {
        /** Select Extra Area */
        p_faci_reg->FASR_b.EXS = 1U;

        /** Write the ID Bytes. If mode is unlocked write all 0xFF. Write the mode mask to the MSB. */
        if(FLASH_ID_CODE_MODE_UNLOCKED == mode)
        {
            p_faci_reg->FWBL0 = 0xFFFFU;
            p_faci_reg->FWBH0 = 0xFFFFU;
        }
        else
        {
            /* The id code array may be unaligned. Do not attempt to optimize this code to prevent unaligned access. */
            p_faci_reg->FWBL0 = (uint32_t)(p_id_code[i] | ((uint32_t)p_id_code[i+1] << 8));
            if(12U == i)
            {
                p_faci_reg->FWBH0 = (uint32_t)(p_id_code[i+2] | ((uint32_t)p_id_code[i+3] << 8)) | mode_mask;
            }
            else
            {
                p_faci_reg->FWBH0 = (uint32_t)(p_id_code[i+2] | ((uint32_t)p_id_code[i+3] << 8));
            }
        }

        /** Execute OCDID command */
        p_faci_reg->FEXCR = fexcr_command;

        /** Increment the command to write to the next OCDID bytes */
        fexcr_command++;

        /** Wait until the operation is complete or an error. */
        do
        {
            err = HW_FLASH_LP_extra_check(p_ctrl);
            wait_cnt--;
            if(wait_cnt == 0U)
            {
                err = SSP_ERR_TIMEOUT;
            }
        } while (SSP_ERR_IN_USE == err);

        /** Select User Area */
        p_faci_reg->FASR_b.EXS = 0U;

        /** If failure return error */
        if(SSP_SUCCESS != err)
        {
            return err;
        }
    }

    return err;
}

/*******************************************************************************************************************//**
 * @brief Modifies the start-up program swap flag (BTFLG) based on the supplied parameters. These changes will
 *        take effect on the next reset. This command DOES modify the configuration via The Configuration
 *        Set command to update the BTFLG.
 *
 * @retval SSP_SUCCESS                Access window successfully removed.
 * @retval SSP_ERR_ERASE_FAILED        Status is indicating an erase error. Possibly from a prior operation.
 * @retval SSP_ERR_CMD_LOCKED   FCU is in locked state, typically as a result of having received an illegal
 * command.
 * @retval SSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation.
 * @retval SSP_ERR_TIMEOUT Timed out waiting for the FCU to become ready.
 *
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @param[in]  swap_type -  specifies the startup area swap being requested.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_set_startup_area_boot (flash_lp_instance_ctrl_t * const p_ctrl, flash_startup_area_swap_t swap_type)
{
    ssp_err_t err               = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;
    uint32_t  startup_area_mask = (uint32_t)((uint32_t)swap_type << 8);       // move selection to bit 8

    /** Select Extra Area */
    p_faci_reg->FASR_b.EXS = 1U;

    /** Call extra operation to set the startup area. */
    HW_FLASH_LP_extra_operation(p_ctrl, startup_area_mask, 0, FLASH_COMMAND_STARTUPAREA);

    /** Wait until the operation is complete or an error occurs. */
    do
    {
        err = HW_FLASH_LP_extra_check(p_ctrl);
    } while (SSP_ERR_IN_USE == err);

    /** Select User Area */
    p_faci_reg->FASR_b.EXS = 0U;

    return err;
}

/*******************************************************************************************************************//**
 * @brief  Command processing for the extra area.
 *
 * @param[in]  p_ctrl                    Pointer to the Flash control block
 * @param[in]  start_addr_startup_value  Determines the Starting block for the Code Flash access window.
 * @param[in]  end_addr                  Determines the Ending block for the Code Flash access window.
 * @param[in]  command                   Select from R_FLASH_ACCESSWINDOW or R_FLASH_STARTUPAREA.
 *
 **********************************************************************************************************************/
void HW_FLASH_LP_extra_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t    start_addr_startup_value,
                                         const uint32_t    end_addr,
                                         r_flash_command_t command)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;
    /* Per the spec: */
    /* Setting data to the FWBL0 register, this command is allowed to select the start-up area from the */
    /* default area (blocks 0-3) to the alternative area (blocks 4-7) and set the security. */
    /* Bit 8 of the FWBL0 register is 0: the alternative area (blocks 4-7) are selected as the start-up area. */
    /* Bit 8 of the FWBL0 register is 1: the default area (blocks 0-3) are selected as the start-up area. */
    /* Bit 14 of the FWBL0 register MUST be 1! Setting this bit to zero will clear the FSPR register and lock the */
    /* FLASH!!! It is not be possible to unlock it. */

    if (FLASH_COMMAND_ACCESSWINDOW == command)
    {
        /** Set the Access Window start and end addresses. */
        /* FWBL0 reg sets the Start Block address. FWBH0 reg sets the end address. */
        /* Convert the addresses to their respective block numbers */
        p_faci_reg->FWBL0 = ((start_addr_startup_value >> 10) & 0xFFFFU);
        p_faci_reg->FWBH0 = ((end_addr >> 10) & 0xFFFFU);

        /** Execute Access Window command */
        p_faci_reg->FEXCR = FEXCR_AW;
    }
    else
    {
        /* Startup Area Flag value setting */
        p_faci_reg->FWBH0 = 0xFFFFU;

        /* FSPR must be set. Unused bits write value should be 1. */
        p_faci_reg->FWBL0 = (start_addr_startup_value | 0xFEFFU);

        /** Execute Startup Area Flag command */
        p_faci_reg->FEXCR = FEXCR_STARTUP;
    }
}

/*******************************************************************************************************************//**
 * @brief      Verifying the execution result for the extra area.
 * @param[in]  p_ctrl                Pointer to the Flash control block
 * @retval     SSP_SUCCESS           The extra command has successfully completed.
 * @retval     SSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation.
 * @retval     SSP_ERR_TIMEOUT       Timed out waiting for completion of extra command.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_extra_check (flash_lp_instance_ctrl_t * const p_ctrl)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /* Timeout counter. */
    volatile uint32_t wait_cnt = FLASH_FRDY_CMD_TIMEOUT;

    /** If the software command of the FEXCR register is not terminated return in use. */
    if (1U != p_faci_reg->FSTATR1_b.EXRDY)
    {
        return SSP_ERR_IN_USE;
    }

    /** Clear the Flash Extra Area Control Register. */
    p_faci_reg->FEXCR = FEXCR_CLEAR;

    /** Wait until the command has completed or a timeout occurs. If timeout return error. */
    while (0U != p_faci_reg->FSTATR1_b.EXRDY)
    {
        /* Check that execute command is completed. */
        /* Confirm that the written value can be read correctly. */
        if (wait_cnt <= (uint32_t)0)
        {
            /* return timeout status*/
            return SSP_ERR_TIMEOUT;
        }
        wait_cnt--;
    }

    /** If Extra Area Illegal Command Error Flag or Error during programming reset the flash and return error. */
    if ((0U != p_faci_reg->FSTATR00_b.EILGLERR) || (0U != p_faci_reg->FSTATR00_b.PRGERR01))
    {
        HW_FLASH_LP_reset(p_ctrl);
        return SSP_ERR_WRITE_FAILED;
    }

    /** Return success. */
    return SSP_SUCCESS;
}

