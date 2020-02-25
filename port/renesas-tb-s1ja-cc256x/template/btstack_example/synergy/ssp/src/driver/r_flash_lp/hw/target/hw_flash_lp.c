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
 * File Name    : hw_flash_lp.c
 * Description  : LLD Interface for the Low power Flash peripheral on SC32 MCUs.
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"

#include "r_flash_lp.h"
#include "hw_flash_lp_private.h"
#include "r_flash_cfg.h"
#include "r_cgc_api.h"
#include "r_cgc.h"
#include "flash_lp_core/hw_dataflash.h"
#include "flash_lp_core/hw_codeflash.h"
#include "flash_lp_core/hw_codeflash_extra.h"

/*******************************************************************************************************************//**
 * @addtogroup FLASH
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @} (end FLASH)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/* The number of CPU cycles per each timeout loop. */
#ifndef R_FLASH_LP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP
#if defined(__GNUC__)
#define R_FLASH_LP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP (6U)
#elif defined(__ICCARM__)
#define R_FLASH_LP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP (6U)
#endif
#endif

#define R_FLASH_LP_HZ_IN_MHZ               (1000000U)

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
static current_parameters_t * gp_flash_parameters = NULL; // passed in via flash_init()

static inline bool HW_FLASH_LP_frdyi_df_bgo_write (flash_lp_instance_ctrl_t *p_ctrl, flash_callback_args_t *p_cb_data);
static inline bool HW_FLASH_LP_frdyi_df_bgo_erase (flash_lp_instance_ctrl_t *p_ctrl, flash_callback_args_t *p_cb_data);
static inline bool HW_FLASH_LP_frdyi_df_bgo_blankcheck (flash_lp_instance_ctrl_t *p_ctrl, flash_callback_args_t *p_cb_data);

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief   This function enables or disables the Flash interrupt. The flash interrupt, if enabled, is called when a
 *          Flash operation completes, or when an Error condition is detected. If the caller has provided a callback
 *          function as part of the provided p_cfg pointer, then that function will be called as a result of the
 *          interrupt.
 * @param[in]  state        Control for the FLASH device context.
 * @param[in]  p_cfg        Pointer to the FLASH configuration structure.
 * @param[in]  irq          Flash interrupt irq number.
 * @retval SSP_SUCCESS              Successful.
 * @retval SSP_ERR_IRQ_BSP_DISABLED Caller is requesting BGO but the Flash interrupts are not enabled.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_irq_cfg (bool state, flash_cfg_t const * const p_cfg, IRQn_Type irq)
{
    SSP_PARAMETER_NOT_USED(state);

    /** If BGO is being used then we require the Flash Rdy interrupt be enabled. If it
     *  is not enabled then we are not going to be doing BGO and we won't generate an ISR routine */
    if (SSP_INVALID_VECTOR != irq)
    {
        /** Enable the Interrupt */
        if (true == state)
        {
            /** Enable FCU interrupts if callback is not null, clear the Interrupt Request bit */
            NVIC_ClearPendingIRQ(irq);
            R_BSP_IrqStatusClear(irq);
            NVIC_EnableIRQ(irq);
        }
        /** Disable the Interrupts */
        else
        {
            /** Disable interrupt in ICU*/
            NVIC_DisableIRQ(irq);

            /** Clear the Interrupt Request bit */
            R_BSP_IrqStatusClear(irq);
        }

        return SSP_SUCCESS;
    }
    else
    {
        /** The Flash Interrupt has not been enabled. If the caller is requesting BGO then we'll flag this as an error. */
        if (p_cfg->data_flash_bgo == true)
        {
            return SSP_ERR_IRQ_BSP_DISABLED;
        }
        else
        {
            return SSP_SUCCESS;
        }
    }
}

/*******************************************************************************************************************//**
 * @brief   This function will initialize the FCU and set the FCU Clock based on the current FCLK frequency.
 * @param[in]       p_ctrl                      Pointer to the Flash control block
 * @param[in, out]  p_current_parameters        Pointer to the g_current_parameters structure created by the HLD layer.
 **********************************************************************************************************************/
void HW_FLASH_LP_init (flash_lp_instance_ctrl_t * const p_ctrl, current_parameters_t * p_current_parameters)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    gp_flash_parameters                    = p_current_parameters; // our copy from now on

    gp_flash_parameters->current_operation = FLASH_OPERATION_FCU_INIT;

    /* Round up the frequency to a whole number */
    uint32_t system_clock_freq_mhz = (gp_flash_parameters->system_clock_freq + (R_FLASH_LP_HZ_IN_MHZ - 1)) / R_FLASH_LP_HZ_IN_MHZ;

    gp_flash_parameters->flash_clock_freq  = gp_flash_parameters->flash_clock_freq / 1000000;  // Scale it down
    gp_flash_parameters->system_clock_freq = gp_flash_parameters->system_clock_freq / 1000000; // Scale it down

    /*  According to HW Manual the Max Programming Time for 4 bytes(S124/S128) or 8 bytes(S1JA/S3A*)(ROM)
     *  is 1411us.  This is with a FCLK of 1MHz. The calculation below
     *  calculates the number of ICLK ticks needed for the timeout delay.
     */
    gp_flash_parameters->wait_max_write_cf = (uint32_t) (1411 * system_clock_freq_mhz)/R_FLASH_LP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the Max Programming Time for 1 byte
     *  (Data Flash) is 886us.  This is with a FCLK of 4MHz. The calculation
     *  below calculates the number of ICLK ticks needed for the timeout delay.
     */
    gp_flash_parameters->wait_max_write_df = (uint32_t) (886 * system_clock_freq_mhz)/R_FLASH_LP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the Max Blank Check time for 2 bytes (S12*) or 8 bytes (S1JA/S3A*)
     *  (Code Flash) is 87.7 usec.  This is with a FCLK of 1MHz. The calculation
     *  below calculates the number of ICLK ticks needed for the timeout delay.
     */
    gp_flash_parameters->wait_max_blank_check = (uint32_t) (88 * system_clock_freq_mhz)/R_FLASH_LP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the Max Erasure Time for a 1KB block (S12*) or 2KB bytes (S1JA/S3A*) is
     *  around 289ms.  This is with a FCLK of 1MHz. The calculation below
     *  calculates the number of ICLK ticks needed for the timeout delay.
     */
    gp_flash_parameters->wait_max_erase_cf_block = (uint32_t) (289000 * system_clock_freq_mhz)/R_FLASH_LP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the Max Erasure Time for a 1KB Data Flash block is
     *  around 299ms.  This is with a FCLK of 4MHz. The calculation below
     *  calculates the number of ICLK ticks needed for the timeout delay.
     */
    gp_flash_parameters->wait_max_erase_df_block = (uint32_t) (299000 * system_clock_freq_mhz)/R_FLASH_LP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /** Per the Flash spec, update the FLWAIT register if FCLK is being changed */
    p_faci_reg->FLWAITR = 0U;
}

/*******************************************************************************************************************//**
 * @brief   This function erases a specified number of Code or Data Flash blocks
 * @param[in]   p_ctrl          Pointer to the Flash control block
 * @param[in]   block_address   The starting address of the first block to erase.
 * @param[in]   num_blocks      The number of blocks to erase.
 * @param[in]   block_size      The Flash block size.
 * @retval SSP_SUCCESS            Successfully erased (non-BGO) mode or operation successfully started (BGO).
 * @retval SSP_ERR_ERASE_FAILED    Status is indicating a Erase error.
 * @retval SSP_ERR_CMD_LOCKED   FCU is in locked state, typically as a result of having received an illegal
 *                                    command.
 * @retval SSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation.
 * @retval SSP_ERR_TIMEOUT Timed out waiting for the FCU to become ready.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_erase (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t block_address, uint32_t num_blocks, uint32_t block_size)
{
    ssp_err_t err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    gp_flash_parameters->block_size = block_size;

    /* We're already in the appropriate P/E mode from the caller */
    /** If the module is configured for Data Flash P/E mode */
    if (p_faci_reg->FENTRYR == 0x0080U)
    {
        /** Erase a block of data flash. */
        HW_FLASH_LP_dataflash_erase(p_ctrl, block_address, num_blocks, block_size);  // Set the start and end and do first block

        /** If configured for Blocking mode then don't return until the entire operation is complete */
        if (gp_flash_parameters->bgo_enabled_df == false)
        {
            do
            {
                /** Waits for the erase commands to be completed and verifies the result of the command execution. */
                err = HW_FLASH_LP_dataflash_erase_monitor(p_ctrl, block_size);
            } while (SSP_ERR_IN_USE == err);
        }
    }

#if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 1)
    /** Else if the module is configured for code flash P/E mode */
    else if (p_faci_reg->FENTRYR == 0x0001U)
    {
        /** Erase a block of code flash */
        HW_FLASH_LP_codeflash_erase(p_ctrl, block_address, num_blocks, block_size); // Set the start and end and do first block

        /* Code Flash is always blocking mode */
        do
        {
            /** Waits for the erase commands to be completed and verifies the result of the command execution. */
            err = HW_FLASH_LP_codeflash_erase_monitor(p_ctrl, block_size);
        } while (SSP_ERR_IN_USE == err);
    }
#endif /* if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 1) */
    else
    {
        /** Otherwise the module is not configured for data or code flash P/E mode. return failure */
        /* should never get here */
        return SSP_ERR_ERASE_FAILED;
    }

    return err;
}

/*******************************************************************************************************************//**
 * @brief   This function writes a specified number of bytes to Code or Data Flash.
 * @param[in]   p_ctrl              Pointer to the Flash control block
 * @param[in]   src_start_address   The starting address of the first byte (from source) to write.
 * @param[in]   dest_start_address  The starting address of the Flash (to destination) to write.
 * @param[in]   num_bytes           The number of bytes to write.
 * @param[in]   min_program_size    The minimum Flash programming size.
 *
 * @retval SSP_SUCCESS            Data successfully written (non-BGO) mode or operation successfully started (BGO).
 * @retval SSP_ERR_IN_USE     Command still executing.
 * @retval SSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation. This
 *                                    may be returned if the requested Flash area is not blank.
 * @retval SSP_ERR_TIMEOUT  Timed out waiting for the Flash sequencer to become ready.
 * @retval SSP_ERR_PE_FAILURE  Unable to enter Programming mode. Flash may be locked (FSPR bit).
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_write (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const src_start_address,
        uint32_t dest_start_address,
        uint32_t num_bytes,
        uint32_t min_program_size)
{
    ssp_err_t err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    gp_flash_parameters->dest_addr     = dest_start_address;
    gp_flash_parameters->src_addr      = src_start_address;
    gp_flash_parameters->current_count = (uint32_t)0;

    /** If flash is in PE mode for Data Flash. */
    if (p_faci_reg->FENTRYR == 0x0080U)
    {
        gp_flash_parameters->total_count = num_bytes;   // NPP7 and NPP4 both have DF write size of 1.

        /** Start the write operation */
        HW_FLASH_LP_dataflash_write(p_ctrl, src_start_address, dest_start_address, num_bytes, min_program_size);

        /** If configured for Blocking mode then don't return until the entire operation is complete */
        if (gp_flash_parameters->bgo_enabled_df == false)
        {
            do
            {
                /** Wait for the write commands to be completed and verifies the result of the command execution. */
                err = HW_FLASH_LP_dataflash_write_monitor(p_ctrl, min_program_size);
            } while (SSP_ERR_IN_USE == err);

            /** Disable P/E mode for data flash. */
            HW_FLASH_LP_dataflash_enter_read_mode(p_ctrl);
        }
    }

#if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 1)
    /** If flash is in P/E mode for code flash */
    else if (p_faci_reg->FENTRYR == 0x0001U)
    {
        uint32_t temp = min_program_size;
        uint32_t right_shift = 0U;
        while (1U != temp)
        {
            temp >>= 1;
            right_shift++;
        }
        /** Calculate the total number of writes. */
        /* This is done with right shift instead of division to avoid using the division library, which would be in flash
         * and cause a jump from RAM to flash. */
        gp_flash_parameters->total_count = num_bytes >> right_shift;

        /** Start the write operation */
        HW_FLASH_LP_codeflash_write(p_ctrl, src_start_address, dest_start_address, num_bytes, min_program_size);

        /** Code Flash is always blocking mode, so we don't return until the entire operation is complete */
        do
        {
            err = HW_FLASH_LP_codeflash_write_monitor(p_ctrl, min_program_size);
        } while (SSP_ERR_IN_USE == err);

        /** Disable P/E mode for code flash. */
        HW_FLASH_LP_codeflash_enter_read_mode(p_ctrl);         // We're done, return to read mode
    }
#endif /* if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 1) */
    else
    {
        /** Otherwise return error. */
        //Flash locked (FSPR bit?)
        return SSP_ERR_PE_FAILURE;
    }
    return err;
}

/*******************************************************************************************************************//**
 * @brief   This function checks if the specified Data Flash address range is blank.
 * @param[in]   p_ctrl             Pointer to the Flash control block
 * @param[in]   start_address      The starting address of the Flash area to blank check.
 * @param[in]   num_bytes          Specifies the number of bytes that need to be checked.
 * @param[out]  result             Pointer that will be populated by the API with the results of the blank check
 *                                 operation in non-BGO (blocking) mode. In this case the blank check operation
 *                                 completes here and the result is returned. In Data Flash BGO mode the blank check
 *                                 operation is only started here and the result obtained later when the
 *                                 supplied callback routine is called.
 * @retval SSP_SUCCESS             Blankcheck operation completed with result in result,
 *                                 or blankcheck started and in-progess (BGO mode).
 * @retval SSP_ERR_ERASE_FAILED    Status is indicating an erase error. Possibly from a prior operation.
 * @retval SSP_ERR_CMD_LOCKED      FCU is in locked state, typically as a result of having received an illegal command.
 * @retval SSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation.
 * @retval SSP_ERR_TIMEOUT Timed out waiting for the FCU to become ready.
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_blankcheck (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t start_address, uint32_t num_bytes, flash_result_t * result)
{
    ssp_err_t err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /* We're already in the appropriate P/E mode from the caller */
    /** If flash is configured for data flash P/E mode */
    if (p_faci_reg->FENTRYR == 0x0080U)
    {
        /** Execute blank check command. */
        HW_FLASH_LP_dataflash_blank_check(p_ctrl, start_address, (start_address + num_bytes) - 1);     // We want to blank check 0
                                                                                                       // - num_bytes-1
        /** If in DF BGO mode, exit here; remaining processing if any will be done in ISR */
        if (gp_flash_parameters->bgo_enabled_df == true)
        {
            *result = FLASH_RESULT_BGO_ACTIVE;
            return err;
        }

        /** If not in DF BGO mode wait for the command to complete. */
        do
        {
            err = HW_FLASH_LP_dataflash_blank_check_monitor(p_ctrl);
        } while (SSP_ERR_IN_USE == err);

        /** If successful check the result of the blank check. */
        if (err == SSP_SUCCESS)
        {
            if (0U != p_faci_reg->FSTATR00_b.BCERR0)           // Tested Flash Area is not blank
            {
                *result = FLASH_RESULT_NOT_BLANK;
            }
            else
            {
                *result = FLASH_RESULT_BLANK;
            }

            /** Reset the flash. */
            HW_FLASH_LP_reset(p_ctrl);                     // Make sure we clear the BCERR bit that reflects the result of
                                                           // our blank check
            /** Return the data flash to P/E mode. */
            HW_FLASH_LP_dataflash_enter_read_mode(p_ctrl); // We're done, return to read mode
        }
    }
    /** If flash is configured for code flash P/E mode */
    else if (p_faci_reg->FENTRYR == 0x0001U)
    {
        /** Give the initial (and possibly only) Blank check command to the FACI. */
        err = HW_FLASH_LP_codeflash_blank_check(p_ctrl, start_address, num_bytes);
        if (err == SSP_SUCCESS)
        {
            do
            {
                /** Wait for the blank check to complete and return result in control block. */
                err = HW_FLASH_LP_codeflash_blank_check_monitor(p_ctrl);
            } while (SSP_ERR_IN_USE == err);

            /** If succeessful reset the flash and put the code flash into read mode. */
            if (err == SSP_SUCCESS)
            {
                *result = gp_flash_parameters->bc_result;
                HW_FLASH_LP_reset(p_ctrl);                     // Make sure we clear the BCERR bit that reflects the result of our bc
                HW_FLASH_LP_codeflash_enter_read_mode(p_ctrl); // We're done, return to read mode
            }
        }
    }

    /** Return status. Blank status is in result. */
    return err;
}

/*******************************************************************************************************************//**
 * @brief   This function switches the peripheral to P/E mode for Code Flash or Data Flash.
 * @param[in]   p_ctrl        Pointer to the Flash control block
 * @param[in]   flash_type    Specifies Code or Data Flash.
 *                             Valid selections are: FLASH_TYPE_CODE_FLASH or FLASH_TYPE_DATA_FLASH
 * @retval SSP_SUCCESS                 Successfully entered P/E mode.
 * @retval SSP_ERR_PE_FAILURE          Failed to enter P/E mode
 * @retval SSP_ERR_INVALID_ARGUMENT    Supplied flash_type was something other than FLASH_TYPE_CODE_FLASH or
 *                                     FLASH_TYPE_DATA_FLASH, or Code Flash Programming is not enabled and a request to
 *                                     is for a CF operation.
 *
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_pe_mode_enter (flash_lp_instance_ctrl_t * const p_ctrl, flash_type_t flash_type)
{
    ssp_err_t err = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** If the flas type is data flash put the data flash in PE mode. */
    if (flash_type == FLASH_TYPE_DATA_FLASH)
    {
        HW_FLASH_LP_dataflash_enter_pe_mode(p_ctrl);            // Sets PCKA clock

        /** Verify that we actually did enter DF P/E mode. If not return error */
        if (p_faci_reg->FENTRYR != 0x0080U)
        {
            err = SSP_ERR_PE_FAILURE;
        }
    }
#if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 1)
    /** If the flash type is code flash put the code flash in PE mode */
    else if (flash_type == FLASH_TYPE_CODE_FLASH)
    {
        HW_FLASH_LP_codeflash_enter_pe_mode(p_ctrl);

        /** Verify that we actually did enter CF P/E mode. If not return error */
        if (p_faci_reg->FENTRYR != 0x0001U)
        {
            err = SSP_ERR_PE_FAILURE;
        }
    }
#endif /* if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 1) */
    else
    {
        /** If neither code flash or data flash command return error. */
        err = SSP_ERR_INVALID_ARGUMENT;
    }

    return err;
}

/*******************************************************************************************************************//**
 * @brief   This function switches the peripheral from P/E mode for Code Flash or Data Flash to Read mode.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @retval SSP_SUCCESS                 Successfully entered P/E mode.
 * @retval SSP_ERR_PE_FAILURE    Failed to exited P/E mode
 * @retval SSP_FLASH_ERR_PARAMETERS    Supplied flash_type was something other than FLASH_TYPE_CODE_FLASH or
 *                                     FLASH_TYPE_DATA_FLASH.
 *
 **********************************************************************************************************************/
ssp_err_t HW_FLASH_LP_pe_mode_exit (flash_lp_instance_ctrl_t * const p_ctrl)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** If device is configured for P/E mode for data flash exit P/E mode */
    if (p_faci_reg->FENTRYR == 0x0080U)
    {
        HW_FLASH_LP_dataflash_enter_read_mode(p_ctrl);
    }

#if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 1)
    /** Otherwise if device is configured for P/E mode for code flash exit P/E mode. */
    else if (p_faci_reg->FENTRYR == 0x0001U)
    {
        HW_FLASH_LP_codeflash_enter_read_mode(p_ctrl);
    }
#endif
    return (SSP_SUCCESS);
}

/*******************************************************************************************************************//**
 * @brief   This function resets the Flash sequencer.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 **********************************************************************************************************************/
void HW_FLASH_LP_reset (flash_lp_instance_ctrl_t * const p_ctrl)
{
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** If not currently in PE mode then enter P/E mode. */
    if (p_faci_reg->FENTRYR == 0x0000UL)
    {
        /* Enter P/E mode so that we can execute some FACI commands. Either Code or Data Flash P/E mode would work
         * but Code Flash P/E mode requires FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE ==1, which may not be true */
        HW_FLASH_LP_pe_mode_enter(p_ctrl, FLASH_TYPE_DATA_FLASH);
    }

    /** Reset the flash. */
    p_faci_reg->FRESETR_b.FRESET = 1U;
    p_faci_reg->FRESETR_b.FRESET = 0U;

    /** Transition to Read mode */
    HW_FLASH_LP_pe_mode_exit(p_ctrl);

    /** If DF BGO is enabled and we have an interrupt defined for FACI, re-enable it */
    if (gp_flash_parameters->bgo_enabled_df == true)
    {
        /** Indicate that we just reset the FCU */
        gp_flash_parameters->current_operation = FLASH_OPERATION_FCU_RESET;
    }

}

/*******************************************************************************************************************//**
 * @brief  Handle the FACI frdyi interrupt when the operation is Data Flash BGO write.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @param[in]  p_cb_data    Pointer to the Flash callback event structure.
 * @retval true, when operation is completed or error has occurred.
 *
 **********************************************************************************************************************/
static inline bool HW_FLASH_LP_frdyi_df_bgo_write (flash_lp_instance_ctrl_t *p_ctrl, flash_callback_args_t *p_cb_data)
{
    bool operation_complete = false;
    ssp_err_t     result = SSP_SUCCESS;

    /** We are handling the interrupt indicating the last write operation has completed. */
    /** Whether we are done or not we want to check the status */

    result = HW_FLASH_LP_dataflash_write_monitor(p_ctrl, gp_flash_parameters->min_pgm_size);
    if ((result != SSP_SUCCESS) && (result != SSP_ERR_IN_USE))
    {
        HW_FLASH_LP_reset(p_ctrl);
        p_cb_data->event = FLASH_EVENT_ERR_FAILURE;            ///< Pass result back to callback
        operation_complete = true;
    }
    else
    {
        /*If there are still bytes to write*/
        if (gp_flash_parameters->total_count == (uint32_t)0)
        {
            p_cb_data->event = FLASH_EVENT_WRITE_COMPLETE;
            operation_complete = true;
        }
    }
    return(operation_complete);
}

/*******************************************************************************************************************//**
 * @brief  Handle the FACI frdyi interrupt when the operation is Data Flash BGO erase.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @param[in]  p_cb_data    Pointer to the Flash callback event structure.
 * @retval true, when operation is completed or error has occurred.
 *
 **********************************************************************************************************************/
static inline bool HW_FLASH_LP_frdyi_df_bgo_erase (flash_lp_instance_ctrl_t *p_ctrl, flash_callback_args_t *p_cb_data)
{
    bool operation_complete = false;
    ssp_err_t     result = SSP_SUCCESS;

    /** We are handling the interrupt indicating the last erase operation has completed. */
    /** Whether we are done or not we want to check the status */
    result = HW_FLASH_LP_dataflash_erase_monitor(p_ctrl, gp_flash_parameters->block_size);
    if ((result != SSP_SUCCESS) && (result != SSP_ERR_IN_USE))
    {
        HW_FLASH_LP_reset(p_ctrl);
        p_cb_data->event = FLASH_EVENT_ERR_FAILURE;            ///< Pass result back to callback
        operation_complete = true;
    }
    else
    {
        if (gp_flash_parameters->current_count > gp_flash_parameters->total_count)
        {
            p_cb_data->event = FLASH_EVENT_ERASE_COMPLETE;
            operation_complete = true;
        }
    }
    return(operation_complete);
}


/*******************************************************************************************************************//**
 * @brief  Handle the FACI frdyi interrupt when the operation is Data Flash BGO blankcheck.
 * @param[in]  p_ctrl       Pointer to the Flash control block
 * @param[in]  p_cb_data    Pointer to the Flash callback event structure.
 * @retval true, when operation is completed or error has occurred.
 *
 **********************************************************************************************************************/
static inline bool HW_FLASH_LP_frdyi_df_bgo_blankcheck (flash_lp_instance_ctrl_t *p_ctrl, flash_callback_args_t *p_cb_data)
{
    bool operation_complete = false;
    ssp_err_t     result = SSP_SUCCESS;
    R_FACI_Type * p_faci_reg = (R_FACI_Type *) p_ctrl->p_reg;

    /** We are handling the interrupt indicating the last blank check operation has completed. */
    /** Whether we are done or not we want to check the status */
    result = HW_FLASH_LP_dataflash_blank_check_monitor(p_ctrl);

    /// All of the following are valid returns.
    if ((result != SSP_SUCCESS) && (result != SSP_ERR_IN_USE))
    {
        HW_FLASH_LP_reset(p_ctrl);
        p_cb_data->event = FLASH_EVENT_ERR_FAILURE;        ///< Pass result back to callback
        operation_complete = true;
    }
    else
    {
        operation_complete = true;
        p_cb_data->event = FLASH_EVENT_BLANK;
        if (p_faci_reg->FSTATR00_b.BCERR0 == 1U)
        {
            p_cb_data->event = FLASH_EVENT_NOT_BLANK;
        }
    }

    return(operation_complete);
}

/*******************************************************************************************************************//**
 * @brief  FLASH ready interrupt routine.
 *
 * @details This function implements the FLASH ready isr. The function clears the interrupt request source on entry
 *          populates the callback structure with the relevant event, and providing
 *          a callback routine has been provided, calls the callback function with the event. <br>
 *
 **********************************************************************************************************************/
void fcu_frdyi_isr (void)
{
    SF_CONTEXT_SAVE
    flash_callback_args_t cb_data;
    bool operation_completed = false;

    ssp_vector_info_t * p_vector_info = NULL;
    R_SSP_VectorInfoGet(R_SSP_CurrentIrqGet(), &p_vector_info);
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) *(p_vector_info->pp_ctrl);

    /** Clear the Interrupt Request*/
    R_BSP_IrqStatusClear(R_SSP_CurrentIrqGet());

    /** Continue the current operation. If unknown operation set callback event to failure. */
    if (FLASH_OPERATION_DF_BGO_WRITE == gp_flash_parameters->current_operation)
    {
        operation_completed = HW_FLASH_LP_frdyi_df_bgo_write(p_ctrl, &cb_data);
    }
    else if ((FLASH_OPERATION_DF_BGO_ERASE == gp_flash_parameters->current_operation))
    {
        operation_completed = HW_FLASH_LP_frdyi_df_bgo_erase(p_ctrl, &cb_data);
    }
    else if (FLASH_OPERATION_DF_BGO_BLANKCHECK == gp_flash_parameters->current_operation)
    {
        operation_completed = HW_FLASH_LP_frdyi_df_bgo_blankcheck(p_ctrl, &cb_data);
    }
    else
    {
        /* This case could only be reached by a memory corruption of some type. */
        cb_data.event = FLASH_EVENT_ERR_FAILURE;
    }

    /** If the operation completed exit read mode, release the flash, and call the callback if available. */
    if (operation_completed == true)
    {
        /* finished current operation. Exit P/E mode*/
        HW_FLASH_LP_pe_mode_exit(p_ctrl);

        /* Release lock and Set current state to Idle*/
        flash_ReleaseState();
        gp_flash_parameters->current_operation = FLASH_OPERATION_IDLE;

        if (NULL != p_ctrl->p_callback)
        {
            /** Set data to identify callback to user, then call user callback. */
            p_ctrl->p_callback(&cb_data);
        }
    }
    SF_CONTEXT_RESTORE
}  /* End of function fcu_frdyi_isr() */
