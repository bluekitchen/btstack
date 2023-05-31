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
#include "bsp_api.h"
#include <string.h>
#include "r_flash_hp.h"

/* Configuration for this package. */
#include "r_flash_hp_cfg.h"

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

#if defined(__ARMCC_VERSION) || defined(__ICCARM__)
typedef void (BSP_CMSE_NONSECURE_CALL * flash_hp_prv_ns_callback)(flash_callback_args_t * p_args);
#elif defined(__GNUC__)
typedef BSP_CMSE_NONSECURE_CALL void (*volatile flash_hp_prv_ns_callback)(flash_callback_args_t * p_args);
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define FLASH_HP_FENTRYR_PE_MODE_BITS                 (0x0081U)
#define FLASH_HP_FSTATR_FLWEERR                       (1U << 6U)
#define FLASH_HP_FSTATR_PRGERR                        (1U << 12U)
#define FLASH_HP_FSTATR_ERSERR                        (1U << 13U)
#define FLASH_HP_FSTATR_ILGLERR                       (1U << 14U)
#define FLASH_HP_FSTATR_OTERR                         (1U << 20U)
#define FLASH_HP_FSTATR_SECERR                        (1U << 21U)
#define FLASH_HP_FSTATR_FESETERR                      (1U << 22U)
#define FLASH_HP_FSTATR_ILGCOMERR                     (1U << 23U)

#define FLASH_HP_FSTATR_ERROR_MASK                    (FLASH_HP_FSTATR_FLWEERR | FLASH_HP_FSTATR_PRGERR | \
                                                       FLASH_HP_FSTATR_ERSERR |                           \
                                                       FLASH_HP_FSTATR_ILGLERR | FLASH_HP_FSTATR_OTERR |  \
                                                       FLASH_HP_FSTATR_SECERR |                           \
                                                       FLASH_HP_FSTATR_FESETERR | FLASH_HP_FSTATR_ILGCOMERR)

/** "OPEN" in ASCII, used to avoid multiple open. */
#define FLASH_HP_OPEN                                 (0x4f50454eU)
#define FLASH_HP_CLOSE                                (0U)

/* Minimum FCLK for Flash Operations in Hz */
#define FLASH_HP_MINIMUM_SUPPORTED_FCLK_FREQ          (4000000U)

/* Smallest Code Flash block size */
#define FLASH_HP_CODE_SMALL_BLOCK_SZ                  (8192U)

/* Largest Code Flash block size */
#define FLASH_HP_CODE_LARGE_BLOCK_SZ                  (32768U)

#define FLASH_HP_DATA_BLOCK_SIZE                      (64U)

/** The maximum timeout for commands is 100usec when FCLK is 16 MHz i.e. 1600 FCLK cycles.
 * Assuming worst case of ICLK at 240 MHz and FCLK at 4 MHz, and optimization set to max such that
 * each count decrement loop takes only 5 cycles, then ((240/4)*1600)/5 = 19200 */
#define FLASH_HP_FRDY_CMD_TIMEOUT                     (19200)

/** Time that it would take for the Data Buffer to be empty (DBFULL Flag) is 90 FCLK cycles.
 * Assuming worst case of ICLK at 240 MHz and FCLK at 4 MHz, and optimization set to max such that
 * each count decrement loop takes only 5 cycles, then ((240/4)*90)/5 = 1080 */
#define FLASH_HP_DBFULL_TIMEOUT                       (1080U)

/** R_FACI Commands */
#define FLASH_HP_FACI_CMD_PROGRAM                     (0xE8U)
#define FLASH_HP_FACI_CMD_PROGRAM_CF                  (0x80U)
#define FLASH_HP_FACI_CMD_PROGRAM_DF                  (0x02U)
#define FLASH_HP_FACI_CMD_BLOCK_ERASE                 (0x20U)
#define FLASH_HP_FACI_CMD_PE_SUSPEND                  (0xB0U)
#define FLASH_HP_FACI_CMD_PE_RESUME                   (0xD0U)
#define FLASH_HP_FACI_CMD_STATUS_CLEAR                (0x50U)
#define FLASH_HP_FACI_CMD_FORCED_STOP                 (0xB3U)
#define FLASH_HP_FACI_CMD_BLANK_CHECK                 (0x71U)
#define FLASH_HP_FACI_CMD_CONFIG_SET_1                (0x40U)
#define FLASH_HP_FACI_CMD_CONFIG_SET_2                (0x08U)
#define FLASH_HP_FACI_CMD_LOCK_BIT_PGM                (0x77U)
#define FLASH_HP_FACI_CMD_LOCK_BIT_READ               (0x71U)
#define FLASH_HP_FACI_CMD_FINAL                       (0xD0U)

/**  Configuration set Command offset*/
#define FLASH_HP_FCU_CONFIG_SET_ID_BYTE               (0x0000A150U)
#if !(defined(BSP_MCU_GROUP_RA6M4) || defined(BSP_MCU_GROUP_RA4M3) || defined(BSP_MCU_GROUP_RA4M2) || \
    defined(BSP_MCU_GROUP_RA6M5) || defined(BSP_MCU_GROUP_RA4E1) || defined(BSP_MCU_GROUP_RA6E1) ||   \
    defined(BSP_MCU_GROUP_RA6T2))
 #define FLASH_HP_FCU_CONFIG_SET_ACCESS_STARTUP       (0x0000A160U)
#else
 #define FLASH_HP_FCU_CONFIG_SET_DUAL_MODE            (0x0100A110U)
 #define FLASH_HP_FCU_CONFIG_SET_ACCESS_STARTUP       (0x0100A130U)
 #define FLASH_HP_FCU_CONFIG_SET_BANK_MODE            (0x0100A190U)
#endif

/* Zero based offset into g_configuration_area_data[] for FAWS */
#define FLASH_HP_FCU_CONFIG_SET_FAWS_OFFSET           (2U)

/* Zero based offset into g_configuration_area_data[] for FAWE and BTFLG */
#define FLASH_HP_FCU_CONFIG_SET_FAWE_BTFLG_OFFSET     (3U)

/* These bits must always be set when writing to the configuration area. */
#define FLASH_HP_FCU_CONFIG_FAWE_BTFLG_UNUSED_BITS    (0x7800U)
#define FLASH_HP_FCU_CONFIG_FAWS_UNUSED_BITS          (0xF800U)

/* 8 words need to be written */
#define FLASH_HP_CONFIG_SET_ACCESS_WORD_CNT           (8U)

#define FLASH_HP_FSUACR_KEY                           (0x6600U)

#define FLASH_HP_SAS_KEY                              (0x6600U)

/** Register masks */
#define FLASH_HP_FPESTAT_NON_LOCK_BIT_PGM_ERROR       (0x0002U) // Bits indicating Non Lock Bit related Programming error.
#define FLASH_HP_FPESTAT_NON_LOCK_BIT_ERASE_ERROR     (0x0012U) // Bits indicating Non Lock Bit related Erasure error.
#define FLASH_HP_FENTRYR_DF_PE_MODE                   (0x0080U) // Bits indicating that Data Flash is in P/E mode.
#define FLASH_HP_FENTRYR_CF_PE_MODE                   (0x0001U) // Bits indicating that CodeFlash is in P/E mode.
#define FLASH_HP_FENTRYR_TRANSITION_TO_CF_PE          (0xAA01U) // Key Code to transition to CF P/E mode.
#define FLASH_HP_FENTRYR_TRANSITION_TO_DF_PE          (0xAA80U) // Key Code to transition to DF P/E mode.

#define FLASH_HP_FREQUENCY_IN_HZ                      (1000000U)

#define FLASH_HP_FENTRYR_READ_MODE                    (0xAA00U)

#define FLASH_HP_FMEPROT_LOCK                         (0xD901)
#define FLASH_HP_FMEPROT_UNLOCK                       (0xD900)

#define FLASH_HP_OFS_SAS_MASK                         (0x7FFFU)

#define FLASH_HP_FAEINT_DFAEIE                        (0x08)
#define FLASH_HP_FAEINT_CMDLKIE                       (0x10)
#define FLASH_HP_FAEINT_CFAEIE                        (0x80)
#define FLASH_HP_ERROR_INTERRUPTS_ENABLE              (FLASH_HP_FAEINT_DFAEIE | FLASH_HP_FAEINT_CMDLKIE | \
                                                       FLASH_HP_FAEINT_CFAEIE)

#define FLASH_HP_FPCKAR_KEY                           (0x1E00U)

#define FLASH_HP_MAX_WRITE_CF_US                      (15800)
#define FLASH_HP_MAX_WRITE_DF_US                      (3800)
#define FLASH_HP_MAX_DBFULL_US                        (2)
#define FLASH_HP_MAX_BLANK_CHECK_US                   (84)
#define FLASH_HP_MAX_WRITE_CONFIG_US                  (307000)
#define FLASH_HP_MAX_ERASE_DF_BLOCK_US                (18000)
#define FLASH_HP_MAX_ERASE_CF_LARGE_BLOCK_US          (1040000)
#define FLASH_HP_MAX_ERASE_CF_SMALL_BLOCK_US          (260000)

#define FLASH_HP_FASTAT_DFAE                          (0x08)
#define FLASH_HP_FASTAT_CFAE                          (0x80)
#define FLASH_HP_FASTAT_CMDLK                         (0x10)

#if BSP_FEATURE_FLASH_HP_SUPPORTS_DUAL_BANK
 #define FLASH_HP_PRV_DUALSEL_BANKMD_MASK             (0x7U)
 #define FLASH_HP_PRV_BANKSEL_BANKSWP_MASK            (0x7U)
 #define FLASH_HP_PRV_BANK1_MASK                      (~BSP_FEATURE_FLASH_HP_CF_DUAL_BANK_START)
#else
 #define FLASH_HP_PRV_BANK1_MASK                      (UINT32_MAX)
#endif

/* The number of CPU cycles per each timeout loop. */
#ifndef R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP
 #if defined(__GNUC__)
  #define R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP    (6U)
 #elif defined(__ICCARM__)
  #define R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP    (6U)
 #endif
#endif

#define FLASH_HP_REGISTER_WAIT_TIMEOUT(val, reg, timeout, err) \
    while (val != reg)                                         \
    {                                                          \
        if (0 == timeout)                                      \
        {                                                      \
            return err;                                        \
        }                                                      \
        timeout--;                                             \
    }

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)
static uint16_t g_configuration_area_data[FLASH_HP_CONFIG_SET_ACCESS_WORD_CNT] = {UINT16_MAX};
#endif

#define FLASH_HP_DF_START_ADDRESS    (BSP_FEATURE_FLASH_DATA_FLASH_START)

static const flash_block_info_t g_code_flash_macro_info[] =
{
    {
        .block_section_st_addr  = 0,
        .block_section_end_addr = UINT16_MAX,
        .block_size             = FLASH_HP_CODE_SMALL_BLOCK_SZ,
        .block_size_write       = BSP_FEATURE_FLASH_HP_CF_WRITE_SIZE
    },
    {
        .block_section_st_addr  = BSP_FEATURE_FLASH_HP_CF_REGION0_SIZE,
        .block_section_end_addr = BSP_ROM_SIZE_BYTES - 1,
        .block_size             = FLASH_HP_CODE_LARGE_BLOCK_SZ,
        .block_size_write       = BSP_FEATURE_FLASH_HP_CF_WRITE_SIZE
    }
};

static const flash_regions_t g_flash_code_region =
{
    .num_regions   = 2,
    .p_block_array = g_code_flash_macro_info
};

const flash_block_info_t g_data_flash_macro_info =
{
    .block_section_st_addr  = FLASH_HP_DF_START_ADDRESS,
    .block_section_end_addr = FLASH_HP_DF_START_ADDRESS + BSP_DATA_FLASH_SIZE_BYTES - 1,
    .block_size             = FLASH_HP_DATA_BLOCK_SIZE,
    .block_size_write       = BSP_FEATURE_FLASH_HP_DF_WRITE_SIZE
};

static flash_regions_t g_flash_data_region =
{
    .num_regions   = 1,
    .p_block_array = &g_data_flash_macro_info
};

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1) && (BSP_FEATURE_FLASH_HP_SUPPORTS_DUAL_BANK == 1)
static volatile uint32_t * const flash_hp_banksel = (uint32_t *) FLASH_HP_FCU_CONFIG_SET_BANK_MODE;
 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)
static volatile uint32_t * const flash_hp_dualsel = (uint32_t *) FLASH_HP_FCU_CONFIG_SET_DUAL_MODE;
 #endif
#endif

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

void fcu_fiferr_isr(void);
void fcu_frdyi_isr(void);

/* Internal functions. */
static fsp_err_t flash_hp_init(flash_hp_instance_ctrl_t * p_ctrl);

static fsp_err_t flash_hp_enter_pe_df_mode(flash_hp_instance_ctrl_t * const p_ctrl);

static fsp_err_t flash_hp_pe_mode_exit() PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_reset(flash_hp_instance_ctrl_t * p_ctrl) PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_stop(void) PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_status_clear() PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_erase_block(flash_hp_instance_ctrl_t * const p_ctrl, uint32_t block_size,
                                      uint32_t timeout) PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_write_data(flash_hp_instance_ctrl_t * const p_ctrl, uint32_t write_size,
                                     uint32_t timeout) PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_check_errors(fsp_err_t previous_error, uint32_t error_bits,
                                       fsp_err_t return_error) PLACE_IN_RAM_SECTION;

static void r_flash_hp_call_callback(flash_hp_instance_ctrl_t * p_ctrl, flash_event_t event);

#if (FLASH_HP_CFG_DATA_FLASH_PROGRAMMING_ENABLE == 1)

static fsp_err_t flash_hp_df_blank_check(flash_hp_instance_ctrl_t * const p_ctrl,
                                         uint32_t const                   address,
                                         uint32_t                         num_bytes,
                                         flash_result_t                 * p_blank_check_result);

static fsp_err_t flash_hp_df_write(flash_hp_instance_ctrl_t * const p_ctrl);

static fsp_err_t flash_hp_df_erase(flash_hp_instance_ctrl_t * p_ctrl, uint32_t block_address, uint32_t num_blocks);

#endif

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)

static fsp_err_t flash_hp_configuration_area_write(flash_hp_instance_ctrl_t * p_ctrl,
                                                   uint32_t                   fsaddr) PLACE_IN_RAM_SECTION;

static void flash_hp_configuration_area_data_setup(uint32_t btflg_swap, uint32_t start_addr,
                                                   uint32_t end_addr) PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_cf_write(flash_hp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

 #if (BSP_FEATURE_FLASH_HP_SUPPORTS_DUAL_BANK == 1)
static fsp_err_t flash_hp_bank_swap(flash_hp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

 #endif

static fsp_err_t flash_hp_cf_erase(flash_hp_instance_ctrl_t * p_ctrl, uint32_t block_address,
                                   uint32_t num_blocks) PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_enter_pe_cf_mode(flash_hp_instance_ctrl_t * const p_ctrl) PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_access_window_set(flash_hp_instance_ctrl_t * p_ctrl,
                                            uint32_t const             start_addr,
                                            uint32_t const             end_addr) PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_set_startup_area_boot(flash_hp_instance_ctrl_t * p_ctrl,
                                                flash_startup_area_swap_t  swap_type,
                                                bool                       is_temporary) PLACE_IN_RAM_SECTION;

static fsp_err_t flash_hp_set_id_code(flash_hp_instance_ctrl_t * p_ctrl,
                                      uint8_t const * const      p_id_code,
                                      flash_id_code_mode_t       mode) PLACE_IN_RAM_SECTION;

#endif

#if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)

static fsp_err_t r_flash_hp_common_parameter_checking(flash_hp_instance_ctrl_t * const p_ctrl);

static fsp_err_t r_flash_hp_write_bc_parameter_checking(flash_hp_instance_ctrl_t * const p_ctrl,
                                                        uint32_t                         flash_address,
                                                        uint32_t const                   num_bytes,
                                                        bool                             check_write);

#endif

/***********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

const flash_api_t g_flash_on_flash_hp =
{
    .open                 = R_FLASH_HP_Open,
    .close                = R_FLASH_HP_Close,
    .write                = R_FLASH_HP_Write,
    .erase                = R_FLASH_HP_Erase,
    .blankCheck           = R_FLASH_HP_BlankCheck,
    .statusGet            = R_FLASH_HP_StatusGet,
    .infoGet              = R_FLASH_HP_InfoGet,
    .accessWindowSet      = R_FLASH_HP_AccessWindowSet,
    .accessWindowClear    = R_FLASH_HP_AccessWindowClear,
    .idCodeSet            = R_FLASH_HP_IdCodeSet,
    .reset                = R_FLASH_HP_Reset,
    .startupAreaSelect    = R_FLASH_HP_StartUpAreaSelect,
    .updateFlashClockFreq = R_FLASH_HP_UpdateFlashClockFreq,
    .bankSwap             = R_FLASH_HP_BankSwap,
    .callbackSet          = R_FLASH_HP_CallbackSet,
};

/*******************************************************************************************************************//**
 * @addtogroup FLASH_HP
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Initializes the high performance flash peripheral. Implements @ref flash_api_t::open.
 *
 * The Open function initializes the Flash.
 *
 * Example:
 * @snippet r_flash_hp_example.c R_FLASH_HP_Open
 *
 * @retval     FSP_SUCCESS               Initialization was successful and timer has started.
 * @retval     FSP_ERR_ALREADY_OPEN      The flash control block is already open.
 * @retval     FSP_ERR_ASSERTION         NULL provided for p_ctrl or p_cfg.
 * @retval     FSP_ERR_IRQ_BSP_DISABLED  Caller is requesting BGO but the Flash interrupts are not enabled.
 * @retval     FSP_ERR_FCLK              FCLK must be a minimum of 4 MHz for Flash operations.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_Open (flash_ctrl_t * const p_api_ctrl, flash_cfg_t const * const p_cfg)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE)

    /* If null pointers return error. */
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_ctrl);

    /* If open return error. */
    FSP_ERROR_RETURN((FLASH_HP_OPEN != p_ctrl->opened), FSP_ERR_ALREADY_OPEN);

    /* Background operations for data flash are enabled but the flash interrupt is disabled. */
    if (p_cfg->data_flash_bgo)
    {
        FSP_ERROR_RETURN(p_cfg->irq >= (IRQn_Type) 0, FSP_ERR_IRQ_BSP_DISABLED);
        FSP_ERROR_RETURN(p_cfg->err_irq >= (IRQn_Type) 0, FSP_ERR_IRQ_BSP_DISABLED);
    }
#endif

    /* Set the parameters struct based on the user supplied settings */
    p_ctrl->p_cfg = p_cfg;

    if (true == p_cfg->data_flash_bgo)
    {
        p_ctrl->p_callback        = p_cfg->p_callback;
        p_ctrl->p_context         = p_cfg->p_context;
        p_ctrl->p_callback_memory = NULL;

        /* Enable FCU interrupts. */
        R_FACI_HP->FRDYIE = 1U;
        R_BSP_IrqCfgEnable(p_cfg->irq, p_cfg->ipl, p_ctrl);

        /* Enable Error interrupts. */
        R_FACI_HP->FAEINT = FLASH_HP_ERROR_INTERRUPTS_ENABLE;
        R_BSP_IrqCfgEnable(p_cfg->err_irq, p_cfg->err_ipl, p_ctrl);
    }
    else
    {
        /* Disable Flash Rdy interrupt in the FLASH peripheral. */
        R_FACI_HP->FRDYIE = 0U;

        /* Disable Flash Error interrupt in the FLASH peripheral */
        R_FACI_HP->FAEINT = 0U;
    }

    /* Calculate timeout values. */
    err = flash_hp_init(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* If successful mark the control block as open. Otherwise release the hardware lock. */
    p_ctrl->opened = FLASH_HP_OPEN;

    return err;
}

/*******************************************************************************************************************//**
 * Writes to the specified Code or Data Flash memory area. Implements @ref flash_api_t::write.
 *
 * Example:
 * @snippet r_flash_hp_example.c R_FLASH_HP_Write
 *
 * @retval     FSP_SUCCESS              Operation successful. If BGO is enabled this means the operation was started
 *                                      successfully.
 * @retval     FSP_ERR_IN_USE           The Flash peripheral is busy with a prior on-going transaction.
 * @retval     FSP_ERR_NOT_OPEN         The Flash API is not Open.
 * @retval     FSP_ERR_CMD_LOCKED       FCU is in locked state, typically as a result of attempting to Write an area
 *                                      that is protected by an Access Window.
 * @retval     FSP_ERR_WRITE_FAILED     Status is indicating a Programming error for the requested operation. This may
 *                                      be returned if the requested Flash area is not blank.
 * @retval     FSP_ERR_TIMEOUT          Timed out waiting for FCU operation to complete.
 * @retval     FSP_ERR_INVALID_SIZE     Number of bytes provided was not a multiple of the programming size or exceeded
 *                                      the maximum range.
 * @retval     FSP_ERR_INVALID_ADDRESS  Invalid address was input or address not on programming boundary.
 * @retval     FSP_ERR_ASSERTION        NULL provided for p_ctrl.
 * @retval     FSP_ERR_PE_FAILURE       Failed to enter or exit P/E mode.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_Write (flash_ctrl_t * const p_api_ctrl,
                            uint32_t const       src_address,
                            uint32_t             flash_address,
                            uint32_t const       num_bytes)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)

    /* Verify write parameters. If failure return error. */
    err = r_flash_hp_write_bc_parameter_checking(p_ctrl, flash_address, num_bytes, true);
    FSP_ERROR_RETURN((err == FSP_SUCCESS), err);
#endif

    p_ctrl->operations_remaining = (num_bytes) >> 1; // Since two bytes will be written at a time
    p_ctrl->source_start_address = src_address;
    p_ctrl->dest_end_address     = flash_address;
    p_ctrl->current_operation    = FLASH_OPERATION_NON_BGO;

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)
    if (flash_address < BSP_FEATURE_FLASH_DATA_FLASH_START)
    {
 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)

        /* Verify the source address is not in code flash. It will not be available in P/E mode. */
        FSP_ASSERT(src_address > BSP_ROM_SIZE_BYTES);
 #endif

        /* Initiate the write operation, may return FSP_ERR_IN_USE via setup_for_pe_mode() */
        err = flash_hp_cf_write(p_ctrl);
    }
    else
#endif
    {
#if (FLASH_HP_CFG_DATA_FLASH_PROGRAMMING_ENABLE == 1)

        /* Initiate the write operation, may return FSP_ERR_IN_USE via setup_for_pe_mode() */
        err = flash_hp_df_write(p_ctrl);
#endif
    }

    return err;
}

/*******************************************************************************************************************//**
 * Erases the specified Code or Data Flash blocks. Implements @ref flash_api_t::erase by the block_erase_address.
 *
 * @note       Code flash may contain blocks of different sizes. When erasing code flash it is important to take this
 *             into consideration to prevent erasing a larger address space than desired.
 *
 * Example:
 * @snippet r_flash_hp_example.c R_FLASH_HP_Erase
 *
 * @retval     FSP_SUCCESS              Successful open.
 * @retval     FSP_ERR_INVALID_BLOCKS   Invalid number of blocks specified
 * @retval     FSP_ERR_INVALID_ADDRESS  Invalid address specified. If the address is in code flash then code flash
 *                                      programming must be enabled.
 * @retval     FSP_ERR_IN_USE           Other flash operation in progress, or API not initialized
 * @retval     FSP_ERR_CMD_LOCKED       FCU is in locked state, typically as a result of attempting to Erase an area
 *                                      that is protected by an Access Window.
 * @retval     FSP_ERR_ASSERTION        NULL provided for p_ctrl
 * @retval     FSP_ERR_NOT_OPEN         The Flash API is not Open.
 * @retval     FSP_ERR_ERASE_FAILED     Status is indicating a Erase error.
 * @retval     FSP_ERR_TIMEOUT          Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_PE_FAILURE       Failed to enter or exit P/E mode.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_Erase (flash_ctrl_t * const p_api_ctrl, uint32_t const address, uint32_t const num_blocks)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)

    /* Verify the control block is not null and is opened. */
    err = r_flash_hp_common_parameter_checking(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    if (true == p_ctrl->p_cfg->data_flash_bgo)
    {
        FSP_ASSERT(NULL != p_ctrl->p_callback);
    }

    /* If invalid number of blocks return error. */
    FSP_ERROR_RETURN(num_blocks != 0U, FSP_ERR_INVALID_BLOCKS);
#endif

    p_ctrl->current_operation = FLASH_OPERATION_NON_BGO;

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)
    if (address < BSP_FEATURE_FLASH_DATA_FLASH_START)
    {
        uint32_t start_address = 0;
 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)
        uint32_t region0_blocks = 0;
 #endif

        /* Configure the current parameters based on if the operation is for code flash or data flash. */
        if ((address & FLASH_HP_PRV_BANK1_MASK) < BSP_FEATURE_FLASH_HP_CF_REGION0_SIZE)
        {
            start_address = address & ~(BSP_FEATURE_FLASH_HP_CF_REGION0_BLOCK_SIZE - 1);

 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)
            region0_blocks = (BSP_FEATURE_FLASH_HP_CF_REGION0_SIZE - (start_address & FLASH_HP_PRV_BANK1_MASK)) /
                             BSP_FEATURE_FLASH_HP_CF_REGION0_BLOCK_SIZE;
 #endif
        }
        else
        {
            start_address = address & ~(BSP_FEATURE_FLASH_HP_CF_REGION1_BLOCK_SIZE - 1);
        }

 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)
        uint32_t num_bytes = (region0_blocks * BSP_FEATURE_FLASH_HP_CF_REGION0_BLOCK_SIZE);
        if (num_blocks > region0_blocks)
        {
            num_bytes += ((num_blocks - region0_blocks) * BSP_FEATURE_FLASH_HP_CF_REGION1_BLOCK_SIZE);
        }

  #if BSP_FEATURE_FLASH_HP_SUPPORTS_DUAL_BANK
        uint32_t rom_end =
            (FLASH_HP_PRV_DUALSEL_BANKMD_MASK ==
             (*flash_hp_dualsel & FLASH_HP_PRV_DUALSEL_BANKMD_MASK)) ? BSP_ROM_SIZE_BYTES : BSP_ROM_SIZE_BYTES / 2;

        FSP_ERROR_RETURN((start_address & FLASH_HP_PRV_BANK1_MASK) + num_bytes <= rom_end, FSP_ERR_INVALID_BLOCKS);
  #else
        FSP_ERROR_RETURN(start_address + num_bytes <= BSP_ROM_SIZE_BYTES, FSP_ERR_INVALID_BLOCKS);
  #endif
 #endif

        err = flash_hp_cf_erase(p_ctrl, start_address, num_blocks);
    }
    else
#endif
    {
#if (FLASH_HP_CFG_DATA_FLASH_PROGRAMMING_ENABLE == 1)
        uint32_t start_address = address & ~(BSP_FEATURE_FLASH_HP_DF_BLOCK_SIZE - 1);

 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)
        uint32_t num_bytes = num_blocks * BSP_FEATURE_FLASH_HP_DF_BLOCK_SIZE;

        FSP_ERROR_RETURN((start_address >= (FLASH_HP_DF_START_ADDRESS)) &&
                         (start_address < (FLASH_HP_DF_START_ADDRESS + BSP_DATA_FLASH_SIZE_BYTES)),
                         FSP_ERR_INVALID_ADDRESS);

        FSP_ERROR_RETURN(start_address + num_bytes <= (FLASH_HP_DF_START_ADDRESS + BSP_DATA_FLASH_SIZE_BYTES),
                         FSP_ERR_INVALID_BLOCKS);
 #endif

        /* Initiate the flash erase. */
        err = flash_hp_df_erase(p_ctrl, start_address, num_blocks);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
#else

        return FSP_ERR_INVALID_ADDRESS;
#endif
    }

    return err;
}

/*******************************************************************************************************************//**
 * Performs a blank check on the specified address area. Implements @ref flash_api_t::blankCheck.
 *
 * Example:
 * @snippet r_flash_hp_example.c R_FLASH_HP_BlankCheck
 *
 * @retval     FSP_SUCCESS                 Blank check operation completed with result in p_blank_check_result, or
 *                                         blank check started and in-progess (BGO mode).
 * @retval     FSP_ERR_INVALID_ADDRESS     Invalid data flash address was input.
 * @retval     FSP_ERR_INVALID_SIZE        'num_bytes' was either too large or not aligned for the CF/DF boundary size.
 * @retval     FSP_ERR_IN_USE              Other flash operation in progress or API not initialized.
 * @retval     FSP_ERR_ASSERTION           NULL provided for p_ctrl.
 * @retval     FSP_ERR_CMD_LOCKED          FCU is in locked state, typically as a result of attempting to Erase an area
 *                                         that is protected by an Access Window.
 * @retval     FSP_ERR_NOT_OPEN            The Flash API is not Open.
 * @retval     FSP_ERR_TIMEOUT             Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_PE_FAILURE          Failed to enter or exit P/E mode.
 * @retval     FSP_ERR_BLANK_CHECK_FAILED  Blank check operation failed.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_BlankCheck (flash_ctrl_t * const p_api_ctrl,
                                 uint32_t const       address,
                                 uint32_t             num_bytes,
                                 flash_result_t     * p_blank_check_result)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)

    /* Check parameters. If failure return error */
    err = r_flash_hp_write_bc_parameter_checking(p_ctrl, address, num_bytes, false);
    FSP_ERROR_RETURN((err == FSP_SUCCESS), err);
#endif

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)

    /* Is this a request to Blank check Code Flash? */
    /* If the address is code flash check if the region is blank. If not blank return error. */
    if (address < BSP_ROM_SIZE_BYTES)
    {
        /* Blank checking for Code Flash does not require any FCU operations. The specified address area
         * can simply be checked for non 0xFF. */
        p_ctrl->current_operation = FLASH_OPERATION_NON_BGO;
        *p_blank_check_result     = FLASH_RESULT_BLANK; // Assume blank until we know otherwise
        uint8_t * p_address = (uint8_t *) address;
        for (uint32_t index = 0U; index < num_bytes; index++)
        {
            if (p_address[index] != UINT8_MAX)
            {
                *p_blank_check_result = FLASH_RESULT_NOT_BLANK;
                break;
            }
        }
    }
    /* Otherwise the address is data flash. Put the flash in the blank check state. Return status. */
    else
#endif
    {
#if (FLASH_HP_CFG_DATA_FLASH_PROGRAMMING_ENABLE == 1)
        err = flash_hp_df_blank_check(p_ctrl, address, num_bytes, p_blank_check_result);
#endif
    }

    return err;
}

/*******************************************************************************************************************//**
 * Query the FLASH peripheral for its status. Implements @ref flash_api_t::statusGet.
 *
 * Example:
 * @snippet r_flash_hp_example.c R_FLASH_HP_StatusGet
 *
 * @retval     FSP_SUCCESS        FLASH peripheral is ready to use.
 * @retval     FSP_ERR_ASSERTION  NULL provided for p_ctrl.
 * @retval     FSP_ERR_NOT_OPEN   The Flash API is not Open.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_StatusGet (flash_ctrl_t * const p_api_ctrl, flash_status_t * const p_status)
{
#if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

    /* If null pointer return error. */
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_status);

    /* If control block is not open return error. */
    FSP_ERROR_RETURN(!(FLASH_HP_OPEN != p_ctrl->opened), FSP_ERR_NOT_OPEN);
#else

    /* Eliminate warning if parameter checking is disabled. */
    FSP_PARAMETER_NOT_USED(p_api_ctrl);
#endif

    /* If the flash is currently in program/erase mode notify the caller that the flash is busy. */
    if ((R_FACI_HP->FENTRYR & FLASH_HP_FENTRYR_PE_MODE_BITS) == 0x0000U)
    {
        *p_status = FLASH_STATUS_IDLE;
    }
    else
    {
        *p_status = FLASH_STATUS_BUSY;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Implements @ref flash_api_t::idCodeSet.
 *
 * @retval     FSP_SUCCESS               ID Code successfully configured.
 * @retval     FSP_ERR_IN_USE            FLASH peripheral is busy with a prior operation.
 * @retval     FSP_ERR_ASSERTION         NULL provided for p_ctrl.
 * @retval     FSP_ERR_UNSUPPORTED       Code Flash Programming is not enabled.
 * @retval     FSP_ERR_NOT_OPEN          Flash API has not yet been opened.
 * @retval     FSP_ERR_PE_FAILURE        Failed to enter or exit Code Flash P/E mode.
 * @retval     FSP_ERR_TIMEOUT           Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_WRITE_FAILED      Status is indicating a Programming error for the requested operation.
 * @retval     FSP_ERR_CMD_LOCKED        FCU is in locked state, typically as a result of having received an illegal
 *                                       command.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_IdCodeSet (flash_ctrl_t * const  p_api_ctrl,
                                uint8_t const * const p_id_code,
                                flash_id_code_mode_t  mode)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

    fsp_err_t err = FSP_SUCCESS;

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1) && (BSP_FEATURE_FLASH_SUPPORTS_ID_CODE == 1)
 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the id bytes are not in code flash. They will not be available in P/E mode. */
    FSP_ASSERT(((uint32_t) p_id_code) > BSP_ROM_SIZE_BYTES);

    /* Verify the control block is not null and is opened. */
    err = r_flash_hp_common_parameter_checking(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
 #endif

    /* If successful set the id code. */
    err = flash_hp_set_id_code(p_ctrl, p_id_code, mode);
#else

    /* Eliminate warning if code flash programming is disabled. */
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_id_code);
    FSP_PARAMETER_NOT_USED(mode);

    err = FSP_ERR_UNSUPPORTED;         // For consistency with _LP API we return error if Code Flash not enabled
#endif

    /* Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * Configure an access window for the Code Flash memory using the provided start and end address. An access window
 * defines a contiguous area in Code Flash for which programming/erase is enabled. This area is on block boundaries. The
 * block containing start_addr is the first block. The block containing end_addr is the last block. The access window
 * then becomes first block --> last block inclusive. Anything outside this range of Code Flash is then write protected.
 * @note       If the start address and end address are set to the same value, then the access window is effectively
 *             removed. This accomplishes the same functionality as R_FLASH_HP_AccessWindowClear().
 *
 * Implements @ref flash_api_t::accessWindowSet.
 *
 * @retval     FSP_SUCCESS               Access window successfully configured.
 * @retval     FSP_ERR_INVALID_ADDRESS   Invalid settings for start_addr and/or end_addr.
 * @retval     FSP_ERR_IN_USE            FLASH peripheral is busy with a prior operation.
 * @retval     FSP_ERR_ASSERTION         NULL provided for p_ctrl.
 * @retval     FSP_ERR_UNSUPPORTED       Code Flash Programming is not enabled.
 * @retval     FSP_ERR_NOT_OPEN          Flash API has not yet been opened.
 * @retval     FSP_ERR_PE_FAILURE        Failed to enter or exit Code Flash P/E mode.
 * @retval     FSP_ERR_TIMEOUT           Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_WRITE_FAILED      Status is indicating a Programming error for the requested operation.
 * @retval     FSP_ERR_CMD_LOCKED        FCU is in locked state, typically as a result of having received an illegal
 *                                       command.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_AccessWindowSet (flash_ctrl_t * const p_api_ctrl,
                                      uint32_t const       start_addr,
                                      uint32_t const       end_addr)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

    fsp_err_t err = FSP_SUCCESS;

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1) && (BSP_FEATURE_FLASH_SUPPORTS_ACCESS_WINDOW == 1)
 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the control block is not null and is opened. */
    err = r_flash_hp_common_parameter_checking(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Note that the end_addr indicates the address up to, but not including the block that contains that address. */
    /* Therefore to allow the very last Block to be included in the access window we must allow for FLASH_CF_BLOCK_END+1 */
    /* If the start or end addresses are invalid return error. */
    FSP_ERROR_RETURN(start_addr <= end_addr, FSP_ERR_INVALID_ADDRESS);
    FSP_ERROR_RETURN(end_addr <= BSP_ROM_SIZE_BYTES, FSP_ERR_INVALID_ADDRESS);
 #endif

    /* Set the access window. */
    err = flash_hp_access_window_set(p_ctrl, start_addr, end_addr);
#else

    /* Eliminate warning if code flash programming is disabled. */
    FSP_PARAMETER_NOT_USED(p_ctrl);

    FSP_PARAMETER_NOT_USED(start_addr);
    FSP_PARAMETER_NOT_USED(end_addr);

    err = FSP_ERR_UNSUPPORTED;         ///< For consistency with _LP API we return error if Code Flash not enabled
#endif

    /* Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * Remove any access window that is currently configured in the Code Flash. Subsequent to this call all Code Flash is
 * writable. Implements @ref flash_api_t::accessWindowClear.
 *
 * @retval     FSP_SUCCESS               Access window successfully removed.
 * @retval     FSP_ERR_IN_USE            FLASH peripheral is busy with a prior operation.
 * @retval     FSP_ERR_ASSERTION         NULL provided for p_ctrl.
 * @retval     FSP_ERR_UNSUPPORTED       Code Flash Programming is not enabled.
 * @retval     FSP_ERR_NOT_OPEN          Flash API has not yet been opened.
 * @retval     FSP_ERR_PE_FAILURE        Failed to enter or exit Code Flash P/E mode.
 * @retval     FSP_ERR_TIMEOUT           Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_WRITE_FAILED      Status is indicating a Programming error for the requested operation.
 * @retval     FSP_ERR_CMD_LOCKED        FCU is in locked state, typically as a result of having received an illegal
 *                                       command.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_AccessWindowClear (flash_ctrl_t * const p_api_ctrl)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

    fsp_err_t err = FSP_SUCCESS;

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1) && (BSP_FEATURE_FLASH_SUPPORTS_ACCESS_WINDOW == 1)
 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the control block is not null and is opened. */
    err = r_flash_hp_common_parameter_checking(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
 #endif

    /* Clear the access window. When the start address and end address are set to the same value, then the access
     * window is effectively removed. */
    err = flash_hp_access_window_set(p_ctrl, 0, 0);
#else

    /* Eliminate warning if code flash programming is disabled. */
    FSP_PARAMETER_NOT_USED(p_ctrl);

    err = FSP_ERR_UNSUPPORTED;         ///< For consistency with _LP API we return error if Code Flash not enabled
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Reset the FLASH peripheral. Implements @ref flash_api_t::reset.
 *
 * No attempt is made to check if the flash is busy before executing the reset since the assumption is that a reset will
 * terminate any existing operation.
 *
 * @retval     FSP_SUCCESS         Flash circuit successfully reset.
 * @retval     FSP_ERR_ASSERTION   NULL provided for p_ctrl.
 * @retval     FSP_ERR_NOT_OPEN    The control block is not open.
 * @retval     FSP_ERR_PE_FAILURE  Failed to enter or exit P/E mode.
 * @retval     FSP_ERR_TIMEOUT     Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_CMD_LOCKED  FCU is in locked state, typically as a result of having received an illegal command.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_Reset (flash_ctrl_t * const p_api_ctrl)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

    /* Eliminate warning if parameter checking is disabled. */
    FSP_PARAMETER_NOT_USED(p_ctrl);

#if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE)

    /* If null pointer return error. */
    FSP_ASSERT(NULL != p_ctrl);

    /* If control block not open return error. */
    FSP_ERROR_RETURN(FLASH_HP_OPEN == p_ctrl->opened, FSP_ERR_NOT_OPEN);
#endif

    /* Reset the flash. */
    return flash_hp_reset(p_ctrl);
}

/*******************************************************************************************************************//**
 * Selects which block, Default (Block 0) or Alternate (Block 1), is used as the startup area block. The provided
 * parameters determine which block will become the active startup block and whether that action will be immediate (but
 * temporary), or permanent subsequent to the next reset. Doing a temporary switch might appear to have limited
 * usefulness. If there is an access window in place such that Block 0 is write protected, then one could do a temporary
 * switch, update the block and switch them back without having to touch the access window. Implements
 * @ref flash_api_t::startupAreaSelect.
 *
 * @retval     FSP_SUCCESS               Start-up area successfully toggled.
 * @retval     FSP_ERR_IN_USE            FLASH peripheral is busy with a prior operation.
 * @retval     FSP_ERR_ASSERTION         NULL provided for p_ctrl.
 * @retval     FSP_ERR_NOT_OPEN          The control block is not open.
 * @retval     FSP_ERR_UNSUPPORTED       Code Flash Programming is not enabled.
 * @retval     FSP_ERR_PE_FAILURE        Failed to enter or exit Code Flash P/E mode.
 * @retval     FSP_ERR_TIMEOUT           Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_WRITE_FAILED      Status is indicating a Programming error for the requested operation.
 * @retval     FSP_ERR_CMD_LOCKED        FCU is in locked state, typically as a result of having received an illegal
 *                                       command.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_StartUpAreaSelect (flash_ctrl_t * const      p_api_ctrl,
                                        flash_startup_area_swap_t swap_type,
                                        bool                      is_temporary)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

    /* Eliminate warning if parameter checking is disabled. */
    FSP_PARAMETER_NOT_USED(p_ctrl);

    fsp_err_t err = FSP_SUCCESS;

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)
 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the control block is not null and is opened. */
    err = r_flash_hp_common_parameter_checking(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* If the swap type is BTFLG and the operation is temporary there's nothing to do. */
    FSP_ASSERT(!((swap_type == FLASH_STARTUP_AREA_BTFLG) && (is_temporary == false)));
 #endif

    err = flash_hp_set_startup_area_boot(p_ctrl, swap_type, is_temporary);
#else

    /* Eliminate warning if code flash programming is disabled. */
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(swap_type);
    FSP_PARAMETER_NOT_USED(is_temporary);

    err = FSP_ERR_UNSUPPORTED;         ///< For consistency with _LP API we return error if Code Flash not enabled
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Swaps the flash bank located at address 0x00000000 and address 0x00200000. This can only be done when in dual bank
 * mode. Dual bank mode can be enabled in the FSP Configuration Tool under BSP Properties. After a bank swap is done
 * the MCU will need to be reset for the changes to take place.
 * @ref flash_api_t::bankSwap.
 *
 * @retval     FSP_SUCCESS               Start-up area successfully toggled.
 * @retval     FSP_ERR_IN_USE            FLASH peripheral is busy with a prior operation.
 * @retval     FSP_ERR_ASSERTION         NULL provided for p_ctrl.
 * @retval     FSP_ERR_NOT_OPEN          The control block is not open.
 * @retval     FSP_ERR_UNSUPPORTED       Code Flash Programming is not enabled.
 * @retval     FSP_ERR_PE_FAILURE        Failed to enter or exit Code Flash P/E mode.
 * @retval     FSP_ERR_TIMEOUT           Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_INVALID_MODE      Cannot switch banks while flash is in Linear mode.
 * @retval     FSP_ERR_WRITE_FAILED      Flash write operation failed.
 * @retval     FSP_ERR_CMD_LOCKED        FCU is in locked state, typically as a result of having received an illegal
 *                                       command.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_BankSwap (flash_ctrl_t * const p_api_ctrl)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

    /* Eliminate warning if parameter checking is disabled. */
    FSP_PARAMETER_NOT_USED(p_ctrl);

    fsp_err_t err = FSP_SUCCESS;

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1) && (BSP_FEATURE_FLASH_HP_SUPPORTS_DUAL_BANK == 1)
 #if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the control block is not null and is opened. */
    err = r_flash_hp_common_parameter_checking(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    FSP_ERROR_RETURN(0 == (*flash_hp_dualsel & FLASH_HP_PRV_DUALSEL_BANKMD_MASK), FSP_ERR_INVALID_MODE)
 #endif

    flash_hp_bank_swap(p_ctrl);
#else

    /* Eliminate warning if code flash programming is disabled. */
    FSP_PARAMETER_NOT_USED(p_ctrl);

    err = FSP_ERR_UNSUPPORTED;         ///< For consistency with _LP API we return error if Code Flash not enabled
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Indicate to the already open Flash API that the FCLK has changed. Implements @ref flash_api_t::updateFlashClockFreq.
 *
 * This could be the case if the application has changed the system clock, and therefore the FCLK. Failure to call this
 * function subsequent to changing the FCLK could result in damage to the flash macro.
 *
 * @retval     FSP_SUCCESS        Start-up area successfully toggled.
 * @retval     FSP_ERR_IN_USE     Flash is busy with an on-going operation.
 * @retval     FSP_ERR_ASSERTION  NULL provided for p_ctrl
 * @retval     FSP_ERR_NOT_OPEN   Flash API has not yet been opened.
 * @retval     FSP_ERR_FCLK       FCLK is not within the acceptable range.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_UpdateFlashClockFreq (flash_ctrl_t * const p_api_ctrl)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

#if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the control block is not null and is opened. */
    fsp_err_t err = r_flash_hp_common_parameter_checking(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
#endif

    /* Calculate timeout values */
    return flash_hp_init(p_ctrl);
}

/*******************************************************************************************************************//**
 * Returns the information about the flash regions. Implements @ref flash_api_t::infoGet.
 *
 * @retval     FSP_SUCCESS        Successful retrieved the request information.
 * @retval     FSP_ERR_NOT_OPEN   The control block is not open.
 * @retval     FSP_ERR_ASSERTION  NULL provided for p_ctrl or p_info.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_InfoGet (flash_ctrl_t * const p_api_ctrl, flash_info_t * const p_info)
{
#if FLASH_HP_CFG_PARAM_CHECKING_ENABLE

    /* If null pointers return error. */
    FSP_ASSERT(NULL != p_api_ctrl);
    FSP_ASSERT(NULL != p_info);

    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

    /* If the flash api is not open return an error. */
    FSP_ERROR_RETURN(FLASH_HP_OPEN == p_ctrl->opened, FSP_ERR_NOT_OPEN);
#else

    /* Eliminate warning if parameter checking is disabled. */
    FSP_PARAMETER_NOT_USED(p_api_ctrl);
#endif

    /* Copy information about the code and data flash to the user structure. */
    p_info->code_flash = g_flash_code_region;
    p_info->data_flash = g_flash_data_region;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Releases any resources that were allocated by the Open() or any subsequent Flash operations. Implements
 * @ref flash_api_t::close.
 *
 * @retval     FSP_SUCCESS        Successful close.
 * @retval     FSP_ERR_NOT_OPEN   The control block is not open.
 * @retval     FSP_ERR_ASSERTION  NULL provided for p_ctrl or p_cfg.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_Close (flash_ctrl_t * const p_api_ctrl)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

    /* Eliminate warning if parameter checking is disabled. */
    FSP_PARAMETER_NOT_USED(p_ctrl);

    fsp_err_t err = FSP_SUCCESS;

#if FLASH_HP_CFG_PARAM_CHECKING_ENABLE

    /* If null pointer return error. */
    FSP_ASSERT(NULL != p_ctrl);

    /* If the flash api is not open return an error. */
    FSP_ERROR_RETURN(FLASH_HP_OPEN == p_ctrl->opened, FSP_ERR_NOT_OPEN);
#endif

    /* Close the API */
    p_ctrl->opened = FLASH_HP_CLOSE;

    if (p_ctrl->p_cfg->irq >= 0)
    {
        /* Disable interrupt in ICU */
        R_BSP_IrqDisable(p_ctrl->p_cfg->irq);
    }

    /* Disable Flash Rdy interrupt in the FLASH peripheral */
    R_FACI_HP->FRDYIE = 0x00U;

    if (p_ctrl->p_cfg->err_irq >= 0)
    {
        /* Disable interrupt in ICU */
        R_BSP_IrqDisable(p_ctrl->p_cfg->err_irq);
    }

    /* Disable Flash Error interrupt in the FLASH peripheral */
    R_FACI_HP->FAEINT = 0x00U;

    return err;
}

/*******************************************************************************************************************//**
 * Updates the user callback with the option to provide memory for the callback argument structure.
 * Implements @ref flash_api_t::callbackSet.
 *
 * @retval  FSP_SUCCESS                  Callback updated successfully.
 * @retval  FSP_ERR_ASSERTION            A required pointer is NULL.
 * @retval  FSP_ERR_NOT_OPEN             The control block has not been opened.
 * @retval  FSP_ERR_NO_CALLBACK_MEMORY   p_callback is non-secure and p_callback_memory is either secure or NULL.
 **********************************************************************************************************************/
fsp_err_t R_FLASH_HP_CallbackSet (flash_ctrl_t * const          p_api_ctrl,
                                  void (                      * p_callback)(flash_callback_args_t *),
                                  void const * const            p_context,
                                  flash_callback_args_t * const p_callback_memory)
{
    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) p_api_ctrl;

#if FLASH_HP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_ctrl);
    FSP_ASSERT(p_callback);
    FSP_ERROR_RETURN(FLASH_HP_OPEN == p_ctrl->opened, FSP_ERR_NOT_OPEN);
#endif

#if BSP_TZ_SECURE_BUILD

    /* Get security state of p_callback */
    bool callback_is_secure =
        (NULL == cmse_check_address_range((void *) p_callback, sizeof(void *), CMSE_AU_NONSECURE));

 #if FLASH_HP_CFG_PARAM_CHECKING_ENABLE

    /* In secure projects, p_callback_memory must be provided in non-secure space if p_callback is non-secure */
    flash_callback_args_t * const p_callback_memory_checked = cmse_check_pointed_object(p_callback_memory,
                                                                                        CMSE_AU_NONSECURE);
    FSP_ERROR_RETURN(callback_is_secure || (NULL != p_callback_memory_checked), FSP_ERR_NO_CALLBACK_MEMORY);
 #endif
#endif

    /* Store callback and context */
#if BSP_TZ_SECURE_BUILD
    p_ctrl->p_callback = callback_is_secure ? p_callback :
                         (void (*)(flash_callback_args_t *))cmse_nsfptr_create(p_callback);
#else
    p_ctrl->p_callback = p_callback;
#endif
    p_ctrl->p_context         = p_context;
    p_ctrl->p_callback_memory = p_callback_memory;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup FLASH_HP)
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Write data to the flash.
 *
 * @param      p_ctrl              Pointer to the control block
 * @param[in]  write_size          The write size
 * @param[in]  timeout             The timeout
 *
 * @retval     FSP_SUCCESS         Write completed succesfully
 * @retval     FSP_ERR_TIMEOUT     Flash timed out during write operation.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_write_data (flash_hp_instance_ctrl_t * const p_ctrl, uint32_t write_size, uint32_t timeout)
{
    volatile uint32_t wait_count;

    /* Set block start address */
    R_FACI_HP->FSADDR = p_ctrl->dest_end_address;

    /* Issue two part Write commands */
    R_FACI_HP_CMD->FACI_CMD8 = FLASH_HP_FACI_CMD_PROGRAM;
    R_FACI_HP_CMD->FACI_CMD8 = (uint8_t) (write_size / 2U);

    /* Write one line. If timeout stop the flash and return error. */
    for (uint32_t i = 0U; i < (write_size / 2U); i++)
    {
        wait_count = p_ctrl->timeout_dbfull;

        /* Copy data from source address to destination area */
        R_FACI_HP_CMD->FACI_CMD16 = *(uint16_t *) p_ctrl->source_start_address;

        while (R_FACI_HP->FSTATR_b.DBFULL == 1U)
        {
            /* Wait until DBFULL is 0 unless timeout occurs. */
            if (wait_count-- == 0U)
            {

                /* If DBFULL is not set to 0 return timeout. */
                return FSP_ERR_TIMEOUT;
            }
        }

        /* Each write handles 2 bytes of data. */
        p_ctrl->source_start_address += 2U;
        p_ctrl->dest_end_address     += 2U;
        p_ctrl->operations_remaining--;
    }

    /* Issue write end command */
    R_FACI_HP_CMD->FACI_CMD8 = FLASH_HP_FACI_CMD_FINAL;

    /* If a timeout has been supplied wait for the flash to become ready. Otherwise return immediately. */
    if (timeout)
    {
        /* Read FRDY bit until it has been set to 1 indicating that the current operation is complete.*/
        /* Wait until operation has completed or timeout occurs. If timeout stop the module and return error. */
        while (1U != R_FACI_HP->FSTATR_b.FRDY)
        {
            /* Wait until FRDY is 1 unless timeout occurs. */
            if (timeout == 0U)
            {
                return FSP_ERR_TIMEOUT;
            }

            timeout--;
        }
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Check for errors after a flash operation
 *
 * @param[in]  previous_error      The previous error
 * @param[in]  error_bits          The error bits
 * @param[in]  return_error        The return error
 *
 * @retval     FSP_SUCCESS         Command completed succesfully
 * @retval     FSP_ERR_CMD_LOCKED  Flash entered command locked state
 **********************************************************************************************************************/
static fsp_err_t flash_hp_check_errors (fsp_err_t previous_error, uint32_t error_bits, fsp_err_t return_error)
{
    /* See "Recovery from the Command-Locked State": Section 47.9.3.6 of the RA6M4 manual R01UH0890EJ0100.*/
    fsp_err_t err = FSP_SUCCESS;
    if (1U == R_FACI_HP->FASTAT_b.CMDLK)
    {
        /* If an illegal error occurred read and clear CFAE and DFAE in FASTAT. */
        if (1U == R_FACI_HP->FSTATR_b.ILGLERR)
        {
            R_FACI_HP->FASTAT;
            R_FACI_HP->FASTAT = 0;
        }

        err = FSP_ERR_CMD_LOCKED;
    }

    /* Check if status is indicating a Programming error */
    /* If a programming error occurred return error. */
    if (R_FACI_HP->FSTATR & error_bits)
    {
        err = return_error;
    }

    /* Return the first error code that was encountered. */
    if (FSP_SUCCESS != previous_error)
    {
        err = previous_error;
    }

    if (FSP_SUCCESS != err)
    {
        /* Stop the flash and return the previous error. */
        (void) flash_hp_stop();
    }

    return err;
}

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * This function initiates the write sequence for the R_FLASH_HP_Write() function.
 * @param[in]  p_ctrl                Flash control block
 * @retval     FSP_SUCCESS           The write started successfully.
 * @retval     FSP_ERR_PE_FAILURE    Failed to enter or exit Code Flash P/E mode.
 * @retval     FSP_ERR_CMD_LOCKED    Flash entered command locked state.
 * @retval     FSP_ERR_TIMEOUT       Flash timed out during write operation.
 * @retval     FSP_ERR_WRITE_FAILED  Flash write operation failed.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_cf_write (flash_hp_instance_ctrl_t * const p_ctrl)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Update Flash state and enter the respective Code or Data Flash P/E mode. If failure return error */
    err = flash_hp_enter_pe_cf_mode(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Iterate through the number of data bytes */
    while (p_ctrl->operations_remaining && (err == FSP_SUCCESS))
    {
        err = flash_hp_write_data(p_ctrl, BSP_FEATURE_FLASH_HP_CF_WRITE_SIZE, p_ctrl->timeout_write_cf);

        err = flash_hp_check_errors(err, FLASH_HP_FSTATR_PRGERR | FLASH_HP_FSTATR_ILGLERR, FSP_ERR_WRITE_FAILED);
    }

    /* Return to read mode*/
    fsp_err_t pe_exit_err = flash_hp_pe_mode_exit();

    if (FSP_SUCCESS == err)
    {
        err = pe_exit_err;
    }

    /* Return status. */
    return err;
}

 #if (BSP_FEATURE_FLASH_HP_SUPPORTS_DUAL_BANK == 1)

/*******************************************************************************************************************//**
 * This function swaps which flash bank will be used to boot from after the next reset.
 * @param[in]  p_ctrl                Flash control block
 * @retval     FSP_SUCCESS           The write started successfully.
 * @retval     FSP_ERR_PE_FAILURE    Failed to enter or exit Code Flash P/E mode.
 * @retval     FSP_ERR_CMD_LOCKED    Flash entered command locked state.
 * @retval     FSP_ERR_TIMEOUT       Flash timed out during write operation.
 * @retval     FSP_ERR_WRITE_FAILED  Flash write operation failed.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_bank_swap (flash_hp_instance_ctrl_t * const p_ctrl)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Unused bits should be written as 1. */
    g_configuration_area_data[0] =
        (uint16_t) ((~FLASH_HP_PRV_BANKSEL_BANKSWP_MASK) | (~(FLASH_HP_PRV_BANKSEL_BANKSWP_MASK & *flash_hp_banksel)));

    memset(&g_configuration_area_data[1], UINT8_MAX, 7 * sizeof(uint16_t));

    flash_hp_enter_pe_cf_mode(p_ctrl);

    /* Write the configuration area to the access/startup region. */
    err = flash_hp_configuration_area_write(p_ctrl, FLASH_HP_FCU_CONFIG_SET_BANK_MODE);

    err = flash_hp_check_errors(err, 0, FSP_ERR_WRITE_FAILED);

    /* Return to read mode*/
    fsp_err_t pe_exit_err = flash_hp_pe_mode_exit();

    if (FSP_SUCCESS == err)
    {
        err = pe_exit_err;
    }

    /* Return status. */
    return err;
}

 #endif
#endif

#if (FLASH_HP_CFG_DATA_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * This function initiates the write sequence for the R_FLASH_HP_Write() function.
 * @param[in]  p_ctrl                Flash control block
 * @retval     FSP_SUCCESS           The write started successfully.
 * @retval     FSP_ERR_PE_FAILURE    Failed to enter or exit P/E mode.
 * @retval     FSP_ERR_CMD_LOCKED    Flash entered command locked state.
 * @retval     FSP_ERR_TIMEOUT       Flash timed out during write operation.
 * @retval     FSP_ERR_WRITE_FAILED  Flash write operation failed.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_df_write (flash_hp_instance_ctrl_t * const p_ctrl)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Update Flash state and enter the respective Code or Data Flash P/E mode. If failure return error */
    err = flash_hp_enter_pe_df_mode(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    uint32_t wait_count;

    /* If in BGO mode, exit here; remaining processing if any will be done in ISR */
    if (p_ctrl->p_cfg->data_flash_bgo == true)
    {
        p_ctrl->current_operation = FLASH_OPERATION_DF_BGO_WRITE;
        wait_count                = 0;
    }
    else
    {
        wait_count = p_ctrl->timeout_write_df;
    }

    do
    {
        err = flash_hp_write_data(p_ctrl, BSP_FEATURE_FLASH_HP_DF_WRITE_SIZE, wait_count);

        if (p_ctrl->p_cfg->data_flash_bgo)
        {

            /* Errors will be handled in the error interrupt when using BGO. */
            return err;
        }

        err = flash_hp_check_errors(err, FLASH_HP_FSTATR_PRGERR, FSP_ERR_WRITE_FAILED);
    } while (p_ctrl->operations_remaining && (err == FSP_SUCCESS));

    /* Return to read mode*/
    fsp_err_t pe_exit_err = flash_hp_pe_mode_exit();

    if (FSP_SUCCESS == err)
    {
        err = pe_exit_err;
    }

    return err;
}

#endif

#if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)

/*******************************************************************************************************************//**
 * This function performs the parameter checking required by the write, read and blank check functions.
 *
 * @param[in]  p_ctrl                   Flash control block
 * @param[in]  flash_address            The flash address to be written to
 * @param[in]  num_bytes                The number bytes
 * @param[in]  check_write              Check that the parameters are valid for a write.
 *
 * @retval     FSP_SUCCESS              Parameter checking completed without error.
 * @retval     FSP_ERR_IN_USE           The Flash peripheral is busy with a prior on-going transaction.
 * @retval     FSP_ERR_NOT_OPEN         The Flash API is not Open.
 * @retval     FSP_ERR_ASSERTION        Null pointer
 * @retval     FSP_ERR_INVALID_SIZE     Number of bytes provided was not a multiple of the programming size or exceeded
 *                                      the maximum range.
 * @retval     FSP_ERR_INVALID_ADDRESS  Invalid address was input or address not on programming boundary.
 **********************************************************************************************************************/
static fsp_err_t r_flash_hp_write_bc_parameter_checking (flash_hp_instance_ctrl_t * const p_ctrl,
                                                         uint32_t                         flash_address,
                                                         uint32_t const                   num_bytes,
                                                         bool                             check_write)
{
    /* Verify the control block is not null and is opened. Verify the flash isn't in use. */
    fsp_err_t err = r_flash_hp_common_parameter_checking(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    uint32_t write_size;

    if (p_ctrl->p_cfg->data_flash_bgo == true)
    {
        FSP_ASSERT(NULL != p_ctrl->p_callback);
    }

    /* If invalid address or number of bytes return error. */
 #if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)
    if (flash_address < BSP_FEATURE_FLASH_DATA_FLASH_START)
    {
        uint32_t rom_end = BSP_ROM_SIZE_BYTES;
  #if BSP_FEATURE_FLASH_HP_SUPPORTS_DUAL_BANK
        if (0 == (FLASH_HP_PRV_DUALSEL_BANKMD_MASK & *flash_hp_dualsel))
        {
            flash_address = flash_address % BSP_FEATURE_FLASH_HP_CF_DUAL_BANK_START;
            rom_end       = BSP_ROM_SIZE_BYTES / 2;
        }
  #endif

        FSP_ERROR_RETURN(flash_address + num_bytes <= rom_end, FSP_ERR_INVALID_SIZE);
        write_size = BSP_FEATURE_FLASH_HP_CF_WRITE_SIZE;
    }
    else
 #endif
    {
 #if (FLASH_HP_CFG_DATA_FLASH_PROGRAMMING_ENABLE == 1)
        FSP_ERROR_RETURN((flash_address >= (FLASH_HP_DF_START_ADDRESS)) &&
                         (flash_address < (FLASH_HP_DF_START_ADDRESS + BSP_DATA_FLASH_SIZE_BYTES)),
                         FSP_ERR_INVALID_ADDRESS);
        FSP_ERROR_RETURN((flash_address + num_bytes <= (FLASH_HP_DF_START_ADDRESS + BSP_DATA_FLASH_SIZE_BYTES)),
                         FSP_ERR_INVALID_SIZE);
        write_size = BSP_FEATURE_FLASH_HP_DF_WRITE_SIZE;
 #else

        return FSP_ERR_INVALID_ADDRESS;
 #endif
    }

    if (check_write)
    {
        FSP_ERROR_RETURN(!(flash_address & (write_size - 1U)), FSP_ERR_INVALID_ADDRESS);
        FSP_ERROR_RETURN(!(num_bytes & (write_size - 1U)), FSP_ERR_INVALID_SIZE);
    }

    /* If invalid number of bytes return error. */
    FSP_ERROR_RETURN((0 != num_bytes), FSP_ERR_INVALID_SIZE);

    /* If control block is not open return error. */
    FSP_ERROR_RETURN((FLASH_HP_OPEN == p_ctrl->opened), FSP_ERR_NOT_OPEN);

    return FSP_SUCCESS;
}

#endif

#if (FLASH_HP_CFG_PARAM_CHECKING_ENABLE == 1)

/*******************************************************************************************************************//**
 * This function performs the common parameter checking required by top level API functions.
 * @param      p_ctrl             Pointer to the control block
 * @retval     FSP_SUCCESS        Parameter checking completed without error.
 * @retval     FSP_ERR_ASSERTION  Null pointer
 * @retval     FSP_ERR_NOT_OPEN   The flash module is not open.
 * @retval     FSP_ERR_IN_USE     The Flash peripheral is busy with a prior on-going transaction.
 **********************************************************************************************************************/
static fsp_err_t r_flash_hp_common_parameter_checking (flash_hp_instance_ctrl_t * const p_ctrl)
{
    /* If null control block return error. */
    FSP_ASSERT(p_ctrl);

    /* If control block is not open return error. */
    FSP_ERROR_RETURN((FLASH_HP_OPEN == p_ctrl->opened), FSP_ERR_NOT_OPEN);

    /* If the flash is currently in program/erase mode return an error. */
    FSP_ERROR_RETURN((R_FACI_HP->FENTRYR & FLASH_HP_FENTRYR_PE_MODE_BITS) == 0x0000U, FSP_ERR_IN_USE);

    return FSP_SUCCESS;
}

#endif

#if (FLASH_HP_CFG_DATA_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * This function performs the Blank check phase required by the R_FLASH_HP_BlankCheck() function.
 *
 * @param[in]  p_ctrl                      Flash control block
 * @param[in]  address                     The start address of the range to blank check
 * @param[in]  num_bytes                   The number bytes
 * @param[out] p_blank_check_result        The blank check result
 *
 * @retval     FSP_SUCCESS                 Setup completed completed without error.
 * @retval     FSP_ERR_PE_FAILURE          Failed to enter P/E mode
 * @retval     FSP_ERR_CMD_LOCKED          Peripheral in command locked state.
 * @retval     FSP_ERR_TIMEOUT             Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_BLANK_CHECK_FAILED  Blank check operation failed.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_df_blank_check (flash_hp_instance_ctrl_t * const p_ctrl,
                                          uint32_t const                   address,
                                          uint32_t                         num_bytes,
                                          flash_result_t                 * p_blank_check_result)
{
    fsp_err_t err;

    /* This is a request to Blank check Data Flash */
    err = flash_hp_enter_pe_df_mode(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Start the Blank check operation. If BGO is enabled then the result of the Blank check will be
     * available when the interrupt occurs and p_blank_check_result will contain FLASH_RESULT_BGO_ACTIVE */

    /* Call blank check. If successful and not DF BGO operation then enter read mode. */
    /* Set the mode to incremental*/
    R_FACI_HP->FBCCNT = 0x00U;

    /* Set the start address for blank checking*/
    R_FACI_HP->FSADDR = address;

    /* Set the end address for blank checking*/
    R_FACI_HP->FEADDR = (address + num_bytes) - 1U;

    /* Issue two part Blank Check command*/
    R_FACI_HP_CMD->FACI_CMD8 = FLASH_HP_FACI_CMD_BLANK_CHECK;
    R_FACI_HP_CMD->FACI_CMD8 = FLASH_HP_FACI_CMD_FINAL;

    /* If in DF BGO mode, exit here; remaining processing if any will be done in ISR */
    if (p_ctrl->p_cfg->data_flash_bgo == true)
    {
        p_ctrl->current_operation = FLASH_OPERATION_DF_BGO_BLANKCHECK;
        *p_blank_check_result     = FLASH_RESULT_BGO_ACTIVE;

        return FSP_SUCCESS;
    }

    /* timeout_blank_check specifies the wait time for a 4 byte blank check,
     * num_bytes is divided by 4 and then multiplied to calculate the wait time for the entire operation*/

    /* Configure the timeout value. */
    uint32_t wait_count = (p_ctrl->timeout_blank_check * ((num_bytes >> 2) + 1));

    /* Wait for the operation to complete or timeout. If timeout stop the module and return error. */

    /* Read FRDY bit until it has been set to 1 indicating that the current
     * operation is complete. */
    while (1U != R_FACI_HP->FSTATR_b.FRDY)
    {
        /* Wait until FRDY is 1 unless timeout occurs. */
        if (wait_count == 0U)
        {
            err = FSP_ERR_TIMEOUT;
            break;
        }

        wait_count--;
    }

    /* Check the status of the Blank Check operation. */
    err = flash_hp_check_errors(err, 0, FSP_ERR_BLANK_CHECK_FAILED);

    /* Set the result to blank or not blank. */
    if (R_FACI_HP->FBCSTAT == 0x01U)
    {
        *p_blank_check_result = FLASH_RESULT_NOT_BLANK;
    }
    else
    {
        *p_blank_check_result = FLASH_RESULT_BLANK;
    }

    /* Return to read mode*/
    fsp_err_t pe_exit_err = flash_hp_pe_mode_exit();

    if (FSP_SUCCESS == err)
    {
        err = pe_exit_err;
    }

    /* Return status. */
    return err;
}

#endif

/*******************************************************************************************************************//**
 * This function will initialize the FCU and set the FCU Clock based on the current FCLK frequency.
 * @param      p_ctrl        Pointer to the instance control block
 * @retval     FSP_SUCCESS   Clock and timeouts configured succesfully.
 * @retval     FSP_ERR_FCLK  FCLK is not within the acceptable range.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_init (flash_hp_instance_ctrl_t * p_ctrl)
{
    p_ctrl->current_operation = FLASH_OPERATION_NON_BGO;

    /*Allow Access to the Flash registers*/
    R_SYSTEM->FWEPROR = 0x01U;

    uint32_t flash_clock_freq_hz  = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_FCLK);
    uint32_t system_clock_freq_hz = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_ICLK);

    FSP_ERROR_RETURN(flash_clock_freq_hz >= FLASH_HP_MINIMUM_SUPPORTED_FCLK_FREQ, FSP_ERR_FCLK);

    /* Round up the frequency to a whole number */
    uint32_t flash_clock_freq_mhz = (flash_clock_freq_hz + (FLASH_HP_FREQUENCY_IN_HZ - 1)) /
                                    FLASH_HP_FREQUENCY_IN_HZ;

    /* Round up the frequency to a whole number */
    uint32_t system_clock_freq_mhz = (system_clock_freq_hz + (FLASH_HP_FREQUENCY_IN_HZ - 1)) /
                                     FLASH_HP_FREQUENCY_IN_HZ;

    /* Set the Clock */
    R_FACI_HP->FPCKAR = (uint16_t) (FLASH_HP_FPCKAR_KEY + flash_clock_freq_mhz);

    /*  According to HW Manual the Max Programming Time for 256 bytes (ROM)
     *  is 15.8ms.  This is with a FCLK of 4MHz. The calculation below
     *  calculates the number of ICLK ticks needed for the timeout delay.
     */
    p_ctrl->timeout_write_cf = (uint32_t) (FLASH_HP_MAX_WRITE_CF_US * system_clock_freq_mhz) /
                               R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the Max Programming Time for 4 bytes
     *  (Data Flash) is 3.8ms.  This is with a FCLK of 4MHz. The calculation
     *  below calculates the number of ICLK ticks needed for the timeout delay.
     */
    p_ctrl->timeout_write_df = (uint32_t) (FLASH_HP_MAX_WRITE_DF_US * system_clock_freq_mhz) /
                               R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the Data Buffer Full time for 2 bytes
     *  is 2 usec.  This is with a FCLK of 4MHz. The calculation
     *  below calculates the number of ICLK ticks needed for the timeout delay.
     */
    p_ctrl->timeout_dbfull = (uint32_t) (FLASH_HP_MAX_DBFULL_US * system_clock_freq_mhz) /
                             R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the Max Blank Check time for 4 bytes
     *  (Data Flash) is 84 usec.  This is with a FCLK of 4MHz. The calculation
     *  below calculates the number of ICLK ticks needed for the timeout delay.
     */
    p_ctrl->timeout_blank_check = (uint32_t) (FLASH_HP_MAX_BLANK_CHECK_US * system_clock_freq_mhz) /
                                  R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the max timeout value when using the configuration set
     *  command is 307 ms. This is with a FCLK of 4MHz. The
     *  calculation below calculates the number of ICLK ticks needed for the
     *  timeout delay.
     */
    p_ctrl->timeout_write_config = (uint32_t) (FLASH_HP_MAX_WRITE_CONFIG_US * system_clock_freq_mhz) /
                                   R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the Max Erasure Time for a 64 byte Data Flash block is
     *  around 18ms.  This is with a FCLK of 4MHz. The calculation below
     *  calculates the number of ICLK ticks needed for the timeout delay.
     */
    p_ctrl->timeout_erase_df_block = (uint32_t) (FLASH_HP_MAX_ERASE_DF_BLOCK_US * system_clock_freq_mhz) /
                                     R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the Max Erasure Time for a 32KB block is
     *  around 1040ms.  This is with a FCLK of 4MHz. The calculation below
     *  calculates the number of ICLK ticks needed for the timeout delay.
     */
    p_ctrl->timeout_erase_cf_large_block =
        (uint32_t) (FLASH_HP_MAX_ERASE_CF_LARGE_BLOCK_US * system_clock_freq_mhz) /
        R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    /*  According to HW Manual the Max Erasure Time for a 8KB block is
     *  around 260ms.  This is with a FCLK of 4MHz. The calculation below
     *  calculates the number of ICLK ticks needed for the timeout delay.
     */
    p_ctrl->timeout_erase_cf_small_block =
        (uint32_t) (FLASH_HP_MAX_ERASE_CF_SMALL_BLOCK_US * system_clock_freq_mhz) /
        R_FLASH_HP_CYCLES_MINIMUM_PER_TIMEOUT_LOOP;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Erase the block at the start address in the control block.
 *
 * @param      p_ctrl          Pointer to the instance control block
 * @param[in]  block_size      The block size
 * @param[in]  timeout         The timeout for the block erase
 *
 * @retval     FSP_SUCCESS     The erase completed successfully.
 * @retval     FSP_ERR_TIMEOUT The erase operation timed out.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_erase_block (flash_hp_instance_ctrl_t * const p_ctrl, uint32_t block_size, uint32_t timeout)
{
    /* Set block start address*/
    R_FACI_HP->FSADDR = p_ctrl->source_start_address;

    /* Issue two part Block Erase commands */
    R_FACI_HP_CMD->FACI_CMD8 = FLASH_HP_FACI_CMD_BLOCK_ERASE;
    R_FACI_HP_CMD->FACI_CMD8 = FLASH_HP_FACI_CMD_FINAL;

    p_ctrl->source_start_address += block_size;

    p_ctrl->operations_remaining--;

    /* Wait until the operation has completed or timeout. If timeout stop the flash and return error. */

    /* Read FRDY bit until it has been set to 1 indicating that the current
     * operation is complete.*/
    if (timeout)
    {
        while (1U != R_FACI_HP->FSTATR_b.FRDY)
        {
            /* Wait until FRDY is 1 unless timeout occurs. */
            if (timeout == 0U)
            {
                return FSP_ERR_TIMEOUT;
            }

            timeout--;
        }
    }

    return FSP_SUCCESS;
}

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * This function erases a specified number of Code Flash blocks
 *
 * @param      p_ctrl                Pointer to the control block
 * @param[in]  block_address         The starting address of the first block to erase.
 * @param[in]  num_blocks            The number of blocks to erase.
 *
 * @retval     FSP_SUCCESS           Successfully erased (non-BGO) mode or operation successfully started (BGO).
 * @retval     FSP_ERR_ERASE_FAILED  Status is indicating a Erase error.
 * @retval     FSP_ERR_CMD_LOCKED    FCU is in locked state, typically as a result of attempting to Write or Erase an
 *                                   area that is protected by an Access Window.
 * @retval     FSP_ERR_TIMEOUT       Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_PE_FAILURE    Failed to enter or exit Code Flash P/E mode.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_cf_erase (flash_hp_instance_ctrl_t * p_ctrl, uint32_t block_address, uint32_t num_blocks)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Set current operation parameters */
    p_ctrl->source_start_address = block_address;
    p_ctrl->operations_remaining = num_blocks;
    uint32_t wait_count;
    uint32_t block_size;

    err = flash_hp_enter_pe_cf_mode(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Set Erasure Priority Mode*/
    R_FACI_HP->FCPSR = 1U;

    while (p_ctrl->operations_remaining && (FSP_SUCCESS == err))
    {
        if ((p_ctrl->source_start_address & FLASH_HP_PRV_BANK1_MASK) < BSP_FEATURE_FLASH_HP_CF_REGION0_SIZE)
        {
            wait_count = p_ctrl->timeout_erase_cf_small_block;
            block_size = BSP_FEATURE_FLASH_HP_CF_REGION0_BLOCK_SIZE;
        }
        else
        {
            wait_count = p_ctrl->timeout_erase_cf_large_block;
            block_size = BSP_FEATURE_FLASH_HP_CF_REGION1_BLOCK_SIZE;
        }

        err = flash_hp_erase_block(p_ctrl, block_size, wait_count);

        /* Check the status of the erase operation. */
        err = flash_hp_check_errors(err, FLASH_HP_FSTATR_ERSERR | FLASH_HP_FSTATR_ILGLERR, FSP_ERR_ERASE_FAILED);
    }

    /* Return to read mode*/
    fsp_err_t pe_exit_err = flash_hp_pe_mode_exit();

    if (FSP_SUCCESS == err)
    {
        err = pe_exit_err;
    }

    return err;
}

#endif

#if (FLASH_HP_CFG_DATA_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * This function erases a specified number of Code or Data Flash blocks
 *
 * @param      p_ctrl                Pointer to the control block
 * @param[in]  block_address         The starting address of the first block to erase.
 * @param[in]  num_blocks            The number of blocks to erase.
 *
 * @retval     FSP_SUCCESS           Successfully erased (non-BGO) mode or operation successfully started (BGO).
 * @retval     FSP_ERR_ERASE_FAILED  Status is indicating a Erase error.
 * @retval     FSP_ERR_CMD_LOCKED    FCU is in locked state, typically as a result of attempting to Write or Erase an
 *                                   area that is protected by an Access Window.
 * @retval     FSP_ERR_TIMEOUT       Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_PE_FAILURE    Failed to enter or exit P/E mode.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_df_erase (flash_hp_instance_ctrl_t * p_ctrl, uint32_t block_address, uint32_t num_blocks)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Set current operation parameters */
    p_ctrl->source_start_address = block_address;
    p_ctrl->operations_remaining = num_blocks;
    uint32_t wait_count;

    err = flash_hp_enter_pe_df_mode(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, FSP_ERR_PE_FAILURE);

    /* If in BGO mode, exit here; remaining processing if any will be done in ISR */
    if (p_ctrl->p_cfg->data_flash_bgo)
    {
        p_ctrl->current_operation = FLASH_OPERATION_DF_BGO_ERASE;
        wait_count                = 0;
    }
    else
    {
        wait_count = p_ctrl->timeout_write_df;
    }

    /* Set Erasure Priority Mode*/
    R_FACI_HP->FCPSR = 1U;

    do
    {
        err = flash_hp_erase_block(p_ctrl, BSP_FEATURE_FLASH_HP_DF_BLOCK_SIZE, wait_count);

        if (p_ctrl->p_cfg->data_flash_bgo)
        {
            return err;
        }

        err = flash_hp_check_errors(err, FLASH_HP_FSTATR_ERSERR, FSP_ERR_ERASE_FAILED);
    } while (p_ctrl->operations_remaining && (err == FSP_SUCCESS));

    /* Return to read mode*/
    fsp_err_t pe_exit_err = flash_hp_pe_mode_exit();

    if (FSP_SUCCESS == err)
    {
        err = pe_exit_err;
    }

    return err;
}

#endif

/*******************************************************************************************************************//**
 * This function switches the peripheral from P/E mode for Code Flash or Data Flash to Read mode.
 *
 * @retval     FSP_SUCCESS         Successfully entered P/E mode.
 * @retval     FSP_ERR_PE_FAILURE  Failed to exited P/E mode
 * @retval     FSP_ERR_CMD_LOCKED  Flash entered command locked state.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_pe_mode_exit (void)
{
    /* See "Transition to Read Mode": Section 47.9.3.5 of the RA6M4 manual R01UH0890EJ0100. */
    /* FRDY and CMDLK are checked after the previous commands complete and do not need to be checked again. */
    fsp_err_t err      = FSP_SUCCESS;
    fsp_err_t temp_err = FSP_SUCCESS;
    uint32_t  pe_mode  = R_FACI_HP->FENTRYR;

    uint32_t wait_count = FLASH_HP_FRDY_CMD_TIMEOUT;

    /* Transition to Read mode */
    R_FACI_HP->FENTRYR = FLASH_HP_FENTRYR_READ_MODE;

    /* Wait until the flash is in read mode or timeout. If timeout return error. */
    /* Read FENTRYR until it has been set to 0x0000 indicating that we have successfully exited P/E mode.*/
    while (0U != R_FACI_HP->FENTRYR)
    {
        /* Wait until FENTRYR is 0x0000 unless timeout occurs. */
        if (wait_count == 0U)
        {
            /* If FENTRYR is not set after max timeout, FSP_ERR_PE_FAILURE*/
            err = FSP_ERR_PE_FAILURE;
        }

        wait_count--;
    }

    /* If the device is coming out of code flash p/e mode restore the flash cache state. */
    if (FLASH_HP_FENTRYR_CF_PE_MODE == pe_mode)
    {
#if BSP_FEATURE_FLASH_HP_HAS_FMEPROT
        R_FACI_HP->FMEPROT = FLASH_HP_FMEPROT_LOCK;
#endif

        R_BSP_FlashCacheEnable();
    }

#if BSP_FEATURE_BSP_HAS_CODE_SYSTEM_CACHE
    else if (FLASH_HP_FENTRYR_DF_PE_MODE == pe_mode)
    {
        /* Flush the C-CACHE. */
        R_CACHE->CCAFCT = 1U;
        FSP_HARDWARE_REGISTER_WAIT(R_CACHE->CCAFCT, 0U);
    }
    else
    {
        /* Do nothing. */
    }
#endif

    /* If a command locked state was detected earlier, then return that error. */
    if (FSP_ERR_CMD_LOCKED == temp_err)
    {
        err = temp_err;
    }

    return err;
}

/*******************************************************************************************************************//**
 * This function resets the Flash peripheral.
 * @param[in]  p_ctrl              Pointer to the Flash HP instance control block.
 * @retval     FSP_SUCCESS         Reset completed
 * @retval     FSP_ERR_PE_FAILURE  Failed to enter or exit P/E mode.
 * @retval     FSP_ERR_TIMEOUT     Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_CMD_LOCKED  FCU is in locked state, typically as a result of having received an illegal command.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_reset (flash_hp_instance_ctrl_t * p_ctrl)
{
    /* Disable the FACI interrupts, we don't want the reset itself to generate an interrupt */
    if (p_ctrl->p_cfg->data_flash_bgo)
    {
        R_BSP_IrqDisable(p_ctrl->p_cfg->irq);
        R_BSP_IrqDisable(p_ctrl->p_cfg->err_irq);
    }

    /* If not in PE mode enter PE mode. */
    if (R_FACI_HP->FENTRYR == 0x0000U)
    {
        /* Enter P/E mode so that we can execute some FACI commands. Either Code or Data Flash P/E mode would work
         * but Code Flash P/E mode requires FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1, which may not be true */
        flash_hp_enter_pe_df_mode(p_ctrl);
    }

    /* Issue a Flash Stop to stop any ongoing operation*/
    fsp_err_t err = flash_hp_stop();

    /* Issue a status clear to clear the locked-command state*/
    fsp_err_t temp_err = flash_hp_status_clear();

    /* Keep track of and return the first error encountered. */
    if (FSP_SUCCESS == err)
    {
        err = temp_err;
    }

    /* Transition back to Read mode*/
    temp_err = flash_hp_pe_mode_exit();

    if (FSP_SUCCESS == err)
    {
        err = temp_err;
    }

    /* Cancel any in progress background operation. */
    p_ctrl->current_operation = FLASH_OPERATION_NON_BGO;

    return err;
}

/*******************************************************************************************************************//**
 * This function is used to force stop the flash during an ongoing operation.
 *
 * @retval     FSP_SUCCESS         Successful stop.
 * @retval     FSP_ERR_TIMEOUT     Timeout executing flash_stop.
 * @retval     FSP_ERR_CMD_LOCKED  Peripheral in command locked state.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_stop (void)
{
    /* See "Forced Stop Command": Section 47.9.3.13 of the RA6M4 manual R01UH0890EJ0100. If the CMDLK bit
     * is still set after issuing the force stop command return an error. */
    volatile uint32_t wait_count = FLASH_HP_FRDY_CMD_TIMEOUT;

    R_FACI_HP_CMD->FACI_CMD8 = (uint8_t) FLASH_HP_FACI_CMD_FORCED_STOP;

    while (1U != R_FACI_HP->FSTATR_b.FRDY)
    {
        if (wait_count == 0U)
        {
            return FSP_ERR_TIMEOUT;
        }

        wait_count--;
    }

    if (0U != R_FACI_HP->FASTAT_b.CMDLK)
    {
        return FSP_ERR_CMD_LOCKED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * This function is used to clear the command-locked state .
 * @retval     FSP_SUCCESS         Successful stop.
 * @retval     FSP_ERR_TIMEOUT     Timeout executing flash_stop.Failed to exited P/E mode
 * @retval     FSP_ERR_CMD_LOCKED  Peripheral in command locked state
 **********************************************************************************************************************/
static fsp_err_t flash_hp_status_clear (void)
{
    /* See "Status Clear Command": Section 47.9.3.12 of the RA6M4 manual R01UH0890EJ0100. */
    /* Timeout counter. */
    volatile uint32_t wait_count = FLASH_HP_FRDY_CMD_TIMEOUT;

    /*Issue stop command to flash command area*/
    R_FACI_HP_CMD->FACI_CMD8 = (uint8_t) FLASH_HP_FACI_CMD_STATUS_CLEAR;

    /* Read FRDY bit until it has been set to 1 indicating that the current
     * operation is complete.*/
    while (1U != R_FACI_HP->FSTATR_b.FRDY)
    {
        /* Wait until FRDY is 1 unless timeout occurs. */
        if (wait_count == 0U)
        {

            /* This should not happen normally.
             * FRDY should get set in 15-20 ICLK cycles on STOP command*/
            return FSP_ERR_TIMEOUT;
        }

        wait_count--;
    }

    /*Check that Command Lock bit is cleared*/
    if (0U != R_FACI_HP->FASTAT_b.CMDLK)
    {
        return FSP_ERR_CMD_LOCKED;
    }

    return FSP_SUCCESS;
}

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * Set up the g_configuration_area_data for writing to the access/startup region of option setting memory.
 *
 * @param[in]  btflg_swap      The btflg swap
 * @param[in]  start_addr      The access window start address
 * @param[in]  end_addr        The access window end address
 **********************************************************************************************************************/
static void flash_hp_configuration_area_data_setup (uint32_t btflg_swap, uint32_t start_addr, uint32_t end_addr)
{
    /* Unused bits should be written as 1. */
    g_configuration_area_data[0] = UINT16_MAX;
    g_configuration_area_data[1] = UINT16_MAX;

    /* Prepare the configuration data. */

    /* Bit 15 is BTFLG(Startup Area Select Flag) */
    /* Bits 10:0 are FAWE(Flash Access Window End Block). */
    /* Unused bits should be written as 1. */
    g_configuration_area_data[FLASH_HP_FCU_CONFIG_SET_FAWE_BTFLG_OFFSET] =
        (uint16_t) (FLASH_HP_FCU_CONFIG_FAWE_BTFLG_UNUSED_BITS | end_addr | (btflg_swap << 15U));

    /* Bits 10:0 are FAWS(Flash Access Window Start Block). */
    /* Unused bits should be written as 1. */
    g_configuration_area_data[FLASH_HP_FCU_CONFIG_SET_FAWS_OFFSET] =
        (uint16_t) (FLASH_HP_FCU_CONFIG_FAWS_UNUSED_BITS | start_addr);

    g_configuration_area_data[4] = UINT16_MAX;
    g_configuration_area_data[5] = UINT16_MAX;
    g_configuration_area_data[6] = UINT16_MAX;
    g_configuration_area_data[7] = UINT16_MAX;
}

#endif

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * Configure an access window for the Code Flash memory using the provided start and end address. An access window
 * defines a contiguous area in Code Flash for which programming/erase is enabled. This area is on block boundaries. The
 * block containing start_addr is the first block. The block containing end_addr is the last block. The access window
 * then becomes first block - last block inclusive. Anything outside this range of Code Flash is then write protected.
 * This command DOES modify the configuration via The Configuration Set command to update the FAWS and FAWE.
 *
 * @param      p_ctrl                Pointer to the control block
 * @param[in]  start_addr            Determines the Starting block for the Code Flash access window.
 * @param[in]  end_addr              Determines the Ending block for the Code Flash access window.
 *
 * @retval     FSP_SUCCESS           Access window successfully configured.
 * @retval     FSP_ERR_CMD_LOCKED    FCU is in locked state, typically as a result of having received an illegal
 *                                   command.
 * @retval     FSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation.
 * @retval     FSP_ERR_TIMEOUT       Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_PE_FAILURE    Failed to enter or exit Code Flash P/E mode.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_access_window_set (flash_hp_instance_ctrl_t * p_ctrl,
                                             uint32_t const             start_addr,
                                             uint32_t const             end_addr)
{
    uint32_t btflg = R_FACI_HP->FAWMON_b.BTFLG;

    /* Update Flash state and enter Code Flash P/E mode */
    fsp_err_t err = flash_hp_enter_pe_cf_mode(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Configure the configuration area to be written. */
    flash_hp_configuration_area_data_setup(btflg, start_addr >> 13U, end_addr >> 13U);

    /* Write the configuration area to the access/startup region. */
    err = flash_hp_configuration_area_write(p_ctrl, FLASH_HP_FCU_CONFIG_SET_ACCESS_STARTUP);

    err = flash_hp_check_errors(err, 0, FSP_ERR_WRITE_FAILED);

    /* Return to read mode*/
    fsp_err_t pe_exit_err = flash_hp_pe_mode_exit();

    if (FSP_SUCCESS == err)
    {
        err = pe_exit_err;
    }

    return err;
}

#endif

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * Modifies the start-up program swap flag (BTFLG) based on the supplied parameters. These changes will take effect on
 * the next reset. This command DOES modify the configuration via The Configuration Set command to update the BTFLG.
 *
 * @param      p_ctrl                Pointer to the instance control block.
 * @param[in]  swap_type             The swap type Alternate or Default.
 * @param[in]  is_temporary          Indicates if the swap should be temporary or permanent.
 *
 * @retval     FSP_SUCCESS           Access window successfully removed.
 * @retval     FSP_ERR_CMD_LOCKED    FCU is in locked state, typically as a result of having received an illegal
 *                                   command.
 * @retval     FSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation.
 * @retval     FSP_ERR_TIMEOUT       Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_PE_FAILURE    Failed to enter or exit Code Flash P/E mode.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_set_startup_area_boot (flash_hp_instance_ctrl_t * p_ctrl,
                                                 flash_startup_area_swap_t  swap_type,
                                                 bool                       is_temporary)
{
    /* Update Flash state and enter Code Flash P/E mode */
    fsp_err_t err = flash_hp_enter_pe_cf_mode(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    if (is_temporary)
    {
        R_FACI_HP->FSUACR = (uint16_t) (FLASH_HP_FSUACR_KEY | swap_type);
    }
    else
    {
 #if BSP_FEATURE_FLASH_SUPPORTS_ACCESS_WINDOW

        /* Do not call functions with multiple volatile parameters. */
        uint32_t faws = R_FACI_HP->FAWMON_b.FAWS;
        uint32_t fawe = R_FACI_HP->FAWMON_b.FAWE;

        /* Configure the configuration area to be written. */
        flash_hp_configuration_area_data_setup(~swap_type & 0x1, faws, fawe);
 #else
        memset(g_configuration_area_data, UINT8_MAX, sizeof(g_configuration_area_data));

        g_configuration_area_data[FLASH_HP_FCU_CONFIG_SET_FAWE_BTFLG_OFFSET] =
            (uint16_t) (((((uint16_t) ~swap_type) & 0x1U) << 15U) | FLASH_HP_OFS_SAS_MASK);
 #endif

        /* Write the configuration area to the access/startup region. */
        err = flash_hp_configuration_area_write(p_ctrl, FLASH_HP_FCU_CONFIG_SET_ACCESS_STARTUP);

        err = flash_hp_check_errors(err, 0, FSP_ERR_WRITE_FAILED);
    }

    /* Return to read mode*/
    fsp_err_t pe_exit_err = flash_hp_pe_mode_exit();

    if (FSP_SUCCESS == err)
    {
        err = pe_exit_err;
    }

    return err;
}

#endif

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * Set the ID code.
 *
 * @param      p_ctrl                Pointer to the control block
 * @param      p_id_code             The identifier code
 * @param[in]  mode                  ID code mode
 *
 * @retval     FSP_SUCCESS           Set Configuration successful
 * @retval     FSP_ERR_PE_FAILURE    Failed to enter or exit Code Flash P/E mode.
 * @retval     FSP_ERR_TIMEOUT       Timed out waiting for the FCU to become ready.
 * @retval     FSP_ERR_WRITE_FAILED  Status is indicating a Programming error for the requested operation.
 * @retval     FSP_ERR_CMD_LOCKED    FCU is in locked state, typically as a result of having received an illegal
 *                                   command.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_set_id_code (flash_hp_instance_ctrl_t * p_ctrl,
                                       uint8_t const * const      p_id_code,
                                       flash_id_code_mode_t       mode)
{
    uint32_t * temp      = (uint32_t *) p_id_code;
    uint32_t * temp_area = (uint32_t *) g_configuration_area_data;

    /* Update Flash state and enter the required P/E mode specified by the hardware manual. */
    fsp_err_t err = flash_hp_enter_pe_cf_mode(p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Configure the configuration area to be written. If the mode is unlocked write all 0xFF. */
    for (uint32_t index = 0U; index < 4U; index++)
    {
        if (FLASH_ID_CODE_MODE_UNLOCKED == mode)
        {
            temp_area[index] = UINT32_MAX;
        }
        else
        {
            temp_area[index] = temp[index];
        }
    }

    /* If the mode is not unlocked set bits 126 and 127 accordingly. */
    if (FLASH_ID_CODE_MODE_UNLOCKED != mode)
    {
        g_configuration_area_data[FLASH_HP_CONFIG_SET_ACCESS_WORD_CNT - 1U] |= (uint16_t) mode;
    }

    /* Write the configuration area to the access/startup region. */
    err = flash_hp_configuration_area_write(p_ctrl, FLASH_HP_FCU_CONFIG_SET_ID_BYTE);

    err = flash_hp_check_errors(err, 0, FSP_ERR_WRITE_FAILED);

    /* Return to read mode*/
    fsp_err_t pe_exit_err = flash_hp_pe_mode_exit();

    if (FSP_SUCCESS == err)
    {
        err = pe_exit_err;
    }

    return err;
}

#endif

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * Execute the Set Configuration sequence using the g_configuration_area_data structure set up the caller.
 *
 * @param      p_ctrl           Pointer to the control block
 * @param[in]  fsaddr           Flash address to be written to.
 *
 * @retval     FSP_SUCCESS      Set Configuration successful
 * @retval     FSP_ERR_TIMEOUT  Timed out waiting for the FCU to become ready.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_configuration_area_write (flash_hp_instance_ctrl_t * p_ctrl, uint32_t fsaddr)
{
    volatile uint32_t timeout = p_ctrl->timeout_write_config;

    /* See "Configuration Set Command": Section 47.9.3.15 of the RA6M4 manual R01UH0890EJ0100. */
    R_FACI_HP->FSADDR        = fsaddr;
    R_FACI_HP_CMD->FACI_CMD8 = FLASH_HP_FACI_CMD_CONFIG_SET_1;
    R_FACI_HP_CMD->FACI_CMD8 = FLASH_HP_FACI_CMD_CONFIG_SET_2;

    /* Write the configuration data. */
    for (uint32_t index = 0U; index < FLASH_HP_CONFIG_SET_ACCESS_WORD_CNT; index++)
    {
        /* There are 8 16 bit words that must be written to accomplish a configuration set */
        R_FACI_HP_CMD->FACI_CMD16 = g_configuration_area_data[index];
    }

    R_FACI_HP_CMD->FACI_CMD8 = FLASH_HP_FACI_CMD_FINAL;

    while (1U != R_FACI_HP->FSTATR_b.FRDY)
    {
        if (timeout <= 0U)
        {
            return FSP_ERR_TIMEOUT;
        }

        timeout--;
    }

    return FSP_SUCCESS;
}

#endif

/** If BGO is being used then we require both interrupts to be enabled (RDY and ERR). If either one
 *  is not enabled then don't generate any ISR routines */

/*******************************************************************************************************************//**
 * FLASH error interrupt routine.
 *
 * This function implements the FLASH error isr. The function clears the interrupt request source on entry populates the
 * callback structure with the FLASH_IRQ_EVENT_ERR_ECC event, and providing a callback routine has been provided, calls
 * the callback function with the event.
 **********************************************************************************************************************/
void fcu_fiferr_isr (void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE
    flash_event_t event;
    IRQn_Type     irq = R_FSP_CurrentIrqGet();

    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) R_FSP_IsrContextGet(irq);

    uint32_t fastat        = R_FACI_HP->FASTAT;
    uint32_t fstatr_errors = R_FACI_HP->FSTATR & FLASH_HP_FSTATR_ERROR_MASK;

    /* The flash access error interrupt register has fired. */
    /* Check for the data flash memory access violation flag. */
    if (fastat & FLASH_HP_FASTAT_DFAE)
    {
        event = FLASH_EVENT_ERR_DF_ACCESS;
    }
    /* Check for the code flash memory access violation flag. */
    else if (fastat & FLASH_HP_FASTAT_CFAE)
    {
        event = FLASH_EVENT_ERR_CF_ACCESS;
    }
    /* Check if the command Lock bit is set. */
    else if (fastat & FLASH_HP_FASTAT_CMDLK)
    {
        if (fstatr_errors & (FLASH_HP_FSTATR_PRGERR | FLASH_HP_FSTATR_ERSERR))
        {
            event = FLASH_EVENT_ERR_FAILURE;
        }
        else
        {
            event = FLASH_EVENT_ERR_CMD_LOCKED;
        }
    }
    else
    {
        event = FLASH_EVENT_ERR_FAILURE;
    }

    /* Reset the FCU: This will stop any existing processes and exit PE mode*/
    flash_hp_reset(p_ctrl);

    /* Clear the Error Interrupt. */
    R_BSP_IrqStatusClear(irq);

    /* Call the user callback. */
    r_flash_hp_call_callback(p_ctrl, event);

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE
}

/*******************************************************************************************************************//**
 * FLASH ready interrupt routine.
 *
 * This function implements the FLASH ready isr. The function clears the interrupt request source on entry populates the
 * callback structure with the relevant event, and providing a callback routine has been provided, calls the callback
 * function with the event.
 **********************************************************************************************************************/
void fcu_frdyi_isr (void)
{
    FSP_CONTEXT_SAVE
    bool operation_completed = false;

    /*Wait counter used for DBFULL flag*/
    flash_event_t event;

    IRQn_Type irq = R_FSP_CurrentIrqGet();

    flash_hp_instance_ctrl_t * p_ctrl = (flash_hp_instance_ctrl_t *) R_FSP_IsrContextGet(irq);

    /* Clear the Interrupt Request*/
    R_BSP_IrqStatusClear(irq);

    /* A reset of the FACI with a BGO operation in progress may still generate a single interrupt
     * subsequent to the reset. We want to ignore that */

    /* Continue the current operation. If unknown operation set callback event to failure. */
    if (FLASH_OPERATION_DF_BGO_WRITE == p_ctrl->current_operation)
    {
        /* If there are still bytes to write */
        if (p_ctrl->operations_remaining)
        {
            fsp_err_t err = flash_hp_write_data(p_ctrl, BSP_FEATURE_FLASH_HP_DF_WRITE_SIZE, 0);

            if (FSP_SUCCESS != err)
            {
                flash_hp_reset(p_ctrl);
                event = FLASH_EVENT_ERR_FAILURE;
            }
        }
        /*Done writing all bytes*/
        else
        {
            event               = FLASH_EVENT_WRITE_COMPLETE;
            operation_completed = true;
        }
    }
    else if ((FLASH_OPERATION_DF_BGO_ERASE == p_ctrl->current_operation))
    {
        if (p_ctrl->operations_remaining)
        {
            flash_hp_erase_block(p_ctrl, BSP_FEATURE_FLASH_HP_DF_BLOCK_SIZE, 0);
        }
        /* If all blocks are erased*/
        else
        {
            event               = FLASH_EVENT_ERASE_COMPLETE;
            operation_completed = true;
        }
    }
    else
    {
        /* Blank check is a single operation */
        operation_completed = true;
        if (R_FACI_HP->FBCSTAT == 0x01U)
        {
            event = FLASH_EVENT_NOT_BLANK;
        }
        else
        {
            event = FLASH_EVENT_BLANK;
        }
    }

    /* If the current operation has completed exit pe mode, release the software lock and call the user callback if used. */
    if (operation_completed == true)
    {
        /* finished current operation. Exit P/E mode*/
        flash_hp_pe_mode_exit();

        /* Release lock and Set current state to Idle*/
        p_ctrl->current_operation = FLASH_OPERATION_NON_BGO;

        /* Set data to identify callback to user, then call user callback. */
        r_flash_hp_call_callback(p_ctrl, event);
    }

    FSP_CONTEXT_RESTORE
}

/*******************************************************************************************************************//**
 * Calls user callback.
 *
 * @param[in]     p_ctrl     Pointer to FLASH_HP instance control block
 * @param[in]     event      Event code
 **********************************************************************************************************************/
static void r_flash_hp_call_callback (flash_hp_instance_ctrl_t * p_ctrl, flash_event_t event)
{
    flash_callback_args_t args;

    /* Store callback arguments in memory provided by user if available.  This allows callback arguments to be
     * stored in non-secure memory so they can be accessed by a non-secure callback function. */
    flash_callback_args_t * p_args = p_ctrl->p_callback_memory;
    if (NULL == p_args)
    {
        /* Store on stack */
        p_args = &args;
    }
    else
    {
        /* Save current arguments on the stack in case this is a nested interrupt. */
        args = *p_args;
    }

    p_args->event     = event;
    p_args->p_context = p_ctrl->p_context;

#if BSP_TZ_SECURE_BUILD

    /* p_callback can point to a secure function or a non-secure function. */
    if (!cmse_is_nsfptr(p_ctrl->p_callback))
    {
        /* If p_callback is secure, then the project does not need to change security state. */
        p_ctrl->p_callback(p_args);
    }
    else
    {
        /* If p_callback is Non-secure, then the project must change to Non-secure state in order to call the callback. */
        flash_hp_prv_ns_callback p_callback = (flash_hp_prv_ns_callback) (p_ctrl->p_callback);
        p_callback(p_args);
    }

#else

    /* If the project is not Trustzone Secure, then it will never need to change security state in order to call the callback. */
    p_ctrl->p_callback(p_args);
#endif
    if (NULL != p_ctrl->p_callback_memory)
    {
        /* Restore callback memory in case this is a nested interrupt. */
        *p_ctrl->p_callback_memory = args;
    }
}

/*******************************************************************************************************************//**
 * This function switches the peripheral to P/E mode for Data Flash.
 * @param[in]  p_ctrl              Pointer to the Flash control block.
 * @retval     FSP_SUCCESS         Successfully entered Data Flash P/E mode.
 * @retval     FSP_ERR_PE_FAILURE  Failed to enter Data Flash P/E mode.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_enter_pe_df_mode (flash_hp_instance_ctrl_t * const p_ctrl)
{
    fsp_err_t err = FSP_SUCCESS;

    /* See "Transition to Data Flash P/E Mode": Section 47.9.3.4 of the RA6M4 manual R01UH0890EJ0100. */
    /* Timeout counter. */
    volatile uint32_t wait_count = FLASH_HP_FRDY_CMD_TIMEOUT;

    /* If BGO mode is enabled and interrupts are being used then enable interrupts. */
    if (p_ctrl->p_cfg->data_flash_bgo == true)
    {
        /* We are supporting Flash Rdy interrupts for Data Flash operations. */
        R_BSP_IrqEnable(p_ctrl->p_cfg->irq);

        /* We are supporting Flash Err interrupts for Data Flash operations. */
        R_BSP_IrqEnable(p_ctrl->p_cfg->err_irq);
    }

    /* Enter Data Flash PE mode. */
    R_FACI_HP->FENTRYR = FLASH_HP_FENTRYR_TRANSITION_TO_DF_PE;

    /* Wait for the operation to complete or timeout. If timeout return error. */
    /* Read FENTRYR until it has been set to 0x0080 indicating that we have successfully entered P/E mode.*/
    while (FLASH_HP_FENTRYR_DF_PE_MODE != R_FACI_HP->FENTRYR)
    {
        /* Wait until FENTRYR is 0x0080 unless timeout occurs. */
        if (wait_count == 0U)
        {

            /* if FENTRYR is not set after max timeout return an error. */
            return FSP_ERR_PE_FAILURE;
        }

        wait_count--;
    }

    return err;
}

#if (FLASH_HP_CFG_CODE_FLASH_PROGRAMMING_ENABLE == 1)

/*******************************************************************************************************************//**
 * This function switches the peripheral to P/E mode for Code Flash.
 * @param[in]  p_ctrl              Pointer to the Flash control block.
 * @retval     FSP_SUCCESS         Successfully entered Code Flash P/E mode.
 * @retval     FSP_ERR_PE_FAILURE  Failed to enter Code Flash P/E mode.
 **********************************************************************************************************************/
static fsp_err_t flash_hp_enter_pe_cf_mode (flash_hp_instance_ctrl_t * const p_ctrl)
{
    fsp_err_t err = FSP_SUCCESS;

    /* See "Transition to Code Flash P/E Mode": Section 47.9.3.3 of the RA6M4 manual R01UH0890EJ0100. */
    /* Timeout counter. */
    volatile uint32_t wait_count = FLASH_HP_FRDY_CMD_TIMEOUT;

    /* While the Flash API is in use we will disable the flash cache. */
 #if BSP_FEATURE_BSP_FLASH_CACHE_DISABLE_OPM
    R_BSP_FlashCacheDisable();
 #elif BSP_FEATURE_BSP_HAS_CODE_SYSTEM_CACHE

    /* Disable the C-Cache. */
    R_CACHE->CCACTL = 0U;
 #endif

    /* If interrupts are being used then disable interrupts. */
    if (p_ctrl->p_cfg->data_flash_bgo == true)
    {
        /* We are not supporting Flash Rdy interrupts for Code Flash operations */
        R_BSP_IrqDisable(p_ctrl->p_cfg->irq);

        /* We are not supporting Flash Err interrupts for Code Flash operations */
        R_BSP_IrqDisable(p_ctrl->p_cfg->err_irq);
    }

 #if BSP_FEATURE_FLASH_HP_HAS_FMEPROT
    R_FACI_HP->FMEPROT = FLASH_HP_FMEPROT_UNLOCK;
 #endif

    /* Enter code flash PE mode */
    R_FACI_HP->FENTRYR = FLASH_HP_FENTRYR_TRANSITION_TO_CF_PE;

    /* Wait for the operation to complete or timeout. If timeout return error. */
    /* Read FENTRYR until it has been set to 0x0001 indicating that we have successfully entered CF P/E mode.*/
    while (FLASH_HP_FENTRYR_CF_PE_MODE != R_FACI_HP->FENTRYR)
    {
        /* Wait until FENTRYR is 0x0001UL unless timeout occurs. */
        if (wait_count == 0U)
        {

            /* if FENTRYR is not set after max timeout, FSP_ERR_PE_FAILURE*/
            return FSP_ERR_PE_FAILURE;
        }

        wait_count--;
    }

    return err;
}

#endif
