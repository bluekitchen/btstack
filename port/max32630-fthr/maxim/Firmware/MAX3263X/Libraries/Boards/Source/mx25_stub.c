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
 * $Date: 2016-06-22 17:24:26 -0500 (Wed, 22 Jun 2016) $
 * $Revision: 23483 $
 *
 ******************************************************************************/

#include "mxc_config.h"
#include "icc_regs.h"
#include "spix.h"
#include "mxc_sys.h"
#include "spim.h"
#include "mx25.h"

#if defined ( __GNUC__ )
#undef IAR_PRAGMAS //Make sure this is not defined for GCC
#endif

/* The stack address is defined by the linker
 * It is typed as a function here to avoid compiler warnings
 */
extern void __StackTop(void);
extern void Reset_Handler(void);
extern void DefaultIRQ_Handler(void);
extern void DebugMon_Handler(void);
extern void MemManage_Handler(void);
extern void BusFault_Handler(void);
extern void UsageFault_Handler(void);
extern void SVC_Handler(void);
extern void DebugMon_Handler(void);
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);

void Reset_Handler_ROM(void);
void NMI_Handler_ROM(void);
void HardFault_Handler_ROM(void);

/* Create a vector table to locate at zero in the ROM for handling reset and startup */
#if IAR_PRAGMAS
// IAR memory section declaration for the IVT location.
#pragma section=".rom_vector"
#endif
#if defined ( __GNUC__ )
__attribute__ ((section(".rom_vector")))
#endif
void (* const rom_vector[])(void) = {
    __StackTop,            /* Top of Stack */
    Reset_Handler_ROM,     /* Reset Handler */
    NMI_Handler_ROM,       /* NMI Handler */
    HardFault_Handler_ROM, /* Hard Fault Handler */
    MemManage_Handler,     /* MPU Fault Handler */
    BusFault_Handler,      /* Bus Fault Handler */
    UsageFault_Handler,    /* Usage Fault Handler */
    0,                     /* Reserved */
    0,                     /* Reserved */
    0,                     /* Reserved */
    0,                     /* Reserved */
    SVC_Handler,           /* SVCall Handler */
    DebugMon_Handler,      /* Debug Monitor Handler */
    0,                     /* Reserved */
    PendSV_Handler,        /* PendSV Handler */
    SysTick_Handler,       /* SysTick Handler */
};

/* This is needed to handle the NMI at POR */
#if IAR_PRAGMAS
// IAR memory section declaration for the IVT location.
#pragma section=".rom_handlers"
#endif
#if defined ( __GNUC__ )
__attribute__ ((section(".rom_handlers")))
#endif

void NMI_Handler_ROM(void)
{
    __NOP();
}

/* This is needed to handle the fault after initial programming */
#if IAR_PRAGMAS
// IAR memory section declaration for the IVT location.
#pragma section=".rom_handlers"
#endif
#if defined ( __GNUC__ )
__attribute__ ((section(".rom_handlers")))
#endif

void HardFault_Handler_ROM(void)
{
    NVIC_SystemReset();
}

/* This is needed to handle the fault after initial programming */
#if IAR_PRAGMAS
// IAR memory section declaration for the IVT location.
#pragma section=".rom_handlers"
#endif
#if defined ( __GNUC__ )
__attribute__ ((section(".rom_handlers")))
#endif

void Reset_Handler_ROM(void)
{
    mxc_spim_fifo_regs_t *fifo;

    // Disable instruction cache
    MXC_ICC->invdt_all = 1;
    MXC_ICC->ctrl_stat &= ~MXC_F_ICC_CTRL_STAT_ENABLE;
    MXC_ICC->invdt_all = 1;

    // Initialize SPIM1 to initialize MX25
    MXC_IOMAN->spim1_req = 0x10110;
    MXC_CLKMAN->sys_clk_ctrl_12_spi1 = 0x1;

    MXC_SPIM1->gen_ctrl = (MXC_F_SPIM_GEN_CTRL_SPI_MSTR_EN | MXC_F_SPIM_GEN_CTRL_TX_FIFO_EN |
                           MXC_F_SPIM_GEN_CTRL_RX_FIFO_EN);

    // Get the TX and RX FIFO for this SPIM
    fifo = MXC_SPIM_GET_SPIM_FIFO(1);

    // Initialize the SPIM to work with the MX25
    MXC_SPIM1->mstr_cfg = 0x4400;

    // Reset the MX25
    fifo->trans_16[0] = (0x1 | (0x1 << 2) | (0x1 << 4) | (0x1 << 13));
    fifo->trans_16[0] = (0xF000 | MX25_CMD_RST_EN);

    fifo->trans_16[0] = (0x1 | (0x1 << 2) | (0x1 << 4) | (0x1 << 13));
    fifo->trans_16[0] = (0xF000 | MX25_CMD_RST_MEM);

    // Write enable
    fifo->trans_16[0] = (0x1 | (0x1 << 2) | (0x1 << 4) | (0x1 << 13));
    fifo->trans_16[0] = (0xF000 | MX25_CMD_WRITE_EN);

    // Enable quad mode
    fifo->trans_16[0] = (0x1 | (0x1 << 2) | (0x2 << 4) | (0x1 << 13));
    fifo->trans_16[0] = ((MX25_QE_MASK << 8) | (MX25_CMD_WRITE_SR << 0));

    // Wait for the busy flag to clear
    uint8_t busy = MX25_WIP_MASK;
    while((busy & MX25_WIP_MASK) || !(busy & MX25_QE_MASK)) {

        // Read SR
        fifo->trans_16[0] = (0x1 | (0x1 << 2) | (0x1 << 4));
        fifo->trans_16[0] = (0xF000 | MX25_CMD_READ_SR);

        fifo->trans_16[0] = (0x2 | (0x1 << 2) | (0x1 << 4) | (0x1 << 13));

        while(!(MXC_SPIM1->fifo_ctrl & MXC_F_SPIM_FIFO_CTRL_RX_FIFO_USED)) {}
        busy = fifo->rslts_8[0];
    }

    // Disable the SPIM clock and I/O
    MXC_IOMAN->spim1_req = 0x0;
    MXC_CLKMAN->sys_clk_ctrl_12_spi1 = 0x0;

    // Enable SPIX I/O
    MXC_IOMAN->spix_req = 0x11110;

    // Enable SPIX clock
    MXC_CLKMAN->sys_clk_ctrl_2_spix = 1;

    // Setup SPIX
    MXC_SPIX->master_cfg = 0x1104;
    MXC_SPIX->fetch_ctrl = (MX25_CMD_QREAD | (0x2 << MXC_F_SPIX_FETCH_CTRL_DATA_WIDTH_POS) |
                            (0x2 << MXC_F_SPIX_FETCH_CTRL_ADDR_WIDTH_POS));

    MXC_SPIX->mode_ctrl = (MX25_QREAD_DUMMY);
    MXC_SPIX->sck_fb_ctrl = (MXC_F_SPIX_SCK_FB_CTRL_ENABLE_SCK_FB_MODE |
                             MXC_F_SPIX_SCK_FB_CTRL_INVERT_SCK_FB_CLK);

#if (MXC_SPIX_REV == 0)
    // 8 bits for command, 6 for address, MX25_QREAD_DUMMY for the dummy bits
    MXC_SPIX->sck_fb_ctrl |= ((14 + MX25_QREAD_DUMMY) << MXC_F_SPIX_SCK_FB_CTRL_IGNORE_CLKS_POS);
#endif
    // Jump to the Rest Handler
    Reset_Handler();
}
