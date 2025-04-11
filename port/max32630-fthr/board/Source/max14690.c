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
 * $Date: 2016-08-15 16:31:22 -0500 (Mon, 15 Aug 2016) $
 * $Revision: 24076 $
 *
 ******************************************************************************/

/***** Includes *****/
#include <stddef.h>
#include "mxc_config.h"
#include "mxc_sys.h"
#include "max14690.h"
#include "board.h"
#include "i2cm.h"
#include "lp.h"

/***** Definitions *****/
#define MAX14690_I2C_ADDR       (0x50 >> 1)

#define MAX14690_USB_OK         0x08

#define MAX14690_LDO_MIN_MV     800
#define MAX14690_LDO_MAX_MV     3600
#define MAX14690_LDO_STEP_MV    100

/***** Function Prototypes *****/
static void VBUS_Interrupt(void *unused);


/******************************************************************************/
int MAX14690_Init(const max14690_cfg_t *max14690cfg)
{
    uint8_t addr;
    uint8_t data[2];

    /* Setup the I2CM Peripheral to talk to the MAX14690 */
    I2CM_Init(MAX14690_I2CM, &max14690_sys_cfg, I2CM_SPEED_100KHZ);

    /* Attempt to read the ID from the device */
    addr = MAX14690_REG_CHIP_ID;
    if (I2CM_Read(MAX14690_I2CM, MAX14690_I2C_ADDR, &addr, 1, data, 2) != 2) {
        return E_COMM_ERR;
    }

    /* Configure the initial state of LDO2 */
    if (MAX14690_LDO2SetV(max14690cfg->ldo2mv) != E_NO_ERROR) {
    	return E_COMM_ERR;
    }
    if (MAX14690_LDO2SetMode(max14690cfg->ldo2mode) != E_NO_ERROR) {
    	return E_COMM_ERR;
    }
    /* Configure the initial state of LDO3 */
    if (MAX14690_LDO3SetV(max14690cfg->ldo3mv) != E_NO_ERROR) {
    	return E_COMM_ERR;
    }    
    if (MAX14690_LDO2SetMode(max14690cfg->ldo2mode) != E_NO_ERROR) {
    	return E_COMM_ERR;
    }
    return E_NO_ERROR;
}

/******************************************************************************/
int MAX14690_InterruptInit(void)
{
    uint8_t data[2];

    /* Configure the initial state of LDO2 */
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
    data[0] = MAX14690_REG_INT_MASK_A; /* IntMaskA */
    data[1] = MAX14690_USB_OK; /* UsbOk */
    if (I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2) != 2) {
        return -1;
    }

    return E_NO_ERROR;
}

/******************************************************************************/
int MAX14690_LDO2SetMode(max14690_ldo_mode_t mode)
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
int MAX14690_LDO2SetV(uint32_t millivolts)
{
    int retval;
    uint8_t data[2] = {MAX14690_REG_LDO2_VSET, 0};

    if ((MAX14690_LDO_MIN_MV <= millivolts)&&(millivolts <= MAX14690_LDO_MAX_MV)){
		data[1] = (millivolts - MAX14690_LDO_MIN_MV) / MAX14690_LDO_STEP_MV;
    } else {
		return E_INVALID;
    }

    retval = I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2);
    if(retval != 2) {
        return retval;
    }

    return E_NO_ERROR;
}

/******************************************************************************/
int MAX14690_LDO3SetMode(max14690_ldo_mode_t mode)
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
int MAX14690_LDO3SetV(uint32_t millivolts)
{
    int retval;
    uint8_t data[2] = {MAX14690_REG_LDO3_VSET, 0};

    if ((MAX14690_LDO_MIN_MV <= millivolts)&&(millivolts <= MAX14690_LDO_MAX_MV)){
		data[1] = (millivolts - MAX14690_LDO_MIN_MV) / MAX14690_LDO_STEP_MV;
    } else {
		return E_INVALID;
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

/******************************************************************************/
static void VBUS_Interrupt(void *unused)
{
    uint8_t addr = MAX14690_REG_STATUS_A;  /* StatusA */
    uint8_t data[5];

    if (I2CM_Read(MAX14690_I2CM, MAX14690_I2C_ADDR, &addr, 1, data, 5) == 5) {
        if (data[1] & MAX14690_USB_OK) {   /* UsbOk */
            /* VBUS is present. Enable LDO2 */
            MAX14690_LDO2SetMode(MAX14690_LDO_ENABLED);
        } else {
            /* VBUS is not present. Disable LDO2 */
            MAX14690_LDO2SetMode(MAX14690_LDO_DISABLED);
        }
    }
}

/******************************************************************************/
int MAX14690_EnableLDO2(uint8_t enable)
{
    int retval;
    uint8_t data[2] = {MAX14690_REG_LDO2_CFG, 0};

    retval = I2CM_Read(MAX14690_I2CM, MAX14690_I2C_ADDR, &data[0], 1, &data[1], 1);
    if(retval != 1) {
        return retval;
    }

    if(enable) {
        data[1] |= MAX14690_LDO_ENABLED;
    } else {
        data[1] &= ~MAX14690_LDO_ENABLED;
    }

    retval = I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2);
    if(retval != 2) {
        return retval;
    }

    return E_NO_ERROR;
}

/******************************************************************************/
int MAX14690_EnableLDO3(uint8_t enable)
{
    int retval;
    uint8_t data[2] = {MAX14690_REG_LDO3_CFG, 0};

    retval = I2CM_Read(MAX14690_I2CM, MAX14690_I2C_ADDR, &data[0], 1, &data[1], 1);
    if(retval != 1) {
        return retval;
    }

    if(enable) {
        data[1] |= MAX14690_LDO_ENABLED;
    } else {
        data[1] &= ~MAX14690_LDO_ENABLED;
    }

    retval = I2CM_Write(MAX14690_I2CM, MAX14690_I2C_ADDR, NULL, 0, data, 2);
    if(retval != 2) {
        return retval;
    }

    return E_NO_ERROR;
}

