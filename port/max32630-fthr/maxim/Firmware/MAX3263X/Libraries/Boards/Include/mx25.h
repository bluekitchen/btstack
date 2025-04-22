/**
 * @file
 * @brief BSP driver to communicate via SPI/QPI with an MX25 Serial Flash Memory.
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
 * $Date: 2016-10-27 15:56:53 -0500 (Thu, 27 Oct 2016) $
 * $Revision: 24842 $
 *
 **************************************************************************** */

/* Define to prevent redundant inclusion */
#ifndef _MX25_H_
#define _MX25_H_

/* **** Includes **** */
#include "mxc_config.h"
#include "mxc_sys.h"
#include "spim.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup bsp
 * @defgroup mx25_driver MX25 SPI Multi-I/O Flash Memory Driver
 * @{
 */
/* **** Definitions **** */
#define MX25_READ_DUMMY             8			/**< Dummy byte sent on a standard read command per the MX25 datasheet. 		*/
#define MX25_QREAD_DUMMY            6			/**< Dummy data sent on a fast-read (Quad) read command per the MX25 datasheet. */

#define MX25_WIP_MASK               0x01         /**< Status Register 				*/
#define MX25_WEL_MASK               0x02         /**< Write Enable Latch mask 		*/
#define MX25_QE_MASK                0x40         /**< Quad-SPI enable mask 			*/

/**
 * @ingroup mx25_driver
 * @defgroup MX25_Commands MX25 SPI Command Definitions
 * @{
 */
#define MX25_CMD_RST_EN             0x66        /**< Reset Enable 	   				*/
#define MX25_CMD_RST_MEM            0x99        /**< Reset Memory 	   				*/
#define MX25_CMD_ID                 0x9F        /**< ID 							*/
#define MX25_CMD_WRITE_EN           0x06        /**< Write Enable 	   				*/
#define MX25_CMD_WRITE_DIS          0x04        /**< Write Disable 	   				*/

#define MX25_CMD_READ               0x0B      	/**< Read 							*/
#define MX25_CMD_QREAD              0xEB      	/**< Quad SPI Read 	   				*/
#define MX25_CMD_HPM                0xA3      	/**< Hardware Protection Mode 		*/

#define MX25_CMD_READ_SR            0x05        /**< Read Status Register 			*/
#define MX25_CMD_WRITE_SR           0x01		/**< Write Status Register 			*/

#define MX25_CMD_PPROG              0x02 		/**< Page Program 					*/
#define MX25_CMD_QUAD_PROG          0X38		/**< Quad (4 x I/O) Page Program 	*/

#define MX25_CMD_4K_ERASE           0x20 		/**< Page Erase 					*/
#define MX25_CMD_32K_ERASE          0x52		/**< Sector Type 2 (32KB) Erase 	*/		
#define MX25_CMD_64K_ERASE          0xD8		/**< Sector Type 3 (64KB) Erase 	*/
#define MX25_CMD_BULK_ERASE         0xC7		/**< Bulk Erase 					*/
/**@} end of group mx25_commands */
/**
 * Enumeration type to select the size for an Erase command. 
 */
typedef enum {
    MX25_ERASE_4K,		/**< 4KB Sector Erase  */
    MX25_ERASE_32K,		/**< 32KB Block Erase */
    MX25_ERASE_64K,		/**< 64KB Block Erase */
}
mx25_erase_t;

/* *** Globals **** */

/* **** Function Prototypes **** */

/**
 * @brief      Initialize SPI configuration and reset n25q
 * @param      _spim  Pointer to the SPI Master peripheral registers for the SPI Master port to use.
 * @param      ssel   Which ssel pin to use.
 */
void MX25_init(mxc_spim_regs_t *_spim, uint8_t ssel);

/**
 * @brief      Reset the MX25 flash memory.
 */
void MX25_reset(void);

/**
 * @brief      Read manufacturer ID.
 * @return     ID of the device.
 */
uint32_t MX25_ID(void);

/**
 * @brief      Enable/Disable the Quad Enable(QE) bit in the status register.
 * @param      enable    @arg @b 1 enables Quad Mode. @arg @b 0 disables Quad Mode.
 * @retval     0         Success
 * @retval     Non-zero  Error condition
 */
int MX25_quad(int enable);

/**
 * @brief Read data out by using 4-wire SPI mode.
 * @param address         Start address to read from
 * @param rx_buf          Pointer to the buffer of receiving data
 * @param rx_len          Size of the data to read
 * @param width           spim_width_t for how many data lines to use
 */
void MX25_read(uint32_t address, uint8_t *rx_buf, uint32_t rx_len,
               spim_width_t width);

/**
 * @brief Program the memory to @p tx_buf and length @p tx_len, applies to both SPI and QPI modes.
 * @details
 *        - SPI mode: All operations are in 4-wire SPI mode.
 *        - QPI mode: All operations are in quad SPI mode.
 * @param address         Start address to program.
 * @param tx_buf          Pointer to the buffer of data to write. 
 * @param tx_len          Size of the data to write.
 * @param width           #spim_width_t for how many data lines to use.
 */
void MX25_program_page(uint32_t address, const uint8_t *tx_buf, uint32_t tx_len,
                       spim_width_t width);

/**
 * @brief      Bulk erase the MX25 flash memory.
 * @warning    Bulk erase typically takes between 100 to 150 seconds.
 */
void MX25_bulk_erase(void);

/**
 * @brief      Erase memory segments
 * @param      address  Start address to begin erasing.
 * @param      size     Size to erase, see #mx25_erase_t.
 */
void MX25_erase(uint32_t address, mx25_erase_t size);

/**
 * @brief      Read status register.
 * @param      buf   Pointer to store the value of the status register.
 */
void MX25_read_SR(uint8_t* buf);

/**
 * @brief      Write status register
 * @param      value  Value to write to the status register.
 */
void MX25_write_SR(uint8_t value);

/**@} end of group mx25_driver */
#ifdef __cplusplus
}
#endif

#endif /* _MX25_H_ */
