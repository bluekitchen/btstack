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
 * $Date: 2015-11-03 (Fri, 03 Nov 2015) $
 * $Revision: $
 *
 ******************************************************************************/

/*
 * This file is meant to be used in conjunction with the 1-Wire Public Domain Kit:
 * https://www.maximintegrated.com/en/products/digital/one-wire/software-tools/public-domain-kit.html
 *
 * This file and the accompanying owm_lnk.c file implement the general purpose
 * templates as described in the 1-Wire PDK documentation.
 */

#include "ownet.h"
#include "mxc_config.h"
#include "owm_regs.h"
#include "mxc_sys.h"
#include "mxc_assert.h"

// local function prototypes
SMALLINT owAcquire(int,char *);
void     owRelease(int);

//---------------------------------------------------------------------------
// Attempt to acquire a 1-Wire net
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
// 'port_zstr'  - zero terminated port name.
//
// Returns: TRUE - success, port opened
//
SMALLINT owAcquire(int portnum, char *port_zstr)
{
    uint32_t freq;

    //only request for the IO pin
    static const ioman_cfg_t cfg = IOMAN_OWM(1, 0);

    // There is only one OWM module
    MXC_ASSERT(portnum == 0);

    // Turn on the clock if its not already on
    freq = SYS_GetFreq(MXC_CLKMAN->sys_clk_ctrl_15_owm);
    if(freq == 0) {
        MXC_CLKMAN->sys_clk_ctrl_15_owm = MXC_V_CLKMAN_CLK_SCALE_DIV_1;
		freq = SystemCoreClock;
    }

    // Set system level configurations
    if (IOMAN_Config(&cfg) != E_NO_ERROR) {
        return FALSE;
    }

    // Set divisor to generate 1MHz clock
    freq /= 1000000;
    MXC_OWM->clk_div_1us = (freq << MXC_F_OWM_CLK_DIV_1US_DIVISOR_POS) & MXC_F_OWM_CLK_DIV_1US_DIVISOR;

    //enabled internal pullup on IO pin
	MXC_OWM->cfg |= MXC_F_OWM_CFG_INT_PULLUP_ENABLE;

    // Clear all interrupt flags
    MXC_OWM->intfl = (MXC_F_OWM_INTFL_OW_RESET_DONE |
                      MXC_F_OWM_INTFL_TX_DATA_EMPTY |
                      MXC_F_OWM_INTFL_RX_DATA_READY);

    return TRUE;
}

//---------------------------------------------------------------------------
// Release the previously acquired a 1-Wire net.
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
//
void owRelease(int portnum)
{
    // There is only one OWM module
    MXC_ASSERT(portnum == 0);

    // Release GPIOs
    static const ioman_cfg_t cfg = IOMAN_OWM(0, 0);
    IOMAN_Config(&cfg);
}
