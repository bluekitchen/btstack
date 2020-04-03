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
 * File Name    : r_flash_lp.c
 * Description  : HLD Interface for the Low power FLASH peripheral on SC32 MCUs.
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"

#include "r_flash_lp.h"
#include "hw/target/hw_flash_lp_private.h"
#include "r_flash_lp_private_api.h"
/* Configuration for this package. */
#include "r_flash_cfg.h"
#include "r_cgc_api.h"
#include "r_cgc.h"
#include "hw/target/flash_lp_core/hw_dataflash.h"
#include "hw/target/flash_lp_core/hw_codeflash_extra.h"
#include "hw/target/flash_lp_core/hw_codeflash.h"

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** "OPEN" in ASCII, used to avoid multiple open. */
#define FLASH_OPEN                (0x4f50454eULL)
#define FLASH_CLOSE               (0U)

#define MINIMUM_SUPPORTED_FCLK_FREQ 4000000U            /// Minimum FCLK for Flash Operations in Hz

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** Macro for error logger. */
#ifndef FLASH_ERROR_RETURN
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define FLASH_ERROR_RETURN(a, err) SSP_ERROR_RETURN((a), (err), &g_module_name[0], &g_flash_lp_version)
#endif

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
static flash_block_info_t g_block_info = {0U};     /// Structure holding block info about an address.
static bsp_lock_t         g_flash_Lock = {0U};     /// Flash commands software lock

/** Structure that holds the parameters for current operations*/
static current_parameters_t  g_current_parameters = {0U};

/** State variable for the Flash API. */
static flash_states_t g_flash_state = FLASH_STATE_UNINITIALIZED;

/** Internal functions. */
/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static ssp_err_t      flash_lock_state (flash_states_t new_state) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static ssp_err_t      setup_for_pe_mode (flash_lp_instance_ctrl_t * const p_ctrl, flash_type_t flash_type, flash_states_t flash_state) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static ssp_err_t      flash_setup (flash_lp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static ssp_err_t      flash_open_setup (flash_lp_instance_ctrl_t * const p_ctrl, ssp_feature_t *p_ssp);

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static ssp_err_t      flash_blank_check_address_checking (uint32_t const address, uint32_t num_bytes) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static ssp_err_t      flash_blank_check_initiate ( flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const address, uint32_t num_bytes, flash_result_t *p_blank_check_result) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static void           flash_operation_complete (flash_lp_instance_ctrl_t * const p_ctrl, ssp_err_t err) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static ssp_err_t      flash_write_initiate (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const src_start_address, uint32_t dest_start_address, uint32_t num_bytes) PLACE_IN_RAM_SECTION;

static ssp_err_t      flash_fmi_setup (flash_lp_instance_ctrl_t * const p_ctrl, flash_cfg_t const * const p_cfg, ssp_feature_t *p_ssp);

#if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1)
/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static ssp_err_t      flash_write_parameter_checking (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t flash_address, uint32_t const num_bytes) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static ssp_err_t      flash_erase_parameter_checking (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const num_blocks) PLACE_IN_RAM_SECTION;

/*LDRA_INSPECTED 219 s - This is an allowed exception to LDRA standard 219 S "User name starts with underscore."*/
static ssp_err_t      flash_blank_check_parameter_checking (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const address, uint32_t num_bytes) PLACE_IN_RAM_SECTION;
#endif

/***********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/
/*LDRA_NOANALYSIS LDRA_INSPECTED below not working. */
/*LDRA_INSPECTED 27 D This structure must be accessible in user code. It cannot be static. */
const flash_api_t g_flash_on_flash_lp =
{
    .open                 = R_FLASH_LP_Open,
    .close                = R_FLASH_LP_Close,
    .write                = R_FLASH_LP_Write,
    .read                 = R_FLASH_LP_Read,
    .erase                = R_FLASH_LP_Erase,
    .blankCheck           = R_FLASH_LP_BlankCheck,
    .statusGet            = R_FLASH_LP_StatusGet,
    .infoGet              = R_FLASH_LP_InfoGet,
    .accessWindowSet      = R_FLASH_LP_AccessWindowSet,
    .accessWindowClear    = R_FLASH_LP_AccessWindowClear,
    .idCodeSet            = R_FLASH_LP_IdCodeSet,
    .reset                = R_FLASH_LP_Reset,
    .startupAreaSelect    = R_FLASH_LP_StartUpAreaSelect,
    .updateFlashClockFreq = R_FLASH_LP_UpdateFlashClockFreq,
    .versionGet           = R_FLASH_LP_VersionGet
};
/*LDRA_ANALYSIS */
#if defined(__GNUC__)
/* This structure is affected by warnings from a GCC compiler bug. This pragma suppresses the warnings in this
 * structure only.*/
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
/** Version data structure used by error logger macro. */
static const ssp_version_t g_flash_lp_version =
{
    .api_version_minor  = FLASH_API_VERSION_MINOR,
    .api_version_major  = FLASH_API_VERSION_MAJOR,
    .code_version_major = FLASH_LP_CODE_VERSION_MAJOR,
    .code_version_minor = FLASH_LP_CODE_VERSION_MINOR
};
#if defined(__GNUC__)
/* Restore warning settings for 'missing-field-initializers' to as specified on command line. */
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic pop
#endif

/** Name of module used by error logger macro */
#if BSP_CFG_ERROR_LOG != 0
static const char          g_module_name[] = "r_flash_lp";
#endif

static flash_fmi_regions_t g_flash_code_region = {0U};
static flash_fmi_regions_t g_flash_data_region = {0U};

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @addtogroup FLASH
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief  Initialize the Low Power flash peripheral. Implements flash_api_t::open.
 *
 * The Open function initializes the Flash. It first copies the FCU firmware to FCURAM
 * and sets the FCU Clock based on the current FCLK frequency. In addition, if Code Flash programming is enabled,
 * the API code responsible for Code Flash programming will be copied to RAM.
 *
 * This function must be called once prior to calling any other FLASH API functions.
 * If a user supplied callback function is supplied, then the Flash Ready interrupt will be configured to
 * call the users callback routine with an Event type describing the source of the interrupt for Data Flash
 * operations.
 * Subsequent to successfully completing this call p_ctrl->opened will be true.
 *
 * @note Providing a callback function in the supplied p_cfg->callback field, automatically configures the
 *       Flash for Data Flash to operate in non-blocking (BGO) mode.
 *
 * Subsequent to a successful Open(), the Flash is ready to process additional Flash commands.
 *
 * @retval SSP_SUCCESS               Initialization was successful and timer has started.
 * @retval SSP_FLASH_ERR_FAILURE     Failed to successfully enter Programming/Erase mode.
 * @retval SSP_ERR_TIMEOUT           Timed out waiting for FCU to be ready.
 * @retval SSP_ERR_ASSERTION         NULL provided for p_ctrl or p_cfg or problem getting FMI info.
 * @retval SSP_ERR_IRQ_BSP_DISABLED  Caller is requesting BGO but the Flash interrupts are not enabled.
 * @retval SSP_ERR_FCLK              FCLK must be a minimum of 4 MHz for Flash operations.
 * @retval SSP_ERR_IN_USE            Flash Open() has already been called.
 * @retval SSP_ERR_HW_LOCKED         Flash module unable to get the Hardware lock for the Flash LP Periperhal.
 * @return                           See @ref Common_Error_Codes or functions called by this function for other possible
 *                                   return codes. This function calls:
 *                                      * fmi_api_t::productFeatureGet
 *                                      * fmi_api_t::eventInfoGet
 *
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_Open (flash_ctrl_t * const p_api_ctrl, flash_cfg_t const * const p_cfg)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;
    ssp_feature_t ssp_feature = {{(ssp_ip_t) 0}};

    /* g_flash_lp_version is accessed by the ASSERT macro only, and so compiler toolchain can issue a
     *  warning that it is not accessed. The code below eliminates this warning and also ensures data
     *  structures are not optimized away. */
    SSP_PARAMETER_NOT_USED(g_flash_lp_version);

    ssp_err_t err = SSP_SUCCESS;

    /** If null pointers return error. */
#if (FLASH_CFG_PARAM_CHECKING_ENABLE)
    SSP_ASSERT(NULL != p_cfg);
    SSP_ASSERT(NULL != p_ctrl);
#endif

    /** If open return error. */
    FLASH_ERROR_RETURN((FLASH_OPEN != p_ctrl->opened), SSP_ERR_IN_USE);

    /** Setup flash FMI. If failure return error. */
    err = flash_fmi_setup(p_ctrl, p_cfg, &ssp_feature);
    FLASH_ERROR_RETURN((err == SSP_SUCCESS), err);

    /** Allow Initialization if not initialized or if no operation is ongoing and re-initialization is desired */
    if ((FLASH_STATE_UNINITIALIZED == g_flash_state) || (FLASH_STATE_READY == g_flash_state))
    {
        /** Acquire the software lock. If failure return error. */
        FLASH_ERROR_RETURN(!(SSP_SUCCESS != flash_lock_state(FLASH_STATE_INITIALIZATION)), SSP_ERR_IN_USE);
    }

    /** Set the parameters struct based on the user supplied settings */
    g_current_parameters.bgo_enabled_df = p_cfg->data_flash_bgo;

    if (g_current_parameters.bgo_enabled_df == true)
    {
        /** Setup the Flash interrupt callback based on the caller's info. If the Flash interrupt is
         * not enabled in the BSP then this will return SSP_ERR_IRQ_BSP_DISABLED */
       err =  HW_FLASH_LP_irq_cfg((bool) (p_cfg->p_callback != NULL), p_cfg, p_ctrl->irq);
    }
    else
    {
        /** Make sure Flash interrupts are disabled, they are only used in BGO mode */
        HW_FLASH_LP_irq_cfg(false, p_cfg, p_ctrl->irq);
    }

    /** Setup the flash. */
    err = flash_open_setup(p_ctrl, &ssp_feature);

    /** Save callback function pointer  */
    p_ctrl->p_callback = p_cfg->p_callback;

    return err;
}

/*******************************************************************************************************************//**
 * @brief  Write to the specified Code or Data Flash memory area. Implements flash_api_t::write.
 *
 * The minimum/maximum number of bytes, as well as the invalid address and programming boundaries supported and
 * enforced by this function are dependent on the MCU in use as well as the part package size.
 * Please see the User manual and/or requirements document for additional information.
 *
 * @retval SSP_SUCCESS              Operation successful. If BGO is enabled this means the operation was started
 *                                  successfully.
 * @retval SSP_ERR_IN_USE           The Flash peripheral is busy with a prior on-going transaction.
 * @retval SSP_ERR_NOT_OPEN         The Flash API is not Open.
 * @retval SSP_ERR_CMD_LOCKED       FCU is in locked state, typically as a result of attempting to Write
 *                                  an area that is protected by an Access Window.
 * @retval SSP_ERR_WRITE_FAILED     Status is indicating a Programming error for the requested operation. This
 *                                  may be returned if the requested Flash area is not blank.
 * @retval SSP_ERR_TIMEOUT          Timed out waiting for FCU operation to complete.
 * @retval SSP_ERR_INVALID_SIZE     Number of bytes provided was not a multiple of the programming size or exceeded
 *                                  the maximum range.
 * @retval SSP_ERR_INVALID_ADDRESS  Invalid address was input or address not on programming boundary.
 * @retval SSP_ERR_ASSERTION        NULL provided for p_ctrl.
 * @retval SSP_ERR_INVALID_ARGUMENT Code Flash Programming is not enabled and a request to write CF was requested.
**********************************************************************************************************************/
ssp_err_t R_FLASH_LP_Write (flash_ctrl_t * const p_api_ctrl,
                            uint32_t const       src_address,
                            uint32_t             flash_address,
                            uint32_t const       num_bytes)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;
    ssp_err_t err = SSP_SUCCESS;

    /** Get the block information for this address. If failure return error. */
    FLASH_ERROR_RETURN((flash_get_block_info(flash_address, &g_block_info)), SSP_ERR_INVALID_ADDRESS);

#if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1)
    /** Check parameters. If failure return error */
    err = flash_write_parameter_checking (p_ctrl, flash_address, num_bytes);
    FLASH_ERROR_RETURN((err == SSP_SUCCESS), err);

    /** If control block is not open return error. */
    FLASH_ERROR_RETURN((FLASH_OPEN == p_ctrl->opened), SSP_ERR_NOT_OPEN);
#endif /* if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1) */

    /** Initiate the write operation. */
    err = flash_write_initiate(p_ctrl, src_address, flash_address, num_bytes);

    /* Complete the flash operation. */
    flash_operation_complete (p_ctrl, err);

    /** Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * @brief  Read the requested number of bytes from the supplied Data or Code Flash address. Implements flash_api_t::read.
 *
 * The minimum/maximum number of blocks, as well as the invalid address and programming boundaries supported and
 * enforced by this function are dependent on the MCU in use as well as the part package size.
 * Please see the User manual and/or requirements document for additional information.
 *         @note This function is provided simply for the purposes of maintaining a complete interface.
 *         It is possible (and recommended), to read Flash memory directly.
 *
 * @retval SSP_SUCCESS              Operation successful.
 * @retval SSP_ERR_INVALID_ADDRESS  Invalid Flash address was supplied.
 * @retval SSP_ERR_ASSERTION        NULL provided for p_ctrl or p_dest_address
 * @retval SSP_ERR_NOT_OPEN         Flash API has not yet been opened.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_Read  (flash_ctrl_t * const p_api_ctrl, uint8_t * p_dest_address, uint32_t const flash_address,
                            uint32_t const num_bytes)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;

    /* Eliminate warning if parameter checking is disabled. */
    SSP_PARAMETER_NOT_USED(p_ctrl);

    ssp_err_t err           = SSP_SUCCESS;
    uint32_t  index;
    uint8_t   * p_flash_ptr = (uint8_t *) flash_address;

#if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1)
    /** If null pointers return error */
    SSP_ASSERT(NULL != p_dest_address);
    SSP_ASSERT(NULL != p_ctrl);

    /** If invalid number of bytes return error. */
    FLASH_ERROR_RETURN((0 != num_bytes), SSP_ERR_INVALID_SIZE);

    /** If not open return error */
    FLASH_ERROR_RETURN(!(FLASH_OPEN != p_ctrl->opened), SSP_ERR_NOT_OPEN);

    /** Get the block information for this address. If failure return error. */
    FLASH_ERROR_RETURN((flash_get_block_info(flash_address, &g_block_info)), SSP_ERR_INVALID_ADDRESS);

    /** Insure the read request doesn't span across the end of the CF or DF range. If it does return error. */
    FLASH_ERROR_RETURN(((flash_address + (num_bytes - 1U)) <= g_block_info.memory_end_addr), SSP_ERR_INVALID_ADDRESS);
#endif /* if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1) */

    /** Copy the data to the destination buffer. */
    for (index = (uint32_t)0; index < num_bytes; index++)
    {
        p_dest_address[index] = p_flash_ptr[index];
    }

    return err;
}

/*******************************************************************************************************************//**
 * @brief  Erase the specified Code or Data Flash blocks. Implements flash_api_t::erase.
 *
 * The minimum/maximum number of blocks, as well as the invalid address and programming boundaries supported and
 * enforced by this function are dependent on the MCU in use as well as the part package size.
 * Please see the User manual and/or requirements document for additional information.
 * @retval SSP_SUCCESS                 Successful open.
 * @retval SSP_ERR_INVALID_BLOCKS      Invalid number of blocks specified
 * @retval SSP_ERR_INVALID_ADDRESS     Invalid address specified
 * @retval SSP_ERR_IN_USE              Other flash operation in progress, or API not initialized
 * @retval SSP_ERR_CMD_LOCKED          FCU is in locked state, typically as a result of attempting to Erase
 *                                     an area that is protected by an Access Window.
 * @retval SSP_ERR_ASSERTION           NULL provided for p_ctrl
 * @retval SSP_ERR_NOT_OPEN            The Flash API is not Open.
 * @retval SSP_ERR_INVALID_ARGUMENT    Code Flash Programming is not enabled and a request to erase CF was requested.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_Erase (flash_ctrl_t * const p_api_ctrl, uint32_t const address, uint32_t const num_blocks)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;

    ssp_err_t err = SSP_SUCCESS;

    /** Get the block information for this address. If failure return error. */
    FLASH_ERROR_RETURN((flash_get_block_info(address, &g_block_info)), SSP_ERR_INVALID_ADDRESS);

#if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1)
    /** Verify erase parameters. If failure return error. */
    err = flash_erase_parameter_checking (p_ctrl, num_blocks);
    FLASH_ERROR_RETURN((err == SSP_SUCCESS), err);
#endif /* if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1) */

    /** Update Flash state and enter the respective Code or Data Flash P/E mode, may return SSP_ERR_IN_USE */
    err = setup_for_pe_mode(p_ctrl, g_block_info.flash_type, FLASH_STATE_ERASING);

    /** If successful */
    if (SSP_SUCCESS == err)
    {
        /** Configure the current parameters based on if the operation is for code flash or data flash. */
        if (g_block_info.is_code_flash_addr == true)
        {
            g_current_parameters.wait_cnt          = g_current_parameters.wait_max_erase_cf_block;
            g_current_parameters.current_operation = FLASH_OPERATION_CF_ERASE;
        }
        else
        {
            g_current_parameters.wait_cnt = g_current_parameters.wait_max_erase_df_block;
            /** If this is a request to erase Data Flash configure BGO mode if it is enabled. */
            if (g_current_parameters.bgo_enabled_df == false)
            {
                g_current_parameters.current_operation = FLASH_OPERATION_DF_ERASE;
            }
            else
            {
                g_current_parameters.current_operation = FLASH_OPERATION_DF_BGO_ERASE;
            }
        }

        /** Initiate the flash erase. */
        err = HW_FLASH_LP_erase(p_ctrl, g_block_info.this_block_st_addr, num_blocks, g_block_info.block_size);
        if (SSP_SUCCESS == err)
        {
            /** If in non-BGO mode, the current operation is complete. Exit PE mode and return status*/
            if (g_current_parameters.current_operation != FLASH_OPERATION_DF_BGO_ERASE)
            {
                err = HW_FLASH_LP_pe_mode_exit(p_ctrl);
            }
        }
    }

    /** Complete the flash operation. */
    /* Execute a reset if any error, release the state if not BGO */
    flash_operation_complete (p_ctrl, err);

    return err;
}

/*******************************************************************************************************************//**
 * @brief  Perform a blank check on the specified address area. Implements flash_api_t::blankCheck.
 *
 * The minimum/maximum number of bytes, as well as the invalid address and programming boundaries supported and
 * enforced by this function are dependent on the MCU in use as well as the part package size.
 * Please see the User manual and/or requirements document for additional information.
 * The number of bytes for Data Flash blank checking must be between (1 and FLASH_DATA_BLANK_CHECK_MAX).
 * The number of bytes for Code Flash blank checking must be between (1 and FLASH_CODE_BLANK_CHECK_MAX).
 *
 * @retval SSP_SUCCESS              Blankcheck operation completed with result in p_blank_check_result,
 *                                  or blankcheck started and in-progess (BGO mode).
 * @retval SSP_ERR_INVALID_ADDRESS  Invalid data flash address was input
 * @retval SSP_ERR_INVALID_SIZE     'num_bytes' was either too large or not aligned for the CF/DF boundary size.
 * @retval SSP_ERR_IN_USE           Flash is busy with an on-going operation.
 * @retval SSP_ERR_ASSERTION        NULL provided for p_ctrl
 * @retval SSP_ERR_NOT_OPEN         Flash API has not yet been opened.
 * @retval SSP_ERR_INVALID_ARGUMENT Code Flash Programming is not enabled and a request to Blank Check CF was requested.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_BlankCheck (flash_ctrl_t * const p_api_ctrl, uint32_t const address, uint32_t num_bytes,
                                 flash_result_t * p_blank_check_result)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;
    ssp_err_t err         = SSP_SUCCESS;


    /** Get the block information for this address. If failure return error. */
    FLASH_ERROR_RETURN((flash_get_block_info(address, &g_block_info)), SSP_ERR_INVALID_ADDRESS);

#if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1)
    err = flash_blank_check_parameter_checking (p_ctrl, address, num_bytes);
    FLASH_ERROR_RETURN((err == SSP_SUCCESS), err);
#endif /* if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1) */


    /** Update Flash state and enter the respective Code or Data Flash P/E mode, may return SSP_ERR_IN_USE */
    err = setup_for_pe_mode(p_ctrl, g_block_info.flash_type, FLASH_STATE_BLANK_CHECK);
    if (SSP_SUCCESS == err)
    {
        /** Initiate the Blank Check operation */
        err = flash_blank_check_initiate(p_ctrl, address, num_bytes, p_blank_check_result);
    }

    /** If failure reset the flash. */
    /* SSP_ERR_IN_USE would indicate that a BGO operation is underway, so don't reset in that case */
    if ((SSP_SUCCESS != err) && (SSP_ERR_IN_USE != err))
    {
        /* This will clear error flags and exit the P/E mode*/
        HW_FLASH_LP_reset(p_ctrl);
    }

    /** If the current operation is not a BGO blank check release the flash. */
    if (g_current_parameters.current_operation != FLASH_OPERATION_DF_BGO_BLANKCHECK)
    {
        flash_ReleaseState();
    }

    return err;
}

/*******************************************************************************************************************//**
 * @brief  Query the FLASH for its status. Implements flash_api_t::statusGet.
 *
 * @retval SSP_SUCCESS              Flash is ready and available to accept commands.
 * @retval SSP_ERR_IN_USE           Flash is busy with an on-going operation.
 * @retval SSP_ERR_ASSERTION        NULL provided for p_ctrl
 * @retval SSP_ERR_NOT_OPEN         Flash API has not yet been opened.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_StatusGet (flash_ctrl_t * const p_api_ctrl)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;

    /* Eliminate warning if parameter checking is disabled. */
    SSP_PARAMETER_NOT_USED(p_ctrl);

#if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1)
    /** If null pointer return error. */
    SSP_ASSERT(NULL != p_ctrl);

    /** If not opern return error. */
    FLASH_ERROR_RETURN(!(FLASH_OPEN != p_ctrl->opened), SSP_ERR_NOT_OPEN);
#endif

    /** Return flash status */
    if (g_flash_state == FLASH_STATE_READY)
    {
        return SSP_SUCCESS;
    }
    else
    {
        return SSP_ERR_IN_USE;
    }

}

/*******************************************************************************************************************//**
 * @brief  Configure an access window for the Code Flash memory. Implements flash_api_t::accessWindowSet.
 *
 * An access window defines a contiguous area in Code Flash for which programming/erase is enabled.
 * This area is on block boundaries.
 * The block containing start_addr is the first block. The block containing end_addr is the last block.
 * The access window then becomes first block --> last block inclusive. Anything outside this range
 * of Code Flash is then write protected.
 * As an example, if you wanted to place an accesswindow on Code Flash Blocks 0 and 1, such that
 * only those two blocks were writable, you would need to specify (address in block 0, address in block 2)
 * as the respective start and end address.
 * @note If the start address and end address are set to the same value, then the access window
 * is effectively removed. This accomplishes the same functionality as R_FLASH_LP_AccessWindowClear().
 *
 * The invalid address and programming boundaries supported and enforced by this function are dependent on the MCU
 * in use as well as the part package size. Please see the User manual and/or requirements document
 * for additional information.
 *
 * @retval SSP_SUCCESS                Access window successfully configured.
 * @retval SSP_ERR_INVALID_ADDRESS    Invalid settings for start_addr and/or end_addr.
 * @retval SSP_ERR_IN_USE             FLASH peripheral is busy with a prior operation.
 * @retval SSP_ERR_ASSERTION          NULL provided for p_ctrl.
 * @retval SSP_ERR_INVALID_ARGUMENT   Code Flash Programming is not enabled.
 * @retval SSP_ERR_NOT_OPEN           Flash API has not yet been opened.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_AccessWindowSet (flash_ctrl_t * const p_api_ctrl, uint32_t const start_addr,
                                      uint32_t const end_addr)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;

    /* Eliminate warning if parameter checking is disabled. */
    SSP_PARAMETER_NOT_USED(p_ctrl);
    SSP_PARAMETER_NOT_USED(start_addr);      // Remove warnings generated when Code Flash code is not compiled in.
    SSP_PARAMETER_NOT_USED(end_addr);

    ssp_err_t err = SSP_SUCCESS;

#if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 1)
#if (FLASH_CFG_PARAM_CHECKING_ENABLE)
    /** If null pointer return error. */
    SSP_ASSERT(NULL != p_ctrl);

    /** If not open return error. */
    FLASH_ERROR_RETURN(!(FLASH_OPEN != p_ctrl->opened), SSP_ERR_NOT_OPEN);
#endif

    uint32_t last_cf_block_index = g_flash_code_region.num_regions - 1U;
    uint32_t cf_st_addr = g_flash_code_region.p_block_array[0].block_section_st_addr;
    uint32_t cf_end_addr = g_flash_code_region.p_block_array[last_cf_block_index].block_section_end_addr;

    /* Note that the end_addr indicates the address up to, but not including the block that contains that address. */
    /* Therefore to allow the very last Block to be included in the access window we must allow for FLASH_CF_BLOCK_END+1 */
    /** If the start or end addresses are invalid return error. */
    if ((start_addr >= cf_st_addr) && (end_addr <= (cf_end_addr + 1)) &&  (start_addr <= end_addr))
    {
        /** Update Flash state and enter Code Flash P/E mode */
        err = setup_for_pe_mode(p_ctrl, FLASH_TYPE_CODE_FLASH, FLASH_STATE_ACCESS_WINDOW);

        /** If successful */
        if (SSP_SUCCESS == err)
        {
            /** Set the access window. */
            err = HW_FLASH_LP_access_window_set(p_ctrl, start_addr, end_addr);

            if (SSP_SUCCESS == err)
            {
                /** Return to read mode */
                err = HW_FLASH_LP_pe_mode_exit(p_ctrl);
            }
        }

        if (SSP_SUCCESS != err)
        {
            /** If there is an error, then reset the Flash. This will clear error flags and exit the P/E mode */
            HW_FLASH_LP_reset(p_ctrl);
        }

        /** Release the flash. */
        flash_ReleaseState();
    }
    else
    {
        err = SSP_ERR_INVALID_ADDRESS;
    }

#else
    /** If not code flash return error. */
    err = SSP_ERR_INVALID_ARGUMENT;     // For consistency with _LP API we return error if Code Flash not enabled
#endif
    return err;
}

/*******************************************************************************************************************//**
 * @brief  Remove any access window that is configured in the Code Flash. Implements flash_api_t::accessWindowClear.
 *         On successful return from this call all Code Flash is writable.
 *
 * @retval SSP_SUCCESS          Access window successfully removed.
 * @retval SSP_ERR_IN_USE       FLASH peripheral is busy with a prior operation.
 * @retval SSP_ERR_ASSERTION    NULL provided for p_ctrl.
 * @retval SSP_ERR_INVALID_ARGUMENT Code Flash Programming is not enabled.
 * @retval SSP_ERR_NOT_OPEN        Flash API has not yet been opened.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_AccessWindowClear (flash_ctrl_t * const p_api_ctrl)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;

    /* Eliminate warning if parameter checking is disabled. */
    SSP_PARAMETER_NOT_USED(p_ctrl);
    ssp_err_t err = SSP_SUCCESS;

#if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 1)
#if (FLASH_CFG_PARAM_CHECKING_ENABLE)
    /** If null pointer return error. */
    SSP_ASSERT(NULL != p_ctrl);

    /** If not open return error. */
    FLASH_ERROR_RETURN(!(FLASH_OPEN != p_ctrl->opened), SSP_ERR_NOT_OPEN);

    /** If operation in progress return error. */
    err = R_FLASH_LP_StatusGet(p_ctrl);
    FLASH_ERROR_RETURN((err == SSP_SUCCESS), err);
#endif

    /** Update Flash state and enter Code Flash P/E mode */
    err = setup_for_pe_mode(p_ctrl, FLASH_TYPE_CODE_FLASH, FLASH_STATE_ACCESS_WINDOW);

    /** If successful clear the access window. */
    if (SSP_SUCCESS == err)
    {
        err = HW_FLASH_LP_access_window_clear(p_ctrl);

        /** If successful return to read mode. */
        if (SSP_SUCCESS == err)
        {
            err = HW_FLASH_LP_pe_mode_exit(p_ctrl);
        }
    }

    /** If failure reset the flash. */
    if (SSP_SUCCESS != err)
    {
        /* If there is an error, then reset the Flash. This will clear error flags and exit the P/E mode */
        HW_FLASH_LP_reset(p_ctrl);
    }

    /** Release the flash. */
    flash_ReleaseState();
#else
    err = SSP_ERR_INVALID_ARGUMENT;     ///< Return error if Code Flash not enabled
#endif

    return err;
}

/*******************************************************************************************************************//**
 * @brief      Write the ID code provided to the id code registers. Implements flash_api_t::idCodeSet.
 *
 * @retval     SSP_SUCCESS               ID code successfully configured.
 * @retval     SSP_ERR_IN_USE            FLASH peripheral is busy with a prior operation.
 * @retval     SSP_ERR_ASSERTION         NULL provided for p_ctrl.
 * @retval     SSP_ERR_INVALID_ARGUMENT  Code Flash Programming is not enabled.
 * @retval     SSP_ERR_NOT_OPEN          Flash API has not yet been opened.
 * @retval     SSP_ERR_WRITE_FAILED      Status is indicating a Programming error for the requested operation.
 * @retval     SSP_ERR_TIMEOUT           Timed out waiting for completion of extra command.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_IdCodeSet (flash_ctrl_t               * const p_api_ctrl, 
                                uint8_t              const * const p_id_code, 
                                flash_id_code_mode_t               mode)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;

    ssp_err_t err = SSP_SUCCESS;

#if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 1)
#if (FLASH_CFG_PARAM_CHECKING_ENABLE)
    /** If null pointer return error. */
    SSP_ASSERT(NULL != p_ctrl);
    SSP_ASSERT(NULL != p_id_code);

    /** If control block is not open return error. */
    FLASH_ERROR_RETURN(!(FLASH_OPEN != p_ctrl->opened), SSP_ERR_NOT_OPEN);
#endif
    /** Update Flash state and enter Code Flash P/E mode */
    err = setup_for_pe_mode(p_ctrl, FLASH_TYPE_CODE_FLASH, FLASH_STATE_ID_CODE);

    /** If successful set the id code. */
    if (SSP_SUCCESS == err)
    {
        err = HW_FLASH_LP_set_id_code(p_ctrl, p_id_code, mode);
    }

    /** If successful return to read mode. Otherwise reset the flash. */
    if (SSP_SUCCESS == err)
    {
        /* Return to read mode*/
        err = HW_FLASH_LP_pe_mode_exit(p_ctrl);
    }
    else
    {
        /* If there is an error, then reset the FCU: This will clear error flags and exit the P/E mode*/
        HW_FLASH_LP_reset(p_ctrl);
    }

    /** Release the flash. */
    flash_ReleaseState();

#else
    /* Eliminate warning if code flash programming is disabled. */
    SSP_PARAMETER_NOT_USED(p_ctrl);
    SSP_PARAMETER_NOT_USED(p_id_code);
    SSP_PARAMETER_NOT_USED(mode);

    err = SSP_ERR_INVALID_ARGUMENT;     // For consistency with _LP API we return error if Code Flash not enabled
#endif
    /** Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * @brief  Reset the FLASH peripheral. Implements flash_api_t::reset.
 *
 * No attempt is made to grab the Flash software lock before executing the reset since the assumption is that a reset
 * will terminate any existing operation.
 *
 * @retval SSP_SUCCESS        Flash circuit successfully reset.
 * @retval SSP_ERR_ASSERTION  NULL provided for p_ctrl
 * @retval SSP_ERR_NOT_OPEN   Flash API has not yet been opened.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_Reset (flash_ctrl_t * const p_api_ctrl)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;

    ssp_err_t err = SSP_SUCCESS;

#if (FLASH_CFG_PARAM_CHECKING_ENABLE)
    /** If null pointer return error. */
    SSP_ASSERT(NULL != p_ctrl);

    /** If not open return error */
    FLASH_ERROR_RETURN(!(FLASH_OPEN != p_ctrl->opened), SSP_ERR_NOT_OPEN);
#endif

    /** Reset the flash. */
    HW_FLASH_LP_reset(p_ctrl);

    /** Release the flash. */
    flash_ReleaseState();

    return err;
}

/******************************************************************************************************************//**
 * @brief  Select which block is used as the startup area block. Implements flash_api_t::startupAreaSelect.
 *
 *         Selects which block - Default (Block 0) or Alternate (Block 1) is used as the startup area block.
 *         The provided parameters determine which block will become the active startup block and whether that
 *         action will be immediate (but temporary), or permanent subsequent to the next reset.
 *         Doing a temporary switch might appear to have limited usefulness. If there is an access window
 *         in place such that Block 0 is write protected, then one could do a temporary switch, update the
 *         block and switch them back without having to touch the access window.
 *
 * @retval SSP_SUCCESS              Start-up area successfully toggled.
 * @retval SSP_ERR_IN_USE           Flash is busy with an on-going operation.
 * @retval SSP_ERR_ASSERTION        NULL provided for p_ctrl
 * @retval SSP_ERR_NOT_OPEN         Flash API has not yet been opened.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_StartUpAreaSelect (flash_ctrl_t * const      p_api_ctrl,
                                        flash_startup_area_swap_t swap_type,
                                        bool                      is_temporary)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;

    ssp_err_t err = SSP_SUCCESS;

#if (FLASH_CFG_PARAM_CHECKING_ENABLE)
    /** If null pointer return error. */
    SSP_ASSERT(NULL != p_ctrl);

    /** If not open return error. */
    FLASH_ERROR_RETURN((FLASH_OPEN == p_ctrl->opened), SSP_ERR_NOT_OPEN);
#endif

    /** If the swap type is BTFLG and the operation is temporary there's nothing to do. Return success. */
    if ((swap_type == FLASH_STARTUP_AREA_BTFLG) && (is_temporary == false))
    {
        return err;
    }

    /** Update Flash state and enter the respective Code or Data Flash P/E mode. */
    err = setup_for_pe_mode(p_ctrl, FLASH_TYPE_CODE_FLASH, FLASH_STATE_STARTUP_AREA);

    /** If successful call the temporary or boot startup area set functions depending on the users flag. */
    if (err == SSP_SUCCESS)
    {
        if (is_temporary == true)
        {
            err = HW_FLASH_LP_set_startup_area_temporary(p_ctrl, swap_type);
        }
        else
        {
            err = HW_FLASH_LP_set_startup_area_boot(p_ctrl, swap_type);
        }
    }

    /** If successful return to read mode otherwise reset the flash. */
    if (SSP_SUCCESS == err)
    {
        err = HW_FLASH_LP_pe_mode_exit(p_ctrl);
    }
    else
    {
        /* If there is an error, then reset the Flash. This will clear error flags and exit the P/E mode */
        HW_FLASH_LP_reset(p_ctrl);
    }

    /** Release the flash. */
    flash_ReleaseState();
    return err;
}


/******************************************************************************************************************//**
 * @brief  Indicate to the already open Flash API that the FCLK has changed. Implements r_flash_t::updateFlashClockFreq.
 *
 * This could be the case if the application has changed the system clock, and therefore the FCLK.
 * Failure to call this function subsequent to changing the FCLK could result in damage to the flash macro.
 * This function uses R_CGC_SystemClockFreqGet() to get the current FCLK frequency.
 *
 *
 * @retval SSP_SUCCESS              Start-up area successfully toggled.
 * @retval SSP_ERR_IN_USE           Flash is busy with an on-going operation.
 * @retval SSP_ERR_ASSERTION        NULL provided for p_ctrl
 * @retval SSP_ERR_NOT_OPEN         Flash API has not yet been opened.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_UpdateFlashClockFreq (flash_ctrl_t * const  p_api_ctrl)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;

    ssp_err_t err = SSP_SUCCESS;

#if (FLASH_CFG_PARAM_CHECKING_ENABLE)
    /** If null pointer return error. */
    SSP_ASSERT(NULL != p_ctrl);

    /** If the flash api is not open return an error. */
    FLASH_ERROR_RETURN(FLASH_OPEN == p_ctrl->opened, SSP_ERR_NOT_OPEN);
#endif

    /** Lock the flash state. If failure return error. */
    FLASH_ERROR_RETURN(SSP_SUCCESS == flash_lock_state(FLASH_UPDATE_FCLK), SSP_ERR_IN_USE);

    /** Setup the Flash. */
    err = flash_setup(p_ctrl);    // Check FCLK, calculate timeout values

    /** Release the flash. */
    flash_ReleaseState();

    return err;
}
/*******************************************************************************************************************//**
 * @brief      Returns the information about the flash regions. Implements
 *             flash_api_t::infoGet.
 *
 * @retval     SSP_SUCCESS       Successful retrieved the request information.
 * @retval SSP_ERR_ASSERTION     NULL provided for p_ctrl or p_info.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_InfoGet (flash_ctrl_t * const      p_api_ctrl, flash_info_t  * const p_info)
{
#if FLASH_CFG_PARAM_CHECKING_ENABLE
    /** If null pointers return error. */
    SSP_ASSERT(NULL != p_api_ctrl);
    SSP_ASSERT(NULL != p_info);
#endif

    /** Copy the region data to the info structure. */
    p_info->code_flash = g_flash_code_region;
    p_info->data_flash = g_flash_data_region;

    return SSP_SUCCESS;
}
/*******************************************************************************************************************//**
 * @brief      Release any resources that were allocated by the Flash API. Implements flash_api_t::close.
 *
 * @retval SSP_SUCCESS           Successful close.
 * @retval SSP_ERR_ASSERTION     NULL provided for p_ctrl or p_cfg.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_Close (flash_ctrl_t * const p_api_ctrl)
{
    flash_lp_instance_ctrl_t * p_ctrl = (flash_lp_instance_ctrl_t *) p_api_ctrl;

    /* Eliminate warning if parameter checking is disabled. */
    SSP_PARAMETER_NOT_USED(p_ctrl);

    ssp_err_t err = SSP_SUCCESS;

#if FLASH_CFG_PARAM_CHECKING_ENABLE
    /** If null pointer return error. */
    SSP_ASSERT(NULL != p_ctrl);
#endif

    ssp_feature_t ssp_feature = {{(ssp_ip_t) 0U}};
    ssp_feature.channel = 0U;
    ssp_feature.unit = 0U;
    ssp_feature.id = SSP_IP_FCU;

    /** Unlock the flash hardware. */
    R_BSP_HardwareUnlock(&ssp_feature);

    /** Disable the flash interrupt. */
    HW_FLASH_LP_irq_cfg(false, NULL, p_ctrl->irq);

    /** Mark the control block as closed. */
    p_ctrl->opened = false;

    /** Release the lock */
    flash_ReleaseState();

    return err;
}

/*******************************************************************************************************************//**
 * @brief   Get FLASH HAL driver version.
 *
 * @retval  SSP_SUCCESS - operation performed successfully
 * @note This function is reentrant.
 **********************************************************************************************************************/
ssp_err_t R_FLASH_LP_VersionGet (ssp_version_t * const p_version)
{
#if FLASH_CFG_PARAM_CHECKING_ENABLE
    /** If null pointer return error. */
    SSP_ASSERT(NULL != p_version);
#endif

    /** Return the version id of the flash lp module. */
    p_version->version_id = g_flash_lp_version.version_id;
    return SSP_SUCCESS;
}  /* End of function R_FLASH_LP_VersionGet() */

/*******************************************************************************************************************//**
 * @} (end addtogroup FLASH)
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief   This function attempts to change the flash state to the new requested state.
 *          This can only happen if we are able to take the FLASH lock. If the lock is currently in use then
 *          we will return FLASH_ERR_BUSY, otherwise we will take the lock and change the state.
 *
 * @param[in]  new_state        Which state to attempt to transition to.
 * @retval SSP_SUCCESS      Successful.
 * @retval SSP_ERR_IN_USE   Flash is busy with an on-going operation.
 **********************************************************************************************************************/
static ssp_err_t flash_lock_state (flash_states_t new_state)
{
    /** Check to see if lock was successfully taken */
    if (R_BSP_SoftwareLock(&g_flash_Lock) == SSP_SUCCESS)
    {
        /** Lock taken, we can change state */
        g_flash_state = new_state;

        /** Return success */
        return SSP_SUCCESS;
    }
    else
    {
        /** Another operation is on-going */
        return SSP_ERR_IN_USE;
    }
}

/*******************************************************************************************************************//**
 * @brief   This function releases the flash state so another flash operation can take place.
 *          This function is called by both the HLD and LLD layers (interrupt routines).
 *
 * @retval None
 **********************************************************************************************************************/
void flash_ReleaseState (void)
{
    /** Done with current operation */
    g_flash_state = FLASH_STATE_READY;

    /** Release hold on lock */
    R_BSP_SoftwareUnlock(&g_flash_Lock);
}

/*******************************************************************************************************************//**
 * @brief   This function places the flash in the requested Code or Data P/E mode.
 *
 * @param[in]   flash_type          FLASH_TYPE_CODE_FLASH or FLASH_TYPE_DATA_FLASH.
 * @param[in]   p_ctrl              Control Instance
 * @param[in]   flash_state         Desired Flash state to transition into (ie FLASH_STATE_WRITING).
 * @retval SSP_SUCCESS              Successful.
 * @retval SSP_ERR_INVALID_ARGUMENT Supplied flash_type was something other than FLASH_TYPE_CODE_FLASH or
 *                                  FLASH_TYPE_DATA_FLASH.
 * @retval SSP_ERR_IN_USE           Flash is busy with an on-going operation.
 **********************************************************************************************************************/
static ssp_err_t setup_for_pe_mode (flash_lp_instance_ctrl_t * const p_ctrl, flash_type_t flash_type, flash_states_t flash_state)
{
    ssp_err_t err = SSP_SUCCESS;

    /** If the API is not ready return error. */
    if (g_flash_state != FLASH_STATE_READY)
    {
        err = SSP_ERR_IN_USE;
    }
    else
    {
        /** Lock the flash state. If failure return error. */
        if (SSP_SUCCESS != flash_lock_state(flash_state))
        {
            err = SSP_ERR_IN_USE;
        }
        else
        {
            /** Enter PE mode. If failure release software lock. */
            err = HW_FLASH_LP_pe_mode_enter(p_ctrl, flash_type);
            if (SSP_SUCCESS != err)
            {
                flash_ReleaseState();
            }
        }
    }

    /** Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * @brief   This function will validate the supplied address, and return information about the block in which
 *          it resides.
 *
 * @param[in]   addr          Code or Data Flash address to return info for.
 * @param[out]  p_block_info  Pointer to a caller allocated flash_block_info_t structure which will be filled
 *                            in by this function.
 * @retval false    Supplied address is neither a valid Code or Data Flash address for this part.
 * @retval true     Supplied address is valid and p_block_infocontains the details on this addresses block
 **********************************************************************************************************************/
bool flash_get_block_info (uint32_t addr, flash_block_info_t * p_block_info)
{
    const flash_fmi_block_info_t * pBInfo = NULL;
    uint32_t                 num_sections = 0U;
    uint32_t                 bnum         = 0U;
    uint32_t                 num_tbl_entries = 0U;
    bool                     ret_value = false;

    p_block_info->total_blocks            = 0U;

    /** Loop over code flash regions */
    for (uint32_t i = 0U; i < g_flash_code_region.num_regions; i++)
    {
        /** If the address is within the code flash region save region information*/
        if ((addr >= g_flash_code_region.p_block_array[i].block_section_st_addr) &&
                (addr <= g_flash_code_region.p_block_array[i].block_section_end_addr))
        {
            p_block_info->is_code_flash_addr = true;
            p_block_info->flash_type = FLASH_TYPE_CODE_FLASH;
            num_tbl_entries                  = g_flash_code_region.num_regions;
            pBInfo                           = &g_flash_code_region.p_block_array[0];
        }
    }
    /** Loop over data flash regions */
    for (uint32_t i = 0U; i < g_flash_data_region.num_regions; i++)
    {
        /** If the address is within the data flash region save region information */
        if ((addr >= g_flash_data_region.p_block_array[i].block_section_st_addr) &&
                (addr <= g_flash_data_region.p_block_array[i].block_section_end_addr))
        {
            p_block_info->is_code_flash_addr = false;
            p_block_info->flash_type = FLASH_TYPE_DATA_FLASH;
            num_tbl_entries                  = g_flash_data_region.num_regions;
            pBInfo                           = &g_flash_data_region.p_block_array[0];
        }
    }

    /** Loop over all regions in the section of flash containing the address. */
    while (num_sections < num_tbl_entries)
    {
        ret_value = true;
        num_sections++;

        /** Calculate the number of blocks in the region */
        uint32_t num_blocks = (((pBInfo->block_section_end_addr + 1U) - pBInfo->block_section_st_addr) /
                pBInfo->block_size);

        /** If the address is within the region save detailed region information. */
        if ((addr >= pBInfo->block_section_st_addr)  && (addr <= pBInfo->block_section_end_addr))
        {
            bnum                                 = ((addr - pBInfo->block_section_st_addr) / pBInfo->block_size);

            p_block_info->this_block_st_addr     = pBInfo->block_section_st_addr + (bnum * pBInfo->block_size);
            p_block_info->this_block_end_addr    = (p_block_info->this_block_st_addr + pBInfo->block_size) - 1;
            p_block_info->this_block_number      = bnum + p_block_info->total_blocks;
            p_block_info->block_section_st_addr  = pBInfo->block_section_st_addr;
            p_block_info->block_section_end_addr = pBInfo->block_section_end_addr;
            p_block_info->block_size             = pBInfo->block_size;
            p_block_info->num_blocks             = num_blocks;
            p_block_info->min_program_size       = pBInfo->block_size_write;
        }

        /** Update the end of memory address and go to the next region. */
        p_block_info->memory_end_addr =   pBInfo->block_section_end_addr;
        pBInfo++;

        /** Increment the total block count by the number of blocks in the region. */
        p_block_info->total_blocks += num_blocks;
    }

    return ret_value;
}

/*******************************************************************************************************************//**
 * @brief   This function verifies that FCLK falls within the allowable range and calculates the timeout values
 *          based on the current FCLK frequency.
 *
 * @retval SSP_SUCCESS               Success
 * @retval SSP_ERR_FCLK              FCLK must be >= 4 MHz.
 * @retval SSP_ERR_INVALID_ARGUMENT  Invalid argument provided to CGC call. Should never occur...
 **********************************************************************************************************************/
static ssp_err_t flash_setup (flash_lp_instance_ctrl_t * const p_ctrl)
{
    ssp_err_t err;
    bsp_feature_flash_lp flash_lp_feature = {0U};

    /** Get BSP specific information about the flash LP. */
    /* We need clock frequencies to calculate the worst case timeout values. Note S124 and S128 do not have a
     *  separate FCLK, they use ICLK */
    R_BSP_FeatureFlashLpGet(&flash_lp_feature);

    /** Save the flash sp information to the global parameters structure. */
    g_current_parameters.flash_cf_macros = flash_lp_feature.flash_cf_macros;   // S3A7 and S3A1 have 2 CF macros. That's a special case for blank check.
    g_current_parameters.cf_macro_size = flash_lp_feature.cf_macro_size;
    g_current_parameters.cf_memory_st_addr = g_flash_code_region.p_block_array[0].block_section_st_addr;              // Starting memory address for Code Flash
    g_current_parameters.df_memory_st_addr = g_flash_data_region.p_block_array[0].block_section_st_addr;              // Starting memory address for Data Flash

    /** Get the frequency of the clock driving the flash. If failure return error. */
    err = g_cgc_on_cgc.systemClockFreqGet((cgc_system_clocks_t)flash_lp_feature.flash_clock_src, &g_current_parameters.flash_clock_freq);
    if (SSP_SUCCESS == err)
    {
        /** FCLK must be a minimum of 4 MHz for Flash operations. If not return error. */
        if (g_current_parameters.flash_clock_freq < MINIMUM_SUPPORTED_FCLK_FREQ)
        {
            err = SSP_ERR_FCLK;
        }
        else
        {
            /** Get the frequency of the system clock. If failure return error. */
            err = g_cgc_on_cgc.systemClockFreqGet(CGC_SYSTEM_CLOCKS_ICLK, &g_current_parameters.system_clock_freq);
        }
    }

    if (SSP_SUCCESS == err)
    {
        /** Initialize the flash timeout calculations and transfer global parameters to code and data flash layers. */
        HW_FLASH_LP_init(p_ctrl, &g_current_parameters);
        HW_FLASH_LP_code_flash_set_flash_settings(&g_current_parameters);      // Give HW layer below access to the configured settings
        HW_FLASH_LP_data_flash_set_flash_settings(&g_current_parameters);      // Give HW layer below access to the configured settings

        /** Enable the DataFlash */
        HW_FLASH_LP_dataflash_enable(p_ctrl);
    }

    return err;
}

/*******************************************************************************************************************//**
 * @brief   This function performs the parameter checking required by the R_FLASH_LP_Write() function.
 *
 * @retval SSP_SUCCESS               Parameter checking completed without error.
 * @retval SSP_ERR_INVALID_SIZE      Number of bytes provided was not a multiple of the programming size or exceeded
 *                                   the maximum range.
 * @retval SSP_ERR_NOT_OPEN          The Flash API is not Open.
 * @retval SSP_ERR_INVALID_ADDRESS   Invalid address was input or address not on programming boundary.
 **********************************************************************************************************************/
static ssp_err_t flash_write_parameter_checking (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t flash_address, uint32_t const num_bytes)
{
    /** If null control block return error. */
    SSP_ASSERT(NULL != p_ctrl);
    volatile bool result1 = false;
    volatile bool result2 = false;
    volatile bool result3 = false;

    result1 = (bool)(flash_address >= g_block_info.block_section_st_addr);
    result2 = (bool)(flash_address <= g_block_info.block_section_end_addr);
    result3 = (bool)(!(flash_address & (g_block_info.min_program_size - 1U)));

    /** If invalid address return error. */
    FLASH_ERROR_RETURN((result1 && result2 && result3), SSP_ERR_INVALID_ADDRESS);

    result1 = (bool)(((num_bytes - 1U) + flash_address) > g_block_info.block_section_end_addr);
    result2 = (bool)((num_bytes & (g_block_info.min_program_size - 1U)));
    result3 = (bool)(0 == num_bytes);

    /** If invalid number of bytes return error. */
    FLASH_ERROR_RETURN(!(result1 || result2 || result3), SSP_ERR_INVALID_SIZE);

    return(SSP_SUCCESS);
}

/*******************************************************************************************************************//**
 * @brief   This function does the completion setup for the R_FLASH_HP_Open() function.
 *
 **********************************************************************************************************************/
static ssp_err_t flash_open_setup (flash_lp_instance_ctrl_t * const p_ctrl, ssp_feature_t *p_ssp)
{
    ssp_err_t err = SSP_SUCCESS;

    /** Release state so other operations can be performed. */
    flash_ReleaseState();

    /** Flash setup. */
    err = flash_setup(p_ctrl);    // Check FCLK, calculate timeout values

    /** If successful mark control block open otherwise release hardware lock and return error. */
    if (err != SSP_SUCCESS)
    {
        /* Return the hardware lock for the Flash */
        R_BSP_HardwareUnlock(p_ssp);
    }
    else
    {
        p_ctrl->opened = FLASH_OPEN;
    }

    return err;
}
/*******************************************************************************************************************//**
 * @brief   This function initiates the write sequence for the R_FLASH_LP_Write() function.
 *
 **********************************************************************************************************************/
static ssp_err_t flash_write_initiate (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const src_start_address, uint32_t dest_start_address, uint32_t num_bytes)
{
    ssp_err_t err;

    /** Set up PE mode */
    err = setup_for_pe_mode(p_ctrl, g_block_info.flash_type, FLASH_STATE_WRITING);

    /** If successful */
    if (SSP_SUCCESS == err)
    {
        /** Configure the current parameters for code flash or data flash depending on address. */
        if (g_block_info.is_code_flash_addr == true)
        {
            g_current_parameters.wait_cnt          = g_current_parameters.wait_max_write_cf;
            g_current_parameters.current_operation = FLASH_OPERATION_CF_WRITE;
            g_current_parameters.min_pgm_size      = (g_block_info.min_program_size >> 1);
        }
        else
        {
            g_current_parameters.wait_cnt = g_current_parameters.wait_max_write_df;
            g_current_parameters.min_pgm_size      = g_block_info.min_program_size;  // For MF3 DF it's always 1

            if (g_current_parameters.bgo_enabled_df == false)
            {
                g_current_parameters.current_operation = FLASH_OPERATION_DF_WRITE;
            }
            else
            {
                g_current_parameters.current_operation = FLASH_OPERATION_DF_BGO_WRITE;
            }
        }

        /** Write the data */
        err = HW_FLASH_LP_write(p_ctrl, src_start_address, dest_start_address, num_bytes, g_block_info.min_program_size);
        if (SSP_SUCCESS == err)
        {
            /** If in non-BGO mode, then current operation is completed. Exit PE mode and return status */
            if (g_current_parameters.current_operation != FLASH_OPERATION_DF_BGO_WRITE)
            {
                err = HW_FLASH_LP_pe_mode_exit(p_ctrl);
            }
        }
    }
    return(err);
}


/*******************************************************************************************************************//**
 * @brief   This function performs the final cleanup for the erase, write and blankcheck functions. Executes a reset
 *          if any error has been indicated. Releases the state if the current operation is NOT BGO.
 *
 **********************************************************************************************************************/
static void flash_operation_complete (flash_lp_instance_ctrl_t * const p_ctrl, ssp_err_t err)
{
    /** SSP_ERR_IN_USE will be returned if we are in the process of executing a BGO operation. In this case we do
     * not want to take any action as that will terminate the in process operation */
    if ((g_current_parameters.bgo_enabled_df == true) &&
        ((g_current_parameters.current_operation == FLASH_OPERATION_DF_BGO_ERASE) ||
         (g_current_parameters.current_operation == FLASH_OPERATION_DF_BGO_WRITE) ||
         (g_current_parameters.current_operation == FLASH_OPERATION_DF_BGO_BLANKCHECK)))
    {
        if ((err == SSP_ERR_IN_USE) || (err == SSP_SUCCESS))
        {
            return;
        }
    }
    else
    {
        /** We are not in a BGO mode, so this operation is complete. Release the software lock. */
        flash_ReleaseState();

        /** If failure reset the flash. */
        if (err != SSP_SUCCESS)
        {
            HW_FLASH_LP_reset(p_ctrl);
        }
    }
}

#if (FLASH_CFG_PARAM_CHECKING_ENABLE == 1)
/*******************************************************************************************************************//**
 * @brief   This function performs the parameter checking required by the R_FLASH_HP_Erase() function.
 *
 * @retval SSP_SUCCESS               Parameter checking completed without error.
 * @retval SSP_ERR_INVALID_BLOCKS    Invalid number of blocks specified
 * @retval SSP_ERR_NOT_OPEN          The Flash API is not Open.
 * @retval SSP_ERR_INVALID_ARGUMENT  Code Flash Programming is not enabled and a request to erase CF was requested.
 **********************************************************************************************************************/
static ssp_err_t flash_erase_parameter_checking (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const num_blocks)
{
    /** If null pointer return error. */
    SSP_ASSERT(NULL != p_ctrl);
    volatile bool result1 = false;
    volatile bool result2 = false;
    volatile bool result3 = false;

    /** If invalid number of blocks return error. */
    result1 = (bool)(num_blocks > g_block_info.total_blocks);
    result2 = (bool)(num_blocks > (g_block_info.total_blocks - g_block_info.this_block_number));
    result3 = (bool)(num_blocks <= 0U);

    FLASH_ERROR_RETURN(!(result1 || result2 || result3), SSP_ERR_INVALID_BLOCKS);

    /** If not open return error */
    FLASH_ERROR_RETURN((FLASH_OPEN == p_ctrl->opened), SSP_ERR_NOT_OPEN);

    /** If Code Flash Programming is not enabled, then the requisite functions are NOT in RAM and we can not allow an Erase request for
     * Code Flash */
#if (FLASH_CFG_PARAM_CODE_FLASH_PROGRAMMING_ENABLE == 0)
    FLASH_ERROR_RETURN((g_block_info.is_code_flash_addr == false), SSP_ERR_INVALID_ARGUMENT);
#endif

    return(SSP_SUCCESS);
}
#endif
/*******************************************************************************************************************//**
 * @brief   This function performs the parameter checking required by the R_FLASH_HP_BlankCheck() function.
 *
 * @retval SSP_SUCCESS               Parameter checking completed without error.
 * @retval SSP_ERR_INVALID_BLOCKS    Invalid number of blocks specified
 * @retval SSP_ERR_NOT_OPEN          The Flash API is not Open.
 * @retval SSP_ERR_IN_USE            The Flash peripheral is busy with a prior on-going transaction.
 **********************************************************************************************************************/
static ssp_err_t flash_blank_check_parameter_checking (flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const address, uint32_t num_bytes)
{
    ssp_err_t err = SSP_SUCCESS;

    SSP_ASSERT(NULL != p_ctrl);

    /** If not open return error. */
    FLASH_ERROR_RETURN((FLASH_OPEN == p_ctrl->opened), SSP_ERR_NOT_OPEN);

    /** If invalid address or bytes return error. */
    err = flash_blank_check_address_checking (address, num_bytes);
    FLASH_ERROR_RETURN((err == SSP_SUCCESS), err);

    /* If the flash is not in the ready state return error. */
    FLASH_ERROR_RETURN((g_flash_state == FLASH_STATE_READY), SSP_ERR_IN_USE);

    return(err);
}

/*******************************************************************************************************************//**
 * @brief   This function performs the address checking required by the R_FLASH_HP_BlankCheck() function.
 *
 * @retval SSP_SUCCESS               Parameter checking completed without error.
 * @retval SSP_ERR_INVALID_ADDRESS   Invalid data flash address was input.
 * @retval SSP_ERR_INVALID_SIZE      'num_bytes' was either too large or not aligned for the CF/DF boundary size.
 **********************************************************************************************************************/
static ssp_err_t flash_blank_check_address_checking (uint32_t const address, uint32_t num_bytes)
{
    ssp_err_t err = SSP_SUCCESS;
    volatile bool result1 = false;
    volatile bool result2 = false;

    result1 = (bool)(address >= g_block_info.block_section_st_addr);
    result2 = (bool)(address <= g_block_info.block_section_end_addr);
    FLASH_ERROR_RETURN((result1 && result2), SSP_ERR_INVALID_ADDRESS);


    result1 = (bool)((num_bytes + address) > (g_block_info.block_section_end_addr + 1U));
    result2 = (bool)(0 == num_bytes);

    FLASH_ERROR_RETURN(!(result1 || result2), SSP_ERR_INVALID_SIZE);


    /** Is this a request to Blank Check Data Flash?.  If so, num_bytes must be a multiple of the programming size */
    if (g_block_info.is_code_flash_addr == false)
    {
        FLASH_ERROR_RETURN(!(num_bytes & (g_block_info.min_program_size - 1U)), SSP_ERR_INVALID_SIZE);
    }

    return(err);
}



/*******************************************************************************************************************//**
 * @brief   This function performs the Blank check phase required by the R_FLASH_HP_BlankCheck() function.
 *
 * @retval SSP_SUCCESS           Setup completed completed without error.
 * @retval SSP_ERR_PE_FAILURE    Failed to enter P/E mode
 **********************************************************************************************************************/

static ssp_err_t flash_blank_check_initiate ( flash_lp_instance_ctrl_t * const p_ctrl, uint32_t const address, uint32_t num_bytes, flash_result_t *p_blank_check_result)
{
    ssp_err_t err = SSP_SUCCESS;

    /** Configure the current operation and wait count based on the number of bytes and if it's a data flash or code flash operation. */
    /* Is this a request to Blank check Code Flash? */
    if (g_block_info.is_code_flash_addr == true)
    {
        /* This is a request to Blank check Code Flash */
        /*No errors in parameters. Enter Code Flash PE mode*/
        /* wait_max_blank_check specifies the wait time for a 4 ROM byte blank check.
         * num_bytes is divided by 4 and then multiplied to calculate the wait time for the entire operation */
        if (num_bytes < (uint32_t)4)
        {
            g_current_parameters.wait_cnt = g_current_parameters.wait_max_blank_check * num_bytes;
        }
        else
        {
            g_current_parameters.wait_cnt = (g_current_parameters.wait_max_blank_check * (num_bytes >> 2));
        }
        g_current_parameters.current_operation = FLASH_OPERATION_CF_BLANKCHECK;
    }
    else
    {
        /* This is a request to Blank check Data Flash */
        /* No errors in parameters. Enter Data Flash PE mode*/
        /* wait_max_blank_check specifies the wait time for a 4 ROM byte blank check. This is the same as the
         * wait time for a 1 byte Data Flash blankcheck.*/
        g_current_parameters.wait_cnt = g_current_parameters.wait_max_blank_check * num_bytes;
        if (g_current_parameters.bgo_enabled_df == false)
        {
            g_current_parameters.current_operation = FLASH_OPERATION_DF_BLANKCHECK;
        }
        else
        {
            g_current_parameters.current_operation = FLASH_OPERATION_DF_BGO_BLANKCHECK;
        }
    }

    /** Start the Blank check operation. */
    /* If BGO is enabled then the result of the Blank check will be
     * available when the interrupt occurs and p_blank_check_result will contain FLASH_RESULT_BGO_ACTIVE */
    err = HW_FLASH_LP_blankcheck(p_ctrl, address, num_bytes, p_blank_check_result);

    if (SSP_SUCCESS == err)
    {
        /** If in non-BGO mode, then current operation is complete. Exit PE mode and return status */
        if (g_current_parameters.current_operation != FLASH_OPERATION_DF_BGO_BLANKCHECK)
        {
            err = HW_FLASH_LP_pe_mode_exit(p_ctrl);
            g_current_parameters.current_operation = FLASH_OPERATION_IDLE;
        }
    }
    return(err);
}

/*******************************************************************************************************************//**
 * @brief   This function initializes data required by the Flash based on information read from the FMI.
 *
 * @retval SSP_SUCCESS               FMI based setup success.
 * @retval SSP_ERR_IN_USE            The Flash peripheral is busy with a prior on-going transaction.
 * @retval SSP_ERR_ASSERTION         Problem getting FMI information.
 * @retval SSP_ERR_HW_LOCKED         Unable to get the HW lock for the Flash.
 *
 **********************************************************************************************************************/
static ssp_err_t flash_fmi_setup (flash_lp_instance_ctrl_t * const p_ctrl, flash_cfg_t const * const p_cfg, ssp_feature_t *p_ssp)
{
    ssp_err_t err = SSP_SUCCESS;

    p_ssp->id = SSP_IP_CFLASH;
    p_ssp->channel = 0U;
    p_ssp->unit = 0U;
    fmi_feature_info_t info = {0};

    /** Initialize the code flash region data. */
    g_fmi_on_fmi.productFeatureGet(p_ssp, &info);
    g_flash_code_region.num_regions = info.channel_count;
    g_flash_code_region.p_block_array = (flash_fmi_block_info_t const *) info.ptr_extended_data;

    /** Initialize the data flash region data. */
    p_ssp->id = SSP_IP_DFLASH;
    g_fmi_on_fmi.productFeatureGet(p_ssp, &info);
    g_flash_data_region.num_regions = info.channel_count;
    g_flash_data_region.p_block_array = (flash_fmi_block_info_t const *) info.ptr_extended_data;

    p_ssp->id = SSP_IP_FCU;
    err = g_fmi_on_fmi.productFeatureGet(p_ssp, &info);
    FLASH_ERROR_RETURN(SSP_SUCCESS == err, err);

    p_ctrl->p_reg = (R_FACI_Type *) info.ptr;

    /** Initialize event information. */
    ssp_vector_info_t * p_vector_info;
    fmi_event_info_t event_info = {(IRQn_Type) 0};
    g_fmi_on_fmi.eventInfoGet(p_ssp, SSP_SIGNAL_FCU_FRDYI, &event_info);
    p_ctrl->irq = event_info.irq;
    if (SSP_INVALID_VECTOR != p_ctrl->irq)
    {
        R_SSP_VectorInfoGet(p_ctrl->irq, &p_vector_info);
        NVIC_SetPriority(p_ctrl->irq, p_cfg->irq_ipl);
        *(p_vector_info->pp_ctrl) = p_ctrl;
    }

    /** Take the hardware lock for the Flash. If failure return error. */
    FLASH_ERROR_RETURN(!(SSP_SUCCESS != R_BSP_HardwareLock(p_ssp)), SSP_ERR_HW_LOCKED);
    return(SSP_SUCCESS);
}


