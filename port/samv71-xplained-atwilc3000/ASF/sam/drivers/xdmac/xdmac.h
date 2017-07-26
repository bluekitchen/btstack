/**
 * \file
 *
 * \brief SAM XDMA Controller (DMAC) driver.
 *
 * Copyright (c) 2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#ifndef XDMAC_H_INCLUDED
#define XDMAC_H_INCLUDED

/**
 * \defgroup asfdoc_sam_drivers_xdmac_group SAMV71/V70/E70/S70 XDMA Controller (XDMAC) Driver
 *
 * This driver for Atmel&reg; | SMART SAM XDMA Controller (XDMAC) is a AHB-protocol central
 * direct memory access controller. It performs peripheral data transfer and memory move operations
 * over one or two bus ports through the unidirectional communication channel.
 * This is a driver for the configuration, enabling, disabling, and use of the XDMAC peripheral.
 *
 * Devices from the following series can use this module:
 * - Atmel | SMART SAMV71
 * - Atmel | SMART SAMV70
 * - Atmel | SMART SAMS70
 * - Atmel | SMART SAME70
 *
 * The outline of this documentation is as follows:
 *  - \ref asfdoc_sam_drivers_xdmac_prerequisites
 *  - \ref asfdoc_sam_drivers_xdmac_module_overview
 *  - \ref asfdoc_sam_drivers_xdmac_special_considerations
 *  - \ref asfdoc_sam_drivers_xdmac_extra_info
 *  - \ref asfdoc_sam_drivers_xdmac_examples
 *  - \ref asfdoc_sam_drivers_xdmac_api_overview
 *
 *
 * \section asfdoc_sam_drivers_xdmac_prerequisites Prerequisites
 *
 * There are no prerequisites for this module.
 *
 *
 * \section asfdoc_sam_drivers_xdmac_module_overview Module Overview
 * The DMA Controller (XDMAC) is a AHB-protocol central direct memory access controller.
 * It performs peripheral data transfer and memory move operations over one or two bus ports
 * through the unidirectional communication channel. Each channel is fully programmable and
 * provides both peripheral or memory to memory transfer. The channel features are configurable
 * at implementation time.
 *
 *
 * \section asfdoc_sam_drivers_xdmac_special_considerations Special Considerations
 * There are no special considerations for this module.
 *
 *
 * \section asfdoc_sam_drivers_xdmac_extra_info Extra Information
 *
 * For extra information, see \ref asfdoc_sam_drivers_xdmac_extra. This includes:
 *  - \ref asfdoc_sam_drivers_xdmac_extra_acronyms
 *  - \ref asfdoc_sam_drivers_xdmac_extra_dependencies
 *  - \ref asfdoc_sam_drivers_xdmac_extra_errata
 *  - \ref asfdoc_sam_drivers_xdmac_extra_history
 *
 * \section asfdoc_sam_drivers_xdmac_examples Examples
 *
 * For a list of examples related to this driver, see
 * \ref asfdoc_sam_drivers_xdmac_exqsg.
 *
 *
 * \section asfdoc_sam_drivers_xdmac_api_overview API Overview
 * @{
 */

#include  <compiler.h>
#include  <status_codes.h>

/** @cond */
/**INDENT-OFF**/
#ifdef __cplusplus
extern "C" {
#endif
/**INDENT-ON**/
/** @endcond */

/** DMA channel hardware interface number */
#define XDAMC_CHANNEL_HWID_HSMCI       0
#define XDAMC_CHANNEL_HWID_SPI0_TX     1
#define XDAMC_CHANNEL_HWID_SPI0_RX     2
#define XDAMC_CHANNEL_HWID_SPI1_TX     3
#define XDAMC_CHANNEL_HWID_SPI1_RX     4
#define XDAMC_CHANNEL_HWID_QSPI_TX     5
#define XDAMC_CHANNEL_HWID_QSPI_RX     6
#define XDAMC_CHANNEL_HWID_USART0_TX   7
#define XDAMC_CHANNEL_HWID_USART0_RX   8
#define XDAMC_CHANNEL_HWID_USART1_TX   9
#define XDAMC_CHANNEL_HWID_USART1_RX   10
#define XDAMC_CHANNEL_HWID_USART2_TX   11
#define XDAMC_CHANNEL_HWID_USART2_RX   12
#define XDAMC_CHANNEL_HWID_PWM0        13
#define XDAMC_CHANNEL_HWID_TWIHS0_TX   14
#define XDAMC_CHANNEL_HWID_TWIHS0_RX   15
#define XDAMC_CHANNEL_HWID_TWIHS1_TX   16
#define XDAMC_CHANNEL_HWID_TWIHS1_RX   17
#define XDAMC_CHANNEL_HWID_TWIHS2_TX   18
#define XDAMC_CHANNEL_HWID_TWIHS2_RX   19
#define XDAMC_CHANNEL_HWID_UART0_TX    20
#define XDAMC_CHANNEL_HWID_UART0_RX    21
#define XDAMC_CHANNEL_HWID_UART1_TX    22
#define XDAMC_CHANNEL_HWID_UART1_RX    23
#define XDAMC_CHANNEL_HWID_UART2_TX    24
#define XDAMC_CHANNEL_HWID_UART2_RX    25
#define XDAMC_CHANNEL_HWID_UART3_TX    26
#define XDAMC_CHANNEL_HWID_UART3_RX    27
#define XDAMC_CHANNEL_HWID_UART4_TX    28
#define XDAMC_CHANNEL_HWID_UART4_RX    29
#define XDAMC_CHANNEL_HWID_DAC         30
#define XDAMC_CHANNEL_HWID_SSC_TX      32
#define XDAMC_CHANNEL_HWID_SSC_RX      33
#define XDAMC_CHANNEL_HWID_PIOA        34
#define XDAMC_CHANNEL_HWID_AFEC0       35
#define XDAMC_CHANNEL_HWID_AFEC1       36
#define XDAMC_CHANNEL_HWID_AES_TX      37
#define XDAMC_CHANNEL_HWID_AES_RX      38
#define XDAMC_CHANNEL_HWID_PWM1        39
#define XDAMC_CHANNEL_HWID_TC0         40
#define XDAMC_CHANNEL_HWID_TC1         41
#define XDAMC_CHANNEL_HWID_TC2         42
#define XDAMC_CHANNEL_HWID_TC3         43

/* XDMA_MBR_UBC */
#define   XDMAC_UBC_NDE            (0x1u << 24)
#define   XDMAC_UBC_NDE_FETCH_DIS  (0x0u << 24)
#define   XDMAC_UBC_NDE_FETCH_EN   (0x1u << 24)
#define   XDMAC_UBC_NSEN           (0x1u << 25)
#define   XDMAC_UBC_NSEN_UNCHANGED (0x0u << 25)
#define   XDMAC_UBC_NSEN_UPDATED   (0x1u << 25)
#define   XDMAC_UBC_NDEN           (0x1u << 26)
#define   XDMAC_UBC_NDEN_UNCHANGED (0x0u << 26)
#define   XDMAC_UBC_NDEN_UPDATED   (0x1u << 26)
#define   XDMAC_UBC_NVIEW_Pos      27
#define   XDMAC_UBC_NVIEW_Msk      (0x3u << XDMAC_UBC_NVIEW_Pos)
#define   XDMAC_UBC_NVIEW_NDV0     (0x0u << XDMAC_UBC_NVIEW_Pos)
#define   XDMAC_UBC_NVIEW_NDV1     (0x1u << XDMAC_UBC_NVIEW_Pos)
#define   XDMAC_UBC_NVIEW_NDV2     (0x2u << XDMAC_UBC_NVIEW_Pos)
#define   XDMAC_UBC_NVIEW_NDV3     (0x3u << XDMAC_UBC_NVIEW_Pos)
#define   XDMAC_UBC_UBLEN_Pos 0
#define   XDMAC_UBC_UBLEN_Msk (0xffffffu << XDMAC_UBC_UBLEN_Pos)
#define   XDMAC_UBC_UBLEN(value) ((XDMAC_UBC_UBLEN_Msk & ((value) << XDMAC_UBC_UBLEN_Pos)))

/** XDMA config register for channel */
typedef struct {
	/** Microblock Control Member. */
	uint32_t mbr_ubc;
	/** Source Address Member. */
	uint32_t mbr_sa;
	/** Destination Address Member. */
	uint32_t mbr_da;
	/** Configuration Register. */
	uint32_t mbr_cfg;
	/** Block Control Member. */
	uint32_t mbr_bc;
	/** Data Stride Member. */
	uint32_t mbr_ds;
	/** Source Microblock Stride Member. */
	uint32_t mbr_sus;
	/** Destination Microblock Stride Member. */
	uint32_t mbr_dus;
} xdmac_channel_config_t;

/**
 * \brief Structure for storing parameters for DMA view0 that can be
 * performed by the DMA Master transfer.
 */
typedef struct {
	/** Next Descriptor Address number. */
	uint32_t mbr_nda;
	/** Microblock Control Member. */
	uint32_t mbr_ubc;
	/** Destination Address Member. */
	uint32_t mbr_da;
} lld_view0;

/**
 * \brief Structure for storing parameters for DMA view1 that can be
 * performed by the DMA Master transfer.
 */
typedef struct {
	/** Next Descriptor Address number. */
	uint32_t mbr_nda;
	/** Microblock Control Member. */
	uint32_t mbr_ubc;
	/** Source Address Member. */
	uint32_t mbr_sa;
	/** Destination Address Member. */
	uint32_t mbr_da;
} lld_view1;

/**
 * \brief Structure for storing parameters for DMA view2 that can be
 * performed by the DMA Master transfer.
 */
typedef struct {
	/** Next Descriptor Address number. */
	uint32_t mbr_nda;
	/** Microblock Control Member. */
	uint32_t mbr_ubc;
	/** Source Address Member. */
	uint32_t mbr_sa;
	/** Destination Address Member. */
	uint32_t mbr_da;
	/** Configuration Register. */
	uint32_t mbr_cfg;
} lld_view2;

/**
 * \brief Structure for storing parameters for DMA view3 that can be
 * performed by the DMA Master transfer.
 */
typedef struct {
	/** Next Descriptor Address number. */
	uint32_t mbr_nda;
	/** Microblock Control Member. */
	uint32_t mbr_ubc;
	/** Source Address Member. */
	uint32_t mbr_sa;
	/** Destination Address Member. */
	uint32_t mbr_da;
	/** Configuration Register. */
	uint32_t mbr_cfg;
	/** Block Control Member. */
	uint32_t mbr_bc;
	/** Data Stride Member. */
	uint32_t mbr_ds;
	/** Source Microblock Stride Member. */
	uint32_t mbr_sus;
	/** Destination Microblock Stride Member. */
	uint32_t mbr_dus;
} lld_view3;

/**
 * \brief Get XDMAC global type.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 */
static inline uint32_t xdmac_get_type( Xdmac *xdmac)
{
	Assert(xdmac);
	return xdmac->XDMAC_GTYPE;
}

/**
 * \brief Get XDMAC global configuration.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 */
static inline uint32_t xdmac_get_config(Xdmac *xdmac)
{
	Assert(xdmac);
	return xdmac->XDMAC_GCFG;
}

/**
 * \brief Get XDMAC global weighted arbiter configuration.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 */
static inline uint32_t xdmac_get_arbiter(Xdmac *xdmac)
{
	Assert(xdmac);
	return xdmac->XDMAC_GWAC;
}

/**
 * \brief Enables XDMAC global interrupt.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] mask A bitmask of channels to be enabled interrupt.
 */
static inline void xdmac_enable_interrupt(Xdmac *xdmac, uint32_t mask)
{
	Assert(xdmac);
	xdmac->XDMAC_GIE = ( XDMAC_GIE_IE0 << mask) ;
}

/**
 * \brief Disables XDMAC global interrupt
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] mask A bitmask of channels to be disabled interrupt.
 */
static inline void xdmac_disable_interrupt(Xdmac *xdmac, uint32_t mask)
{
	Assert(xdmac);
	xdmac->XDMAC_GID = (XDMAC_GID_ID0 << mask);
}

/**
 * \brief Get XDMAC global interrupt mask.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 */
static inline uint32_t xdmac_get_interrupt_mask(Xdmac *xdmac)
{
	Assert(xdmac);
	return (xdmac->XDMAC_GIM);
}

/**
 * \brief Get XDMAC global interrupt status.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 */
static inline uint32_t xdmac_get_interrupt_status(Xdmac *xdmac)
{
	Assert(xdmac);
	return (xdmac->XDMAC_GIS);
}

/**
 * \brief enables the relevant channel of given XDMAC.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in]  channel_num  XDMA Channel number (range 0 to 23)
 */
static inline void xdmac_channel_enable(Xdmac *xdmac, uint32_t channel_num)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_GE = (XDMAC_GE_EN0 << channel_num);
}

/**
 * \brief Disables the relevant channel of given XDMAC.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in]  channel_num  XDMA Channel number (range 0 to 23)
 */
static inline void xdmac_channel_disable(Xdmac *xdmac, uint32_t channel_num)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_GD =(XDMAC_GD_DI0 << channel_num);
}

/**
 * \brief Get Global channel status of given XDMAC.
 * \note: When set to 1, this bit indicates that the channel x is enabled.
          If a channel disable request is issued, this bit remains asserted
          until pending transaction is completed.
 * \param[out] xdmac Module hardware register base address pointer.
 */
static inline uint32_t xdmac_channel_get_status(Xdmac *xdmac)
{
	Assert(xdmac);
	return xdmac->XDMAC_GS;
}

/**
 * \brief Suspend the relevant channel's read.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] channel_num  XDMA Channel number (range 0 to 23).
 */
static inline void xdmac_channel_read_suspend(Xdmac *xdmac, uint32_t channel_num)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_GRS |= XDMAC_GRS_RS0 << channel_num;
}

/**
 * \brief Suspend the relevant channel's write.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] channel_num  XDMA Channel number (range 0 to 23).
 */
static inline void xdmac_channel_write_suspend(Xdmac *xdmac, uint32_t channel_num)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_GWS |= XDMAC_GWS_WS0 << channel_num;
}

/**
 * \brief Suspend the relevant channel's read & write.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] channel_num  XDMA Channel number (range 0 to 23).
 */
static inline void xdmac_channel_readwrite_suspend(Xdmac *xdmac, uint32_t channel_num)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_GRWS = (XDMAC_GRWS_RWS0 << channel_num);
}

/**
 * \brief Resume the relevant channel's read & write.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] channel_num  XDMA Channel number (range 0 to 23).
 */
static inline void xdmac_channel_readwrite_resume(Xdmac *xdmac, uint32_t channel_num)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_GRWR = (XDMAC_GRWR_RWR0 << channel_num);
}

/**
 * \brief Set software transfer request on the relevant channel.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] channel_num  XDMA Channel number (range 0 to 23).
 */
static inline void xdmac_channel_software_request(Xdmac *xdmac, uint32_t channel_num)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_GSWR = (XDMAC_GSWR_SWREQ0 << channel_num);
}

/**
 * \brief Get software transfer status of the relevant channel.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 */
static inline uint32_t xdmac_get_software_request_status(Xdmac *xdmac)
{
	Assert(xdmac);
	return xdmac->XDMAC_GSWS;
}

/**
 * \brief Enable interrupt with mask on the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] channel_num  XDMA Channel number (range 0 to 23).
 * \param[in] mask Interrupt mask.
 */
static inline void xdmac_channel_enable_interrupt(Xdmac *xdmac, uint32_t channel_num, uint32_t mask)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CIE = mask;
}

/**
 * \brief Disable interrupt with mask on the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] channel_num  XDMA Channel number (range 0 to 23).
 * \param[in] mask Interrupt mask.
 */
static inline void xdmac_channel_disable_interrupt(Xdmac *xdmac, uint32_t channel_num, uint32_t mask)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CID = mask;
}

/**
 * \brief Get interrupt mask for the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] channel_num  XDMA Channel number (range 0 to 23).
 */
static inline uint32_t xdmac_channel_get_interrupt_mask(Xdmac *xdmac, uint32_t channel_num)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	return xdmac->XDMAC_CHID[channel_num].XDMAC_CIM;
}

/**
 * \brief Get interrupt status for the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] channel_num  XDMA Channel number (range 0 to 23).
 */
static inline uint32_t xdmac_channel_get_interrupt_status(Xdmac *xdmac, uint32_t channel_num)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	return xdmac->XDMAC_CHID[channel_num].XDMAC_CIS;
}

/**
 * \brief Set software flush request on the relevant channel.
 *
 * \param[out] xdmac Module hardware register base address pointer.
 * \param[in] channel_num  XDMA Channel number (range 0 to 23).
 */
static inline void xdmac_channel_software_flush_request(Xdmac *xdmac, uint32_t channel_num)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_GSWF = (XDMAC_GSWF_SWF0 << channel_num);
	while( !(xdmac_channel_get_interrupt_status(xdmac, channel_num) & XDMAC_CIS_FIS) );
}

/**
 * \brief Set source address for the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer
 * \param[in] channel_num  DMA Channel number (range 0 to 23)
 * \param[in] src_addr Source address
 */
static inline void xdmac_channel_set_source_addr(Xdmac *xdmac, uint32_t channel_num, uint32_t src_addr)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CSA = src_addr;
}

/**
 * \brief Set destination address for the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer
 * \param[in] channel_num  DMA Channel number (range 0 to 23)
 * \param[in] dst_addr Destination address
 */
static inline void xdmac_channel_set_destination_addr(Xdmac *xdmac, uint32_t channel_num, uint32_t dst_addr)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CDA = dst_addr;
}

/**
 * \brief Set next descriptor's address & interface for the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer
 * \param[in] channel_num  DMA Channel number (range 0 to 23)
 * \param[in] desc_addr Address of next descriptor.
 * \param[in] ndaif Interface of next descriptor.
 */
static inline void xdmac_channel_set_descriptor_addr(Xdmac *xdmac, uint32_t channel_num,
		uint32_t desc_addr, uint8_t ndaif)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	Assert(ndaif<2);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CNDA = (desc_addr & 0xFFFFFFFC) | ndaif;
}

/**
 * \brief Set next descriptor's configuration for the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer
 * \param[in] channel_num  DMA Channel number (range 0 to 23)
 * \param[in] config Configuration of next descriptor.
 */
static inline void xdmac_channel_set_descriptor_control(Xdmac *xdmac, uint32_t channel_num, uint32_t config)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CNDC = config;
}

/**
 * \brief Set microblock length for the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer
 * \param[in] channel_num  DMA Channel number (range 0 to 23)
 * \param[in] ublen Microblock length.
 */
static inline void xdmac_channel_set_microblock_control(Xdmac *xdmac, uint32_t channel_num, uint32_t ublen)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CUBC = XDMAC_CUBC_UBLEN(ublen);
}

/**
 * \brief Set block length for the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer
 * \param[in] channel_num  XDMA Channel number (range 0 to 23)
 * \param[in] blen Block length.
 */
static inline void xdmac_channel_set_block_control(Xdmac *xdmac, uint32_t channel_num, uint32_t blen)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CBC = XDMAC_CBC_BLEN(blen);
}

/**
 * \brief Set configuration for the relevant channel of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer
 * \param[in] channel_num  XDMA Channel number (range 0 to 23)
 * \param[in] config Channel configuration.
 */
static inline void xdmac_channel_set_config(Xdmac *xdmac, uint32_t channel_num, uint32_t config)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CC = config;
}

/**
 * \brief Set the relevant channel's data stride memory pattern of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer
 * \param[in] channel_num  XDMA Channel number (range 0 to 23)
 * \param[in] dds_msp Data stride memory pattern.
 */
static inline void xdmac_channel_set_datastride_mempattern(Xdmac *xdmac, uint32_t channel_num, uint32_t dds_msp)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CDS_MSP = dds_msp;
}

/**
 * \brief Set the relevant channel's source microblock stride of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer
 * \param[in] channel_num  XDMA Channel number (range 0 to 23)
 * \param[in] subs Source microblock stride.
 */
static inline void xdmac_channel_set_source_microblock_stride(Xdmac *xdmac,
		uint32_t channel_num, uint32_t subs)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CSUS = XDMAC_CSUS_SUBS(subs);
}

/**
 * \brief Set the relevant channel's destination microblock stride of given XDMA.
 *
 * \param[out] xdmac Module hardware register base address pointer
 * \param[in] channel_num  XDMA Channel number (range 0 to 23)
 * \param[in] dubs Destination microblock stride.
 */
static inline void xdmac_channel_set_destination_microblock_stride(Xdmac *xdmac,
		uint32_t channel_num, uint32_t dubs)
{
	Assert(xdmac);
	Assert(channel_num < XDMACCHID_NUMBER);
	xdmac->XDMAC_CHID[channel_num].XDMAC_CDUS = XDMAC_CDUS_DUBS(dubs);
}

void xdmac_configure_transfer(Xdmac *xdmac, uint32_t channel_num,
		xdmac_channel_config_t *p_cfg);

/** @cond */
/**INDENT-OFF**/
#ifdef __cplusplus
}
#endif
/**INDENT-ON**/
/** @endcond */

 /** @} */

/**
 * \page asfdoc_sam_drivers_xdmac_extra Extra Information for Extensible Direct Memory Access Controller Driver
 *
 * \section asfdoc_sam_drivers_xdmac_extra_acronyms Acronyms
 * Below is a table listing the acronyms used in this module, along with their
 * intended meanings.
 *
 * <table>
 *  <tr>
 *      <th>Acronym</th>
 *      <th>Definition</th>
 *  </tr>
 *  <tr>
 *      <td>AHB</td>
 *      <td>AMBA High-performance Bus</td>
 * </tr>
 *  <tr>
 *      <td>AMBA</td>
 *      <td>Advanced Microcontroller Bus Architecture</td>
 * </tr>
 *  <tr>
 *      <td>FIFO</td>
 *      <td>First In First Out</td>
 * </tr>
 *  <tr>
 *      <td>LLD</td>
 *      <td>Linked List Descriptor</td>
 * </tr>
 *  <tr>
 *      <td>QSG</td>
 *      <td>Quick Start Guide</td>
 * </tr>
 * </table>
 *
 *
 * \section asfdoc_sam_drivers_xdmac_extra_dependencies Dependencies
 * This driver has the following dependencies:
 *
 *  - None
 *
 *
 * \section asfdoc_sam_drivers_xdmac_extra_errata Errata
 * There are no errata related to this driver.
 *
 *
 * \section asfdoc_sam_drivers_xdmac_extra_history Module History
 * An overview of the module history is presented in the table below, with
 * details on the enhancements and fixes made to the module since its first
 * release. The current version of this corresponds to the newest version in
 * the table.
 *
 * <table>
 * <tr>
 *  <th>Changelog</th>
 * </tr>
 * <tr>
 *  <td>Initial document release</td>
 * </tr>
 * </table>
 */

/**
 * \page asfdoc_sam_drivers_xdmac_exqsg Examples for Direct Memory Access Controller Driver
 *
 * This is a list of the available Quick Start Guides (QSGs) and example
 * applications for \ref asfdoc_sam_drivers_xdmac_group. QSGs are simple examples with
 * step-by-step instructions to configure and use this driver in a selection of
 * use cases. Note that QSGs can be compiled as a standalone application or be
 * added to the user application.
 *
 *  - \subpage asfdoc_sam_drivers_xdmac_qsg
 *  - \subpage asfdoc_sam_drivers_xdmac_example
 *
 * \page asfdoc_sam_drivers_xdmac_document_revision_history Document Revision History
 *
 * <table>
 * <tr>
 *  <th>Doc. Rev.</td>
 *  <th>Date</td>
 *  <th>Comments</td>
 * </tr>
 * <tr>
 *  <td>XXXXXA</td>
 *  <td>08/2015</td>
 *  <td>Initial document release</td>
 * </tr>
 * </table>
 *
 */

 /**
 * \page asfdoc_sam_drivers_xdmac_qsg Quick Start Guide for the XDMAC driver
 *
 * This is the quick start guide for the \ref asfdoc_sam_drivers_xdmac_group, with
 * step-by-step instructions on how to configure and use the driver for
 * a specific use case.The code examples can be copied into e.g the main
 * application loop or any other function that will need to control the
 * XDMAC module.
 *
 * \section asfdoc_sam_drivers_xdmac_qsg_use_cases Use Cases
 * - \ref asfdoc_sam_drivers_xdmac_qsg_basic
 *
 * \section asfdoc_sam_drivers_xdmac_qsg_basic XDMAC Basic Usage
 *
 * This use case will demonstrate how to config the XDMAC module to
 * perform a single memory to memory transfer.
 *
 *
 * \section asfdoc_sam_drivers_xdmac_qsg_basic_setup Setup Steps
 *
 * \subsection asfdoc_sam_drivers_xdmac_qsg_basic_prereq Prerequisites
 *
 * This module requires the following service
 * - \ref clk_group "System Clock Management (sysclock)"
 *
 * \subsection asfdoc_sam_drivers_xdmac_qsg_basic_setup_code Setup Code
 *
 * Add these macros and global variable to the top of your application's C-file:
 * \snippet xdmac_example.c xdmac_define_channel
 * \snippet xdmac_example.c xdmac_define_buffer
 *
 * Add this to the main loop or a setup function:
  * \snippet xdmac_example.c xdmac_configure_transfer
 *
 * \subsection asfdoc_sam_drivers_xdmac_qsg_basic_setup_workflow Workflow
 *
 * -# Configure XDMAC transfer:
 * \snippet xdmac_example.c xdmac_configure_transfer
 * -# Enable the XDMAC interrupt:
 * \snippet xdmac_example.c xdmac_enable_interrupt
 * \snippet xdmac_example.c xdmac_channel_enable_interrupt
 * -# Eanble the channel:
 * \snippet xdmac_example.c xdmac_channel_enable
 *
 */

#endif /* XDMAC_H_INCLUDED */
