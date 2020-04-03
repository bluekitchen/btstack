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
 * File Name    : hw_codeflash.c
 * Description  : Code Flash Control processing for Low Power Flash
 **********************************************************************************************************************/

/******************************************************************************
 * Includes   <System Includes> , “Project Includes”
 ******************************************************************************/
#include "bsp_api.h"
#include "r_flash_lp.h"
#include "../hw_flash_lp_private.h"
#include "r_flash_cfg.h"
#include "hw_flash_common.h"
#include "hw_codeflash.h"

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

static r_codeflash_data_t g_code_flash_info = {0U};
static void   HW_FLASH_LP_codeflash_write_fpmcr (flash_lp_instance_ctrl_t * const p_ctrl, uint8_t value) PLACE_IN_RAM_SECTION;
static current_parameters_t * gp_flash_settings = {0U};
static flash_lp_macro_info_t macro_info = {0U};


/*******************************************************************************************************************//**
 * @brief   Transition to Code Flash P/E mode.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @retval     none
 **********************************************************************************************************************/
void HW_FLASH_LP_codeflash_enter_pe_mode (flash_lp_instance_ctrl_t * const p_ctrl)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** While the Flash API is in use we will disable the FLash Cache. */
    p_ctrl->cache_state = BSP_CACHE_STATE_OFF;
    R_BSP_CacheOff(&p_ctrl->cache_state);

    if (SSP_INVALID_VECTOR != p_ctrl->irq)
    {
        NVIC_DisableIRQ(p_ctrl->irq);           ///< We are not supporting Flash Rdy interrupts for Code Flash operations
    }
    p_faci_reg->FENTRYR = FENTRYR_CODEFLASH_PE_MODE;

    HW_FLASH_LP_codeflash_write_fpmcr(p_ctrl, DISCHARGE_1);

    /** Wait for 2us over (tDIS) */
    HW_FLASH_LP_delay_us(WAIT_TDIS, gp_flash_settings->system_clock_freq);

    if (R_SYSTEM->OPCCR_b.OPCM == 0U)        ///< High speed mode?
    {
        HW_FLASH_LP_codeflash_write_fpmcr(p_ctrl, DISCHARGE_2);
        HW_FLASH_LP_codeflash_write_fpmcr(p_ctrl, CODEFLASH_PE_MODE);

        /** Wait for 5us over (tMS) */
        HW_FLASH_LP_delay_us(WAIT_TMS_HIGH, gp_flash_settings->system_clock_freq);
    }
    else
    {
        HW_FLASH_LP_codeflash_write_fpmcr(p_ctrl, DISCHARGE_2 | LVPE_MODE);
        HW_FLASH_LP_codeflash_write_fpmcr(p_ctrl, CODEFLASH_PE_MODE | LVPE_MODE);

        /** Wait for 3us over (tMS) */
        HW_FLASH_LP_delay_us(WAIT_TMS_MID, gp_flash_settings->system_clock_freq);

    }

    p_faci_reg->FISR_b.PCKA = (uint32_t)((gp_flash_settings->flash_clock_freq - 1UL) & (uint32_t)0x1F);
}

/*******************************************************************************************************************//**
 * @brief   This function switches the peripheral from P/E mode for Code Flash to Read mode.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @retval SSP_SUCCESS              Successfully entered read mode.
 * @retval SSP_ERR_TIMEOUT    Timed out waiting for confirmation of transition to read mode
 *
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_codeflash_enter_read_mode (flash_lp_instance_ctrl_t * const p_ctrl)
{
    ssp_err_t         err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** Timeout counter. */
    volatile uint32_t wait_cnt = FLASH_FRDY_CMD_TIMEOUT;

    HW_FLASH_LP_codeflash_write_fpmcr(p_ctrl, DISCHARGE_2);

    /** Wait for 2us over (tDIS) */
    HW_FLASH_LP_delay_us(WAIT_TDIS, gp_flash_settings->system_clock_freq);

    HW_FLASH_LP_codeflash_write_fpmcr(p_ctrl, DISCHARGE_1);
    HW_FLASH_LP_codeflash_write_fpmcr(p_ctrl, READ_MODE);

    /** Wait for 5us over (tMS) */
    HW_FLASH_LP_delay_us(WAIT_TMS_HIGH, gp_flash_settings->system_clock_freq);

    /** Clear the P/E mode register */
    p_faci_reg->FENTRYR = FENTRYR_READ_MODE;

    /** Loop until the Flash P/E mode entry register is cleared or a timeout occurs. If timeout occurs return error. */
    while (0x0000U != p_faci_reg->FENTRYR)
    {
        /* Confirm that the written value can be read correctly. */
        if (wait_cnt <= (uint32_t)0)
        {
            /** Restore the FLash Cache state to what it was before we opened the Flash API. */
            R_BSP_CacheSet(p_ctrl->cache_state);

            /* return timeout status*/
            return SSP_ERR_TIMEOUT;
        }
        wait_cnt--;
    }

    /** Restore the FLash Cache state to what it was before we opened the Flash API. */
    R_BSP_CacheSet(p_ctrl->cache_state);

    return err;
}

/*******************************************************************************************************************//**
 * @brief   Initiates a Write sequence to the Low Power Code Flash data. Address validation has already been
 *          performed by the caller.
 * @param[in] p_ctrl              Pointer to the Flash control block
 * @param[in] src_start_address   Start address of the (RAM) area which stores the programming data.
 * @param[in] dest_start_address  Flash Start address which will be written.
 * @param[in] num_bytes           Number of bytes to write.
 * @param[in] min_program_size    Minimum flash programming size.
 * @retval none.
 **********************************************************************************************************************/
void HW_FLASH_LP_codeflash_write (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const src_start_address, uint32_t dest_start_address, uint32_t num_bytes,
        uint32_t min_program_size)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;
    g_code_flash_info.start_addr = src_start_address;                // Ram Source for data to write
    g_code_flash_info.end_addr   = dest_start_address;               // Flash Start address which will be written
    uint32_t temp = min_program_size;
    uint32_t right_shift = 0U;
    while (1U != temp)
    {
        temp >>= 1;
        right_shift++;
    }
    /** Calculate the number of writes needed. */
    /* This is done with right shift instead of division to avoid using the division library, which would be in flash
     * and cause a jump from RAM to flash. */
    g_code_flash_info.write_cnt  = num_bytes >> right_shift;     //  Number of units to write

    /** Select User Area */
    p_faci_reg->FASR_b.EXS = 0U;

    /** Initiate the code flash write operation. */
    HW_FLASH_LP_codeflash_write_operation(p_ctrl, src_start_address, dest_start_address, min_program_size);
}

/*******************************************************************************************************************//**
 * @brief   Execute a single Write operation on the Low Power Code Flash data.
 * @param[in] p_ctrl       Pointer to the Flash control block
 * @param[in] psrc_addr    Source address for data to be written.
 * @param[in] dest_addr    End address (read form) for writing.
 * @param[in] min_program_size    Minimum flash programming size.
 * @retval none.
 **********************************************************************************************************************/
void HW_FLASH_LP_codeflash_write_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t psrc_addr, const uint32_t dest_addr,
        uint32_t min_program_size)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;
    uint8_t * data8_ptr;
    data8_ptr = (uint8_t *)psrc_addr;
    uint16_t load_value;
    uint8_t byteL;
    uint8_t byteH;

    /** Write flash address setting */
    p_faci_reg->FSARH = (uint16_t) ((dest_addr >> 16) & 0xFFFF);
    p_faci_reg->FSARL = (uint16_t) (dest_addr & 0xFFFF);

    /** Write data address setting. */
    /* Note that the caller could be providing a data buffer that was defined as a char[] buffer. As
     * a result it might not be 16 bit aligned. For the CM0 data accesses that are not aligned will generate
     * a fault. Therefore we will read the data 8 bits at a time.*/
    byteL = *data8_ptr;
    data8_ptr++;
    byteH = *data8_ptr;
    load_value = (uint16_t)(((uint16_t)byteH << 8) | (uint16_t)byteL);
    data8_ptr++;

    p_faci_reg->FWBL0 = load_value;       // Move to bits 31 - 0 for next write.

    byteL = *data8_ptr;
    data8_ptr++;
    byteH = *data8_ptr;
    load_value = (uint16_t)(((uint16_t)byteH << 8) | (uint16_t)byteL);
    data8_ptr++;
    p_faci_reg->FWBH0 = load_value;

    if (min_program_size > 4U)
    {
        byteL = *data8_ptr;
        data8_ptr++;
        byteH = *data8_ptr;
        load_value = (uint16_t)(((uint16_t)byteH << 8) | (uint16_t)byteL);
        data8_ptr++;
        p_faci_reg->FWBL1 = load_value;

        byteL = *data8_ptr;
        data8_ptr++;
        byteH = *data8_ptr;
        load_value = (uint16_t)(((uint16_t)byteH << 8) | (uint16_t)byteL);
        data8_ptr++;
        p_faci_reg->FWBH1 = load_value;
    }

    /** Execute Write command */
    p_faci_reg->FCR = FCR_WRITE;
}

/*******************************************************************************************************************//**
 * @brief   Waits for the write command to be completed and verifies the result of the command execution.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @param[in]  min_program_size   Minimum flash programming size.
 * @retval SSP_SUCCESS            Write command successfully completed.
 * @retval SSP_ERR_IN_USE         Write command still in progress.
 * @retval SSP_ERR_TIMEOUT        Timed out waiting for write command completion.
 * @retval SSP_ERR_WRITE_FAILED   Write failed. Flash could be locked, area has not been erased or
 *                                    address could be under access window control.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_codeflash_write_monitor (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t min_program_size)
{
    ssp_err_t status;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

   /* Worst case timeout */
    gp_flash_settings->wait_cnt = gp_flash_settings->wait_max_write_cf;

    /** Check the Flash Ready Flag bit*/
    if (1U != p_faci_reg->FSTATR1_b.FRDY)
    {
        if (gp_flash_settings->wait_cnt <= (uint32_t)0)
        {
            /* if FRDY is not set to 0 after max timeout, return error*/
            return SSP_ERR_TIMEOUT;
        }
        gp_flash_settings->wait_cnt--;
        return SSP_ERR_IN_USE;
    }

    /** Clear FCR register */
    p_faci_reg->FCR = FCR_CLEAR;

    /* Worst case timeout */
    gp_flash_settings->wait_cnt = gp_flash_settings->wait_max_write_cf;

    /** Wait for the Flash Ready Flag bit to indicate ready or a timeout to occur. If timeout return error. */
    while (0U != p_faci_reg->FSTATR1_b.FRDY)
    {
        /** Check that execute command is completed. */
        /** Wait until FRDY is 1 unless timeout occurs. */
        if (gp_flash_settings->wait_cnt <= (uint32_t)0)
        {
            /** if FRDY is not set to 0 after max timeout, return error*/
            return SSP_ERR_TIMEOUT;
        }
        gp_flash_settings->wait_cnt--;
    }

    /** If invalid command status or programming failed status return error*/
    if ((0U != p_faci_reg->FSTATR2_b.ILGLERR) || (0U != p_faci_reg->FSTATR2_b.PRGERR1))
    {
        HW_FLASH_LP_reset(p_ctrl);
        status = SSP_ERR_WRITE_FAILED;
    }
    else
    {
        /** If there is more data to write then write the next data. */
        g_code_flash_info.start_addr += min_program_size;
        g_code_flash_info.end_addr   += min_program_size;
        g_code_flash_info.write_cnt--;

        if (g_code_flash_info.write_cnt)
        {
            HW_FLASH_LP_codeflash_write_operation(p_ctrl, g_code_flash_info.start_addr, g_code_flash_info.end_addr,
                    min_program_size);
            status = SSP_ERR_IN_USE;
        }
        else
        {
            status = SSP_SUCCESS;
        }
    }

    /** Return status. */
    return status;
}

/*******************************************************************************************************************//**
 * @brief   Initiates the Erase sequence to Erase the number of Code Flash blocks specified by num_blocks, starting with the
 *          Block containing 'address'.
 * @param[in] p_ctrl       Pointer to the Flash control block
 * @param[in] start_addr - The block containing this address is the first block to be erased.
 * @param[in] num_blocks - The # of blocks to be erased.
 * @param[in] block_size - Size of this Flash block.
 * @retval None.
 **********************************************************************************************************************/
void HW_FLASH_LP_codeflash_erase (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t start_addr, const uint32_t num_blocks, uint32_t block_size)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** Configure the start and end addresses. */
    g_code_flash_info.start_addr = start_addr;
    g_code_flash_info.end_addr   = (start_addr + ((num_blocks * block_size) - 1U));

    /** Select User Area */
    p_faci_reg->FASR_b.EXS = 0U;

    /** Start the code flash erase operation. */
    HW_FLASH_LP_codeflash_erase_operation(p_ctrl, start_addr, block_size);
}

/*******************************************************************************************************************//**
 * @brief   Execute a single Erase operation on the Low Power Code Flash data.
 * @param[in] p_ctrl       Pointer to the Flash control block
 * @param[in] start_addr   Starting Code Flash address to erase.
 * @param[in] block_size - Size of this Flash block.
 * @retval none.
 **********************************************************************************************************************/
void HW_FLASH_LP_codeflash_erase_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t start_addr, uint32_t block_size)
{
    uint32_t block_start_addr;
    uint32_t block_end_addr;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    block_start_addr = start_addr;
    block_end_addr   = (block_start_addr + (block_size - 1U));

    /* Erase start address setting */
    p_faci_reg->FSARH = (uint16_t) ((block_start_addr >> 16) & 0xFFFFU);
    p_faci_reg->FSARL = (uint16_t) (block_start_addr & 0xFFFFU);

    /* Erase end address setting */
    p_faci_reg->FEARH = ((block_end_addr >> 16) & 0xFFFFU);
    p_faci_reg->FEARL = (uint16_t) (block_end_addr & 0xFFFFU);

    /* Execute Erase command */
    p_faci_reg->FCR = FCR_ERASE;
}

/*******************************************************************************************************************//**
 * @brief   Waits for the erase command to be completed and verifies the result of the command execution.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @param[in]  block_size - Size of this Flash block.
 * @retval SSP_SUCCESS      Erase command successfully completed.
 * @retval SSP_ERR_IN_USE   Erase command still in progress.
 * @retval SSP_ERR_TIMEOUT  Timed out waiting for erase command completion.
 * @retval SSP_ERR_WRITE_FAILED  Erase failed. Flash could be locked or address could be under access window
 * control.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_codeflash_erase_monitor (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t block_size)
{
    ssp_err_t status;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** If the flash operation has not completed return error. */
    if (1U != p_faci_reg->FSTATR1_b.FRDY)
    {
        return SSP_ERR_IN_USE;
    }

    /** Clear the Flash Control Register */
    p_faci_reg->FCR = FCR_CLEAR;

    /** Prepare worst case timeout */
    gp_flash_settings->wait_cnt = gp_flash_settings->wait_max_erase_cf_block;

    /** Wait until the ready flag is set or timeout. If timeout return error */
    while (0U != p_faci_reg->FSTATR1_b.FRDY)
    {
        /** Check that execute command is completed. */
        /** Wait until FRDY is 1 unless timeout occurs. */
        if (gp_flash_settings->wait_cnt <= (uint32_t)0)
        {
            /* if FRDY is not set to 0 after max timeout, return error*/
            return SSP_ERR_TIMEOUT;
        }
        gp_flash_settings->wait_cnt--;
    }

    /** If invalid command or erase error flag is set reset and return error. */
    if ((0U != p_faci_reg->FSTATR2_b.ILGLERR) || (0U != p_faci_reg->FSTATR2_b.ERERR))
    {
        HW_FLASH_LP_reset(p_ctrl);
        return SSP_ERR_WRITE_FAILED;
    }
    else
    {
        /** If there are more blocks to erase initiate another erase operation. Otherwise return success. */
        g_code_flash_info.start_addr += block_size;

        /* Check for CF0 overflow. */
        if ((g_code_flash_info.start_addr < g_code_flash_info.end_addr) && (g_code_flash_info.start_addr != 0U))
        {
            HW_FLASH_LP_codeflash_erase_operation(p_ctrl, g_code_flash_info.start_addr, block_size);
            status = SSP_ERR_IN_USE;
        }
        else
        {
            status = SSP_SUCCESS;
        }
    }

    return status;
}

/*******************************************************************************************************************//**
 * @brief   Initiates a Blank check sequence to the Low Power Code Flash data. Address validation has already been
 *          performed by the caller.
 * @param[in] p_ctrl       Pointer to the Flash control block
 * @param[in] start_addr   Start address of the Code Flash area to blank check.
 * @param[in] num_bytes    Number of bytes to blank check beginning at start_addr.
 *
 * @retval SSP_SUCCESS             Blank check command successfully issued to FACI controller.
 * @retval SSP_ERR_INVALID_ADDRESS The address and num_bytes supplied do not exist in any of the Code Flash macros.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_codeflash_blank_check (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t start_addr, uint32_t num_bytes)
{
    ssp_err_t status = SSP_SUCCESS;

    gp_flash_settings->total_count   = num_bytes;  /*  Total Number of bytes to blank check */
    gp_flash_settings->dest_addr     = start_addr;

    /** Get the macro information for this address. If failure skip blank check and return error. */
    status = HW_FLASH_LP_code_get_macro_info(start_addr, num_bytes, &macro_info);

    gp_flash_settings->current_count = macro_info.allowed_bytes;

    /** If successful initiate the code flash blank check operation. */
    if (status == SSP_SUCCESS)
    {
        HW_FLASH_LP_codeflash_blank_check_operation(p_ctrl, start_addr, macro_info.allowed_bytes);
    }

    /** Return status */
    return(status);
}

/*******************************************************************************************************************//**
 * @brief   Initiates a Blank check sequence to the Low Power Code Flash data. Address validation has already been
 *          performed by the caller.
 * @param[in] p_ctrl       Pointer to the Flash control block
 * @param[in] start_addr   Start address of the Code Flash area to blank check.
 * @param[in] num_bytes    Number of bytes to blank check beginning at start_addr.
 *
 * @retval none.
 **********************************************************************************************************************/
void HW_FLASH_LP_codeflash_blank_check_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t start_addr, uint32_t num_bytes)
{
    uint32_t start_addr_idx;
    uint32_t end_addr_idx;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    start_addr_idx = start_addr;
    end_addr_idx   = start_addr_idx + (num_bytes - 1U);

    /** Select User Area */
    p_faci_reg->FASR_b.EXS = 0U;

    /** BlankCheck start address setting */
    p_faci_reg->FSARH = (uint16_t) ((start_addr_idx >> 16) & 0xFFFFU);
    p_faci_reg->FSARL = (uint16_t) (start_addr_idx & 0xFFFFU);

    /** BlankCheck end address setting */
    p_faci_reg->FEARH = ((end_addr_idx >> 16) & 0xFFFFU);
    p_faci_reg->FEARL = (uint16_t) (end_addr_idx & 0xFFFFU);

    /** Execute BlankCheck command */
    p_faci_reg->FCR = FCR_BLANKCHECK;
}

/*******************************************************************************************************************//**
 * @brief   Waits for the blank check command to be completed and verifies the result of the command execution.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @retval SSP_SUCCESS            Blank check command successfully completed.
 * @retval SSP_ERR_IN_USE         Blank check command still in progress.
 * @retval SSP_ERR_TIMEOUT       Timed out waiting for Blank check command completion.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_codeflash_blank_check_monitor (flash_lp_instance_ctrl_t * const p_ctrl)
{
    ssp_err_t status = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;
    gp_flash_settings->bc_result = FLASH_RESULT_BLANK;

    /** If the flash has not completed the software command return error. */
    if (1U != p_faci_reg->FSTATR1_b.FRDY)
    {
        return SSP_ERR_IN_USE;
    }

    /** Clear the Flash Control Register */
    p_faci_reg->FCR = FCR_CLEAR;

    /** Prepare worst case timeout counter */
    gp_flash_settings->wait_cnt = gp_flash_settings->wait_max_blank_check;

    while (0U != p_faci_reg->FSTATR1_b.FRDY)
    {
        /** Check that execute command is completed. */
        /** Wait until FRDY is 1 unless timeout occurs. If timeout return error. */
        if (gp_flash_settings->wait_cnt <= 0U)
        {
            /** if FRDY is not set to 0 after max timeout, return error*/
            return SSP_ERR_TIMEOUT;
        }
        gp_flash_settings->wait_cnt--;
    }

    /** If illegal command, reset and return error. */
    if (0U != p_faci_reg->FSTATR2_b.ILGLERR)
    {
        HW_FLASH_LP_reset(p_ctrl);
        status = SSP_ERR_WRITE_FAILED;
    }
    else
    {
        if (0U != p_faci_reg->FSTATR00_b.BCERR0)           // Tested Flash Area is not blank
        {
            /** If the result is already NOT Blank there is no reason to continue with any subsequent checks, simply return */
            gp_flash_settings->bc_result = FLASH_RESULT_NOT_BLANK;
            status = SSP_SUCCESS;
        }
        else
        {
            /* Only S3A7 (2 CF macros) Code Flash blank checking is concerned with hardware macros and spanning boundaries. */
            /** If the flash has more than one code flash macro */
            if (gp_flash_settings->flash_cf_macros > 1U)
            {
                /** If there is more data to blank check adjust the operation settings. */
                gp_flash_settings->total_count -= gp_flash_settings->current_count;  /*  Update remaining amount to BC. */
                gp_flash_settings->dest_addr += gp_flash_settings->current_count;

                /** Get the macro information for this address */
                status = HW_FLASH_LP_code_get_macro_info(gp_flash_settings->dest_addr, gp_flash_settings->total_count, &macro_info);
                gp_flash_settings->current_count = macro_info.allowed_bytes;

                /** If the original request was split as it spanms flash hardware macros */
                if (gp_flash_settings->total_count != 0U)
                {
                    /** Begin next Blankcheck operation. */
                    HW_FLASH_LP_codeflash_blank_check_operation(p_ctrl, gp_flash_settings->dest_addr, macro_info.allowed_bytes);
                    status = SSP_ERR_IN_USE;
                }
                else
                {
                    status = SSP_SUCCESS;
                }
            }
        }
    }

    /** Return status. */
    return status;
}

/*******************************************************************************************************************//**
 * @brief   Sets the FPMCR register, used to place the Flash sequencer in Code Flash P/E mode.
 * @param[in] p_ctrl  Pointer to the Flash control block
 * @param[in] value - 8 bit value to be written to the FPMCR register.
 * @retval none.
 **********************************************************************************************************************/
static void HW_FLASH_LP_codeflash_write_fpmcr (flash_lp_instance_ctrl_t * const p_ctrl, uint8_t value)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    p_faci_reg->FPR   = 0xA5U;

    p_faci_reg->FPMCR = value;
    p_faci_reg->FPMCR = (uint8_t) ~value;
    p_faci_reg->FPMCR = value;

    if (value == p_faci_reg->FPMCR)
    {
        __NOP();
    }
}

/*******************************************************************************
 * Outline      : Give the Code Flash HW layer access to the flash settings
 * Header       : none
 * Function Name: set_flash_settings
 * Description  : Give the HW layer access to the flash settings
 * @param[in] p_current_parameters - Pointer the settings.
 *             :
 * Return Value : none
 *******************************************************************************/
void HW_FLASH_LP_code_flash_set_flash_settings (current_parameters_t * const p_current_parameters)
{
    gp_flash_settings = p_current_parameters;
}


/*******************************************************************************************************************//**
 * @brief   Given an address and number of bytes, determine how many of those bytes are in a single Flash macro.
 * @param[in]  address      Address to query
 * @param[in]  num_bytes    Number of byte starting at Address
 * @param[in]  p_macro      Pointer to the Flash macro information
 * @retval SSP_ERR_INVALID_ADDRESS The address and num_bytes supplied do not exist in any of the Code or Data Flash macros.
 * @retval SSP_SUCCESS             Successful query. p_macro contains details.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_code_get_macro_info (uint32_t const address, uint32_t num_bytes, flash_lp_macro_info_t * const p_macro)
{
    ssp_err_t err;

    p_macro->allowed_bytes = num_bytes;
    p_macro->macro_st_addr = address;

    /* Only S3A7 (2 CF macros) Code Flash blank checking is concerned with hardware macros and spanning boundaries. */
    /** If the MCU has more than one code flash macro determine the macro the requested address is in. */
    if (gp_flash_settings->flash_cf_macros > 1U)
    {
        err = SSP_ERR_INVALID_ADDRESS;
        p_macro->macro_size = gp_flash_settings->cf_macro_size;   // The size of the implemented Code Flash macro
        p_macro->total_macros = gp_flash_settings->flash_cf_macros;
        p_macro->macro_st_addr = gp_flash_settings->cf_memory_st_addr;
        p_macro->macro_end_addr = (p_macro->macro_st_addr + p_macro->macro_size) - 1U;

        for (uint32_t i=0U; i<p_macro->total_macros; i++)
        {
            /* Is the address in the range of this macro? */
            if ((address >= p_macro->macro_st_addr) && (address <= p_macro->macro_end_addr))
            {
                /* Yes, how many bytes can be accommodated out of the request? */
                if (((p_macro->macro_end_addr - address) + 1U) < num_bytes)
                {
                    p_macro->allowed_bytes = (p_macro->macro_end_addr - address) + 1U;
                }
                err = SSP_SUCCESS;
                break;
            }
            else
            {
                p_macro->macro_st_addr = p_macro->macro_end_addr + 1U;
                p_macro->macro_end_addr = (p_macro->macro_st_addr + p_macro->macro_size) - 1U;
            }
        }
    }
    else
    {
        err = SSP_SUCCESS;
    }
    return err;
}
