/*******************************************************************************
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
 * $Date: 2016-03-11 10:46:37 -0700 (Fri, 11 Mar 2016) $
 * $Revision: 21839 $
 *
 ******************************************************************************/

/**
 * @file    max14690n.h
 * @brief   MAX14690 PMIC driver API.
 */

#ifndef _MAX14690_H_
#define _MAX14690_H_

#ifdef __cplusplus
extern "C" {
#endif


/***** Definitions *****/

typedef enum {  // I2C Register Addresses
    MAX14690_REG_CHIP_ID,
    MAX14690_REG_CHIP_REV,
    MAX14690_REG_STATUS_A,
    MAX14690_REG_STATUS_B,
    MAX14690_REG_STATUS_C,
    MAX14690_REG_INT_A,
    MAX14690_REG_INT_B,
    MAX14690_REG_INT_MASK_A,
    MAX14690_REG_INT_MASK_B,
    MAX14690_REG_ILIM_CNTL,
    MAX14690_REG_CHG_CNTL_A,
    MAX14690_REG_CHG_CNTL_B,
    MAX14690_REG_CHG_TMR,
    MAX14690_REG_BUCK1_CFG,
    MAX14690_REG_BUCK1_VSET,
    MAX14690_REG_BUCK2_CFG,
    MAX14690_REG_BUCK2_VSET,
    MAX14690_REG_RSVD_11,
    MAX14690_REG_LDO1_CFG,
    MAX14690_REG_LDO1_VSET,
    MAX14690_REG_LDO2_CFG,
    MAX14690_REG_LDO2_VSET,
    MAX14690_REG_LDO3_CFG,
    MAX14690_REG_LDO3_VSET,
    MAX14690_REG_THRM_CFG,
    MAX14690_REG_MON_CFG,
    MAX14690_REG_BOOT_CFG,
    MAX14690_REG_PIN_STAT,
    MAX14690_REG_BUCK_EXTRA,
    MAX14690_REG_PWR_CFG,
    MAX14690_REG_RSVD_1E,
    MAX14690_REG_PWR_OFF,
} max14690_reg_map_t;

typedef enum {
	LDO_OUTPUT_DISABLED,
	SW_OUTPUT_DISABLED,
	LDO_OUTPUT_ENABLED,
	SW_OUTPUT_ENABLED,
	LDO_OUTPUT_MPC0,
	SW_OUTPUT_MPC0,
	LDO_OUTPUT_MPC1,
	SW_OUTPUT_MPC1,
	LDO_OUTPUT_DISABLED_ACT_DIS,
	SW_OUTPUT_DISABLED_ACT_DIS,
	LDO_OUTPUT_ENABLED_ACT_DIS,
	SW_OUTPUT_ENABLED_ACT_DIS,
	LDO_OUTPUT_MPC0_ACT_DIS,
	SW_OUTPUT_MPC0_ACT_DIS,
	LDO_OUTPUT_MPC1_ACT_DIS,
	SW_OUTPUT_MPC1_ACT_DIS,
} ldo_enable_mode_t;

typedef enum {
	MAX14690_MUX_SEL_PULLDOWN,
	MAX14690_MUX_SEL_BAT,
	MAX14690_MUX_SEL_SYS,
	MAX14690_MUX_SEL_BUCK1,
	MAX14690_MUX_SEL_BUCK2,
	MAX14690_MUX_SEL_LDO1,
	MAX14690_MUX_SEL_LDO2,
	MAX14690_MUX_SEL_LDO3,
	MAX14690_MUX_SEL_HIZ,
} max14690_mux_ch_t;

typedef enum {
	MAX14690_MUX_DIV_4,
	MAX14690_MUX_DIV_3,
	MAX14690_MUX_DIV_2,
	MAX14690_MUX_DIV_1,
} max14690_mux_div_t;

/***** Function Prototypes *****/

/**
 * @brief   Initialize the MAX14690.
 * @returns #E_NO_ERROR if everything is successful, error if unsuccessful.
 */
int MAX14690N_Init(float ldo2v, ldo_enable_mode_t ldo2en, float ldo3v, ldo_enable_mode_t ldo3en);

/**
 * @brief   Enable or disable LDO2.
 * @param   enable  1 to enable, 0 to disable LDO2.
 * @returns #E_NO_ERROR if everything is successful, error if unsuccessful.
 */
int MAX14690_LDO2setMode(ldo_enable_mode_t mode);
int MAX14690_LDO2setV(float voltage);
int MAX14690_LDO3setMode(ldo_enable_mode_t mode);
int MAX14690_LDO3setV(float voltage);
int MAX14690_MuxSet(max14690_mux_ch_t ch, max14690_mux_div_t div);

#ifdef __cplusplus
}
#endif

#endif /* _MAX14690_H_ */
