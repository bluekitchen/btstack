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
 * File Name    : r_dtc.c
 * Description  : DTC implementation of the transfer interface.
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <string.h>
#include "r_dtc.h"
#include "hw/hw_dtc_private.h"
#include "r_dtc_private_api.h"
#include "r_elc.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** Driver ID (DTC in ASCII), used to identify Data Transfer Controller (DTC) configuration  */
#define DTC_ID (0x44544300)

/** Macro for error logger. */
#ifndef DTC_ERROR_RETURN
/*LDRA_INSPECTED 77 S This macro does not work when surrounded by parentheses. */
#define DTC_ERROR_RETURN(a, err) SSP_ERROR_RETURN((a), (err),&g_module_name[0], &g_dtc_version)
#endif

/** Size of vector table is based on number of vectors defined in BSP. */
#define DTC_VECTOR_TABLE_SIZE (BSP_MAX_NUM_IRQn)

/** Compiler specific macro to specify vector table section. */
#ifndef DTC_CFG_VECTOR_TABLE_SECTION_NAME
#ifndef SUPPRESS_WARNING_DTC_CFG_VECTOR_TABLE_SECTION_NAME
#warning "DTC vector table is aligned on 1K boundary. Automatic placing could lead to memory holes."
#endif
#endif

/** Used to generate a compiler error (divided by 0 error) if the assertion fails.  This is used in place of "#error"
 * for expressions that cannot be evaluated by the preprocessor like sizeof(). */
/*LDRA_INSPECTED 340 S Using function like macro because inline function does not generate divided by 0 error. */
#define DTC_COMPILE_TIME_ASSERT(e) ((void) sizeof(char[1 - 2 * !(e)]))

#define DTC_PRV_MASK_ALIGN_2_BYTES     (0x1U)
#define DTC_PRV_MASK_ALIGN_4_BYTES     (0x3U)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
static IRQn_Type dtc_irq_lookup(elc_event_t const event);
static void r_dtc_initialize_repeat_block_mode(transfer_cfg_t const * const    p_cfg);
static ssp_err_t r_dtc_state_initialize(dtc_instance_ctrl_t * p_ctrl,transfer_cfg_t const * const    p_cfg );
#if DTC_CFG_PARAM_CHECKING_ENABLE
static ssp_err_t r_dtc_parameter_check(dtc_instance_ctrl_t * p_ctrl,transfer_cfg_t const * const    p_cfg );
static ssp_err_t r_dtc_block_reset_parameter_check(dtc_instance_ctrl_t * p_ctrl,uint16_t const length, uint16_t const num_transfers);
static ssp_err_t r_dtc_reset_parameter_check(dtc_instance_ctrl_t * p_ctrl,uint16_t const num_transfers);
static ssp_err_t r_dtc_enable_alignment_check(transfer_info_t * p_info);
#endif


/***********************************************************************************************************************
 * ISR function prototypes
 **********************************************************************************************************************/
void elc_software_event_isr(void);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
/** Name of module used by error logger macro */
#if BSP_CFG_ERROR_LOG != 0
static const char g_module_name[] = "dtc";
#endif

/** Stores initialization state to skip initialization in ::R_DTC_Open after the first call. */
static bool g_dtc_state_initialized = false;

/** Stores pointer to DTC base address. */
static R_DTC_Type * gp_dtc_regs = NULL;

/** Stores pointer to ICU base address. */
static R_ICU_Type * gp_icu_regs = NULL;

#ifdef  DTC_CFG_VECTOR_TABLE_SECTION_NAME
#define DTC_SECTION_ATTRIBUTE   BSP_PLACE_IN_SECTION_V2(DTC_CFG_VECTOR_TABLE_SECTION_NAME)
#else
#define DTC_SECTION_ATTRIBUTE
#endif
/*LDRA_INSPECTED 293 S Compiler specific attributes are the best way to define section and alignment of variable. */
/*LDRA_INSPECTED 219 S In the GCC compiler, section placement requires a GCC attribute, which starts with underscore. */
/*This variable is not a part of data section so it is not initialized here. It is being initialized to zero later in the code, before it is being used. */
/*LDRA_INSPECTED 57 D *//*LDRA_INSPECTED 57 D */
/*LDRA_INSPECTED 27 D variable is declared as static */
static transfer_info_t * gp_dtc_vector_table[BSP_VECTOR_TABLE_MAX_ENTRIES] BSP_ALIGN_VARIABLE_V2(1024) DTC_SECTION_ATTRIBUTE;

#if defined(__GNUC__)
/* This structure is affected by warnings from a GCC compiler bug. This pragma suppresses the warnings in this 
 * structure only.*/
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
/** Version data structure used by error logger macro. */
static const ssp_version_t g_dtc_version =
{
    .api_version_minor  = TRANSFER_API_VERSION_MINOR,
    .api_version_major  = TRANSFER_API_VERSION_MAJOR,
    .code_version_major = DTC_CODE_VERSION_MAJOR,
    .code_version_minor = DTC_CODE_VERSION_MINOR,
};
#if defined(__GNUC__)
/* Restore warning settings for 'missing-field-initializers' to as specified on command line. */
/*LDRA_INSPECTED 69 S */
#pragma GCC diagnostic pop
#endif

/***********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/
/** DTC implementation of transfer API. */
/*LDRA_INSPECTED 27 D This structure must be accessible in user code. It cannot be static. */
const transfer_api_t g_transfer_on_dtc =
{
    .open       = R_DTC_Open,
    .reset      = R_DTC_Reset,
    .infoGet    = R_DTC_InfoGet,
    .start      = R_DTC_Start,
    .stop       = R_DTC_Stop,
    .enable     = R_DTC_Enable,
    .disable    = R_DTC_Disable,
    .close      = R_DTC_Close,
    .versionGet = R_DTC_VersionGet,
    .blockReset = R_DTC_BlockReset
};

/*******************************************************************************************************************//**
 * @addtogroup DTC
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief  Set transfer data in the vector table and enable transfer in ICU. Implements transfer_api_t::open.
 *
 * @retval SSP_SUCCESS           Successful open.  Transfer is configured and will start when trigger occurs.
 * @retval SSP_ERR_ASSERTION     An input parameter is invalid.
 * @retval SSP_ERR_IN_USE        The BSP hardware lock for the DTC is not available, or the index for this IRQ in the
 *                               DTC vector table is already configured.
 * @retval SSP_ERR_HW_LOCKED     DTC hardware resource is locked.
 * @retval SSP_ERR_IRQ_BSP_DISABLED  The IRQ associated with the activation source is not enabled in the BSP.
 * @retval SSP_ERR_NOT_ENABLED   Auto-enable was requested, but enable failed due to an invalid input parameter.
 **********************************************************************************************************************/
ssp_err_t R_DTC_Open (transfer_ctrl_t         * const p_api_ctrl,
                      transfer_cfg_t const * const    p_cfg)
{
    ssp_err_t err = SSP_SUCCESS;
    /* Generate a compiler error if transfer_info_t and dtc_reg_t are not the same size. */
    /*LDRA_INSPECTED 57 S No effect on runtime code, used for compiler error. */
    DTC_COMPILE_TIME_ASSERT(sizeof(transfer_info_t) == sizeof(dtc_reg_t));

    dtc_instance_ctrl_t * p_ctrl = (dtc_instance_ctrl_t *) p_api_ctrl;

    err = r_dtc_state_initialize(p_ctrl,p_cfg);
    DTC_ERROR_RETURN(SSP_SUCCESS == err, err);

    /** Make sure the activation source is mapped in the ICU. */
    IRQn_Type                irq     = dtc_irq_lookup(p_cfg->activation_source);
    p_ctrl->trigger                  = HW_DTC_ICUEventGet(gp_icu_regs, irq);
    DTC_ERROR_RETURN(SSP_INVALID_VECTOR != irq, SSP_ERR_IRQ_BSP_DISABLED);

    /** Make sure the activation source is not already being used by the DTC. */
    DTC_ERROR_RETURN(NULL == gp_dtc_vector_table[irq], SSP_ERR_IN_USE);
    r_dtc_initialize_repeat_block_mode(p_cfg);

#if DTC_CFG_SOFTWARE_START_ENABLE
    /** If p_callback is valid and trigger is set to ELC_EVENT_ELC_SOFTWARE_EVENT_0 or ELC_EVENT_ELC_SOFTWARE_EVENT_1,
     *  enable interrupts and store the p_callback in an array for access by ISR. */
    if ((ELC_EVENT_ELC_SOFTWARE_EVENT_0 == p_ctrl->trigger) ||
            (ELC_EVENT_ELC_SOFTWARE_EVENT_1 == p_ctrl->trigger))
    {
        p_ctrl->p_callback = p_cfg->p_callback;
        p_ctrl->p_context  = p_cfg->p_context;
        if (NULL != p_cfg->p_callback)
        {
            ssp_vector_info_t * p_vector_info;
            R_SSP_VectorInfoGet(irq, &p_vector_info);
            *(p_vector_info->pp_ctrl) = p_ctrl;
            NVIC_SetPriority(irq, p_cfg->irq_ipl);
            R_BSP_IrqStatusClear(irq);
            NVIC_ClearPendingIRQ(irq);
            NVIC_EnableIRQ(irq);
        }
    }
#endif

    /** Configure the DTC transfer. See the hardware manual for details. */
    HW_DTC_ReadSkipEnableSet(gp_dtc_regs, DTC_READ_SKIP_DISABLED);
    gp_dtc_vector_table[irq] = p_cfg->p_info;

    /** Update internal variables. */
    p_ctrl->irq     = irq;
    /** Mark driver as open by initializing it to "DTC" in its ASCII equivalent. */
    p_ctrl->id      = DTC_ID;

    /** If auto_enable is true, enable transfer and ELC events if software start is used. */
    if (p_cfg->auto_enable)
    {
        err = R_DTC_Enable(p_ctrl);
    }

    /** Enable read skip after all settings are complete. */
    HW_DTC_ReadSkipEnableSet(gp_dtc_regs, DTC_READ_SKIP_ENABLED);

    DTC_ERROR_RETURN(SSP_SUCCESS == err, SSP_ERR_NOT_ENABLED);

    return SSP_SUCCESS;
} /* End of function R_DTC_Open */

/*******************************************************************************************************************//**
 * @brief  Reset transfer source, destination, and number of transfers.  Implements transfer_api_t::reset.
 *
 * @retval SSP_SUCCESS           Transfer reset successfully.
 * @retval SSP_ERR_ASSERTION     An input parameter is invalid.
 * @retval SSP_ERR_NOT_OPEN      Handle is not initialized.  Call R_DTC_Open to initialize the control block.
 * @retval SSP_ERR_NOT_ENABLED   Transfer length must not be 0 for repeat and block mode, or enable failed due
 *                               to an invalid input parameter:
 *                                 - Transfer source must not be NULL.
 *                                 - Transfer destination must not be NULL.
 **********************************************************************************************************************/
ssp_err_t R_DTC_Reset (transfer_ctrl_t         * const   p_api_ctrl,
                       void const * volatile             p_src,
                       void                   * volatile p_dest,
                       uint16_t const                    num_transfers)
{
    ssp_err_t err = SSP_SUCCESS;
    dtc_instance_ctrl_t * p_ctrl = (dtc_instance_ctrl_t *) p_api_ctrl;
#if DTC_CFG_PARAM_CHECKING_ENABLE
    err =  r_dtc_reset_parameter_check(p_ctrl,num_transfers );
    DTC_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif

    /** Disable transfers on this activation source. */
    HW_ICU_DTCDisable(gp_icu_regs, p_ctrl->irq);

    /** Disable read skip prior to modifying settings. It will be enabled later. */
    HW_DTC_ReadSkipEnableSet(gp_dtc_regs, DTC_READ_SKIP_DISABLED);

    /** Reset transfer based on input parameters. */
    if (NULL != p_src)
    {
        gp_dtc_vector_table[p_ctrl->irq]->p_src = p_src;
    }

    if (NULL != p_dest)
    {
        gp_dtc_vector_table[p_ctrl->irq]->p_dest = p_dest;
    }

    if (TRANSFER_MODE_BLOCK == gp_dtc_vector_table[p_ctrl->irq]->mode)
    {
        gp_dtc_vector_table[p_ctrl->irq]->num_blocks = num_transfers;
    }
    else if (TRANSFER_MODE_NORMAL == gp_dtc_vector_table[p_ctrl->irq]->mode)
    {
        gp_dtc_vector_table[p_ctrl->irq]->length = num_transfers;
    }
    else /* (TRANSFER_MODE_REPEAT == gp_dtc_vector_table[p_ctrl->irq]->mode) */
    {
        /* Do nothing. */
    }

    /** Enables transfers on this activation source. */
    err = R_DTC_Enable(p_ctrl);

    /** Enable read skip after all settings are complete. */
    HW_DTC_ReadSkipEnableSet(gp_dtc_regs, DTC_READ_SKIP_ENABLED);

    DTC_ERROR_RETURN(SSP_SUCCESS == err, SSP_ERR_NOT_ENABLED);

    return SSP_SUCCESS;
} /* End of function R_DTC_Reset */

/*******************************************************************************************************************//**
 * @brief  Start transfer. Implements transfer_api_t::start.
 *
 * @retval SSP_SUCCESS              Transfer started successfully.
 * @retval SSP_ERR_ASSERTION        An input parameter is invalid.
 * @retval SSP_ERR_NOT_OPEN         Handle is not initialized.  Call R_DMAC_Open to initialize the control block.
 * @retval SSP_ERR_UNSUPPORTED      One of the following is invalid:
 *                                    - Handle was not configured for software activation.
 *                                    - Mode not set to TRANSFER_START_MODE_SINGLE.
 *                                    - DTC_SOFTWARE_START_ENABLE set to 0 (disabled) in ssp_cfg/driver/r_dtc_cfg.h.
 **********************************************************************************************************************/
ssp_err_t R_DTC_Start (transfer_ctrl_t         * const p_api_ctrl,
                       transfer_start_mode_t           mode)
{
    dtc_instance_ctrl_t * p_ctrl = (dtc_instance_ctrl_t *) p_api_ctrl;
#if DTC_CFG_SOFTWARE_START_ENABLE
#if DTC_CFG_PARAM_CHECKING_ENABLE
    /** Verify parameters are valid */
    SSP_ASSERT(NULL != p_ctrl);
    DTC_ERROR_RETURN(p_ctrl->id == DTC_ID, SSP_ERR_NOT_OPEN);
    DTC_ERROR_RETURN(TRANSFER_START_MODE_SINGLE == mode, SSP_ERR_UNSUPPORTED);
    DTC_ERROR_RETURN((ELC_EVENT_ELC_SOFTWARE_EVENT_0 == p_ctrl->trigger) ||
            (ELC_EVENT_ELC_SOFTWARE_EVENT_1 == p_ctrl->trigger), SSP_ERR_UNSUPPORTED);
#else
    SSP_PARAMETER_NOT_USED(mode);
#endif


    /** Clear the interrupt status flag */
    R_BSP_IrqStatusClear(p_ctrl->irq);

    /** Generate a software event in the Event Link Controller */
    if (ELC_EVENT_ELC_SOFTWARE_EVENT_0 == p_ctrl->trigger)
    {
        g_elc_on_elc.softwareEventGenerate(ELC_SOFTWARE_EVENT_0);
    }
    else
    {
        g_elc_on_elc.softwareEventGenerate(ELC_SOFTWARE_EVENT_1);
    }

    return SSP_SUCCESS;
#else
    SSP_PARAMETER_NOT_USED(p_ctrl);
    SSP_PARAMETER_NOT_USED(mode);
    return SSP_ERR_UNSUPPORTED;
#endif
} /* End of function R_DTC_Start */

/*******************************************************************************************************************//**
 * @brief  Placeholder for unsupported stop function. Implements transfer_api_t::stop.
 *
 * @retval SSP_ERR_UNSUPPORTED      DTC software start is not supported.
 **********************************************************************************************************************/
ssp_err_t R_DTC_Stop (transfer_ctrl_t         * const p_ctrl)
{
    /* This function isn't supported.  It is defined only to implement a required function of transfer_api_t. */
    /** Mark the input parameter as unused since this function isn't supported. */
    SSP_PARAMETER_NOT_USED(p_ctrl);

    return SSP_ERR_UNSUPPORTED;
} /* End of function R_DTC_Stop */

/*******************************************************************************************************************//**
 * @brief  Enable transfer and ELC events if they are used for software start. Implements transfer_api_t::enable.
 *
 * @retval SSP_SUCCESS              Counter value written successfully.
 * @retval SSP_ERR_ASSERTION        An input parameter is invalid.
 * @retval SSP_ERR_NOT_OPEN         Handle is not initialized.  Call R_DTC_Open to initialize the control block.
 * @retval SSP_ERR_IRQ_BSP_DISABLED The IRQ associated with the p_ctrl is not enabled in the BSP.
 **********************************************************************************************************************/
ssp_err_t R_DTC_Enable (transfer_ctrl_t         * const p_api_ctrl)
{
    dtc_instance_ctrl_t * p_ctrl = (dtc_instance_ctrl_t *) p_api_ctrl;
#if DTC_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(NULL != p_ctrl);
    DTC_ERROR_RETURN(p_ctrl->id == DTC_ID, SSP_ERR_NOT_OPEN);
    DTC_ERROR_RETURN(SSP_INVALID_VECTOR != (IRQn_Type)p_ctrl->irq, SSP_ERR_IRQ_BSP_DISABLED);
    transfer_info_t * p_local_info = gp_dtc_vector_table[p_ctrl->irq];
    SSP_ASSERT(NULL != p_local_info->p_dest);
    SSP_ASSERT(NULL != p_local_info->p_src);

    ssp_err_t err = r_dtc_enable_alignment_check(p_local_info);
    DTC_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif

    /** Enable transfer. */
    HW_ICU_DTCEnable(gp_icu_regs, p_ctrl->irq);

#if DTC_CFG_SOFTWARE_START_ENABLE
    if ((NULL != p_ctrl->p_callback) && ((ELC_EVENT_ELC_SOFTWARE_EVENT_0 == p_ctrl->trigger) ||
            (ELC_EVENT_ELC_SOFTWARE_EVENT_1 == p_ctrl->trigger)))
    {
        /** If this is a software event, enable the IRQ. */
        NVIC_EnableIRQ(p_ctrl->irq);
    }
#endif

    return SSP_SUCCESS;
} /* End of function R_DTC_Enable */

/*******************************************************************************************************************//**
 * @brief  Disable transfer. Implements transfer_api_t::disable.
 *
 * @retval SSP_SUCCESS              Counter value written successfully.
 * @retval SSP_ERR_ASSERTION        An input parameter is invalid.
 * @retval SSP_ERR_NOT_OPEN         Handle is not initialized.  Call R_DTC_Open to initialize the control block.
 **********************************************************************************************************************/
ssp_err_t R_DTC_Disable (transfer_ctrl_t         * const p_api_ctrl)
{
    dtc_instance_ctrl_t * p_ctrl = (dtc_instance_ctrl_t *) p_api_ctrl;
#if DTC_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(NULL != p_ctrl);
    DTC_ERROR_RETURN(p_ctrl->id == DTC_ID, SSP_ERR_NOT_OPEN);
#endif

    /** Disable transfer. */
    HW_ICU_DTCDisable(gp_icu_regs, p_ctrl->irq);

#if DTC_CFG_SOFTWARE_START_ENABLE
    if ((NULL != p_ctrl->p_callback) && ((ELC_EVENT_ELC_SOFTWARE_EVENT_0 == p_ctrl->trigger) ||
            (ELC_EVENT_ELC_SOFTWARE_EVENT_1 == p_ctrl->trigger)))
    {
        /** If this is a software event, enable the IRQ. */
        if (SSP_INVALID_VECTOR != p_ctrl->irq)
        {
            NVIC_DisableIRQ(p_ctrl->irq);
        }
    }
#endif

    return SSP_SUCCESS;
} /* End of function R_DTC_Disable */

/*******************************************************************************************************************//**
 * @brief  Set driver specific information. Implements transfer_api_t::infoGet.
 *
 * @retval SSP_SUCCESS              Counter value written successfully.
 * @retval SSP_ERR_ASSERTION        An input parameter is invalid.
 * @retval SSP_ERR_NOT_OPEN         Handle is not initialized. Call R_DTC_Open to initialize the control block.
 **********************************************************************************************************************/
ssp_err_t R_DTC_InfoGet   (transfer_ctrl_t        * const p_api_ctrl,
                           transfer_properties_t  * const p_info)
{
    dtc_instance_ctrl_t * p_ctrl = (dtc_instance_ctrl_t *) p_api_ctrl;
#if DTC_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(NULL != p_ctrl);
    DTC_ERROR_RETURN(p_ctrl->id == DTC_ID, SSP_ERR_NOT_OPEN);
    SSP_ASSERT(NULL != p_info);
#endif

    /** If a transfer is active, store it in p_in_progress. */
    uint16_t status = HW_DTC_StatusGet(gp_dtc_regs);
    p_info->in_progress = false;
    if (status >> 15)
    {
        IRQn_Type irq = (IRQn_Type) (status & 0x00FF);
        if (p_ctrl->irq == irq)
        {
            p_info->in_progress = true;
        }
    }

    /** Transfer information for the activation source is taken from DTC vector table. */
    transfer_info_t* p_dtc_transfer_info = gp_dtc_vector_table[p_ctrl->irq];
    if (TRANSFER_MODE_BLOCK == p_dtc_transfer_info->mode)
    {
        p_info->transfer_length_remaining = p_dtc_transfer_info->num_blocks;
    }
    else
    {
        p_info->transfer_length_remaining = p_dtc_transfer_info->length;

        /** Mask out the high byte in case of repeat transfer. */
        if (TRANSFER_MODE_REPEAT == p_dtc_transfer_info->mode)
        {
            p_info->transfer_length_remaining &= 0x00FFU;
        }
    }

    /** Store maximum transfer length. */
    if (TRANSFER_MODE_NORMAL == gp_dtc_vector_table[p_ctrl->irq]->mode)
    {
        p_info->transfer_length_max = DTC_NORMAL_MAX_LENGTH;
    }
    else
    {
        p_info->transfer_length_max = DTC_REPEAT_BLOCK_MAX_LENGTH;
    }

    return SSP_SUCCESS;
} /* End of function R_DTC_InfoGet */

/*******************************************************************************************************************//**
 * @brief      Disables transfer in the ICU, then clears transfer data from the DTC vector table. Implements
 *             transfer_api_t::close.
 *
 * @retval SSP_SUCCESS              Successful close.
 * @retval SSP_ERR_ASSERTION        An input parameter is invalid.
 * @retval SSP_ERR_NOT_OPEN         Handle is not initialized.  Call R_DTC_Open to initialize the control block.
 * @retval SSP_ERR_IRQ_BSP_DISABLED The IRQ associated with the p_ctrl is not enabled in the BSP.
 **********************************************************************************************************************/
ssp_err_t R_DTC_Close (transfer_ctrl_t * const p_api_ctrl)
{
    dtc_instance_ctrl_t * p_ctrl = (dtc_instance_ctrl_t *) p_api_ctrl;
    ssp_err_t err = SSP_SUCCESS;

#if DTC_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(NULL != p_ctrl);
    DTC_ERROR_RETURN(p_ctrl->id == DTC_ID, SSP_ERR_NOT_OPEN);
#endif
    DTC_ERROR_RETURN(SSP_INVALID_VECTOR != (IRQn_Type)p_ctrl->irq, SSP_ERR_IRQ_BSP_DISABLED);
    /** Clear DTC enable bit in ICU. */
    HW_ICU_DTCDisable(gp_icu_regs, p_ctrl->irq);

    /** Clear pointer in vector table. */
    gp_dtc_vector_table[p_ctrl->irq] = NULL;
    p_ctrl->id                       = 0U;

#if DTC_CFG_SOFTWARE_START_ENABLE
    if ((NULL != p_ctrl->p_callback) && ((ELC_EVENT_ELC_SOFTWARE_EVENT_0 == p_ctrl->trigger) ||
            (ELC_EVENT_ELC_SOFTWARE_EVENT_1 == p_ctrl->trigger)))
    {
        /** If this is a software event, disable the IRQ. */
        NVIC_DisableIRQ(p_ctrl->irq);
        ssp_vector_info_t * p_vector_info;
        R_SSP_VectorInfoGet(p_ctrl->irq, &p_vector_info);
        *(p_vector_info->pp_ctrl) = NULL;
    }
#endif

    return err;
} /* End of function R_DTC_Close */

/*******************************************************************************************************************//**
 * @brief      Set driver version based on compile time macros.  Implements transfer_api_t::versionGet.
 *
 * @retval SSP_SUCCESS              Successful close.
 * @retval SSP_ERR_ASSERTION        An input parameter is invalid.
 **********************************************************************************************************************/
ssp_err_t R_DTC_VersionGet (ssp_version_t     * const p_version)
{
#if DTC_CFG_PARAM_CHECKING_ENABLE
    /** Verify parameters are valid */
    SSP_ASSERT(NULL != p_version);
#endif

    /** Set driver version based on compile time macros */
    p_version->version_id = g_dtc_version.version_id;

    return SSP_SUCCESS;
} /* End of function R_DTC_VersionGet */

/*******************************************************************************************************************//**
 * @brief  BlockReset transfer source, destination, length and number of transfers.  Implements transfer_api_t::blockReset.
 *
 * @retval SSP_SUCCESS           Transfer reset successfully.
 * @retval SSP_ERR_ASSERTION     An input parameter is invalid.
 * @retval SSP_ERR_NOT_OPEN      Handle is not initialized.  Call R_DTC_Open to initialize the control block.
 * @retval SSP_ERR_UNSUPPORTED   If mode is other than Block Transfer Mode.
 * @retval SSP_ERR_NOT_ENABLED   Enable failed due to an invalid input parameter:
 *                                 - Transfer source must not be NULL.
 *                                 - Transfer destination must not be NULL.
 **********************************************************************************************************************/
ssp_err_t R_DTC_BlockReset (transfer_ctrl_t         * const   p_api_ctrl,
                       void                  const * volatile p_src,
                       void                        * volatile p_dest,
                       uint16_t                         const length,
                       transfer_size_t                        size,
                       uint16_t const                         num_transfers)
{
    ssp_err_t err = SSP_SUCCESS;
    dtc_instance_ctrl_t * p_ctrl = (dtc_instance_ctrl_t *) p_api_ctrl;
#if DTC_CFG_PARAM_CHECKING_ENABLE
    err =  r_dtc_block_reset_parameter_check(p_ctrl,length,num_transfers );
    DTC_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif

    /** Disable transfers on this activation source. */
    HW_ICU_DTCDisable(gp_icu_regs, p_ctrl->irq);

    /** Disable read skip prior to modifying settings. It will be enabled later. */
    HW_DTC_ReadSkipEnableSet(gp_dtc_regs, DTC_READ_SKIP_DISABLED);

    /** Reset transfer based on input parameters. */
    if (NULL != p_src)
    {
        gp_dtc_vector_table[p_ctrl->irq]->p_src = p_src;
    }

    if (NULL != p_dest)
    {
        gp_dtc_vector_table[p_ctrl->irq]->p_dest = p_dest;
    }

    gp_dtc_vector_table[p_ctrl->irq]->num_blocks = num_transfers;
    gp_dtc_vector_table[p_ctrl->irq]->length = length;
    gp_dtc_vector_table[p_ctrl->irq]->size = size;

    dtc_reg_t * p_reg = (dtc_reg_t *) gp_dtc_vector_table[p_ctrl->irq];
    p_reg->CRA_b.CRAH = p_reg->CRA_b.CRAL;

    /** Enables transfers on this activation source. */
    err = R_DTC_Enable(p_ctrl);

    /** Enable read skip after all settings are complete. */
    HW_DTC_ReadSkipEnableSet(gp_dtc_regs, DTC_READ_SKIP_ENABLED);

    DTC_ERROR_RETURN(SSP_SUCCESS == err, SSP_ERR_NOT_ENABLED);

    return SSP_SUCCESS;
} /* End of function R_DTC_Reset */

/*******************************************************************************************************************//**
 * @} (end addtogroup DTC)
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief      Finds the IRQ associated with the requested ELC event.
 *
 * @return     IRQ value              IRQ matches with the requested ELC event.
 * @return     SSP_INVALID_VECTOR     IRQ not matches with the requested ELC event.
 **********************************************************************************************************************/
static IRQn_Type dtc_irq_lookup(elc_event_t const event)
{
    for (IRQn_Type i = (IRQn_Type) 0; i < (IRQn_Type) BSP_VECTOR_TABLE_MAX_ENTRIES; i++)
    {
        if (event == HW_DTC_ICUEventGet(gp_icu_regs, i))
        {
            /* This IRQ is tied to the requested ELC event.  Return this IRQ to be used as DTC trigger */
            return i;
        }
    }

    return SSP_INVALID_VECTOR;
}

/*******************************************************************************************************************//**
 * @brief      Interrupt called when transfer triggered from ELC software event is complete.
 *
 * Saves context if RTOS is used, calls callback if one was provided in the open function, and restores context if
 * RTOS is used.
 **********************************************************************************************************************/
void elc_software_event_isr(void)
{
    SF_CONTEXT_SAVE

    ssp_vector_info_t * p_vector_info = NULL;
    R_SSP_VectorInfoGet(R_SSP_CurrentIrqGet(), &p_vector_info);
    dtc_instance_ctrl_t * p_ctrl = (dtc_instance_ctrl_t *) *(p_vector_info->pp_ctrl);

    /** Clear pending IRQ to make sure it doesn't fire again after exiting */
    R_BSP_IrqStatusClear(R_SSP_CurrentIrqGet());

    /** Call user p_callback */
    if ((NULL != p_ctrl) && (NULL != p_ctrl->p_callback))
    {
        transfer_callback_args_t args;
        args.p_context = p_ctrl->p_context;
        p_ctrl->p_callback(&args);
    }

    SF_CONTEXT_RESTORE
}

/*******************************************************************************************************************//**
 * @brief   For repeat and block modes, copy the data from the initial length into the reload length.
 *
 * @param[in]   p_cfg                 Pointer to configuration structure.
  **********************************************************************************************************************/
static void r_dtc_initialize_repeat_block_mode( transfer_cfg_t const * const    p_cfg)
{
    transfer_info_t * p_info_temp = p_cfg->p_info;
    bool chain_mode_disable_flag = true;
    do
    {
        if (TRANSFER_MODE_NORMAL != p_info_temp->mode)
        {
            dtc_reg_t * p_reg = (dtc_reg_t *) p_info_temp;
            p_reg->CRA_b.CRAH = p_reg->CRA_b.CRAL;
        }
        if(TRANSFER_CHAIN_MODE_DISABLED != p_info_temp->chain_mode)
        {
            /* There should be more elements on the chain */
            p_info_temp++;
        }
        else
        {
            /* No more elements on the chain */
            chain_mode_disable_flag = false;
        }
    } while (chain_mode_disable_flag);
}

/*******************************************************************************************************************//**
 * @brief   For dtc state initialization.
 *
 * @param[in]   p_ctrl                 Pointer to instance control structure.
 * @param[in]   p_cfg                  Pointer to configuration structure.
 *
 * @retval      SSP_SUCCESS            Successful open.
 * @retval      SSP_ERR_ASSERTION      An input parameter is invalid.
 * @retval      SSP_ERR_IN_USE         DTC resource is used.
 * @retval      SSP_ERR_HW_LOCKED      DTC hardware resource is locked.
 * @return                             See @ref Common_Error_Codes or functions called by this function for other possible
 *                                     return codes. This function calls:
 *                                       * fmi_api_t::productFeatureGet
 *
 **********************************************************************************************************************/
static ssp_err_t r_dtc_state_initialize(dtc_instance_ctrl_t * p_ctrl,transfer_cfg_t const * const    p_cfg )
{
    ssp_err_t     err          = SSP_SUCCESS;
#if DTC_CFG_PARAM_CHECKING_ENABLE
    SSP_ASSERT(NULL != p_ctrl);
    SSP_ASSERT(NULL != p_cfg);
    SSP_ASSERT(NULL != p_cfg->p_info);
    err =  r_dtc_parameter_check(p_ctrl,p_cfg);
    DTC_ERROR_RETURN(SSP_SUCCESS == err, err);
#endif
    SSP_PARAMETER_NOT_USED(p_ctrl);
    SSP_PARAMETER_NOT_USED(p_cfg);
    /** DTC requires a one time initialization.  This will be handled only the first time this function
     *  is called. This initialization:
     *  -# Acquires the BSP hardware lock for the DTC to keep this section thread safe and prevent use of
     *     this driver if another driver has locked the DTC block.
     *  -# Stores the register base addresses for DTC and ICU.
     *  -# Powers on the DTC block.
     *  -# Initializes the vector table to NULL pointers.
     *  -# Sets the vector table base address.
     *  -# Enables DTC transfers. */
    if (!g_dtc_state_initialized)
    {
        ssp_feature_t ssp_feature = {{(ssp_ip_t) 0U}};
        ssp_feature.channel = 0U;
        ssp_feature.unit = 0U;
        ssp_feature.id = SSP_IP_DTC;
        fmi_feature_info_t info = {0};
        err = g_fmi_on_fmi.productFeatureGet(&ssp_feature, &info);
        DTC_ERROR_RETURN(SSP_SUCCESS == err, err);
        gp_dtc_regs = (R_DTC_Type *) info.ptr;
        ssp_err_t bsp_err = R_BSP_HardwareLock(&ssp_feature);
        DTC_ERROR_RETURN(SSP_SUCCESS == bsp_err, SSP_ERR_HW_LOCKED);
        R_BSP_ModuleStart(&ssp_feature);
        memset(&gp_dtc_vector_table, 0U, BSP_VECTOR_TABLE_MAX_ENTRIES * sizeof(transfer_info_t *));
        HW_DTC_VectorTableAddressSet(gp_dtc_regs, &gp_dtc_vector_table);
        HW_DTC_StartStop(gp_dtc_regs, DTC_START);
        ssp_feature.id = SSP_IP_ICU;
        g_fmi_on_fmi.productFeatureGet(&ssp_feature, &info);
        gp_icu_regs = (R_ICU_Type *) info.ptr;
        g_dtc_state_initialized = true;
    }

    return err;
}

#if DTC_CFG_PARAM_CHECKING_ENABLE
/*******************************************************************************************************************//**
 * @brief   For dtc parameter checking.
 *
 * @param[in]   p_ctrl                 Pointer to instance control structure.
 * @param[in]   p_cfg                  Pointer to configuration structure.
 *
 * @retval      SSP_SUCCESS            Successful open.
 * @retval      SSP_ERR_ASSERTION      An input parameter is invalid.
 * @retval      SSP_ERR_IN_USE         DTC resource is used.
 **********************************************************************************************************************/
static ssp_err_t r_dtc_parameter_check(dtc_instance_ctrl_t * p_ctrl,transfer_cfg_t const * const    p_cfg )
{
    if (TRANSFER_MODE_NORMAL != p_cfg->p_info->mode)
    {
        SSP_ASSERT(p_cfg->p_info->length <= DTC_REPEAT_BLOCK_MAX_LENGTH);
    }
    DTC_ERROR_RETURN(p_ctrl->id != DTC_ID, SSP_ERR_IN_USE);

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief   For dtc reset parameter checking.
 *
 * @param[in]   p_ctrl                 Pointer to instance control structure.
 * @param[in]   num_transfers          Number of times to transfer.
 *
 * @retval      SSP_SUCCESS            Successful open.
 * @retval      SSP_ERR_ASSERTION      An input parameter is invalid.
 * @retval      SSP_ERR_NOT_ENABLED    Transfer length must not be 0 for repeat and block mode.
 * @retval      SSP_ERR_NOT_OPEN       Handle is not initialized.  Call R_DTC_Open to initialize the control block.
 **********************************************************************************************************************/
static ssp_err_t r_dtc_reset_parameter_check(dtc_instance_ctrl_t * p_ctrl, uint16_t const num_transfers)
{
    SSP_ASSERT(NULL != p_ctrl);
    if (TRANSFER_MODE_NORMAL != gp_dtc_vector_table[p_ctrl->irq]->mode)
    {
        DTC_ERROR_RETURN(0 != num_transfers, SSP_ERR_NOT_ENABLED);
    }
    DTC_ERROR_RETURN(p_ctrl->id == DTC_ID, SSP_ERR_NOT_OPEN);
    if (TRANSFER_MODE_BLOCK == gp_dtc_vector_table[p_ctrl->irq]->mode)
    {
        SSP_ASSERT(0 != gp_dtc_vector_table[p_ctrl->irq]->length);
    }

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief   For dtc block reset parameter checking.
 * @param[in]   p_ctrl                 Pointer to instance control structure.
 * @param[in]   length                 Length of each transfer.
 * @param[in]   num_transfers          Number of times to transfer.
 *
 * @retval      SSP_SUCCESS            Successful open.
 * @retval      SSP_ERR_ASSERTION      An input parameter is invalid.
 * @retval      SSP_ERR_UNSUPPORTED    If mode is other than Block Transfer Mode.
 * @retval      SSP_ERR_NOT_OPEN       Handle is not initialized.  Call R_DTC_Open to initialize the control block.
 **********************************************************************************************************************/
static ssp_err_t r_dtc_block_reset_parameter_check(dtc_instance_ctrl_t * p_ctrl,uint16_t const length, uint16_t const num_transfers)
{
    SSP_ASSERT(NULL != p_ctrl);
    DTC_ERROR_RETURN(TRANSFER_MODE_BLOCK == gp_dtc_vector_table[p_ctrl->irq]->mode, SSP_ERR_UNSUPPORTED);
    SSP_ASSERT(0 != num_transfers);
    SSP_ASSERT(0 != length);
    DTC_ERROR_RETURN(p_ctrl->id == DTC_ID, SSP_ERR_NOT_OPEN);

    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Alignment checking for source and destination pointers.
 *
 * @param[in]   p_info                 Pointer to DTC descriptor block.
 *
 * @retval      SSP_SUCCESS            Alignment on source and destination pointers is valid.
 * @retval      SSP_ERR_ASSERTION      Alignment on source and destination pointers is invalid.
 **********************************************************************************************************************/
static ssp_err_t r_dtc_enable_alignment_check(transfer_info_t * p_info)
{
    if (TRANSFER_SIZE_2_BYTE == p_info->size)
    {
        SSP_ASSERT(0U == ((uint32_t) p_info->p_dest & DTC_PRV_MASK_ALIGN_2_BYTES));
        SSP_ASSERT(0U == ((uint32_t) p_info->p_src & DTC_PRV_MASK_ALIGN_2_BYTES));
    }
    if (TRANSFER_SIZE_4_BYTE == p_info->size)
    {
        SSP_ASSERT(0U == ((uint32_t) p_info->p_dest & DTC_PRV_MASK_ALIGN_4_BYTES));
        SSP_ASSERT(0U == ((uint32_t) p_info->p_src & DTC_PRV_MASK_ALIGN_4_BYTES));
    }

    return SSP_SUCCESS;
}
#endif
