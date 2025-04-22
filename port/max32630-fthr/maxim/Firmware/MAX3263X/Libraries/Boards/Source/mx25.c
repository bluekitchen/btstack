/**
 * @file    
 * @brief   BSP Driver for the Micron MX25 Serial Multi-I/O Flash Memory.
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
 **************************************************************************** */

/* **** Includes **** */
#include <string.h>
#include "mxc_config.h"
#include "mx25.h"
#include "spim.h"
#include "spim_regs.h"

 /**
 * @ingroup mx25 
 * @{
 */ 

/* **** Definitions **** */

/* **** Globals **** */
static mxc_spim_regs_t *spim;
static spim_req_t req;

/* **** Static Functions **** */

/* ************************************************************************* */
static int flash_busy()
{
    uint8_t buf;

    MX25_read_SR(&buf);

    if(buf & MX25_WIP_MASK) {
        return 1;
    } else
        return 0;
}

/* ************************************************************************* */
static int write_enable()
{
    uint8_t cmd = MX25_CMD_WRITE_EN;
    uint8_t buf;

    // Send the command
    req.len     = 1;
    req.tx_data  = &cmd;
    req.rx_data  = NULL;
    req.deass   = 1;
    SPIM_Trans(spim, &req);

    MX25_read_SR(&buf);

    if(buf & MX25_WEL_MASK)
        return 0;
    return -1;
}

/* ************************************************************************* */
static void inline read_reg(uint8_t cmd, uint8_t* buf)
{
    // Send the command
    req.len     = 1;
    req.tx_data  = &cmd;
    req.rx_data  = NULL;
    req.deass   = 0;
    SPIM_Trans(spim, &req);

    // Read the data
    req.len     = 1;
    req.tx_data  = NULL;
    req.rx_data  = buf;
    req.deass   = 1;
    SPIM_Trans(spim, &req);
}

/* ************************************************************************* */
static void inline write_reg(uint8_t* buf, unsigned len)
{
    if(write_enable() != 0)
        return;

    // Send the command and data
    req.len     = len;
    req.tx_data  = buf;
    req.rx_data  = NULL;
    req.deass   = 1;
    SPIM_Trans(spim, &req);
}

/* **** Functions **** */

/* ************************************************************************* */
void MX25_init(mxc_spim_regs_t* _spim, uint8_t ssel)
{
    // Save the SPIM that the driver will use
    spim = _spim;

    req.ssel = ssel;
}

/* ************************************************************************* */
void MX25_reset(void)
{
    // Send the Reset command
    uint8_t cmd;
    req.len     = 1;
    req.tx_data  = &cmd;
    req.rx_data  = NULL;
    req.deass   = 1;

    cmd = MX25_CMD_RST_EN;
    SPIM_Trans(spim, &req);
    cmd = MX25_CMD_RST_MEM;
    SPIM_Trans(spim, &req);

    while(flash_busy()) {}
}

/* ************************************************************************* */
uint32_t MX25_ID(void)
{
    uint8_t cmd = MX25_CMD_ID;
    uint8_t id[3];

    // Send the command
    req.len     = 1;
    req.tx_data  = &cmd;
    req.rx_data  = NULL;
    req.deass   = 0;
    SPIM_Trans(spim, &req);

    // Read the data
    req.len     = 3;
    req.tx_data  = NULL;
    req.rx_data  = id;
    req.deass   = 1;
    SPIM_Trans(spim, &req);


    return ((uint32_t)(id[2] | (id[1] << 8) | (id[0] << 16)));
}

/* ************************************************************************* */
int MX25_quad(int enable)
{
    // Enable QSPI mode
    uint8_t pre_buf;
    uint8_t post_buf;

    MX25_read_SR(&pre_buf);

    if(enable) {
        pre_buf |= MX25_QE_MASK;
    } else {
        pre_buf &= ~MX25_QE_MASK;
    }

    if(write_enable() != 0)
        return -1;

    MX25_write_SR(pre_buf);

    while(flash_busy()) {}

    MX25_read_SR(&post_buf);

    if(enable) {
        if(!(post_buf & MX25_QE_MASK)) {
            return -1;
        }
    } else {
        if(post_buf & MX25_QE_MASK) {
            return -1;
        }
    }

    return 0;
}

/* ************************************************************************* */
void MX25_read(uint32_t address, uint8_t *rx_buf, uint32_t rx_len,
               spim_width_t width)
{
    uint8_t cmd[4];

    if(flash_busy())
        return;

    cmd[1] = (address >> 16) & 0xFF;
    cmd[2] = (address >>  8) & 0xFF;
    cmd[3] = address & 0xFF;

    // Send the command
    req.len     = 1;
    req.tx_data  = cmd;
    req.rx_data  = NULL;
    req.deass   = 0;

    // Send the command and dummy bits
    if(width == SPIM_WIDTH_1) {
        cmd[0] = MX25_CMD_READ;
        SPIM_Trans(spim, &req);

        // Send the address
        req.len = 3;
        req.tx_data = &cmd[1];
        req.width = width;
        SPIM_Trans(spim, &req);

        // Send dummy bits
        SPIM_Clocks(spim, MX25_READ_DUMMY, req.ssel, 0);

    } else {
        cmd[0] = MX25_CMD_QREAD;
        SPIM_Trans(spim, &req);

        // Send the address
        req.len = 3;
        req.tx_data = &cmd[1];
        req.width = width;
        SPIM_Trans(spim, &req);

        // Send dummy bits
        SPIM_Clocks(spim, MX25_QREAD_DUMMY, req.ssel, 0);

    }

    // Receive the data
    req.len     = rx_len;
    req.tx_data  = NULL;
    req.rx_data  = rx_buf;
    req.deass   = 1;

    SPIM_Trans(spim, &req);

    // Restore the width
    req.width = SPIM_WIDTH_1;

    while(flash_busy()) {}
}

/* ************************************************************************* */
void MX25_program_page(uint32_t address, const uint8_t *tx_buf, uint32_t tx_len,
                       spim_width_t width)
{

    int tx_cnt = 0;
    uint8_t cmd[4];

    if(flash_busy())
        return;

    while(tx_len > 0) {

        if(write_enable())
            return;

        cmd[1] = (address >> 16) & 0xFF;
        cmd[2] = (address >>  8) & 0xFF;
        cmd[3] = address & 0xFF;

        // Send the command
        req.len     = 1;
        req.tx_data = cmd;
        req.rx_data = NULL;
        req.deass   = 0;

        // Send the command and dummy bits
        if(width == SPIM_WIDTH_1) {
            cmd[0] = MX25_CMD_PPROG;
            SPIM_Trans(spim, &req);

            // Send the address
            req.len = 3;
            req.tx_data = &cmd[1];
            req.width = width;
            SPIM_Trans(spim, &req);

        } else {
            cmd[0] = MX25_CMD_QUAD_PROG;
            SPIM_Trans(spim, &req);

            // Send the address
            req.len = 3;
            req.tx_data = &cmd[1];
            req.width = width;
            SPIM_Trans(spim, &req);
        }

        // Send the data
        if(tx_len >= 256) {
            req.len     = 256;
        } else {
            req.len     = tx_len;
        }
        req.tx_data = &tx_buf[tx_cnt*256];
        req.rx_data = NULL;
        req.deass   = 1;
        SPIM_Trans(spim, &req);

        // Restore the width
        req.width = SPIM_WIDTH_1;

        if(tx_len >= 256) {
            tx_len -= 256;
        } else {
            tx_len = 0;
        }
        address += 256;
        tx_cnt++;

        while(flash_busy());
    }
}

/* ************************************************************************* */
void MX25_bulk_erase(void)
{
    uint8_t cmd;

    if(flash_busy())
        return;

    if(write_enable())
        return;

    cmd = MX25_CMD_BULK_ERASE;

    // Send the command
    req.len     = 1;
    req.tx_data  = &cmd;
    req.rx_data  = NULL;
    req.deass   = 1;
    SPIM_Trans(spim, &req);

    while(flash_busy()) {}
}

/* ************************************************************************* */
void MX25_erase(uint32_t address, mx25_erase_t size)
{
    uint8_t cmd[4];

    if(flash_busy())
        return;

    if(write_enable())
        return;

    switch(size) {
        case MX25_ERASE_4K:
        default:
            cmd[0] = MX25_CMD_4K_ERASE;
            break;
        case MX25_ERASE_32K:
            cmd[0] = MX25_CMD_32K_ERASE;
            break;
        case MX25_ERASE_64K:
            cmd[0] = MX25_CMD_64K_ERASE;
            break;
    }

    cmd[1] = (address >> 16) & 0xFF;
    cmd[2] = (address >>  8) & 0xFF;
    cmd[3] = address & 0xFF;

    // Send the command and the address
    req.len     = 4;
    req.tx_data  = cmd;
    req.rx_data  = NULL;
    req.deass   = 1;
    SPIM_Trans(spim, &req);

    while(flash_busy()) {}
}

/* ************************************************************************* */
void MX25_read_SR(uint8_t* buf)
{
    uint8_t cmd = MX25_CMD_READ_SR;

    read_reg(cmd, buf);
}

/* ************************************************************************* */
void MX25_write_SR(uint8_t value)
{
    uint8_t cmd[2] = {MX25_CMD_WRITE_SR, value};

    write_reg(cmd, 2);
}
/**@} end of ingroup mx25 */
