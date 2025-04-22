/**
 * @file
 * @brief      MAX14690 PMIC driver API.
 */
/* ****************************************************************************
 * Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 * $Date: 2016-10-07 15:24:25 -0500 (Fri, 07 Oct 2016) $
 * $Revision: 24635 $
 *
 *************************************************************************** */

#ifndef _MAX14690_H_
#define _MAX14690_H_

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @ingroup bsp
 * @defgroup max14690_bsp MAX14690 Board Support API.
 * @{
 */
/* **** Definitions **** */

/**
 * Enumeration type that defines the location of the register addresses for the MAX14690.
 */
typedef enum {  
    MAX14690_REG_CHIP_ID,           /**< CHIP_ID Register       */
    MAX14690_REG_CHIP_REV,          /**< CHIP_REV Register      */
    MAX14690_REG_STATUS_A,          /**< STATUS_A Register      */
    MAX14690_REG_STATUS_B,          /**< STATUS_B Register      */
    MAX14690_REG_STATUS_C,          /**< STATUS_C Register      */
    MAX14690_REG_INT_A,             /**< INT_A Register         */
    MAX14690_REG_INT_B,             /**< INT_B Register         */
    MAX14690_REG_INT_MASK_A,        /**< INT_MASK_A Register    */
    MAX14690_REG_INT_MASK_B,        /**< INT_MASK_B Register    */
    MAX14690_REG_ILIM_CNTL,         /**< ILIM_CNTL Register     */
    MAX14690_REG_CHG_CNTL_A,        /**< CHG_CNTL_A Register    */
    MAX14690_REG_CHG_CNTL_B,        /**< CHG_CNTL_B Register    */
    MAX14690_REG_CHG_TMR,           /**< CHG_TMR Register       */
    MAX14690_REG_BUCK1_CFG,         /**< BUCK1_CFG Register     */
    MAX14690_REG_BUCK1_VSET,        /**< BUCK1_VSET Register    */
    MAX14690_REG_BUCK2_CFG,         /**< BUCK2_CFG Register     */
    MAX14690_REG_BUCK2_VSET,        /**< BUCK2_VSET Register    */
    MAX14690_REG_RSVD_11,           /**< RSVD_11 Register       */
    MAX14690_REG_LDO1_CFG,          /**< LDO1_CFG Register      */
    MAX14690_REG_LDO1_VSET,         /**< LDO1_VSET Register     */
    MAX14690_REG_LDO2_CFG,          /**< LDO2_CFG Register      */
    MAX14690_REG_LDO2_VSET,         /**< LDO2_VSET Register     */
    MAX14690_REG_LDO3_CFG,          /**< LDO3_CFG Register      */
    MAX14690_REG_LDO3_VSET,         /**< LDO3_VSET Register     */
    MAX14690_REG_THRM_CFG,          /**< THRM_CFG Register      */
    MAX14690_REG_MON_CFG,           /**< MON_CFG Register       */
    MAX14690_REG_BOOT_CFG,          /**< BOOT_CFG Register      */
    MAX14690_REG_PIN_STAT,          /**< PIN_STAT Register      */
    MAX14690_REG_BUCK_EXTRA,        /**< BUCK_EXTRA Register    */
    MAX14690_REG_PWR_CFG,           /**< PWR_CFG Register       */
    MAX14690_REG_RSVD_1E,           /**< RSVD_1E Register       */
    MAX14690_REG_PWR_OFF,           /**< PWR_OFF Register       */
} max14690_reg_map_t;

/**
 * Enumeration type for setting the LDO mode on the MAX14690. 
 */
typedef enum {
    MAX14690_LDO_DISABLED,         /**< LDO mode, disabled, no active discharge. */
    MAX14690_SW_DISABLED,          /**< Switch mode, disabled, no active discharge. */
    MAX14690_LDO_ENABLED,          /**< LDO mode, enabled, no active discharge. */
    MAX14690_SW_ENABLED,           /**< Switch mode, enabled, no active discharge. */
    MAX14690_LDO_MPC0,             /**< LDO mode, MPC0 enabled, no active discharge. */
    MAX14690_SW_MPC0,              /**< Switch mode, MPC0 enabled, no active discharge. */
    MAX14690_LDO_MPC1,             /**< LDO mode, MPC1 enabled, no active discharge. */
    MAX14690_SW_MPC1,              /**< Switch mode, MPC1 enabled, no active discharge. */
    MAX14690_LDO_DISABLED_ACT_DIS, /**< LDO mode, disabled, active discharge . */
    MAX14690_SW_DISABLED_ACT_DIS,  /**< Switch mode, disabled, active discharge. */
    MAX14690_LDO_ENABLED_ACT_DIS,  /**< LDO mode, enabled, active discharge. */
    MAX14690_SW_ENABLED_ACT_DIS,   /**< Switch mode, enabled, active discharge. */
    MAX14690_LDO_MPC0_ACT_DIS,     /**< LDO mode, MPC0 enabled, active discharge. */
    MAX14690_SW_MPC0_ACT_DIS,      /**< Switch mode, MPC0 enabled, active discharge. */
    MAX14690_LDO_MPC1_ACT_DIS,     /**< LDO mode, MPC1 enabled, active discharge. */
    MAX14690_SW_MPC1_ACT_DIS,      /**< Switch mode, MPC1 enabled, active discharge. */
} max14690_ldo_mode_t;

/**
 * Enumeration type to select the multiplexer channel behavior. 
 */

typedef enum {
    MAX14690_MUX_SEL_PULLDOWN, /**< Mux disabled with pulldown.     */
    MAX14690_MUX_SEL_BAT,      /**< Mux select BAT.                 */
    MAX14690_MUX_SEL_SYS,      /**< Mux select SYS.                 */
    MAX14690_MUX_SEL_BUCK1,    /**< Mux select BUCK1.               */
    MAX14690_MUX_SEL_BUCK2,    /**< Mux select BUCK2.               */
    MAX14690_MUX_SEL_LDO1,     /**< Mux select LDO1.                */
    MAX14690_MUX_SEL_LDO2,     /**< Mux select LDO2.                */
    MAX14690_MUX_SEL_LDO3,     /**< Mux select LDO3.                */
    MAX14690_MUX_SEL_HIZ,      /**< Mux disabled, high impedance.   */
} max14690_mux_ch_t;

/**
 * Enumeration type to set the multiplexer voltage divider. 
 */
typedef enum {
    MAX14690_MUX_DIV_4, /**< Mux divides voltage by 4.              */
    MAX14690_MUX_DIV_3, /**< Mux divides voltage by 3.              */
    MAX14690_MUX_DIV_2, /**< Mux divides voltage by 2.              */
    MAX14690_MUX_DIV_1, /**< Mux divides voltage by 1.              */
} max14690_mux_div_t;

/**
 * Structure type to configure the LDOs on the MAX14690.
 */
typedef struct {
    max14690_ldo_mode_t ldo2mode;  /**< LDO2 configuration mode.    */
    uint32_t ldo2mv;               /**< LDO2 voltage in mV.         */
    max14690_ldo_mode_t ldo3mode;  /**< LDO3 configuration mode.    */
    uint32_t ldo3mv;               /**< LDO3 voltage in mV.         */
} max14690_cfg_t;

/* **** Function Prototypes **** */

/**
 * @brief      Initialize the MAX14690.
 * @param      max14690cfg   Pointer to a structure containing LDO configuration
 * @retval     #E_NO_ERROR   Push buttons intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int MAX14690_Init(const max14690_cfg_t *max14690cfg);

/**
 * @brief      Initialize the MAX14690 interrupt.
 * @retval     #E_NO_ERROR   Push buttons intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int MAX14690_InterruptInit(void);

/**
 * @brief      Set LDO2 mode.
 * @param      mode  sets the operating mode for LDO2.
 * @retval     #E_NO_ERROR   Push buttons intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int MAX14690_LDO2SetMode(max14690_ldo_mode_t mode);

/**
 * @brief   Set LDO2 voltage.
 * @param   millivolts  sets the operating voltage for LDO2 (in mV).
 * @retval     #E_NO_ERROR   Push buttons intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int MAX14690_LDO2SetV(uint32_t millivolts);

/**
 * @brief   Set LDO3 mode.
 * @param   mode  sets the operating mode for LDO3.
 * @retval     #E_NO_ERROR   Push buttons intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int MAX14690_LDO3SetMode(max14690_ldo_mode_t mode);

/**
 * @brief   Set LDO3 voltage.
 * @param   millivolts  sets the operating voltage for LDO3 (in mV).
 * @retval     #E_NO_ERROR   Push buttons intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int MAX14690_LDO3SetV(uint32_t millivolts);

/**
 * @brief   Set Multiplexer.
 * @param   ch  sets the channel for the muliplexer.
 * @param   div  sets the divider value for the multiplexer.
 * @retval     #E_NO_ERROR   Push buttons intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int MAX14690_MuxSet(max14690_mux_ch_t ch, max14690_mux_div_t div);

/**
 * @brief      Enables LDO2.   
 * @deprecated Use MAX14690_LDO2SetMode(max14690_ldo_mode_t mode)
 * @param   enable  1 to enable, 0 to disable LDO2.
 * @retval     #E_NO_ERROR   Push buttons intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int MAX14690_EnableLDO2(uint8_t enable);

/*
 * @brief      Enables LDO3.
 * @deprecated Use MAX14690_LDO3SetMode(max14690_ldo_mode_t mode)
 * @param      enable        1 to enable, 0 to disable LDO3.
 * @retval     #E_NO_ERROR   Push buttons intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int MAX14690_EnableLDO3(uint8_t enable);
/**@}*/
#ifdef __cplusplus
}
#endif

#endif /* _MAX14690_H_ */
