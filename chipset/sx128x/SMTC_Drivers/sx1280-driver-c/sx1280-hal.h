/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2015 Semtech

Description: Handling of the node configuration protocol

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis, Matthieu Verdy and Benjamin Boulet
*/
#ifndef __SX1280_HAL_H__
#define __SX1280_HAL_H__

#include "hw.h"

/*!
 * * \brief Define which DIOs are connected 
*/
#define RADIO_DIO1_ENABLE	1
#define RADIO_DIO2_ENABLE	0
#define RADIO_DIO3_ENABLE	0

void SX1280HalWaitOnBusy( void );

void SX1280HalInit( DioIrqHandler **irqHandlers );

void SX1280HalIoInit( void );

/*!
 * \brief Soft resets the radio
 */
void SX1280HalReset( void );

/*!
 * \brief Clears the instruction ram memory block
 */
void SX1280HalClearInstructionRam( void );

/*!
 * \brief Wakes up the radio
 */
void SX1280HalWakeup( void );

/*!
 * \brief Send a command that write data to the radio
 *
 * \param [in]  opcode        Opcode of the command
 * \param [in]  buffer        Buffer to be send to the radio
 * \param [in]  size          Size of the buffer to send
 */
void SX1280HalWriteCommand( RadioCommands_t opcode, uint8_t *buffer, uint16_t size );

/*!
 * \brief Send a command that read data from the radio
 *
 * \param [in]  opcode        Opcode of the command
 * \param [out] buffer        Buffer holding data from the radio
 * \param [in]  size          Size of the buffer
 */
void SX1280HalReadCommand( RadioCommands_t opcode, uint8_t *buffer, uint16_t size );

/*!
 * \brief Write data to the radio memory
 *
 * \param [in]  address       The address of the first byte to write in the radio
 * \param [in]  buffer        The data to be written in radio's memory
 * \param [in]  size          The number of bytes to write in radio's memory
 */
void SX1280HalWriteRegisters( uint16_t address, uint8_t *buffer, uint16_t size );

/*!
 * \brief Write a single byte of data to the radio memory
 *
 * \param [in]  address       The address of the first byte to write in the radio
 * \param [in]  value         The data to be written in radio's memory
 */
void SX1280HalWriteRegister( uint16_t address, uint8_t value );

/*!
 * \brief Read data from the radio memory
 *
 * \param [in]  address       The address of the first byte to read from the radio
 * \param [out] buffer        The buffer that holds data read from radio
 * \param [in]  size          The number of bytes to read from radio's memory
 */
void SX1280HalReadRegisters( uint16_t address, uint8_t *buffer, uint16_t size );

/*!
 * \brief Read a single byte of data from the radio memory
 *
 * \param [in]  address       The address of the first byte to write in the
     *                            radio
 *
 * \retval      value         The value of the byte at the given address in
     *                            radio's memory
 */
uint8_t SX1280HalReadRegister( uint16_t address );

/*!
 * \brief Write data to the buffer holding the payload in the radio
 *
 * \param [in]  offset        The offset to start writing the payload
 * \param [in]  buffer        The data to be written (the payload)
 * \param [in]  size          The number of byte to be written
 */
void SX1280HalWriteBuffer( uint8_t offset, uint8_t *buffer, uint8_t size );

/*!
 * \brief Read data from the buffer holding the payload in the radio
 *
 * \param [in]  offset        The offset to start reading the payload
 * \param [out] buffer        A pointer to a buffer holding the data from the radio
 * \param [in]  size          The number of byte to be read
 */
void SX1280HalReadBuffer( uint8_t offset, uint8_t *buffer, uint8_t size );

/*!
 * \brief Returns the status of DIOs pins
 *
 * \retval      dioStatus     A byte where each bit represents a DIO state:
 *                            [ DIOx | BUSY ]
 */
uint8_t SX1280HalGetDioStatus( void );

void SX1280HalIoIrqInit( DioIrqHandler **irqHandlers );

#endif // __SX1280_HAL_H__
