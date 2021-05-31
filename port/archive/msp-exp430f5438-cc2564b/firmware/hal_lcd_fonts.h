/*******************************************************************************
    Filename: hal_lcd_fonts.h

    Copyright 2008 Texas Instruments, Inc.
***************************************************************************/
#ifndef __HAL_LCD_FONTS_H
#define __HAL_LCD_FONTS_H

#define FONT_HEIGHT		12                    // Each character has 13 lines 

#include <stdint.h>

extern const uint8_t fonts_lookup[];
extern const uint16_t fonts[];

#endif // __HAL_LCD_FONTS_H
