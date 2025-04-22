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
#include "spim_regs.h"
#include "mxc_sys.h"
#include "gpio.h"
#include "clkman.h"
#include "ioman.h"

/***** Definitions *****/
#define MXC_SPIMn           MXC_SPIM2
#define MXC_SPIMn_FIFO      MXC_SPIM2_FIFO
#define SPIMn_IRQn          SPIM2_IRQn
#define SPIMn_IRQHandler    SPIM2_IRQHandler

/* Setup SPIM2 pins, mapping as EvKit for EM9301 */
static const ioman_cfg_t ioman_cfg = IOMAN_SPIM2(IOMAN_MAP_B, 1, 1, 0, 0, 1, 0, 0, 0);

#define HCI_SPI_FREQ        5000000

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

extern const gpio_cfg_t hci_rst;
extern const gpio_cfg_t hci_irq;
extern const gpio_cfg_t hci_spi;

/***** Function Prototypes *****/
static void spiInit(void);
void hciDrvIsr(void *);

/*************************************************************************************************/
void hciDrvInit(void)
{
    SYS_IOMAN_UseVDDIOH(&hci_rst);
    SYS_IOMAN_UseVDDIOH(&hci_irq);
    SYS_IOMAN_UseVDDIOH(&hci_spi);

    /* Setup SPI IRQ, CSN and RST pins */
    GPIO_OutSet(&hci_rst);
    GPIO_Config(&hci_rst);
    GPIO_Config(&hci_irq);

    CLKMAN_SetClkScale(CLKMAN_CLK_SPIM2, CLKMAN_SCALE_DIV_2);

    IOMAN_Config(&ioman_cfg);

    spiInit();

    NVIC_EnableIRQ(SPIMn_IRQn);

    GPIO_IntDisable(&hci_irq);
    GPIO_IntConfig(&hci_irq, GPIO_INT_HIGH_LEVEL);  
    GPIO_RegisterCallback(&hci_irq, hciDrvIsr, NULL);
    NVIC_EnableIRQ(MXC_GPIO_GET_IRQ(hci_irq.port));

    // Set interrupt priorities so that the SPI interrupt is higher priority than the external interrupt.
    NVIC_SetPriority(SPIMn_IRQn, 2);
    NVIC_SetPriority(MXC_GPIO_GET_IRQ(hci_irq.port), 4);
}

/*************************************************************************************************/
static void spiInit(void)
{
    MXC_SPIMn->inten = 0;     // disable interrupts
    MXC_SPIMn->gen_ctrl = 0;  // reset state and clear FIFOs
    MXC_SPIMn->intfl = ~0;    // clear interrupts

    uint32_t clk_val = ((SystemCoreClock/2) + ((2 * HCI_SPI_FREQ) - 1)) / (2 * HCI_SPI_FREQ);
    MXC_SPIMn->mstr_cfg = ((clk_val << MXC_F_SPIM_MSTR_CFG_SCK_HI_CLK_POS) | (clk_val << MXC_F_SPIM_MSTR_CFG_SCK_LO_CLK_POS) | (0x1 << MXC_F_SPIM_MSTR_CFG_ACT_DELAY_POS));

    MXC_SPIMn->ss_sr_polarity = (0x1 << MXC_F_SPIM_SS_SR_POLARITY_FC_POLARITY_POS);
    MXC_SPIMn->spcl_ctrl = 0;

    // enable operation
    MXC_SPIMn->gen_ctrl = MXC_F_SPIM_GEN_CTRL_SPI_MSTR_EN | MXC_F_SPIM_GEN_CTRL_TX_FIFO_EN | MXC_F_SPIM_GEN_CTRL_RX_FIFO_EN;

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
    GPIO_IntDisable(&hci_irq);
    MXC_SPIMn->inten = 0;

    // assert reset
    GPIO_OutSet(&hci_rst);

    // Hold EM3901 in reset for at least 1ms
    SYS_SysTick_Delay(SYS_SysTick_GetFreq() / 1000);

    // Clear HCI state and SPI interface
    hciTrReset();
    spiInit();

    /* setup IRQ handler */
    GPIO_IntClr(&hci_irq);
    GPIO_IntEnable(&hci_irq);

    // clear reset
    GPIO_OutClr(&hci_rst);
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
    GPIO_IntDisable(&hci_irq);

    // wait for in progress transactions
    while (reading || writing);

    writing = 1;
    writeData = pData;
    writeLen = len + 1;
    writeSent = 0;

    // clear FIFOs and interrupts
    MXC_SPIMn->gen_ctrl = 0;
    MXC_SPIMn->inten = 0;
    MXC_SPIMn->intfl = ~0;

    // enable SPI
    MXC_SPIMn->gen_ctrl = (MXC_F_SPIM_GEN_CTRL_SPI_MSTR_EN | MXC_F_SPIM_GEN_CTRL_TX_FIFO_EN);

    // configure the flow control
    uint32_t temp = MXC_SPIMn->spcl_ctrl;
    temp &= ~(MXC_F_SPIM_SPCL_CTRL_SS_SA_SDIO_OUT | MXC_F_SPIM_SPCL_CTRL_SS_SA_SDIO_DR_EN);
    temp |=  (MXC_F_SPIM_SPCL_CTRL_SS_SAMPLE_MODE | MXC_F_SPIM_SPCL_CTRL_MISO_FC_EN | (0x1 << MXC_F_SPIM_SPCL_CTRL_SS_SA_SDIO_OUT_POS) | (0x1 << MXC_F_SPIM_SPCL_CTRL_SS_SA_SDIO_DR_EN_POS));
    MXC_SPIMn->spcl_ctrl = temp;

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
    MXC_SPIMn_FIFO->trans_16[0] = header.header;

    // Clear interrupts
    MXC_SPIMn->intfl = ~0;

    // Write type and first data byte
    MXC_SPIMn_FIFO->trans_16[0] = ((uint16_t)*pData << 8) | type;
    pData += 1;
    writeSent += 2;

    writePtr = (uint16_t*)pData;

    /* Write remaining data until the FIFO is full.
     * Note: Since the FIFO write size is 16-bits, an extra byte may be written
     * to the FIFO if the message is not an even length. The byte will not
     * actually be sent.
     */
    while ( (writeSent < writeLen) && (writeSent & 0x1F) && (((MXC_SPIMn->fifo_ctrl & MXC_F_SPIM_FIFO_CTRL_TX_FIFO_USED) >> MXC_F_SPIM_FIFO_CTRL_TX_FIFO_USED_POS) < (MXC_CFG_SPIM_FIFO_DEPTH * 2)) ) {
        MXC_SPIMn_FIFO->trans_16[0] = *writePtr;
        writePtr++;
        writeSent += 2;
    }

    if (writeSent < writeLen) {
        uint32_t temp = MXC_SPIMn->fifo_ctrl;
        temp &= ~MXC_F_SPIM_FIFO_CTRL_TX_FIFO_AE_LVL;
        temp |= (0x1 << MXC_F_SPIM_FIFO_CTRL_TX_FIFO_AE_LVL_POS);
        MXC_SPIMn->fifo_ctrl = temp;

        // enable interrupt
        MXC_SPIMn->inten |= MXC_F_SPIM_INTEN_TX_FIFO_AE;
    } else {
        /* free buffer */
        WsfMsgFree(writeData);
        writeData = NULL;
        MXC_SPIMn->inten |= MXC_F_SPIM_INTEN_TX_READY;
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
    if (!GPIO_IntStatus(&hci_irq)) {
        MXC_SPIMn->gen_ctrl = 0;
        reading = 0;
        GPIO_IntEnable(&hci_irq);  // enable new reads
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

    uint32_t temp = MXC_SPIMn->fifo_ctrl;
    temp &= ~MXC_F_SPIM_FIFO_CTRL_RX_FIFO_AF_LVL;

    if (len >= MAX_SPI_PACKET) {
        temp |= (((MXC_CFG_SPIM_FIFO_DEPTH * 2) - 4) << MXC_F_SPIM_FIFO_CTRL_RX_FIFO_AF_LVL_POS);
    } else {
        temp |= ((len - 1) << MXC_F_SPIM_FIFO_CTRL_RX_FIFO_AF_LVL_POS);
    }

    MXC_SPIMn->fifo_ctrl = temp;

    // clear and enable interrupts
    MXC_SPIMn->intfl = ~0;
    MXC_SPIMn->inten |= MXC_F_SPIM_INTEN_RX_FIFO_AF;

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
                page_size = MXC_S_SPIM_MSTR_CFG_PAGE_4B;
                header.size = (remaining / 4) & 0x1F;
                remaining -= (remaining & 0xFFFC);
            } else if ((remaining / 8) <= 32) {
                page_size = MXC_S_SPIM_MSTR_CFG_PAGE_8B;
                header.size = (remaining / 8) & 0x1F;
                remaining -= (remaining & 0xFFF8);
            } else if ((remaining / 16) <= 32) {
                page_size = MXC_S_SPIM_MSTR_CFG_PAGE_16B;
                header.size = (remaining / 16) & 0x1F;
                remaining -= (remaining & 0xFFF0);
            } else {
                page_size = MXC_S_SPIM_MSTR_CFG_PAGE_32B;
                header.size = (remaining / 32) & 0x1F;
                remaining -= (remaining & 0xFFE0);
            }

            MXC_SPIMn->mstr_cfg = (MXC_SPIMn->mstr_cfg & ~MXC_F_SPIM_MSTR_CFG_PAGE_SIZE) | (page_size << MXC_F_SPIM_MSTR_CFG_PAGE_SIZE_POS);
        }

        // Deassert the SSEL on the last segment
        header.deassert_ss = ((remaining == 0) && !!last);

        /* Write header to FIFO
         * Note: the assumption is made that the read length would never be 
         * large enough to fill the entire transmit FIFO with headers.
         */
        MXC_SPIMn_FIFO->trans_16[0] = header.header;
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
        MXC_SPIMn_FIFO->trans_16[0] = header.header;

        // write any remaining data until the FIFO is full
        do {
            MXC_SPIMn_FIFO->trans_16[0] = *writePtr;
            writePtr++;
            writeSent += 2;
        } while ( (writeSent < writeLen) && (writeSent & 0x1F) && (((MXC_SPIMn->fifo_ctrl & MXC_F_SPIM_FIFO_CTRL_TX_FIFO_USED) >> MXC_F_SPIM_FIFO_CTRL_TX_FIFO_USED_POS) < (MXC_CFG_SPIM_FIFO_DEPTH * 2)) );
    }

    if (writeSent == writeLen) {
        if (writeData != NULL) {
            /* free buffer */
            WsfMsgFree(writeData);
            writeData = NULL;
        }
        MXC_SPIMn->inten &= ~MXC_F_SPIM_INTEN_TX_FIFO_AE;
        MXC_SPIMn->inten |= MXC_F_SPIM_INTEN_TX_READY;
    }
}

/*************************************************************************************************/
static void spiReadHandler(void)
{
    uint32_t fifo_ctrl;

    while ( (readReceived < readLen) && ((fifo_ctrl = MXC_SPIMn->fifo_ctrl) & MXC_F_SPIM_FIFO_CTRL_RX_FIFO_USED) ) {
        *readPtr = MXC_SPIMn_FIFO->rslts_8[0];

        /* Under some circumstances, reading of the FIFO provides data without advancing the FIFO. Detect
         * and handle the the condition by reading another byte from the FIFO.
         */
        if (MXC_SPIMn->fifo_ctrl == fifo_ctrl) {     /* did the FIFO status change? */
            uint8_t temp = MXC_SPIMn_FIFO->rslts_8[0];         /* read the next byte */
            if (temp != *readPtr) {                 /* is the byte the same as the last? */
                /* this was a false positive. carry on as usual */
                readPtr++;
                readReceived++;
                *readPtr = temp;
            }
        }

        MXC_SPIMn->intfl = MXC_F_SPIM_INTFL_RX_FIFO_AF;    // this flag can only be cleared after the FIFO level is below the threshold
        readPtr++;
        readReceived++;
    }

    if ((readLen > 0) && (readReceived == readLen)) {
        int last = readLast;
        if (last) {
            MXC_SPIMn->gen_ctrl = 0;
            reading = 0;
        }
        uint16_t len = readLen;
        readLen = 0;
        MXC_SPIMn->inten &= ~MXC_F_SPIM_INTEN_RX_FIFO_AF;
        hciTrRxIncoming(len);

        if (last) {
            GPIO_IntEnable(&hci_irq);  // enable new reads
        }
    }
}

/*************************************************************************************************/
void SPIMn_IRQHandler(void)
{
    uint32_t intfl = MXC_SPIMn->intfl & MXC_SPIMn->inten;
    MXC_SPIMn->intfl = intfl;

    if (intfl & MXC_F_SPIM_INTFL_RX_FIFO_AF) {
        spiReadHandler();
    }

    if (intfl & MXC_F_SPIM_INTFL_TX_FIFO_AE) {
        spiWriteHandler();
    }

    if (intfl & MXC_F_SPIM_INTFL_TX_READY) {
        MXC_SPIMn->inten &= ~MXC_F_SPIM_INTEN_TX_READY;
        MXC_SPIMn->gen_ctrl = 0;
        writing = 0;

        // enable reads
        GPIO_IntEnable(&hci_irq);  // enable new reads
    }

    // prevent extra interrupts
    NVIC_ClearPendingIRQ(SPIMn_IRQn);
}

/*************************************************************************************************/
void hciDrvIsr(void *unused)
{
    // Confirm that the IRQ is still active
    if (!GPIO_IntStatus(&hci_irq)) {
        return;
    }

    // disable interrupt to prevent further reads
    GPIO_IntDisable(&hci_irq);

    // set flag to block writes
    reading = 1;

    // clear FIFOs and interrupts
    MXC_SPIMn->gen_ctrl = 0;
    MXC_SPIMn->intfl = ~0;

    // enable SPI
    MXC_SPIMn->gen_ctrl = (MXC_F_SPIM_GEN_CTRL_SPI_MSTR_EN | MXC_F_SPIM_GEN_CTRL_TX_FIFO_EN | MXC_F_SPIM_GEN_CTRL_RX_FIFO_EN);

    // configure the flow control
    uint32_t temp = MXC_SPIMn->spcl_ctrl;
    temp &= ~(MXC_F_SPIM_SPCL_CTRL_MISO_FC_EN | MXC_F_SPIM_SPCL_CTRL_SS_SA_SDIO_OUT | MXC_F_SPIM_SPCL_CTRL_SS_SA_SDIO_DR_EN);
    temp |=  (MXC_F_SPIM_SPCL_CTRL_SS_SAMPLE_MODE | (0x1 << MXC_F_SPIM_SPCL_CTRL_SS_SA_SDIO_DR_EN_POS));
    MXC_SPIMn->spcl_ctrl = temp;

    hciTrRxStart();
}
