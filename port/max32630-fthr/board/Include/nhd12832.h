/**
 * @file    
 * @brief   Header file for NHD12832 NHD12832 driver
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
 * $Date: 2017-02-16 15:06:27 -0600 (Thu, 16 Feb 2017) $
 * $Revision: 26490 $
 *
 *************************************************************************** */

#ifndef _NHD12832_H
#define _NHD12832_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup bsp
 * @defgroup nhd12832_bsp NHD12832 Display Driver BSP
 * @{
 */
/* **** Definitions **** */
/**
 * Width of NHD12832 display in pixels, per page.
 */
#define NHD12832_WIDTH                      128
/**
 * Height of NHD12832 display in pixels, per page.
 */
 #define NHD12832_HEIGHT                     32

/**
 * Structure type for to map a bitmap to the NHD12832 display.
 */
typedef struct {
    uint8_t width; 												/**< Bit map width, must be less than or equal to NHDD12832_WIDTH. 		*/
    uint8_t height;												/**< Bit map height, must be less than or equal to NHDD12832_HEIGHT. 	*/
    uint16_t delay;												/**< Unused */
    uint32_t bmp[NHD12832_WIDTH * (NHD12832_HEIGHT + 31)/32 ];	/**< storage array for bitmap. */
} nhd12832_bitmap_t;

/* **** Globals **** */

/* **** Function Prototypes **** */

/**
 * @brief      Initialize the NHD12832 display.
 *
 * @retval     #E_NO_ERROR   NHD12832 intialized successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int NHD12832_Init(void);

/**
 * @brief      Loads the @p image to the NHD12832 memory. 
 *
 * @param      image  Pointer to the bitmap image to display.
 *
 * @retval     #E_NO_ERROR   Image loaded successfully.
 * @retval     "Error Code"  @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int NHD12832_LoadImage(nhd12832_bitmap_t *image);

/**
 * @brief      Displays the image loaded by NHD12832_LoadImage(nhd12832_bitmap_t *image).
 */
void NHD12832_PrintScreen(void);

/**
 * @brief      Enables or Disables the Display.
 *
 * @param[in]  en    @arg 1 to Enable the Display. @arg 0 to Disable the Display.
 */
void NHD12832_TurnOnDisplay(uint8_t en);

/**
 * @brief      Set the clock for the NHD12832 display.
 *
 * @param[in]  clk_div  The display clock divider to set.
 * @param[in]  freq     The display oscillator frequency.
 */
void NHD12832_SetDisplayClock(uint8_t clk_div, uint8_t freq);

/**
 * @brief      Set the multiplex ration (duty cycle) of the display.
 *
 * @param[in]  ratio  The ratio to set, the default is 1/64 duty, @p ratio = 0x3F.
 */
void NHD12832_SetMultiplexRatio(uint8_t ratio);

/**
 * @brief      Set the display offset.
 *
 * @param[in]  offset  The offset desired.
 */
void NHD12832_SetDisplayOffset(uint8_t offset);

/**
 * @brief      Set the display start line number.
 *
 * @param[in]  pos   The starting line number.
 */
void NHD12832_SetStartLine(uint8_t pos);

/**
 * @brief      Turn on the internal DC regulator for the display.
 *
 * @param[in]  en    @arg 1 to enable. @arg 0 to disable. 
 */
void NHD12832_TurnOnInternalDC(uint8_t en);

/**
 * @brief      Sets the addressing mode of the display.
 *
 * @param[in]  mode  The mode @arg 0x00: Horizontal addressing mode. @arg 0x01: Vertical addressing mode. @arg 0x02: Page addressing mode (default).
 */
void NHD12832_SetAddressingMode(uint8_t mode);

/**
 * @brief      Sets the segment mapping of the display.
 *
 * @param[in]  d     @arg 0: Column address 0 mapped to SEG0. @arg 1: Column address 0 mapped to SEG131.
 */
void NHD12832_SetSegRemap(uint8_t d);

/**
 * @brief      Set the remapping of the COM.
 *
 * @param[in]  d     @arg 0: Scan from COM0 to COM63. @arg 8: scan from COM63 to COM0.
 */
void NHD12832_SetCommonRemap(uint8_t d);

/**
 * @brief      Left Right remapping.
 * @note       Currently only sets the remapping to it's default state (0x12)
 *
 * @param[in]  en    enable or disable. 
 */
void NHD12832_TurnOnLeftRightRemap(uint8_t en);

/**
 * @brief      Set the pulse width of the banks.
 *
 * @param[in]  a     Width of pulse for bank a.
 * @param[in]  b     Width of pulse for bank b.
 * @param[in]  c     Width of pulse for bank c.
 * @param[in]  d     Width of pulse for bank d.
 */
void NHD12832_SetBanksPulseWidth(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

/**
 * @brief      Set the contrast of the display.
 *
 * @param[in]  val   The desired contrast setting.
 */
void NHD12832_SetContrast(uint8_t val);

/**
 * @brief      Sets the Area Brightness of the Display.
 *
 * @param[in]  val   The desired area brightness setting.
 */
void NHD12832_SetAreaBrightness(uint8_t val);

/**
 * @brief      Sets the precharge period.
 * @details    The default precharge period is 2 display clocks for phase 1 and
 *             2 display clocks for phase 2.
 *
 * @param[in]  phase1  Phase 1 period in 1 to 15 display clock cycles.
 * @param[in]  phase2  Phase 2 period in 1 to 15 display clock cycles.
 */
void NHD12832_SetPrechargePeriod(uint8_t phase1, uint8_t phase2);

/**
 * @brief      Enable or disable Area Color Mode.
 *
 * @param[in]  en    @arg 1 to enable area color mode. @arg 0 to disable area color mode.
 */
void NHD12832_TurnOnAreaColorMode(uint8_t en);

/**
 * @brief      Sets the desired voltage for the Vcomh.
 * @details    The default setting for the display is val = 0x34 resulting in
 *             \f$ V_{COMH} = 0.77 * V_{CC} \f$.
 *             
 * @note       See NHD12832 data sheet for details on setting the Vcomh.
 *
 * @param[in]  val   The desired setting.
 */
void NHD12832_SetVcomh(uint8_t val);

/**
 * @brief      Turns on or off the entire display.
 *
 * @param[in]  en    @arg 1 to turn on entire display. @arg 0 to set to normal display.
 */
void NHD12832_TurnOnEntireDisplay(uint8_t en);

/**
 * @brief      Turns on or off the inverse display.
 *
 * @param[in]  en    @arg 1 to turn on inverse display. @arg 0 to set to normal display.
 */
void NHD12832_TurnOnInverseDisplay(uint8_t en);

/**
 * @brief      Sets the display starting column.
 *
 * @param[in]  pos   The column desired. 
 */
void NHD12832_SetStartColumn(uint8_t pos);

/**
 * @brief      Sets the start page in page addressing mode.
 *
 * @param[in]  addr  The page number.
 */
void NHD12832_SetStartPage(uint8_t addr);

/**
 * @brief      Sets the column address start and end.
 * @details 	The display defaults to a @p start_addr of 0x00 and a @p end_addr of 0x83.
 * @param[in]  start_addr  The start address
 * @param[in]  end_addr    The end address
 */
void NHD12832_SetColumnAddr(uint8_t start_addr, uint8_t end_addr);

/**
 * @brief      Sets the page address in page address mode.
 * @details 	The display defaults to page @p start_addr of 0 and a page @p end_addr of 7.
 *
 * @param[in]  start_addr  The page start address.
 * @param[in]  end_addr    The page end address.
 */
void NHD12832_SetPageAddr(uint8_t start_addr, uint8_t end_addr);

/**
 * @brief      Sets the dim mode for the display.
 * @details    The displays defaults to a @p contrast of 0x80 and a @p brightness of 0x80.
 * 
 * @param[in]  contrast    The contrast control desired.
 * @param[in]  brightness  The brightness for the area color banks.
 */
void NHD12832_SetDimMode(uint8_t contrast, uint8_t brightness);

/**
 * @brief      Enables or disables Read Modify Write mode for the display.
 *
 * @param[in]  en    @arg 1 to enable Read-Modify-Write mode. @arg 0 to disable Read-Modify-Write mode.
 */
void NHD12832_SetReadModifyWriteMode(uint8_t en);

/**
 * @brief      Sends a NOP command to the display.
 */
void NHD12832_SetNOP(void);

/**
 * @brief      Fill the display RAM with the value @p data.
 *
 * @param[in]  data  The data to write to the display RAM.
 */
void NHD12832_FillRAM(uint8_t data);

/**
 * @brief      Fill a block of display RAM with the @p data value.
 *
 * @param[in]  data          The data to write to the display RAM.
 * @param[in]  start_page    The start page.
 * @param[in]  end_page      The end page.
 * @param[in]  start_column  The start column.
 * @param[in]  total_column  The total number of columns to write.
 */
void NHD12832_FillBlock(uint8_t data, uint8_t start_page, uint8_t end_page, uint8_t start_column, uint8_t total_column);

/**
 * @brief      Show a checker board pattern on the display.
 */
void NHD12832_ShowChecerkBoard(void);

/**
 * @brief      Shows the pattern requested on the display.
 *
 * @param      data          A pointer to the pattern to show.
 * @param[in]  start_page    The start page
 * @param[in]  end_page      The end page
 * @param[in]  start_column  The start column
 * @param[in]  total_column  The total number of columns
 */
void NHD12832_ShowPattern(uint8_t *data, uint8_t start_page, uint8_t end_page, uint8_t start_column, uint8_t total_column);

/**
 * @brief      Show the 5 x 7 font on the display.
 *
 * @param[in]  ascii         The ascii font  character to display.
 * @param[in]  start_page    The start page
 * @param[in]  start_column  The start column
 */
void NHD12832_ShowFont5x7(uint8_t ascii, uint8_t start_page, uint8_t start_column);

/**
 * @brief      Show the string @p str on the display.
 *
 * @param      str           The string to display.
 * @param[in]  start_page    The start page.
 * @param[in]  start_column  The start column.
 */
void NHD12832_ShowString(uint8_t *str, uint8_t start_page, uint8_t start_column);

/**
 * @brief      Clear the display.
 *
 * @param[in]  start_page    The start page
 * @param[in]  start_column  The start column
 */
void NHD12832_Clear(uint8_t start_page, uint8_t start_column);

/**@}*/
#endif
