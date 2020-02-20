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
 * File Name    : hw_dtc_common.h
 * Description  : DTC register access functions.
 **********************************************************************************************************************/

#ifndef HW_DTC_COMMON_H
#define HW_DTC_COMMON_H

/**********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Sets vector table base address
 * @param  p_vectors    Vector table base address.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_DTC_VectorTableAddressSet (R_DTC_Type * p_dtc_regs, void * p_vectors)
{
    p_dtc_regs->DTCVBR = (uint32_t) p_vectors;
}

/*******************************************************************************************************************//**
 * Sets vector table base address
 * @returns Base address of vector table
 **********************************************************************************************************************/
__STATIC_INLINE void * HW_DTC_VectorTableAddressGet (R_DTC_Type * p_dtc_regs)
{
    return (void *) p_dtc_regs->DTCVBR;
}

/*******************************************************************************************************************//**
 * Sets read skip enable bit (allows vector table read step to be skipped if vector is same as last transfer)
 * @param  read_skip    Whether to allow read skip
 **********************************************************************************************************************/
__STATIC_INLINE void HW_DTC_ReadSkipEnableSet (R_DTC_Type * p_dtc_regs, dtc_read_skip_t const read_skip)
{
    p_dtc_regs->DTCCR_b.RRS = read_skip;
}

/*******************************************************************************************************************//**
 * Starts or stops the DTC (globally for all transfers).
 * @param  on_off       Whether to start or stop the DTC
 **********************************************************************************************************************/
__STATIC_INLINE void HW_DTC_StartStop (R_DTC_Type * p_dtc_regs, dtc_startstop_t on_off)
{
    p_dtc_regs->DTCST_b.DTCST = on_off;
}

/*******************************************************************************************************************//**
 * Enables DTC transfer in ICU
 * @param  irq          Interrupt source used to activate transfer
 **********************************************************************************************************************/
__STATIC_INLINE void HW_ICU_DTCEnable (R_ICU_Type * p_icu_regs, IRQn_Type irq)
{
	p_icu_regs->IELSRn_b[irq].IR = 0U;
    p_icu_regs->IELSRn_b[irq].DTCE = 1U;
}

/*******************************************************************************************************************//**
 * Disables DTC transfer in ICU
 * @param  irq          Interrupt source used to deactivate transfer
 **********************************************************************************************************************/
__STATIC_INLINE void HW_ICU_DTCDisable (R_ICU_Type * p_icu_regs, IRQn_Type irq)
{
	p_icu_regs->IELSRn_b[irq].DTCE = 0U;
}

/*******************************************************************************************************************//**
 * Returns DTC status register.
 * @returns DTC status register
 **********************************************************************************************************************/
__STATIC_INLINE uint16_t HW_DTC_StatusGet (R_DTC_Type * p_dtc_regs)
{
    return p_dtc_regs->DTCSTS;
}

__STATIC_INLINE elc_event_t HW_DTC_ICUEventGet (R_ICU_Type * p_icu_regs, IRQn_Type irq)
{
    return (elc_event_t) p_icu_regs->IELSRn_b[irq].IELS;
}


/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* HW_DTC_COMMON_H */
