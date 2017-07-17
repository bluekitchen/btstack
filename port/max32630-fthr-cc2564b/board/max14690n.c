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

/***** Includes *****/
#include <stddef.h>
#include "mxc_config.h"
#include "mxc_sys.h"
#include "max14690n.h"
#include "board.h"
#include "i2cm.h"
#include "lp.h"

/***** Definitions *****/
#define MAX14690_I2C_ADDR       (0x50 >> 1)
#define MAX14690_ADDR_ID        0x00

#define MAX14690_ADDR_LDO2      0x14
#define MAX14690_LDO_EN         0x02

/***** Function Prototypes *****/
static void VBUS_Interrupt(void *unused);


/******************************************************************************/
int MAX14690N_Init(float ldo2v, ldo_enable_mode_t ldo2en, float ldo3v, ldo_enable_mode_t ldo3en)
{
    uint8_t addr;
    uint8_t data[2];

    /* Setup the I2CM Peripheral to talk to the MAX14690 */
    sys_cfg_i2cm_t cfg;
    cfg.clk_scale = CLKMAN_SCALE_DIV_1;
    cfg.io_cfg = max14690_io_cfg;
    I2CM_Init(MAX14690_I2CM, &cfg, I2CM_SPEED_100KHZ);

    /* Attempt to read the ID from the device */
    addr = MAX14690_ADDR_ID;
    if (I2CM_Read(MAX14690_I2CM, MAX14690_I2C_ADDR, &addr, 1, data, 2) != 2) {
        return E_COMM_ERR;
    }

    /* Configure the initial state of LDO2 */
    if ((0.8 <= ldo2v)&&(ldo2v <= 3.6)){
		data[0] = MAX14690_REG_LDO2_VSET;
		data[1] = (10 * ldo2v) - 8;
		if (I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2) != 2) {
			return -1;
		}
    }
	data[0] = MAX14690_REG_LDO2_CFG;
	data[1] = ldo2en;
	if (I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2) != 2) {
		return -1;
	}

    /* Configure the initial state of LDO3 */
    if ((0.8 <= ldo3v)&&(ldo3v <= 3.6)){
		data[0] = MAX14690_REG_LDO3_VSET;
		data[1] = (10 * ldo3v) - 8;
		if (I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2) != 2) {
			return -1;
		}
    } else {
		return -1;
    }
	data[0] = MAX14690_REG_LDO3_CFG;
	data[1] = ldo3en;
	if (I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2) != 2) {
		return -1;
	}

    VBUS_Interrupt(NULL);

    /* Configure GPIO for interrupt pin from PMIC */
    if (GPIO_Config(&max14690_int) != E_NO_ERROR) {
        return E_UNKNOWN;
    }

    /* Configure and enable interrupt */
    GPIO_RegisterCallback(&max14690_int, VBUS_Interrupt, NULL);
    GPIO_IntConfig(&max14690_int, GPIO_INT_FALLING_EDGE);
    GPIO_IntEnable(&max14690_int);
    NVIC_EnableIRQ(MXC_GPIO_GET_IRQ(max14690_int.port));

    /* Configure interrupt wakeup */
    if (LP_ConfigGPIOWakeUpDetect(&max14690_int, 0, LP_WEAK_PULL_UP) != E_NO_ERROR) {
        return E_UNKNOWN;
    }

    /* Enable the VBUS interrupt */
    data[0] = 0x07; /* IntMaskA */
    data[1] = 0x08; /* UsbOk */
    if (I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2) != 2) {
        return -1;
    }

    return E_NO_ERROR;
}

/******************************************************************************/
static void VBUS_Interrupt(void *unused)
{
    uint8_t addr = 0x02;  /* StatusA */
    uint8_t data[5];

    if (I2CM_Read(MAX14690_I2CM, MAX14690_I2C_ADDR, &addr, 1, data, sizeof(data)) == sizeof(data)) {
        if (data[1] & 0x08) {   /* UsbOk */
            /* VBUS is present. Enable LDO2 */
//            MAX14690_EnableLDO2(1);
        } else {
            /* VBUS is not present. Disable LDO2 */
//            MAX14690_EnableLDO2(0);
        }
    }
}

/******************************************************************************/
int MAX14690_LDO2setMode(ldo_enable_mode_t mode)
{
    int retval;
    uint8_t data[2] = {MAX14690_REG_LDO2_CFG, mode};

    retval = I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2);
    if(retval != 2) {
        return retval;
    }

    return E_NO_ERROR;
}

/******************************************************************************/
int MAX14690_LDO2setV(float voltage)
{
    int retval;
    uint8_t data[2] = {MAX14690_REG_LDO2_VSET, 0};

    if ((0.8 <= voltage)&&(voltage <= 3.6)){
		data[1] = (10 * voltage) - 8;
    } else {
		return -1;
    }

    retval = I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2);
    if(retval != 2) {
        return retval;
    }

    return E_NO_ERROR;
}

/******************************************************************************/
int MAX14690_LDO3setMode(ldo_enable_mode_t mode)
{
    int retval;
    uint8_t data[2] = {MAX14690_REG_LDO3_CFG, mode};

    retval = I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2);
    if(retval != 2) {
        return retval;
    }

    return E_NO_ERROR;
}

/******************************************************************************/
int MAX14690_LDO3setV(float voltage)
{
    int retval;
    uint8_t data[2] = {MAX14690_REG_LDO3_VSET, 0};

    if ((0.8 <= voltage)&&(voltage <= 3.6)){
		data[1] = (10 * voltage) - 8;
    } else {
		return -1;
    }

    retval = I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2);
    if(retval != 2) {
        return retval;
    }

    return E_NO_ERROR;
}

/******************************************************************************/
int MAX14690_MuxSet(max14690_mux_ch_t ch, max14690_mux_div_t div)
{
    int retval;
    uint8_t data[2] = {MAX14690_REG_MON_CFG, 0};

	data[1] = (div << 4) + ch;

    retval = I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2);
    if(retval != 2) {
        return retval;
    }

    return E_NO_ERROR;
}
