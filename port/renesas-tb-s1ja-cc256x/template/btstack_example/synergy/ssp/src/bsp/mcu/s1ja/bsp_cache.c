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
/***********************************************************************************************************************
* File Name    : bsp_cache.c
* Description  : This module provides APIs for turning cache on and off.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "bsp_api.h"

#if defined(BSP_MCU_GROUP_S1JA)

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
#define BSP_CACHE_TIMEOUT (UINT32_MAX)   /* This timeout is not precise and is only to prevent the code from being
                                          * stuck in a "while" loop.
                                          */
/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables (to be accessed by other files)
***********************************************************************************************************************/
 
/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief Attempt to turn cache off.
 *
 * @param[in] p_state Pointer to the on/off state of cache when the function was called.
 *
 * @retval SSP_SUCCESS          Cache was turned off.
 * @retval SSP_ERR_ASSERTION    NULL pointer.
 **********************************************************************************************************************/

ssp_err_t   R_BSP_CacheOff(bsp_cache_state_t * p_state)
{
#if BSP_CFG_PARAM_CHECKING_ENABLE
    /* Check pointer for NULL value. */
    SSP_ASSERT(NULL != p_state);         /** return error if NULL p_state */
#endif

    *p_state = (bsp_cache_state_t)R_ROMC->ROMCE_b.ROMCEN;
    R_ROMC->ROMCE_b.ROMCEN = 0U;         /** Disable ROM cache. */
    return SSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief Attempt to set the cache on or off.
 *
 * @param[in] state on/off state to set cache to.
 *
 * @retval SSP_SUCCESS          Cache was set or restored.
 * @retval SSP_ERR_TIMEOUT      Cache was not restored.
 **********************************************************************************************************************/

ssp_err_t   R_BSP_CacheSet(bsp_cache_state_t state)
{
    ssp_err_t ret_val;
    ret_val = SSP_SUCCESS;
    uint32_t i = BSP_CACHE_TIMEOUT;

    R_ROMC->ROMCIV_b.ROMCIV = 1U;      /** Invalidate cache. */
    while ((R_ROMC->ROMCIV_b.ROMCIV != 0U) && (i != 0x00U))
    {
        /* Wait for bit to clear. Timeout on hardware failure.*/
        i--;
    }
    if (0U == i)
    {
        ret_val = SSP_ERR_TIMEOUT;                          /** Return error on timeout. */
    }
    else
    {
        R_ROMC->ROMCE_b.ROMCEN = (uint16_t)state & 0x01U;   /** Enable ROM cache. */
    }
    return ret_val;
}


#endif



