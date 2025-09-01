/**
 * @file    
 * @brief   Source for NHD12832 driver
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



/***** Includes *****/
#include <stdio.h>
#include <string.h>
#include "mxc_config.h"
#include "mxc_sys.h"
#include "board.h"
#include "gpio.h"
#include "spim.h"
#include "nhd12832.h"
/**
 * @ingroup nhd12832_bsp
 * @{
 */

/* **** Definitions **** */
#define DEFAULT_BRIGHTNESS              0xBF /**< Default brightness setting. */

// NHD12832 Command Address
#define NHD12832_SET_ADDRESSING_MODE        0x20 /**< Set addressing mode.    */
#define NHD12832_SET_COLUMN_ADDR            0x21 /**< Set the column address.     */
#define NHD12832_SET_PAGE_ADDR              0x22 /**< Set the page address.   */
#define NHD12832_SET_DISPLAY_START_LINE     0x40 /**< Set the display start line number.  */
#define NHD12832_SET_CONTRAST               0x81 /**< Set contrast control for bank 0.    */
#define NHD12832_SET_BRIGHTNESS             0x82 /**< Set brightness for area color banks.    */
#define NHD12832_SET_BANKS_PULSE_WIDTH      0x91 /**< Define look up table of area color A-D pulse width.     */
#define NHD12832_SET_AREA_COLOR_PAGE0       0x92 /**< define area color for bank 1-16 (page 0).   */
#define NHD12832_SET_AREA_COLOR_PAGE1       0x93 /**< define area color for bank 17-32 (page 1).  */
#define NHD12832_SET_SEG_REMAP              0xA0 /**< Set segment re-map.     */
#define NHD12832_SET_ENTIRE_DISPLAY         0xA5 /**< Turn on/off entire display.     */
#define NHD12832_SET_INVERSE_DISPLAY        0xA7 /**< Turn on/off inverse display.    */
#define NHD12832_SET_MULTI_RATIO            0xA8 /**< Set multiple ratio.     */
#define NHD12832_SET_DIM_MODE_CFG           0xAB /**< Set dim mode configuration.     */
#define NHD12832_SET_DIM_MODE               0xAC /**< Set display on in dim mode.     */
#define NHD12832_SET_INTERNAL_DC            0xAD /**< Set master config internal DC.  */
#define NHD12832_SET_DISPLAY_OFF            0xAF /**< Set entire display on/off.  */
#define NHD12832_SET_START_PAGE             0xB0 /**< Set page start address for page addressing mode.    */
#define NHD12832_SET_COM_REMAP              0xC0 /**< Set COM output scan direction.  */
#define NHD12832_SET_DISPLAY_OFFSET         0xD3 /**< Set display offset.     */
#define NHD12832_SET_CLK_DIV_FREQ           0xD5 /**< Set display clock divide ratio/ oscillator frequency.   */
#define NHD12832_SET_AREA_COLOR_MODE        0xD8 /**< Turn on/off area color mode and low power display mode.     */
#define NHD12832_SET_VCOMH                  0xD8 /**< Set VCOMH deselect level.   */
#define NHD12832_SET_PRE_CHARGE_PERIOD      0xD9 /**< Set pre-charge period.  */
#define NHD12832_SET_COM_CFG                0xDA /**< Set COM poins hardware configuration.   */
#define NHD12832_SET_READ_MOD_WRITE_EN      0xE0 /**< Set read-modify-write mode.     */
#define NHD12832_NOP                        0xE3 /**< Command for no-operation.   */

#define NUM_COM_PINS                    32  /**< NUMBER OF COM PINS */
#define NUM_PAGES                       (NUM_COM_PINS/8)  /**< NUMBER OF PAGES. */
#define MIN_PAGE                        0                   /**< Minimum value for a page number. */
#define MAX_PAGE                        NUM_PAGES - 1       /**< Maximum value for a page number. */

static const uint8_t stdfont5x7[] = {
    0x0, 0x0, // size of zero indicates fixed width font, actual length is width * height
    0x05, // width
    0x07, // height
    0x20, // first char
    0x60, // char count

    // Fixed width; char width table not used !!!!

    // font data starts here; offset = 6
    0x00, 0x00, 0x00, 0x00, 0x00,// (space)
    0x00, 0x00, 0x5F, 0x00, 0x00,// !
    0x00, 0x07, 0x00, 0x07, 0x00,// "
    0x14, 0x7F, 0x14, 0x7F, 0x14,// #
    0x24, 0x2A, 0x7F, 0x2A, 0x12,// $
    0x23, 0x13, 0x08, 0x64, 0x62,// %
    0x36, 0x49, 0x55, 0x22, 0x50,// &
    0x00, 0x05, 0x03, 0x00, 0x00,// '
    0x00, 0x1C, 0x22, 0x41, 0x00,// (
    0x00, 0x41, 0x22, 0x1C, 0x00,// )
    0x08, 0x2A, 0x1C, 0x2A, 0x08,// *
    0x08, 0x08, 0x3E, 0x08, 0x08,// +
    0x00, 0x50, 0x30, 0x00, 0x00,// ,
    0x08, 0x08, 0x08, 0x08, 0x08,// -
    0x00, 0x60, 0x60, 0x00, 0x00,// .
    0x20, 0x10, 0x08, 0x04, 0x02,// /
    0x3E, 0x51, 0x49, 0x45, 0x3E,// 0
    0x00, 0x42, 0x7F, 0x40, 0x00,// 1
    0x42, 0x61, 0x51, 0x49, 0x46,// 2
    0x21, 0x41, 0x45, 0x4B, 0x31,// 3
    0x18, 0x14, 0x12, 0x7F, 0x10,// 4
    0x27, 0x45, 0x45, 0x45, 0x39,// 5
    0x3C, 0x4A, 0x49, 0x49, 0x30,// 6
    0x01, 0x71, 0x09, 0x05, 0x03,// 7
    0x36, 0x49, 0x49, 0x49, 0x36,// 8
    0x06, 0x49, 0x49, 0x29, 0x1E,// 9
    0x00, 0x36, 0x36, 0x00, 0x00,// :
    0x00, 0x56, 0x36, 0x00, 0x00,// ;
    0x00, 0x08, 0x14, 0x22, 0x41,// <
    0x14, 0x14, 0x14, 0x14, 0x14,// =
    0x41, 0x22, 0x14, 0x08, 0x00,// >
    0x02, 0x01, 0x51, 0x09, 0x06,// ?
    0x32, 0x49, 0x79, 0x41, 0x3E,// @
    0x7E, 0x11, 0x11, 0x11, 0x7E,// A
    0x7F, 0x49, 0x49, 0x49, 0x36,// B
    0x3E, 0x41, 0x41, 0x41, 0x22,// C
    0x7F, 0x41, 0x41, 0x22, 0x1C,// D
    0x7F, 0x49, 0x49, 0x49, 0x41,// E
    0x7F, 0x09, 0x09, 0x01, 0x01,// F
    0x3E, 0x41, 0x41, 0x51, 0x32,// G
    0x7F, 0x08, 0x08, 0x08, 0x7F,// H
    0x00, 0x41, 0x7F, 0x41, 0x00,// I
    0x20, 0x40, 0x41, 0x3F, 0x01,// J
    0x7F, 0x08, 0x14, 0x22, 0x41,// K
    0x7F, 0x40, 0x40, 0x40, 0x40,// L
    0x7F, 0x02, 0x04, 0x02, 0x7F,// M
    0x7F, 0x04, 0x08, 0x10, 0x7F,// N
    0x3E, 0x41, 0x41, 0x41, 0x3E,// O
    0x7F, 0x09, 0x09, 0x09, 0x06,// P
    0x3E, 0x41, 0x51, 0x21, 0x5E,// Q
    0x7F, 0x09, 0x19, 0x29, 0x46,// R
    0x46, 0x49, 0x49, 0x49, 0x31,// S
    0x01, 0x01, 0x7F, 0x01, 0x01,// T
    0x3F, 0x40, 0x40, 0x40, 0x3F,// U
    0x1F, 0x20, 0x40, 0x20, 0x1F,// V
    0x7F, 0x20, 0x18, 0x20, 0x7F,// W
    0x63, 0x14, 0x08, 0x14, 0x63,// X
    0x03, 0x04, 0x78, 0x04, 0x03,// Y
    0x61, 0x51, 0x49, 0x45, 0x43,// Z
    0x00, 0x00, 0x7F, 0x41, 0x41,// [
    0x02, 0x04, 0x08, 0x10, 0x20,// "\"
    0x41, 0x41, 0x7F, 0x00, 0x00,// ]
    0x04, 0x02, 0x01, 0x02, 0x04,// ^
    0x40, 0x40, 0x40, 0x40, 0x40,// _
    0x00, 0x01, 0x02, 0x04, 0x00,// `
    0x20, 0x54, 0x54, 0x54, 0x78,// a
    0x7F, 0x48, 0x44, 0x44, 0x38,// b
    0x38, 0x44, 0x44, 0x44, 0x20,// c
    0x38, 0x44, 0x44, 0x48, 0x7F,// d
    0x38, 0x54, 0x54, 0x54, 0x18,// e
    0x08, 0x7E, 0x09, 0x01, 0x02,// f
    0x08, 0x14, 0x54, 0x54, 0x3C,// g
    0x7F, 0x08, 0x04, 0x04, 0x78,// h
    0x00, 0x44, 0x7D, 0x40, 0x00,// i
    0x20, 0x40, 0x44, 0x3D, 0x00,// j
    0x00, 0x7F, 0x10, 0x28, 0x44,// k
    0x00, 0x41, 0x7F, 0x40, 0x00,// l
    0x7C, 0x04, 0x18, 0x04, 0x78,// m
    0x7C, 0x08, 0x04, 0x04, 0x78,// n
    0x38, 0x44, 0x44, 0x44, 0x38,// o
    0x7C, 0x14, 0x14, 0x14, 0x08,// p
    0x08, 0x14, 0x14, 0x18, 0x7C,// q
    0x7C, 0x08, 0x04, 0x04, 0x08,// r
    0x48, 0x54, 0x54, 0x54, 0x20,// s
    0x04, 0x3F, 0x44, 0x40, 0x20,// t
    0x3C, 0x40, 0x40, 0x20, 0x7C,// u
    0x1C, 0x20, 0x40, 0x20, 0x1C,// v
    0x3C, 0x40, 0x30, 0x40, 0x3C,// w
    0x44, 0x28, 0x10, 0x28, 0x44,// x
    0x0C, 0x50, 0x50, 0x50, 0x3C,// y
    0x44, 0x64, 0x54, 0x4C, 0x44,// z
    0x00, 0x08, 0x36, 0x41, 0x00,// {
    0x00, 0x00, 0x7F, 0x00, 0x00,// |
    0x00, 0x41, 0x36, 0x08, 0x00,// }
    0x08, 0x08, 0x2A, 0x1C, 0x08,// ->
    0x08, 0x1C, 0x2A, 0x08, 0x08, // <-
};

/***** Globals *****/
static uint32_t nhd12832_img_buf[NHD12832_WIDTH *(NUM_PAGES/4)];

/***** Functions *****/

/* ************************************************************************* */
static int NHD12832_SendCmd(const uint8_t *cmd, uint32_t size)
{
    GPIO_OutClr(&nhd12832_dc);

    spim_req_t req;
    req.ssel = NHD12832_SSEL;
    req.deass = 1;
    req.tx_data = cmd;
    req.rx_data = NULL;
    req.width = SPIM_WIDTH_1;
    req.len = size;

    if (SPIM_Trans(NHD12832_SPI, &req) != size) {
        return E_COMM_ERR;
    }

    // Wait for transaction to complete
    while(SPIM_Busy(NHD12832_SPI) != E_NO_ERROR) {}

    return E_NO_ERROR;
}

/* ************************************************************************* */
static int NHD12832_SendData(const uint8_t *data, uint32_t size)
{
    GPIO_OutSet(&nhd12832_dc);

    spim_req_t req;
    req.ssel = NHD12832_SSEL;
    req.deass = 1;
    req.tx_data = data;
    req.rx_data = NULL;
    req.width = SPIM_WIDTH_1;
    req.len = size;

    if (SPIM_Trans(NHD12832_SPI, &req) != size) {
        return E_COMM_ERR;
    }

    // Wait for transaction to complete
    while(SPIM_Busy(NHD12832_SPI) != E_NO_ERROR) {}

    return E_NO_ERROR;
}

    
int NHD12832_LoadImage(nhd12832_bitmap_t *image)
{
    uint32_t size = image->width * NUM_PAGES;

    if(size > (NHD12832_WIDTH * NUM_PAGES))
        return -1;
    memcpy(nhd12832_img_buf, image->bmp, size);
    return 0;
}

/* ************************************************************************* */
void NHD12832_PrintScreen(void)
{
    NHD12832_SetAddressingMode(1);
    NHD12832_SetPageAddr(MIN_PAGE, MAX_PAGE);
    NHD12832_SetColumnAddr(0, (NHD12832_WIDTH - 1));
    NHD12832_SendData((uint8_t *)nhd12832_img_buf, (NHD12832_WIDTH * NUM_PAGES));
}

/* ************************************************************************* */
void NHD12832_TurnOnDisplay(uint8_t en)
{
    uint8_t cmd = NHD12832_SET_DISPLAY_OFF;

    if (en) {
        cmd |= en;
    }
    NHD12832_SendCmd(&cmd, 1);
}

/* ************************************************************************* */
void NHD12832_SetDisplayClock(uint8_t clk_div, uint8_t freq)
{
    uint8_t cmd[2] = {NHD12832_SET_CLK_DIV_FREQ, 0xA6};

    /*
     * default: 0xA6;
     * D[3:0] => display clock divider;
     * D[7:4] => oscillator frequency
     */
    cmd[1] = (freq << 4) | (clk_div && 0xf);
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_SetMultiplexRatio(uint8_t ratio)
{
    uint8_t cmd[2] = {NHD12832_SET_MULTI_RATIO, ratio}; // default: 0x3F(1/64 duty)
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_SetDisplayOffset(uint8_t offset)
{
    uint8_t cmd[2] = {NHD12832_SET_DISPLAY_OFFSET, offset};
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_SetStartLine(uint8_t pos)
{
    uint8_t cmd = (NHD12832_SET_DISPLAY_START_LINE | pos);
    NHD12832_SendCmd(&cmd, 1);
}

/* ************************************************************************* */
void NHD12832_TurnOnInternalDC(uint8_t en)
{
    uint8_t cmd[2] = {NHD12832_SET_INTERNAL_DC, 0x8E}; // Set external Vcc supply (default)

    if(en) { // Set internal DC/DC voltage converter
        cmd[1] |= en;
    }
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_SetAddressingMode(uint8_t mode)
{
    /*
     * 0x00: horizontal addressing mode;
     * 0x01: vertical addressing mode;
     * 0x02: page addressing mode (default)
     */
    uint8_t cmd[2] = {NHD12832_SET_ADDRESSING_MODE, mode};
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_SetSegRemap(uint8_t d)
{
    /*
     * 0: Column address 0 mapped to SEG0
     * 1: Column address 0 mapped to SEG131
     */
    uint8_t cmd = (NHD12832_SET_SEG_REMAP | (d & 1));
    NHD12832_SendCmd(&cmd, 1);
}

/* ************************************************************************* */
void NHD12832_SetCommonRemap(uint8_t d)
{
    /*
     * 0: scan from COM0 to COM63
     * 8: scan from COM63 to COM0
     */
    uint8_t cmd = (NHD12832_SET_COM_REMAP | (d & 8));
    NHD12832_SendCmd(&cmd, 1);
}

/* ************************************************************************* */
void NHD12832_TurnOnLeftRightRemap(uint8_t en)
{
    // 0x12: disable COM left/right re-map (default)
    uint8_t cmd[2] = {NHD12832_SET_COM_CFG, 0x12};
    if(en) {
        cmd[1] = 0x2;
    }
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_SetBanksPulseWidth(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    uint8_t cmd[5] = {NHD12832_SET_BANKS_PULSE_WIDTH, a, b, c, d};
    NHD12832_SendCmd(cmd, 5);
}

/* ************************************************************************* */
void NHD12832_SetContrast(uint8_t val)
{
    uint8_t cmd[2] = {NHD12832_SET_CONTRAST, val};
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_SetAreaBrightness(uint8_t val)
{
    uint8_t cmd[2] = {NHD12832_SET_BRIGHTNESS, val};
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_SetPrechargePeriod(uint8_t phase1, uint8_t phase2)
{
    /*
     * default: 0x22(2 display clocks at phase 2; 2 display clocks at phase 1)
     * D[3:0] => phase 1 period in 1-15 display clocks
     * D[7:4] => phase 2 period in 1-15 display clocks
     */
    uint8_t cmd[2] = {0xD9, 0x22};
    cmd[1] = (phase2 << 4) | phase1;
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_TurnOnAreaColorMode(uint8_t en)
{
    uint8_t cmd[2] = {NHD12832_SET_AREA_COLOR_MODE, 0}; // 0: monochrome mode and normal power display mode (default)

    if(en) {
        cmd[1] = 0x5;
    }
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_SetVcomh(uint8_t val)
{
    // default: 0x34 (0.77*Vcc)
    uint8_t cmd[2] = {0xD8, val};
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_TurnOnEntireDisplay(uint8_t en)
{
    // 0x0: normal displ
    // 0x1: entire display
    uint8_t cmd = NHD12832_SET_ENTIRE_DISPLAY;
    if(!en) {
        cmd &= ~1;
    }
    NHD12832_SendCmd(&cmd, 1);
}

/* ************************************************************************* */
void NHD12832_TurnOnInverseDisplay(uint8_t en)
{
    // 0x0: normal displa
    // 0x1: inverse dispaly
    uint8_t cmd = NHD12832_SET_INVERSE_DISPLAY;
    if(!en) {
        cmd &= ~1;
    }
    NHD12832_SendCmd(&cmd, 1);
}

/* ************************************************************************* */
void NHD12832_SetStartColumn(uint8_t pos)
{
    // 0x00: set lower column start address for page addressing mode (default)
    // 0x10: set higher column start address for page addressing mode (default)
    uint8_t cmd[2] = {(0x00 + (pos%16)), (0x10 + (pos/16))};
    NHD12832_SendCmd(cmd, 2);
}

/* ************************************************************************* */
void NHD12832_SetStartPage(uint8_t addr)
{
    // Set page start address for page addressing mode
    uint8_t cmd = (NHD12832_SET_START_PAGE | addr);
    NHD12832_SendCmd(&cmd, 1);
}

/* ************************************************************************* */
void NHD12832_SetColumnAddr(uint8_t start_addr, uint8_t end_addr)
{
    // start_addr: set column start address (default: 0)
    // end_addr: set column end address (default: 0x83)
    uint8_t cmd[3] = {NHD12832_SET_COLUMN_ADDR, start_addr, end_addr};
    NHD12832_SendCmd(cmd, 3);
}

/* ************************************************************************* */
void NHD12832_SetPageAddr(uint8_t start_addr, uint8_t end_addr)
{
    // start_addr: set page start address (default: 0)
    // end_addr: set page end address (default: 7)
    uint8_t cmd[3] = {NHD12832_SET_PAGE_ADDR, start_addr, end_addr};
    NHD12832_SendCmd(cmd, 3);
}

/* ************************************************************************* */
void NHD12832_SetDimMode(uint8_t contrast, uint8_t brightness)
{
    // contrast: contrast control for bank 0 (default: 0x80)
    // brightness: brightness for area color banks (default: 0x80)
    uint8_t cmd[4] = {NHD12832_SET_DIM_MODE_CFG, 0, contrast, brightness};
    NHD12832_SendCmd(cmd, 4);
}

/* ************************************************************************* */
void NHD12832_SetReadModifyWriteMode(uint8_t en)
{
    uint8_t cmd = NHD12832_SET_READ_MOD_WRITE_EN; // 0: enable read-modify-write mode (default)

    if(!en) {
        cmd |= 0xE; // 0xE: disable read-modify-write mode
    }
    NHD12832_SendCmd(&cmd, 1);
}

/* ************************************************************************* */
void NHD12832_SetNOP(void)
{
    uint8_t cmd = NHD12832_NOP;
    NHD12832_SendCmd(&cmd, 1);
}

/* ************************************************************************* */
void NHD12832_FillRAM(uint8_t data) // show regular pattern
{
    uint8_t i, j;

    for(i = 0; i < 8; i++) {
        NHD12832_SetStartPage(i);
        NHD12832_SetStartColumn(0);

        for(j = 0; j < 132; j++) {
            NHD12832_SendData(&data, 1);
        }
    }
}

/* ************************************************************************* */
void NHD12832_SetBankColor(void)
{
    uint8_t cmd[10] = {NHD12832_SET_AREA_COLOR_PAGE0, 0, 0x55, 0xAA, 0xFF, NHD12832_SET_AREA_COLOR_PAGE1, 0xFF, 0xFF, 0xFF, 0xFF};
    NHD12832_SendCmd(cmd, sizeof(cmd));
}

// Show regular pattern (partial or full screen)
/* ************************************************************************* */
void NHD12832_FillBlock(uint8_t data, uint8_t start_page, uint8_t end_page, uint8_t start_column, uint8_t total_column)
{
    uint8_t i, j;

    for(i = start_page; i < (end_page + 1); i++) {
        NHD12832_SetStartPage(i);
        NHD12832_SetStartColumn(start_column);
        for(j = 0; j < total_column; j++) {
            NHD12832_SendData(&data, 1);
        }
    }
}

/* ************************************************************************* */
void NHD12832_ShowChecerkBoard(void) // Show checker board (full screen)
{
    uint8_t i, j, data[2] = {0x55, 0xAA};

    for(i = 0; i < 8; i++) {
        NHD12832_SetStartPage(i);
        NHD12832_SetStartColumn(0);

        for(j = 0; j < 66; j++) {
            NHD12832_SendData(data, 2);
        }
    }
}

/* ************************************************************************* */
void NHD12832_ShowPattern(uint8_t *data, uint8_t start_page, uint8_t end_page, uint8_t start_column, uint8_t total_column)
{
    uint8_t i, *src = data;

    for(i = start_page; i < (end_page + 1); i++) {
        NHD12832_SetStartPage(i);
        NHD12832_SetStartColumn(start_column);
        NHD12832_SendData(src, total_column);
    }
}

/* ************************************************************************* */
void NHD12832_ShowFont5x7(uint8_t ascii, uint8_t start_page, uint8_t start_column)
{
    uint8_t space = 0;

    if(ascii >= 0x20 && ascii <= 0x7F) {
        NHD12832_SetStartPage(start_page);
        NHD12832_SetStartColumn(start_column);
        NHD12832_SendData(&stdfont5x7[(ascii - 0x20)*5 + 6], 5);
        NHD12832_SendData(&space, 1);
    }
}

/* ************************************************************************* */
void NHD12832_ShowString(uint8_t *str, uint8_t start_page, uint8_t start_column)
{
    uint8_t *src = str;

    while(*src) {
        NHD12832_ShowFont5x7(*src++, start_page, start_column);
        start_column+=6;
    }

    while (start_column < NHD12832_WIDTH) {
        NHD12832_ShowFont5x7(' ', start_page, start_column);
        start_column+=6;
    }
}

/* ************************************************************************* */
void NHD12832_Clear(uint8_t start_page, uint8_t start_column)
{
    uint8_t column;
    uint8_t space = 0;

    NHD12832_SetStartPage(start_page);
    NHD12832_SetStartColumn(start_column);

    for (column = start_column; column < NHD12832_WIDTH; column++) {
        NHD12832_SendData(&space, 1);
    }
}

/* ************************************************************************* */
int NHD12832_Init(void)
{
    int err;

    // Sets GPIO to desired level for the board
    Board_nhd12832_Init();

    // Configure GPIO
    GPIO_OutClr(&nhd12832_res);
    if ((err = GPIO_Config(&nhd12832_res)) != E_NO_ERROR) {
        return err;
    }
    GPIO_OutClr(&nhd12832_dc);
    if ((err =  GPIO_Config(&nhd12832_dc)) != E_NO_ERROR) {
        return err;
    }

    // Configure SPI interface
    if ((err = SPIM_Init(NHD12832_SPI, &nhd12832_spim_cfg, &nhd12832_sys_cfg)) != E_NO_ERROR) {
        return err;
    }

    // Reset RES pin for 200us
    GPIO_OutSet(&nhd12832_res);

    // Turn off display
    NHD12832_TurnOnDisplay(0);

    // Set display clock to 160fps
    NHD12832_SetDisplayClock(0, 1);

    // Set display duty to 1/32 (0xF - 0x3F)
    NHD12832_SetMultiplexRatio(0x1F);

    // Shift mapping RAM counter (0x0 - 0x3F)
    NHD12832_SetDisplayOffset(0);

    // Set mapping RAM display start line (0x0 - 0x3F)
    NHD12832_SetStartLine(0);

    // Disable embedded DC/DC converter
    NHD12832_TurnOnInternalDC(0);

    // Set monochrome & low power save mode
    NHD12832_TurnOnAreaColorMode(5);

    // Set page addressing mode (0x0 - 0x2)
    NHD12832_SetAddressingMode(2);

    // Set seg/column mapping (0x0 - 0x1)
    NHD12832_SetSegRemap(1);

    // Set COM scan direction (0x0/0x8)
    NHD12832_SetCommonRemap(0x8);

    // Set alternative configuration (0x0/0x10)
    NHD12832_TurnOnLeftRightRemap(0);

    // Set all banks pulse width as 64 clocks
    NHD12832_SetBanksPulseWidth(0x3F, 0x3F, 0x3F, 0x3F);

    // Set SEG output current
    NHD12832_SetContrast(DEFAULT_BRIGHTNESS);

    // Set area brightness
    NHD12832_SetAreaBrightness(DEFAULT_BRIGHTNESS);

    // Set pre-charge as 13 clocks and discharge as 2 clocks
    NHD12832_SetPrechargePeriod(0x2, 0xD);

    // Set VCOM deselect level
    NHD12832_SetVcomh(0x8);

    // Disable entire display (0x0/0x1)
    NHD12832_TurnOnEntireDisplay(0);

    // Disable inverse display (0x0/0x1)
    NHD12832_TurnOnInverseDisplay(0);

    // Clear screen
    NHD12832_FillRAM(0);

    // Turn on display
    NHD12832_TurnOnDisplay(1);

    return E_NO_ERROR;
}
/**@}*/

