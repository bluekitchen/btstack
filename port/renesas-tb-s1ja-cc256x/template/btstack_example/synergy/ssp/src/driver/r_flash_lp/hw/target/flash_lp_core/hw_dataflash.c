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
 * File Name    : hw_dataflash.c
 * Description  : Data Flash Control processing for Low Power Flash
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

/*******************************************************************************************************************//**
 * @addtogroup FLASH
 * @{
 **********************************************************************************************************************/
/*******************************************************************************************************************//**
 * @} (end FLASH)
 **********************************************************************************************************************/

/******************************************************************************
 * Macro definitions
 ******************************************************************************/

/******************************************************************************
 * Private global variables and functions
 ******************************************************************************/
static current_parameters_t * gp_flash_settings = NULL;

static void  HW_FLASH_LP_write_fpmcr (flash_lp_instance_ctrl_t * const p_ctrl, uint8_t value);

/*******************************************************************************************************************//**
 * @brief   Enable Data Flash Access and wait for the Data Flash STOP recovery time.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @retval     none
 **********************************************************************************************************************/
void HW_FLASH_LP_dataflash_enable (flash_lp_instance_ctrl_t * const p_ctrl)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;
    /** Data Flash Access enable */
    p_faci_reg->DFLCTL = 1U;

    if (1U == p_faci_reg->DFLCTL)
    {
        __NOP();
    }

    /** Wait for 5us over (tDSTOP) */
    HW_FLASH_LP_delay_us(WAIT_TDSTOP, gp_flash_settings->system_clock_freq);

}

/*******************************************************************************************************************//**
 * @brief   Transition to Data Flash P/E mode.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @retval     none
 **********************************************************************************************************************/
void HW_FLASH_LP_dataflash_enter_pe_mode (flash_lp_instance_ctrl_t * const p_ctrl)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

	p_faci_reg->FENTRYR = FENTRYR_DATAFLASH_PE_MODE;

    /** If BGO mode is enabled for dataflash and the vector is valid enable the vector. */
    if (gp_flash_settings->bgo_enabled_df == true)
    {
        if (SSP_INVALID_VECTOR != p_ctrl->irq)
        {
            NVIC_EnableIRQ(p_ctrl->irq);           // We are supporting Flash Rdy interrupts for Data Flash operations
        }
    }

    HW_FLASH_LP_delay_us(WAIT_TDSTOP, gp_flash_settings->system_clock_freq);

    if (R_SYSTEM->OPCCR_b.OPCM == 0U)                               ///< High speed mode?
    {
        HW_FLASH_LP_write_fpmcr(p_ctrl, DATAFLASH_PE_MODE);
    }
    else
    {
        HW_FLASH_LP_write_fpmcr(p_ctrl, (uint8_t)DATAFLASH_PE_MODE | (uint8_t)LVPE_MODE);
    }

    p_faci_reg->FISR_b.PCKA = (gp_flash_settings->flash_clock_freq - 1U) & (uint32_t)0x1F;
}

/*******************************************************************************************************************//**
 * @brief   This function switches the peripheral from P/E mode for Data Flash to Read mode.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @retval SSP_SUCCESS              Successfully entered read mode.
 * @retval SSP_ERR_TIMEOUT    Timed out waiting for confirmation of transition to read mode
 *
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_dataflash_enter_read_mode (flash_lp_instance_ctrl_t * const p_ctrl)
{
    ssp_err_t         err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /* Timeout counter. */
    volatile uint32_t wait_cnt = FLASH_FRDY_CMD_TIMEOUT;

    /** Set the Flash P/E Mode Control Register to Read mode. */
    HW_FLASH_LP_write_fpmcr(p_ctrl, READ_MODE);

    /** Wait for 5us over (tMS) */
    HW_FLASH_LP_delay_us(WAIT_TMS_HIGH, gp_flash_settings->system_clock_freq);

    /** Clear the flash P/E mode entry register */
    p_faci_reg->FENTRYR = FENTRYR_READ_MODE;

    /** Loop until the Flash P/E mode entry register is cleared or a timeout occurs. If timeout occurs return error. */
    while (0x0000U != p_faci_reg->FENTRYR)
    {
        /* Confirm that the written value can be read correctly. */
        if (wait_cnt <= (uint32_t)0)
        {
            /* return timeout status*/
            return SSP_ERR_TIMEOUT;
        }
        wait_cnt--;
    }

    return err;
}

/*******************************************************************************************************************//**
 * @brief   Initiates a Write sequence to the Low Power Data Flash data. Address validation has already been
 *          performed by the caller.
 * @param[in] p_ctrl              Pointer to the Flash control block
 * @param[in] src_start_address   Start address of the (RAM) area which stores the programming data.
 * @param[in] dest_start_address  Flash Start address which will be written.
 * @param[in] num_bytes           Number of bytes to write.
 * @param[in] min_program_size    The minimum Flash programming size.
 * @retval none.
 **********************************************************************************************************************/
void HW_FLASH_LP_dataflash_write (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const src_start_address,
                                  uint32_t dest_start_address,
                                  uint32_t num_bytes,
                                  uint32_t min_program_size)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;
    uint32_t temp = min_program_size;
    uint32_t right_shift = 0U;
    while (1U != temp)
    {
        temp >>= 1;
        right_shift++;
    }
    /** Calculate the number of writes. Each write is of length min_program_size. */
    /** Configure the flash settings. */
    /* This is done with right shift instead of division to avoid using the division library, which would be in flash
     * and cause a jump from RAM to flash. */
    gp_flash_settings->total_count   = num_bytes >> right_shift;  /*  Number of bytes to write */
    gp_flash_settings->dest_addr     = dest_start_address;
    gp_flash_settings->src_addr      = src_start_address;
    gp_flash_settings->current_count = (uint32_t)0;

    p_faci_reg->FASR_b.EXS               = 0U;

    /** Initiate the data flash write operation. */
    HW_FLASH_LP_dataflash_write_operation(p_ctrl, src_start_address, dest_start_address);
}

/*******************************************************************************************************************//**
 * @brief   Execute a single Write operation on the Low Power Data Flash data.
 * @param[in]  p_ctrl      Pointer to the Flash control block
 * @param[in] psrc_addr    Source address for data to be written.
 * @param[in] dest_addr    End address (read form) for writing.
 * @retval none.
 **********************************************************************************************************************/
void HW_FLASH_LP_dataflash_write_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t psrc_addr,  uint32_t dest_addr)
{
    uint32_t dest_addr_idx;
    uint8_t * data8_ptr;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;
    data8_ptr = (uint8_t *)psrc_addr;

    dest_addr_idx = dest_addr + DATAFLASH_ADDR_OFFSET;  /* Conversion to the P/E address from the read address */

    /** Write flash address setting */
    p_faci_reg->FSARH = (uint16_t) ((uint32_t)(dest_addr_idx >> 16) & 0xFFFF);
    p_faci_reg->FSARL = (uint16_t) (dest_addr_idx & 0xFFFF);

    /** Write data address setting */
    p_faci_reg->FWBL0 = *data8_ptr;                    // For data flash there are only 8 bits used of the 16 in the reg

    /** Execute Write command */
    p_faci_reg->FCR = FCR_WRITE;
}

/*******************************************************************************************************************//**
 * @brief   Waits for the write command to be completed and verifies the result of the command execution.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @param[in]  min_program_size    The minimum Flash programming size.
 * @retval SSP_SUCCESS      Write command successfully completed.
 * @retval SSP_ERR_IN_USE   Write command still in progress.
 * @retval SSP_ERR_TIMEOUT  Timed out waiting for write command completion.
 * @retval SSP_ERR_WRITE_FAILED  Write failed. Flash could be locked, area has not been erased,  or address
 *                                    could be under access window control.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_dataflash_write_monitor (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t min_program_size)
{
    ssp_err_t status;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;


    /** If the flash has not completed the software command return error. */
    if (1U != p_faci_reg->FSTATR1_b.FRDY)
    {
        return SSP_ERR_IN_USE;
    }

    /** Clear the Flash Control Register */
    p_faci_reg->FCR = FCR_CLEAR;

    /** Prepare worst case timeout counter */
    gp_flash_settings->wait_cnt = gp_flash_settings->wait_max_write_df;

    while (0U != p_faci_reg->FSTATR1_b.FRDY)
    {
        /** Check that execute command is completed. */
        /** Wait until FRDY is 1 unless timeout occurs. If timeout return error. */
        if (gp_flash_settings->wait_cnt <= (uint32_t)0)
        {
            /* if FRDY is not set to 0 after max timeout, return error*/
            return SSP_ERR_TIMEOUT;
        }
        gp_flash_settings->wait_cnt--;
    }

    /** If illegal command or programming error reset and return error. */
    if ((0U != p_faci_reg->FSTATR2_b.ILGLERR) || (0U != p_faci_reg->FSTATR2_b.PRGERR1))
    {
        HW_FLASH_LP_reset(p_ctrl);
        status = SSP_ERR_WRITE_FAILED;
    }
    else
    {
        /** If there are more blocks to write initiate another write operation. If failure return error. */
        gp_flash_settings->src_addr  += min_program_size;
        gp_flash_settings->dest_addr += min_program_size;
        gp_flash_settings->total_count--;

        if (gp_flash_settings->total_count)
        {
            HW_FLASH_LP_dataflash_write_operation(p_ctrl, gp_flash_settings->src_addr, gp_flash_settings->dest_addr);
            status = SSP_ERR_IN_USE;
        }
        else
        {
            status = SSP_SUCCESS;
        }
    }

    /** Return status */
    return status;
}

/*******************************************************************************************************************//**
 * @brief   Initiates the Erase sequence to Erase the # of Data Flash blocks specified by num_blocks, starting with the
 *          Block containing 'address'.
 * @param[in] p_ctrl       Pointer to the Flash control block
 * @param[in] start_addr - The block containing this address is the first block to be erased.
 * @param[in] num_blocks - The # of blocks to be erased.
 * @param[in] block_size - The block size for this Flash block.
 * @retval None.
 **********************************************************************************************************************/
void HW_FLASH_LP_dataflash_erase (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t start_addr, uint32_t num_blocks, uint32_t block_size)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** Save the current operation parameters. */
    gp_flash_settings->dest_addr     = start_addr;
    gp_flash_settings->total_count   = num_blocks;
    gp_flash_settings->wait_cnt      = gp_flash_settings->wait_max_erase_df_block;
    gp_flash_settings->current_count = 1U;       // Include the one we are doing right here

    p_faci_reg->FASR_b.EXS               = 0U;

    /** Call the data flash erase operation. */
    HW_FLASH_LP_dataflash_erase_operation(p_ctrl, gp_flash_settings->dest_addr, block_size);
}

/*******************************************************************************************************************//**
 * @brief   Execute a single Erase operation on the Low Power Data Flash data.
 * @param[in] p_ctrl       Pointer to the Flash control block
 * @param[in] start_addr  Starting Code Flash address to erase.
 * @param[in] block_size - The block size for this Flash block.
 * @retval none.
 **********************************************************************************************************************/
void HW_FLASH_LP_dataflash_erase_operation (flash_lp_instance_ctrl_t * const p_ctrl, const uint32_t start_addr, uint32_t block_size)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    uint32_t block_start_addr;
    uint32_t block_end_addr;

    block_start_addr = start_addr + (uint32_t)DATAFLASH_ADDR_OFFSET;  /* Conversion to the P/E address from the read address */
    block_end_addr   = (block_start_addr + (block_size - 1U));

    /** Set the erase start address. */
    p_faci_reg->FSARH = (uint16_t) ((block_start_addr >> 16) & 0xFFFFU);
    p_faci_reg->FSARL = (uint16_t) (block_start_addr & 0xFFFFU);

    /** Set the erase end address. */
    p_faci_reg->FEARH = ((block_end_addr >> 16) & 0xFFFFU);
    p_faci_reg->FEARL = (uint16_t) (block_end_addr & 0xFFFFU);

    /** Execute the erase command. */
    p_faci_reg->FCR = FCR_ERASE;
}

/*******************************************************************************************************************//**
 * @brief   Waits for the erase command to be completed and verifies the result of the command execution.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @param[in] block_size -  The block size for this Flash block.
 * @retval SSP_SUCCESS            Erase command successfully completed.
 * @retval SSP_ERR_IN_USE         Erase command still in progress.
 * @retval SSP_ERR_TIMEOUT        Timed out waiting for erase command completion.
 * @retval SSP_ERR_WRITE_FAILED  Erase failed. Flash could be locked or address could be under access window
 * control.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_dataflash_erase_monitor (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t block_size)
{
    ssp_err_t status;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** If the flash has not completed the software command return error. */
    if (1U != p_faci_reg->FSTATR1_b.FRDY)
    {
        return SSP_ERR_IN_USE;
    }

    /** Clear Flash Control Register. */
    p_faci_reg->FCR = FCR_CLEAR;

    /** Prepare worst case timeout counter */
    gp_flash_settings->wait_cnt = gp_flash_settings->wait_max_erase_df_block;

    /** Wait until the ready flag is set or timeout. If timeout return error */
    while (0U != p_faci_reg->FSTATR1_b.FRDY)
    {
        /* Check that execute command is completed. */
        /* Wait until FRDY is 1 unless timeout occurs. If timeout return error. */
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
        status = SSP_ERR_WRITE_FAILED;
    }
    else
    {
        /** If there are more blocks to erase initiate another erase operation. Otherwise return success. */
        gp_flash_settings->dest_addr += block_size;

        if (gp_flash_settings->current_count < gp_flash_settings->total_count)
        {
            HW_FLASH_LP_dataflash_erase_operation(p_ctrl, gp_flash_settings->dest_addr, block_size);
            status = SSP_ERR_IN_USE;
        }
        else
        {
            status = SSP_SUCCESS;
        }
        gp_flash_settings->current_count++;
    }

    return status;
}

/*******************************************************************************************************************//**
 * @brief   Initiates a Blank check sequence to the Low Power Data Flash data. Address validation has already been
 *          performed by the caller.
 * @param[in] p_ctrl       Pointer to the Flash control block
 * @param[in] start_addr   Start address of the Data Flash area to blank check.
 * @param[in] end_addr     End address of the Data flash area to blank check This address is included in the blank
 * check.
 * @retval none.
 **********************************************************************************************************************/
void HW_FLASH_LP_dataflash_blank_check (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t start_addr, uint32_t end_addr)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;
    uint32_t start_addr_idx;
    uint32_t end_addr_idx;

    start_addr_idx     = start_addr + DATAFLASH_ADDR_OFFSET; /* Conversion to the P/E address from the read address */
    end_addr_idx       = end_addr + DATAFLASH_ADDR_OFFSET;   /* Conversion to the P/E address from the read address */

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
 * @param[in]  p_ctrl             Pointer to the Flash control block
 * @retval SSP_SUCCESS            Blank check command successfully completed.
 * @retval SSP_ERR_IN_USE         Blank check command still in progress.
 * @retval SSP_ERR_TIMEOUT        Timed out waiting for Blank check command completion.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_dataflash_blank_check_monitor (flash_lp_instance_ctrl_t * const p_ctrl)
{
    ssp_err_t err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

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
        if (gp_flash_settings->wait_cnt <= (uint32_t)0)
        {
            /* if FRDY is not set to 0 after max timeout, return error*/
            return SSP_ERR_TIMEOUT;
        }
        gp_flash_settings->wait_cnt--;
    }

    /** If illegal command reset and return error. */
    if (0U != p_faci_reg->FSTATR2_b.ILGLERR)
    {
        HW_FLASH_LP_reset(p_ctrl);
        err = SSP_ERR_WRITE_FAILED;
    }

    /** Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * @brief   Sets the FPMCR register, used to place the Flash sequencer in Data Flash P/E mode.
 * @param[in] p_ctrl       Pointer to the Flash control block
 * @param[in] value - 8 bit value to be written to the FPMCR register.
 * @retval none.
 **********************************************************************************************************************/
static void HW_FLASH_LP_write_fpmcr (flash_lp_instance_ctrl_t * const p_ctrl, uint8_t value)
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
 * Outline      : Give the Data Flash HW layer access to the flash settings
 * Header       : none
 * Function Name: set_flash_settings
 * Description  : Give the Data Flash HW layer access to the flash settings
 * Arguments    : current_parameters_t *p_current_parameters - Pointer the settings.
 *             :
 * Return Value : none
 *******************************************************************************/
void HW_FLASH_LP_data_flash_set_flash_settings (current_parameters_t * const p_current_parameters)
{
    gp_flash_settings = p_current_parameters;
}
