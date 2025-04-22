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
 * $Date: 2016-03-11 11:46:37 -0600 (Fri, 11 Mar 2016) $ 
 * $Revision: 21839 $
 *
 ******************************************************************************/

#include <string.h>
#include "wsf_types.h"
#include "wsf_os.h"
#include "wsf_msg.h"
#include "wsf_assert.h"
#include "hci_drv.h"
#include "hci_tr.h"
#include "hci_api.h"
#include "mxc_config.h"
#include "spi_regs.h"
#include "pwrman_regs.h"
#include "pwrseq_regs.h"
#include "ioman_regs.h"
#include "gpio.h"
#include "clkman.h"
#include "ioman.h"

/***** Definitions *****/
#define MXC_SPIn            MXC_SPI2
#define MXC_SPIn_FIFO       MXC_SPI2_FIFO
#define SPIn_IRQn           SPI2_IRQn
#define SPIn_IRQHandler     SPI2_IRQHandler
#define MXC_SPIn_TXFIFO     (*((volatile uint16_t *)(MXC_BASE_SPI2_FIFO + MXC_R_SPI_FIFO_OFFS_TRANS)))
#define MXC_SPIn_RXFIFO     (*((volatile uint8_t *)(MXC_BASE_SPI2_FIFO + MXC_R_SPI_FIFO_OFFS_RSLTS)))

#define HCI_SPI_FREQ        1000000

typedef enum {
    HCI_NONE        = 0,
    HCI_CMD         = 1,
    HCI_ACL_DATA    = 2,
    HCI_SCO_DATA    = 3,
    HCI_EVENT       = 4
} hci_packet_t;

typedef union {
    struct {
        uint16_t direction   : 2; /* 0:none; 1:Tx; 2:Rx; 3:both */
        uint16_t unit        : 2; /* 0:bit; 1:byte; 2:page */
        uint16_t size        : 5; /* up to 0x1F; need to give it a dummy byte if Tx size is odd */
        uint16_t width       : 2; /* bit wide: 0:1; 1:2; 2:4, etc. */
        uint16_t alt_clk     : 1;
        uint16_t flow_ctrl   : 1;
        uint16_t deassert_ss : 1;
        uint16_t             : 2;
    };
    uint16_t header;
} spi_header_t;

#define MAX_SPI_PACKET      32

/***** File Scope Data *****/
static volatile unsigned int reading;
static uint8_t *readPtr;
static int readLen;
static int readReceived;
static int readLast;

static volatile unsigned int writing;
static uint8_t *writeData;
static uint16_t *writePtr;
static int writeLen;
static int writeSent;

static uint8_t _rstPort;
static uint8_t _rstPin;
static uint8_t _irqPort;
static uint8_t _irqPin;

/***** Function Prototypes *****/
static void spiInit(void);

/*************************************************************************************************/
void hciDrvInit(uint8_t rstPort, uint8_t rstPin, uint8_t irqPort, uint8_t irqPin)
{
    _rstPort = rstPort;
    _rstPin = rstPin;
    _irqPort = irqPort;
    _irqPin = irqPin;

    /* Setup SPI IRQ, CSN and RST pins */
    GPIO_SetOutVal(rstPort, rstPin, 0);
    GPIO_SetOutMode(rstPort, rstPin, MXC_E_GPIO_OUT_MODE_NORMAL);
    GPIO_SetInMode(irqPort, irqPin, MXC_E_GPIO_IN_MODE_NORMAL, MXC_E_GPIO_RESISTOR_PULL_UP);

    CLKMAN_SetClkScale(MXC_E_CLKMAN_CLK_SPI2, MXC_E_CLKMAN_CLK_SCALE_DIV_2);

    /* Setup SPIM2 pins, mapping as EvKit for EM9301 */
    IOMAN_SPI2(MXC_E_IOMAN_MAPPING_B, 1, 1, 0, 0, 1, 0, 0, 0);

    spiInit();
    NVIC_EnableIRQ(SPIn_IRQn);

    __disable_irq();
    GPIO_SetIntMode(_irqPort, _irqPin, MXC_E_GPIO_INT_MODE_HIGH_LVL, MXC_E_GPIO_RESISTOR_PULL_UP, hciDrvIsr);
    GPIO_DisableInt(_irqPort, _irqPin);
    __enable_irq();

    // Set interrupt priorities so that the SPI interrupt is higher priority than the external interrupt.
    NVIC_SetPriority(SPIn_IRQn, 2);
    NVIC_SetPriority(MXC_GPIO_GET_IRQ(irqPort), 4);
}

/*************************************************************************************************/
static void spiInit(void)
{
    MXC_SPIn->inten = 0;     // disable interrupts
    MXC_SPIn->gen_ctrl = 0;  // reset state and clear FIFOs
    MXC_SPIn->intfl = ~0;    // clear interrupts

    uint32_t clk_val = ((SystemCoreClock/2) + ((2 * HCI_SPI_FREQ) - 1)) / (2 * HCI_SPI_FREQ);
    MXC_SPIn->mstr_cfg = ((clk_val << MXC_F_SPI_MSTR_CFG_SCK_HI_CLK_POS) | (clk_val << MXC_F_SPI_MSTR_CFG_SCK_LO_CLK_POS) | (0x1 << MXC_F_SPI_MSTR_CFG_ACT_DELAY_POS));

    MXC_SPIn->ss_sr_polarity = (0x1 << MXC_F_SPI_SS_SR_POLARITY_FC_POLARITY_POS);
    MXC_SPIn->spcl_ctrl = 0;

    // enable operation
    MXC_SPIn->gen_ctrl = MXC_F_SPI_GEN_CTRL_SPI_MSTR_EN | MXC_F_SPI_GEN_CTRL_TX_FIFO_EN | MXC_F_SPI_GEN_CTRL_RX_FIFO_EN;

    reading = 0;
    writing = 0;
    readLen = 0;
    writeLen = 0;
    writeData = NULL;
}

/*************************************************************************************************/
void hciDrvReset(void)
{
    // disable interrupts to prevent further operations
    GPIO_DisableInt(_irqPort, _irqPin);
    MXC_SPIn->inten = 0;

    // assert reset
    BITBAND_SetBit((uint32_t)&MXC_GPIO->out_val[_rstPort], _rstPin);

    /* Work around for MAX3262X EvKits */
    HciResetCmd();  /* send an HCI Reset command to start the sequence */

    // Hold EM3901 in reset for at least 1ms
    volatile unsigned int counter;
    for (counter = 0; counter < 4500; counter++);

    // Clear HCI state and SPI interface
    hciTrReset();
    spiInit();

    /* setup IRQ handler */
    GPIO_ClearInt(_irqPort, _irqPin);
    GPIO_EnableInt(_irqPort, _irqPin);

    // clear reset
    BITBAND_ClrBit((uint32_t)&MXC_GPIO->out_val[_rstPort], _rstPin);
}

/*************************************************************************************************/
/*!
 *  \fn     hciDrvWrite
 *
 *  \brief  Write data the driver.
 *
 *  \param  type     HCI packet type
 *  \param  len      Number of bytes to write. Must be > 0.
 *  \param  pData    Byte array to write.
 *
 *  \return Return actual number of data bytes written.
 *
 *  \note   The type parameter allows the driver layer to prepend the data with a header on the
 *          same write transaction.
 */
/*************************************************************************************************/
uint16_t hciDrvWrite(uint8_t type, uint16_t len, uint8_t *pData)
{
    spi_header_t header;

    if ((len == 0) || (pData == NULL)) {
        return 0;
    }

    // disable interrupt to prevent further reads
    GPIO_DisableInt(_irqPort, _irqPin);

    // wait for in progress transactions
    while (reading || writing);

    writing = 1;
    writeData = pData;
    writeLen = len + 1;
    writeSent = 0;

    // clear FIFOs and interrupts
    MXC_SPIn->gen_ctrl = 0;
    MXC_SPIn->inten = 0;
    MXC_SPIn->intfl = ~0;

    // enable SPI
    MXC_SPIn->gen_ctrl = (MXC_F_SPI_GEN_CTRL_SPI_MSTR_EN | MXC_F_SPI_GEN_CTRL_TX_FIFO_EN);

    // configure the flow control
    uint32_t temp = MXC_SPIn->spcl_ctrl;
    temp &= ~(MXC_F_SPI_SPCL_CTRL_SS_SA_SDIO_OUT | MXC_F_SPI_SPCL_CTRL_SS_SA_SDIO_DR_EN);
    temp |=  (MXC_F_SPI_SPCL_CTRL_SS_SAMPLE_MODE | MXC_F_SPI_SPCL_CTRL_MISO_FC_EN | (0x1 << MXC_F_SPI_SPCL_CTRL_SS_SA_SDIO_OUT_POS) | (0x1 << MXC_F_SPI_SPCL_CTRL_SS_SA_SDIO_DR_EN_POS));
    MXC_SPIn->spcl_ctrl = temp;

    // prepare header
    header.header = 0;
    header.direction = 1;
    header.flow_ctrl = 1;
    header.width = 0;
    header.unit = 1;    // bytes

    if (writeLen > MAX_SPI_PACKET) {
        header.size = 0;    // MAX_SPI_PACKET units
        header.deassert_ss = 0;
    } else {
        header.size = writeLen & 0x1F;
        header.deassert_ss = 1;
    }

    // write header
    MXC_SPIn_TXFIFO = header.header;

    // Clear interrupts
    MXC_SPIn->intfl = ~0;

    // Write type and first data byte
    MXC_SPIn_TXFIFO = ((uint16_t)*pData << 8) | type;
    pData += 1;
    writeSent += 2;

    writePtr = (uint16_t*)pData;

    /* Write remaining data until the FIFO is full.
     * Note: Since the FIFO write size is 16-bits, an extra byte may be written
     * to the FIFO if the message is not an even length. The byte will not
     * actually be sent.
     */
    while ( (writeSent < writeLen) && (writeSent & 0x1F) && (((MXC_SPIn->fifo_ctrl & MXC_F_SPI_FIFO_CTRL_TX_FIFO_USED) >> MXC_F_SPI_FIFO_CTRL_TX_FIFO_USED_POS) < (MXC_CFG_SPI_FIFO_DEPTH * 2)) ) {
        MXC_SPIn_TXFIFO = *writePtr;
        writePtr++;
        writeSent += 2;
    }

    if (writeSent < writeLen) {
        uint32_t temp = MXC_SPIn->fifo_ctrl;
        temp &= ~MXC_F_SPI_FIFO_CTRL_TX_FIFO_AE_LVL;
        temp |= (0x1 << MXC_F_SPI_FIFO_CTRL_TX_FIFO_AE_LVL_POS);
        MXC_SPIn->fifo_ctrl = temp;

        // enable interrupt
        MXC_SPIn->inten |= MXC_F_SPI_INTEN_TX_FIFO_AE;
    } else {
        /* free buffer */
        WsfMsgFree(writeData);
        writeData = NULL;
        MXC_SPIn->inten |= MXC_F_SPI_INTEN_TX_READY;
    }

    return len;
}

/*************************************************************************************************/
/*!
 *  \fn     hciDrvRead
 *
 *  \brief  Read data bytes from the driver.
 *
 *  \param  len      Number of bytes to read.
 *  \param  pData    Byte array to store data.
 *
 *  \return Return actual number of data bytes read.
 */
/*************************************************************************************************/
uint16_t hciDrvRead(uint16_t len, uint8_t *pData, bool_t last)
{
    spi_header_t header;

    // Confirm that the IRQ is still active
    if (!GPIO_GetInt(_irqPort, _irqPin)) {
        MXC_SPIn->gen_ctrl = 0;
        reading = 0;
        GPIO_EnableInt(_irqPort, _irqPin);  // enable new reads
        return 0;
    }

    // initialize state
    readLen = len;
    readReceived = 0;
    readPtr = pData;
    readLast = last;

    // prepare header
    header.header = 0;
    header.direction = 2;
    header.flow_ctrl = 1;
    header.width = 0;

    uint32_t temp = MXC_SPIn->fifo_ctrl;
    temp &= ~MXC_F_SPI_FIFO_CTRL_RX_FIFO_AF_LVL;

    if (len >= MAX_SPI_PACKET) {
        temp |= (((MXC_CFG_SPI_FIFO_DEPTH * 2) - 4) << MXC_F_SPI_FIFO_CTRL_RX_FIFO_AF_LVL_POS);
    } else {
        temp |= ((len - 1) << MXC_F_SPI_FIFO_CTRL_RX_FIFO_AF_LVL_POS);
    }

    MXC_SPIn->fifo_ctrl = temp;

    // clear and enable interrupts
    MXC_SPIn->intfl = ~0;
    MXC_SPIn->inten |= MXC_F_SPI_INTEN_RX_FIFO_AF;

    uint16_t remaining = len;
    while (remaining > 0) {

        // Try to minimize the number of required headers
        if (remaining <= MAX_SPI_PACKET) {
            header.unit = 1;    // bytes
            header.size = remaining;
            remaining -= remaining;
        } else {
            header.unit = 2;    // pages

            uint32_t page_size;

            if ((remaining / 4) <= 32) {
                page_size = MXC_S_SPI_MSTR_CFG_PAGE_4B;
                header.size = (remaining / 4) & 0x1F;
                remaining -= (remaining & 0xFFFC);
            } else if ((remaining / 8) <= 32) {
                page_size = MXC_S_SPI_MSTR_CFG_PAGE_8B;
                header.size = (remaining / 8) & 0x1F;
                remaining -= (remaining & 0xFFF8);
            } else if ((remaining / 16) <= 32) {
                page_size = MXC_S_SPI_MSTR_CFG_PAGE_16B;
                header.size = (remaining / 16) & 0x1F;
                remaining -= (remaining & 0xFFF0);
            } else {
                page_size = MXC_S_SPI_MSTR_CFG_PAGE_32B;
                header.size = (remaining / 32) & 0x1F;
                remaining -= (remaining & 0xFFE0);
            }

            MXC_SPIn->mstr_cfg = (MXC_SPIn->mstr_cfg & ~MXC_F_SPI_MSTR_CFG_PAGE_SIZE) | (page_size << MXC_F_SPI_MSTR_CFG_PAGE_SIZE_POS);
        }

        // Deassert the SSEL on the last segment
        header.deassert_ss = ((remaining == 0) && !!last);

        /* Write header to FIFO
         * Note: the assumption is made that the read length would never be 
         * large enough to fill the entire transmit FIFO with headers.
         */
        MXC_SPIn_TXFIFO = header.header;
    }

    return len;
}

/*************************************************************************************************/
static void spiWriteHandler(void)
{
    spi_header_t header;

    if (writeSent < writeLen) {
        // prepare header
        header.header = 0;
        header.direction = 1;
        header.flow_ctrl = 1;
        header.width = 0;
        header.unit = 1;    // bytes

        if ((writeLen - writeSent) > MAX_SPI_PACKET) {
            header.size = 0;    // MAX_SPI_PACKET units
            header.deassert_ss = 0;
        } else {
            header.size = (writeLen - writeSent) & 0x1F;
            header.deassert_ss = 1;
        }

        // write header
        MXC_SPIn_TXFIFO = header.header;

        // write any remaining data until the FIFO is full
        do {
            MXC_SPIn_TXFIFO = *writePtr;
            writePtr++;
            writeSent += 2;
        } while ( (writeSent < writeLen) && (writeSent & 0x1F) && (((MXC_SPIn->fifo_ctrl & MXC_F_SPI_FIFO_CTRL_TX_FIFO_USED) >> MXC_F_SPI_FIFO_CTRL_TX_FIFO_USED_POS) < (MXC_CFG_SPI_FIFO_DEPTH * 2)) );
    }

    if (writeSent == writeLen) {
        if (writeData != NULL) {
            /* free buffer */
            WsfMsgFree(writeData);
            writeData = NULL;
        }
        MXC_SPIn->inten &= ~MXC_F_SPI_INTEN_TX_FIFO_AE;
        MXC_SPIn->inten |= MXC_F_SPI_INTEN_TX_READY;
    }
}

/*************************************************************************************************/
static void spiReadHandler(void)
{
    uint32_t fifo_ctrl;

    while ( (readReceived < readLen) && ((fifo_ctrl = MXC_SPIn->fifo_ctrl) & MXC_F_SPI_FIFO_CTRL_RX_FIFO_USED) ) {
        *readPtr = MXC_SPIn_RXFIFO;

        /* Under some circumstances, reading of the FIFO provides data without advancing the FIFO. Detect
         * and handle the the condition by reading another byte from the FIFO.
         */
        if (MXC_SPIn->fifo_ctrl == fifo_ctrl) {     /* did the FIFO status change? */
            uint8_t temp = MXC_SPIn_RXFIFO;         /* read the next byte */
            if (temp != *readPtr) {                 /* is the byte the same as the last? */
                /* this was a false positive. carry on as usual */
                readPtr++;
                readReceived++;
                *readPtr = temp;
            }
        }

        MXC_SPIn->intfl = MXC_F_SPI_INTFL_RX_FIFO_AF;    // this flag can only be cleared after the FIFO level is below the threshold
        readPtr++;
        readReceived++;
    }

    if ((readLen > 0) && (readReceived == readLen)) {
        int last = readLast;
        if (last) {
            MXC_SPIn->gen_ctrl = 0;
            reading = 0;
        }
        uint16_t len = readLen;
        readLen = 0;
        MXC_SPIn->inten &= ~MXC_F_SPI_INTEN_RX_FIFO_AF;
        hciTrRxIncoming(len);

        if (last) {
            GPIO_EnableInt(_irqPort, _irqPin);  // enable new reads
        }
    }
}

/*************************************************************************************************/
void SPIn_IRQHandler(void)
{
    uint32_t intfl = MXC_SPIn->intfl & MXC_SPIn->inten;
    MXC_SPIn->intfl = intfl;

    if (intfl & MXC_F_SPI_INTFL_RX_FIFO_AF) {
        spiReadHandler();
    }

    if (intfl & MXC_F_SPI_INTFL_TX_FIFO_AE) {
        spiWriteHandler();
    }

    if (intfl & MXC_F_SPI_INTFL_TX_READY) {
        MXC_SPIn->inten &= ~MXC_F_SPI_INTEN_TX_READY;
        MXC_SPIn->gen_ctrl = 0;
        writing = 0;

        // enable reads
        GPIO_EnableInt(_irqPort, _irqPin);  // enable new reads
    }

    // prevent extra interrupts
    NVIC_ClearPendingIRQ(SPIn_IRQn);
}

/*************************************************************************************************/
void hciDrvIsr(void)
{
    // Confirm that the IRQ is still active
    if (!GPIO_GetInt(_irqPort, _irqPin)) {
        return;
    }

    // disable interrupt to prevent further reads
    GPIO_DisableInt(_irqPort, _irqPin);

    // set flag to block writes
    reading = 1;

    // clear FIFOs and interrupts
    MXC_SPIn->gen_ctrl = 0;
    MXC_SPIn->intfl = ~0;

    // enable SPI
    MXC_SPIn->gen_ctrl = (MXC_F_SPI_GEN_CTRL_SPI_MSTR_EN | MXC_F_SPI_GEN_CTRL_TX_FIFO_EN | MXC_F_SPI_GEN_CTRL_RX_FIFO_EN);

    // configure the flow control
    uint32_t temp = MXC_SPIn->spcl_ctrl;
    temp &= ~(MXC_F_SPI_SPCL_CTRL_MISO_FC_EN | MXC_F_SPI_SPCL_CTRL_SS_SA_SDIO_OUT | MXC_F_SPI_SPCL_CTRL_SS_SA_SDIO_DR_EN);
    temp |=  (MXC_F_SPI_SPCL_CTRL_SS_SAMPLE_MODE | (0x1 << MXC_F_SPI_SPCL_CTRL_SS_SA_SDIO_DR_EN_POS));
    MXC_SPIn->spcl_ctrl = temp;

    hciTrRxStart();
}
