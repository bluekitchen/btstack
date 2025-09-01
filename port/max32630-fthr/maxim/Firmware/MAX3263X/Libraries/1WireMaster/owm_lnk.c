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
 * This file and the accompanying owm_ses.c file implement the general purpose
 * templates as described in the 1-Wire PDK documentation.
 */

#include "ownet.h"
#include "mxc_config.h"
#include "owm_regs.h"
#include "mxc_sys.h"
#include "mxc_assert.h"

// exportable link-level functions
SMALLINT owTouchReset(int);
SMALLINT owTouchBit(int,SMALLINT);
SMALLINT owTouchByte(int,SMALLINT);
SMALLINT owWriteByte(int,SMALLINT);
SMALLINT owReadByte(int);
SMALLINT owSpeed(int,SMALLINT);
SMALLINT owLevel(int,SMALLINT);
SMALLINT owProgramPulse(int);
void msDelay(int);
long msGettick(void);

//--------------------------------------------------------------------------
// Reset all of the devices on the 1-Wire Net and return the result.
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
//
// Returns: TRUE(1):  presence pulse(s) detected, device(s) reset
//          FALSE(0): no presence pulses detected
//
SMALLINT owTouchReset(int portnum)
{
    MXC_ASSERT(portnum == 0);                                       // There is only one OWM module
    MXC_OWM->ctrl_stat |= MXC_F_OWM_CTRL_STAT_START_OW_RESET;       // Generate a reset pulse and look for reply
    while((MXC_OWM->intfl & MXC_F_OWM_INTFL_OW_RESET_DONE) == 0);   // Wait for reset time slot to complete
    MXC_OWM->intfl = MXC_F_OWM_INTFL_OW_RESET_DONE;                 // Clear the flag
    return (MXC_OWM->ctrl_stat & MXC_F_OWM_CTRL_STAT_PRESENCE_DETECT) ? TRUE : FALSE;
}

//--------------------------------------------------------------------------
// Send 1 bit of communication to the 1-Wire Net and return the
// result 1 bit read from the 1-Wire Net.  The parameter 'sendbit'
// least significant bit is used and the least significant bit
// of the result is the return bit.
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
// 'sendbit'    - the least significant bit is the bit to send
//
// Returns: 0:   0 bit read from sendbit
//          1:   1 bit read from sendbit
//
SMALLINT owTouchBit(int portnum, SMALLINT sendbit)
{
    MXC_ASSERT(portnum == 0);                                       // There is only one OWM module
    MXC_OWM->cfg |= MXC_F_OWM_CFG_SINGLE_BIT_MODE;                  // Send a single bit
    MXC_OWM->data = (sendbit << MXC_F_OWM_DATA_TX_RX_POS) & MXC_F_OWM_DATA_TX_RX;       // Write data
    while((MXC_OWM->intfl & MXC_F_OWM_INTFL_TX_DATA_EMPTY) == 0);   // Wait for data to be sent
    while((MXC_OWM->intfl & MXC_F_OWM_INTFL_RX_DATA_READY) == 0);   // Wait for data to be read
    MXC_OWM->intfl = (MXC_F_OWM_INTFL_TX_DATA_EMPTY | MXC_F_OWM_INTFL_RX_DATA_READY);   // Clear the flags
    return (MXC_OWM->data >> MXC_F_OWM_DATA_TX_RX_POS) & 1;
}

//--------------------------------------------------------------------------
// Send 8 bits of communication to the 1-Wire Net and return the
// result 8 bits read from the 1-Wire Net.  The parameter 'sendbyte'
// least significant 8 bits are used and the least significant 8 bits
// of the result is the return byte.
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
// 'sendbyte'   - 8 bits to send (least significant byte)
//
// Returns:  8 bytes read from sendbyte
//
SMALLINT owTouchByte(int portnum, SMALLINT sendbyte)
{
    MXC_ASSERT(portnum == 0);                                       // There is only one OWM module
    MXC_OWM->cfg &= ~MXC_F_OWM_CFG_SINGLE_BIT_MODE;                 // Send 8 bits
    MXC_OWM->data = (sendbyte << MXC_F_OWM_DATA_TX_RX_POS) & MXC_F_OWM_DATA_TX_RX;      // Write data
    while((MXC_OWM->intfl & MXC_F_OWM_INTFL_TX_DATA_EMPTY) == 0);   // Wait for data to be sent
    while((MXC_OWM->intfl & MXC_F_OWM_INTFL_RX_DATA_READY) == 0);   // Wait for data to be read
    MXC_OWM->intfl = (MXC_F_OWM_INTFL_TX_DATA_EMPTY | MXC_F_OWM_INTFL_RX_DATA_READY);   // Clear the flags
    return (MXC_OWM->data >> MXC_F_OWM_DATA_TX_RX_POS) & 0xFF;
}

//--------------------------------------------------------------------------
// Send 8 bits of communication to the 1-Wire Net and verify that the
// 8 bits read from the 1-Wire Net is the same (write operation).
// The parameter 'sendbyte' least significant 8 bits are used.
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
// 'sendbyte'   - 8 bits to send (least significant byte)
//
// Returns:  TRUE: bytes written and echo was the same
//           FALSE: echo was not the same
//
SMALLINT owWriteByte(int portnum, SMALLINT sendbyte)
{
   return (owTouchByte(portnum,sendbyte) == sendbyte) ? TRUE : FALSE;
}

//--------------------------------------------------------------------------
// Send 8 bits of read communication to the 1-Wire Net and and return the
// result 8 bits read from the 1-Wire Net.
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
//
// Returns:  8 bytes read from 1-Wire Net
//
SMALLINT owReadByte(int portnum)
{
   return owTouchByte(portnum,0xFF);
}

//--------------------------------------------------------------------------
// Set the 1-Wire Net communication speed.
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
// 'new_speed'  - new speed defined as
//                MODE_NORMAL     0x00
//                MODE_OVERDRIVE  0x01
//
// Returns:  current 1-Wire Net speed
//
SMALLINT owSpeed(int portnum, SMALLINT new_speed)
{
    MXC_ASSERT(portnum == 0);                       // There is only one OWM module
    if(new_speed == MODE_NORMAL) {
        MXC_OWM->cfg &= ~MXC_F_OWM_CFG_OVERDRIVE;   // Set standard speed
        return MODE_NORMAL;
    }
    else {
        MXC_OWM->cfg |= MXC_F_OWM_CFG_OVERDRIVE;    // Set overdrive speed
        return MODE_OVERDRIVE;
    }
}

//--------------------------------------------------------------------------
// Set the 1-Wire Net line level.  The values for NewLevel are
// as follows:
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
// 'new_level'  - new level defined as
//                MODE_NORMAL     0x00
//                MODE_STRONG5    0x02
//                MODE_PROGRAM    0x04
//                MODE_BREAK      0x08
//
// Returns:  current 1-Wire Net level
//
SMALLINT owLevel(int portnum, SMALLINT new_level)
{
   // this driver only supports normal 1-wire levels.
   return MODE_NORMAL;
}

//--------------------------------------------------------------------------
// This procedure creates a fixed 480 microseconds 12 volt pulse
// on the 1-Wire Net for programming EPROM iButtons.
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
//
// Returns:  TRUE  successful
//           FALSE program voltage not available
//
SMALLINT owProgramPulse(int portnum)
{
    // 12V pulses are not supported by this driver
   return FALSE;
}

//--------------------------------------------------------------------------
//  Description:
//     Delay for at least 'len' ms
//
void msDelay(int len)
{
    int sysFreqHz = SYS_SysTick_GetFreq();
    int ticks = (uint64_t)sysFreqHz * len / 1000;
    SYS_SysTick_Delay(ticks);
}

//--------------------------------------------------------------------------
// Get the current millisecond tick count.  Does not have to represent
// an actual time, it just needs to be an incrementing timer.
//
long msGettick(void)
{
    static long ticks = 0;
    return ticks++;
}
