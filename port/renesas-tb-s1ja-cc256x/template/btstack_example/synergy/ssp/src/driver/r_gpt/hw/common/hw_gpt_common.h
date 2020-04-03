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
 * File Name    : hw_gpt_common.h
 * Description  : Lower level driver (register access) functions for GPT.
 **********************************************************************************************************************/

#ifndef HW_GPT_COMMON_H
#define HW_GPT_COMMON_H

/**********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"

/**********************************************************************************************************************
 * Macro Definitions
 **********************************************************************************************************************/

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
/** Pointer to GPT channels */
#ifdef R_GPTA0_BASE
/*LDRA_INSPECTED 77 S Adding parentheses to this macro changes the meaning. */
#define GPT_BASE_PTR  R_GPTA0_Type *
#elif defined(R_GPTB0_BASE)
/*LDRA_INSPECTED 77 S Adding parentheses to this macro changes the meaning. */
#define GPT_BASE_PTR  R_GPTB0_Type *
#endif

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/
/*******************************************************************************************************************//**
 * Sets the timer operational mode.
 * @param  p_gpt_base  Pointer to base register of GPT channel.
 * @param  mode        Select mode to set.  See ::gpt_mode_t.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_ModeSet (GPT_BASE_PTR p_gpt_base, gpt_mode_t mode)
{
    /* Set mode */
    p_gpt_base->GTCR_b.MD = mode;
}

/*******************************************************************************************************************//**
 * Sets the timer cycle (compare match value).  The timer cycle, along with the PCLK frequency and the GPT PCLK divisor
 * control the timer period.
 * @param  p_gpt_base   Pointer to base register of GPT channel.
 * @param  timer_cycle  Any number from 0 to 0xFFFFFFFF.  The resulting delay period can be calculated according to:
 *                           \f$period=\frac{(timer_cycle+1) \times GPTPCLKDivisor}{PCLKFrequencyHz}\f$
 *                           where GPTPCLKDivisor is any of the following: 1, 4, 16, 64, 256, or 1024
 *                           (see ::HW_GPT_DivisorSet)
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_TimerCycleSet (GPT_BASE_PTR p_gpt_base, uint32_t timer_cycle)
{
    /* Set delay */
    p_gpt_base->GTPR = timer_cycle;
}

/*******************************************************************************************************************//**
 * Returns the timer cycle (compare match value).  The timer cycle, along with the PCLK frequency and the GPT PCLK
 * divisor control the timer period.
 * @param   p_gpt_base  Pointer to base register of GPT channel.
 * @return  The timer cycle value.  See ::HW_GPT_TimerCycleSet for relationship to timer period.
 **********************************************************************************************************************/
__STATIC_INLINE uint32_t HW_GPT_TimerCycleGet (GPT_BASE_PTR p_gpt_base)
{
    return p_gpt_base->GTPR;
}

/*******************************************************************************************************************//**
 * Sets the GPT PCLK divisor.  The GPT PCLK divisor, along with the PCLK frequency and the timer cycle control the
 * timer period.
 * @param   p_gpt_base    Pointer to base register of GPT channel.
 * @param   pclk_divisor  See ::gpt_pclk_div_t for available divisors.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_DivisorSet (GPT_BASE_PTR p_gpt_base, gpt_pclk_div_t pclk_divisor)
{
    p_gpt_base->GTCR_b.TPCS = pclk_divisor;
}

/*******************************************************************************************************************//**
 * Gets the GPT PCLK divisor.  The GPT PCLK divisor, along with the PCLK frequency and the timer cycle control the
 * timer period.
 * @param   p_gpt_base  Pointer to base register of GPT channel.
 * @retval  PCLK divisor value. See ::gpt_pclk_div_t for the detail.
 **********************************************************************************************************************/
__STATIC_INLINE gpt_pclk_div_t HW_GPT_DivisorGet (GPT_BASE_PTR p_gpt_base)
{
    return (gpt_pclk_div_t) p_gpt_base->GTCR_b.TPCS;
}

/*******************************************************************************************************************//**
 * Starts the counter.
 * @param  p_gpt_base  Pointer to base register of GPT channel.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_CounterStartStop (GPT_BASE_PTR p_gpt_base, gpt_start_status_t start_stop)
{
    p_gpt_base->GTCR_b.CST = start_stop;
}

/*******************************************************************************************************************//**
 * Sets the counter value.
 * @param  p_gpt_base   Pointer to base register of GPT channel.
 * @param  value        Any number from 0 to 0xFFFFFFFF.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_CounterSet (GPT_BASE_PTR p_gpt_base, uint32_t value)
{
    /* Set counter value */
    p_gpt_base->GTCNT = value;
}


/*******************************************************************************************************************//**
 * Returns the current counter value.
 * @param   p_gpt_base  Pointer to base register of GPT channel.
 * @return  Current counter value in the range 0 to 0xFFFFFFFF.
 **********************************************************************************************************************/
__STATIC_INLINE uint32_t HW_GPT_CounterGet (GPT_BASE_PTR p_gpt_base)
{
    /* Get counter value */
    return p_gpt_base->GTCNT;
}

/*******************************************************************************************************************//**
 * Sets the direction the counter counts.
 * @param  p_gpt_base   Pointer to base register of GPT channel.
 * @param  dir          See ::gpt_dir_t for available values.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_DirectionSet (GPT_BASE_PTR p_gpt_base, gpt_dir_t dir)
{
    /* Force setting */
    p_gpt_base->GTUDDTYC_b.UDF = 1U;

    /* Start timer */
    p_gpt_base->GTUDDTYC_b.UD = dir;

    /* Clear forcing */
    p_gpt_base->GTUDDTYC_b.UDF = 0U;
}

/*******************************************************************************************************************//**
 * Sets the direction the counter counts.
 * @param  p_gpt_base  Pointer to base register of GPT channel.
 * @retval Count Direction Setting(GPT_DIR_COUNT_DOWN or GPT_DIR_COUNT_UP)
 **********************************************************************************************************************/
__STATIC_INLINE timer_direction_t HW_GPT_DirectionGet (GPT_BASE_PTR p_gpt_base)
{
    /* Read cound direction setting */
    return (timer_direction_t) p_gpt_base->GTUDDTYC_b.UD;
}

/*******************************************************************************************************************//**
 * Clears interrupts for specified channel.
 * @param  p_gpt_base  Pointer to base register of GPT channel.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_InterruptClear (GPT_BASE_PTR p_gpt_base)
{
    p_gpt_base->GTST_b.TCPFO = 0U;
}

/*******************************************************************************************************************//**
 * Returns the value of the start bit, indicating if the counter is counting or not.
 * @param   p_gpt_base  Pointer to base register of GPT channel.
 * @return  Start bit value
 **********************************************************************************************************************/
__STATIC_INLINE gpt_start_status_t HW_GPT_CounterStartBitGet (GPT_BASE_PTR p_gpt_base)
{
    return (gpt_start_status_t) p_gpt_base->GTCR_b.CST;
}

/*******************************************************************************************************************//**
 * Lookup base address of input/output control register for specified channel and pin.
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   gtioc        Specify which output pin to use.
 * @param[out] pp_reg    Pointer to pointer to GTIORH or GTIORL.
 **********************************************************************************************************************/
__STATIC_INLINE gtior_t * HW_GPT_GTIOCRegLookup (GPT_BASE_PTR p_gpt_base, gpt_gtioc_t gtioc)
{
    if (GPT_GTIOCA == gtioc)
    {
        return ((gtior_t *) &p_gpt_base->GTIOR) + 1;
    }
    else
    {
        return ((gtior_t *) &p_gpt_base->GTIOR);
    }
}

/*******************************************************************************************************************//**
 * Set level to output when counter is stopped.
 * @param   p_reg        Pointer to input/output control register
 * @param   level        Level to output when counter is stopped.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_GTIOCPinLevelStoppedSet (gtior_t * const p_reg, gpt_pin_level_t const level)
{
    p_reg->GTIOR_b.OADFLT = level;
}

/*******************************************************************************************************************//**
 * Enable output to pin.
 * @param   p_reg        Pointer to input/output control register
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_GTIOCPinOutputEnable (gtior_t * const p_reg, bool enable)
{
    p_reg->GTIOR_b.OAE = (uint16_t) (enable & 1U);
}

/*******************************************************************************************************************//**
 * Disable output to pin.
 * @param   p_reg        Pointer to input/output control register
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_GTIOCPinOutputDisable (gtior_t * const p_reg)
{
    p_reg->GTIOR_b.OAE = 0U;
}

/*******************************************************************************************************************//**
 * Set how to change output at cycle end.
 * @param   p_reg        Pointer to input/output control register
 * @param   mode         Level to output at cycle end
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_GTIOCCycleEndOutputSet (gtior_t * const p_reg, gpt_output_t const output)
{
    p_reg->GTIOR_b.GTIOCE = output;
}

/*******************************************************************************************************************//**
 * Set compare match mode
 * @param   p_reg        Pointer to input/output control register
 * @param   value        Compare match value to set
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_DutyCycleModeSet (GPT_BASE_PTR p_gpt_base, gpt_gtioc_t gtio, gpt_duty_cycle_mode_t mode)
{
    if (GPT_GTIOCA == gtio)
    {
        p_gpt_base->GTUDDTYC_b.OADTY = mode;
    }
    else if (GPT_GTIOCB == gtio)
    {
        p_gpt_base->GTUDDTYC_b.OBDTY = mode;
    }
}

/*******************************************************************************************************************//**
 * Set compare match value
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   value        Compare match value to set
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_InitialCompareMatchSet (GPT_BASE_PTR p_gpt_base, gpt_gtioc_t gtio, uint32_t const value)
{
    if (GPT_GTIOCA == gtio)
    {
        /* GTCCRA buffer register. */
        p_gpt_base->GTCCRA = value;
    }
    else
    {
        /* GTCCRB buffer register. */
        p_gpt_base->GTCCRB = value;
    }
}

/*******************************************************************************************************************//**
 * Enable buffer.
 * @param   p_reg        Pointer to input/output control register
 * @param   value        Compare match value to set
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_SingleBufferEnable (GPT_BASE_PTR p_gpt_base, gpt_gtioc_t gtio)
{
    if (GPT_GTIOCA == gtio)
    {
        /* GTCCRA buffer enable. */
        p_gpt_base->GTBER_b.CCRA = 1U;
    }
    else
    {
        /* GTCCRB buffer enable. */
        p_gpt_base->GTBER_b.CCRB = 1U;
    }
}

/*******************************************************************************************************************//**
 * Set compare match value
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   value        Compare match value to set
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_CompareMatchSet (GPT_BASE_PTR p_gpt_base, gpt_gtioc_t gtio, uint32_t const value)
{
    if (GPT_GTIOCA == gtio)
    {
        /* GTCCRA buffer register. */
        p_gpt_base->GTCCRC = value;
    }
    else
    {
        /* GTCCRB buffer register. */
        p_gpt_base->GTCCRE = value;
    }
}

/*******************************************************************************************************************//**
 * Set compare match value
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   gtio         GPT output pin used.
 * @return  Pointer to duty cycle register.
 **********************************************************************************************************************/
__STATIC_INLINE volatile void * HW_GPT_DutyCycleAddrGet(GPT_BASE_PTR p_gpt_base, gpt_gtioc_t gtio)
{
    volatile uint32_t * p_reg;
    if (GPT_GTIOCA == gtio)
    {
        /* GTCCRA buffer register. */
        p_reg = &p_gpt_base->GTCCRC;
    }
    else
    {
        /* GTCCRB buffer register. */
        p_reg = &p_gpt_base->GTCCRE;
    }
    return p_reg;
}

/*******************************************************************************************************************//**
 * Set how to change output at compare match.
 * @param   p_reg        Pointer to input/output control register
 * @param   output       Level to output at compare match
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_GTIOCCompareMatchOutputSet (gtior_t * const p_reg, gpt_output_t const output)
{
    p_reg->GTIOR_b.GTIOCM = output;
}

/*******************************************************************************************************************//**
 * Set initial output value
 * @param   p_reg        Pointer to input/output control register
 * @param   output       Level to output at compare match
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_GTIOCInitialOutputSet (gtior_t * const p_reg, gpt_pin_level_t const output)
{
    p_reg->GTIOR_b.GTIOCI = output;
}

/*******************************************************************************************************************//**
 * Select Additional Stop Source
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   source       Signal which triggers the action.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_StopSourceSelectAdd (GPT_BASE_PTR p_gpt_base, gpt_trigger_t const source)
{
    p_gpt_base->GTPSR |= (uint32_t) source;
}
/*******************************************************************************************************************//**
 * Select Start Source
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   source       Signal which triggers the action.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_StartSourceSelect(GPT_BASE_PTR p_gpt_base, gpt_trigger_t const source)
{
    p_gpt_base->GTSSR = source;
}

/*******************************************************************************************************************//**
 * Select Stop Source
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   source       Signal which triggers the action.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_StopSourceSelect(GPT_BASE_PTR p_gpt_base, gpt_trigger_t const source)
{
    p_gpt_base->GTPSR = source;
}

/*******************************************************************************************************************//**
 * Select Clear Source
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   source       Signal which triggers the action.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_ClearSourceSelect(GPT_BASE_PTR p_gpt_base, gpt_trigger_t const source)
{
    p_gpt_base->GTCSR = source;
}

/*******************************************************************************************************************//**
 * Select Capture-to-RegA Source
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   source       Signal which triggers the action.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_CaptureASourceSelect(GPT_BASE_PTR p_gpt_base, gpt_trigger_t const source)
{
    p_gpt_base->GTICASR = source;
}

/*******************************************************************************************************************//**
 * Select Capture-to-RegB Source
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   source       Signal which triggers the action.
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_CaptureBSourceSelect(GPT_BASE_PTR p_gpt_base, gpt_trigger_t const source)
{
    p_gpt_base->GTICBSR = source;
}

/*******************************************************************************************************************//**
 * Return captured value from register GTCCRA
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 **********************************************************************************************************************/
__STATIC_INLINE uint32_t HW_GPT_RegisterAGet(GPT_BASE_PTR p_gpt_base)
{
    return p_gpt_base->GTCCRA;
}

/*******************************************************************************************************************//**
 * Initialize channel specific registers to default values.
 * @param   p_gpt_base   Pointer to base register of GPT channel.
 * @param   variant      0 for base, 1 for matsu
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_RegisterInit(GPT_BASE_PTR p_gpt_base, uint16_t variant)
{
    /* Skip these since they affect all channels, and are initialized in other registers: GTSTR, GTSTP, GTCLR. */
    p_gpt_base->GTWP = 0xA500UL;
    p_gpt_base->GTSSR = 0U;
    p_gpt_base->GTPSR = 0U;
    p_gpt_base->GTPSR = 0U;
    p_gpt_base->GTUPSR = 0U;
    p_gpt_base->GTDNSR = 0U;
    p_gpt_base->GTICASR = 0U;
    p_gpt_base->GTICBSR = 0U;
    p_gpt_base->GTCR = 0U;
    p_gpt_base->GTUDDTYC = 1U;
    p_gpt_base->GTIOR = 0U;
    p_gpt_base->GTINTAD = 0U;
    p_gpt_base->GTST = 0U;
    p_gpt_base->GTBER = 0U;
    p_gpt_base->GTCNT = 0U;
    p_gpt_base->GTCCRA = 0xFFFFFFFFULL;
    p_gpt_base->GTCCRB = 0xFFFFFFFFULL;
    p_gpt_base->GTCCRC = 0xFFFFFFFFULL;
    p_gpt_base->GTCCRE = 0xFFFFFFFFULL;
    p_gpt_base->GTPR = 0xFFFFFFFFULL;
    p_gpt_base->GTPBR = 0xFFFFFFFFULL;
    p_gpt_base->GTDTCR = 0U;
    p_gpt_base->GTDVU = 0xFFFFFFFFULL;
#ifdef R_GPTA0_BASE
    if (variant > 0U)
    {
        /* These registers available on GPTA only. */
        p_gpt_base->GTITC = 0U;
        p_gpt_base->GTPDBR = 0xFFFFFFFFULL;
        p_gpt_base->GTADTRA = 0xFFFFFFFFULL;
        p_gpt_base->GTADTRB = 0xFFFFFFFFULL;
        p_gpt_base->GTADTBRA = 0xFFFFFFFFULL;
        p_gpt_base->GTADTBRB = 0xFFFFFFFFULL;
        p_gpt_base->GTADTDBRA = 0xFFFFFFFFULL;
        p_gpt_base->GTADTDBRB = 0xFFFFFFFFULL;
        p_gpt_base->GTDVD = 0xFFFFFFFFULL;
        p_gpt_base->GTDBU = 0xFFFFFFFFULL;
        p_gpt_base->GTDBD = 0xFFFFFFFFULL;
        /* GTSOS skipped - read only */
        p_gpt_base->GTSOTR = 0U;
    }
#else
    SSP_PARAMETER_NOT_USED(variant);
#endif
}

/*******************************************************************************************************************//**
 * Return the event associated with the GPT counter overflow
 * @param   channel           The channel corresponds to the hardware channel number.
 **********************************************************************************************************************/
__STATIC_INLINE elc_event_t HW_GPT_GetCounterOverFlowEvent(uint8_t const channel)
{
    /* For Synergy MCUs, same event of different channels are maintained at fixed offsets from each other.
     * S7 and S3 has the same offsets (10), while S1 has a different offset (6).
     */
    return (elc_event_t) ((uint32_t) ELC_EVENT_GPT0_COUNTER_OVERFLOW +
           (channel * ((uint32_t) ELC_EVENT_GPT1_COUNTER_OVERFLOW - (uint32_t) ELC_EVENT_GPT0_COUNTER_OVERFLOW)));
}

/*******************************************************************************************************************//**
 * Clears the counter counts.
 * @param  p_gpt_base   Pointer to base register of GPT channel.
 * @param  channel      Channel number from 0 to 6
 **********************************************************************************************************************/
__STATIC_INLINE void HW_GPT_ClearCounter (GPT_BASE_PTR p_gpt_base, uint8_t channel)
{
    /* Setting CCLR bit in GTCSR enables GTCLR register to reset GTCNT register */
    p_gpt_base->GTCSR_b.CCLR = 1U;
    /* Clears the GTCNT counter operation for the particular channel */
    p_gpt_base->GTCLR = (1UL << channel);
    /* clear CCLR bit */
    p_gpt_base->GTCSR_b.CCLR = 0U;
}

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* HW_GPT_COMMON_H */
