/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2016 Semtech

Description: Handling of the node configuration protocol

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis, Matthieu Verdy and Benjamin Boulet
*/
#include "hw.h"
#include "sx1280-hal.h"
#include "radio.h"
#include <string.h>

// logging on
#include "SEGGER_RTT.h"
#define printf(format, ...) SEGGER_RTT_printf(0, format,  ## __VA_ARGS__)

// make CubeMX defines usable
#ifndef RADIO_BUSY_PORT
#define RADIO_BUSY_PORT RADIO_BUSY_GPIO_Port
#endif
#ifndef RADIO_BUSY_PIN
#define RADIO_BUSY_PIN RADIO_BUSY_Pin
#endif
#ifndef RADIO_nRESET_PORT
#define RADIO_nRESET_PORT RADIO_nRESET_GPIO_Port
#endif
#ifndef RADIO_nRESET_PIN
#define RADIO_nRESET_PIN RADIO_nRESET_Pin
#endif
#ifndef RADIO_NSS_PORT
#define RADIO_NSS_PORT RADIO_NSS_GPIO_Port
#endif
#ifndef RADIO_NSS_PIN
#define RADIO_NSS_PIN RADIO_NSS_Pin
#endif
/*!
 * \brief Define the size of tx and rx hal buffers
 *
 * The Tx and Rx hal buffers are used for SPI communication to
 * store data to be sent/receive to/from the chip.
 *
 * \warning The application must ensure the maximal useful size to be much lower
 *          than the MAX_HAL_BUFFER_SIZE
 */
#define MAX_HAL_BUFFER_SIZE   0xFFF

#define IRQ_HIGH_PRIORITY  0

/*!
 * Radio driver structure initialization
 */
const struct Radio_s Radio =
{
    SX1280Init,
    SX1280HalReset,
    SX1280GetStatus,
    SX1280HalWriteCommand,
    SX1280HalReadCommand,
    SX1280HalWriteRegisters,
    SX1280HalWriteRegister,
    SX1280HalReadRegisters,
    SX1280HalReadRegister,
    SX1280HalWriteBuffer,
    SX1280HalReadBuffer,
    SX1280HalGetDioStatus,
    SX1280GetFirmwareVersion,
    SX1280SetRegulatorMode,
    SX1280SetStandby,
    SX1280SetPacketType,
    SX1280SetModulationParams,
    SX1280SetPacketParams,
    SX1280SetRfFrequency,
    SX1280SetBufferBaseAddresses,
    SX1280SetTxParams,
    SX1280SetDioIrqParams,
    SX1280SetSyncWord,
    SX1280SetRx,
    SX1280GetPayload,
    SX1280SendPayload,
    SX1280SetRangingRole,
    SX1280SetPollingMode,
    SX1280SetInterruptMode,
    SX1280SetRegistersDefault,
    SX1280GetOpMode,
    SX1280SetSleep,
    SX1280SetFs,
    SX1280SetTx,
    SX1280SetRxDutyCycle,
    SX1280SetCad,
    SX1280SetTxContinuousWave,
    SX1280SetTxContinuousPreamble,
    SX1280GetPacketType,
    SX1280SetCadParams,
    SX1280GetRxBufferStatus,
    SX1280GetPacketStatus,
    SX1280GetRssiInst,
    SX1280GetIrqStatus,
    SX1280ClearIrqStatus,
    SX1280Calibrate,
    SX1280SetSaveContext,
    SX1280SetAutoTx,
    SX1280StopAutoTx,
    SX1280SetAutoFS,
    SX1280SetLongPreamble,
    SX1280SetPayload,
    SX1280SetSyncWordErrorTolerance,
    SX1280SetCrcSeed,
    SX1280SetBleAccessAddress,
    SX1280SetBleAdvertizerAccessAddress,
    SX1280SetCrcPolynomial,
    SX1280SetWhiteningSeed,
    SX1280EnableManualGain,
    SX1280DisableManualGain,
    SX1280SetManualGainValue,
    SX1280SetLNAGainSetting,
    SX1280SetRangingIdLength,
    SX1280SetDeviceRangingAddress,
    SX1280SetRangingRequestAddress,
    SX1280GetRangingResult,
    SX1280SetRangingCalibration,
    SX1280GetRangingPowerDeltaThresholdIndicator,
    SX1280RangingClearFilterResult,
    SX1280RangingSetFilterNumSamples,
    SX1280GetFrequencyError,
};

#ifndef USE_BK_SPI
static uint8_t halRxBuffer[MAX_HAL_BUFFER_SIZE] = {0x00};
#endif
static uint8_t halTxBuffer[MAX_HAL_BUFFER_SIZE] = {0x00};

static DioIrqHandler **dioIrqHandlers;

extern SPI_HandleTypeDef RADIO_SPI_HANDLE;

#ifdef USE_BK_SPI

static void spi_tx_then_rx(SPI_HandleTypeDef *hspi, const uint8_t * tx_data, uint16_t tx_len, uint8_t * rx_buffer, uint16_t rx_len){

    /* Set fiforxthreshold according the reception data length: 8bit */
    SET_BIT(hspi->Instance->CR2, SPI_RXFIFO_THRESHOLD);

    /* Check if the SPI is already enabled */
    if ((hspi->Instance->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE)
    {
        /* Enable SPI peripheral */
        __HAL_SPI_ENABLE(hspi);
    }

    // send tx / ignore rx

    uint8_t tx_byte = *tx_data++;
    while (tx_len > 0){
        tx_len--;
        // while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE) == 0);
        *(__IO uint8_t *)&hspi->Instance->DR = tx_byte;
        tx_byte = *tx_data++;
        while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_RXNE) == 0);
        // *rx_buffer++ = *(__IO uint8_t *)&hspi->Instance->DR;
        uint8_t rx_byte = *(__IO uint8_t *)&hspi->Instance->DR;
        (void) rx_byte;
    }

    // send NOP / store rx

    while (rx_len > 0){
        rx_len--;
        // while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE) == 0);
        *(__IO uint8_t *)&hspi->Instance->DR = 0;
        while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_RXNE) == 0);
        *rx_buffer++ = *(__IO uint8_t *)&hspi->Instance->DR;
    }
}
#else
// map onto standard hal functions
void SpiIn( uint8_t *txBuffer, uint16_t size ){
    HAL_SPI_Transmit( &RADIO_SPI_HANDLE, txBuffer, size, HAL_MAX_DELAY );
}
void SpiInOut( uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t size )
{
#ifdef STM32L4XX_FAMILY
    // Comment For STM32L0XX and STM32L1XX Intégration, uncomment for STM32L4XX Intégration
    HAL_SPIEx_FlushRxFifo( &RADIO_SPI_HANDLERADIO_SPI_HANDLE );
#endif
    HAL_SPI_TransmitReceive( &RADIO_SPI_HANDLE, txBuffer, rxBuffer, size, HAL_MAX_DELAY );
}
#endif

/*!
 * \brief Used to block execution waiting for low state on radio busy pin.
 *        Essentially used in SPI communications
 */
void SX1280HalWaitOnBusy( void )
{
    while( HAL_GPIO_ReadPin( RADIO_BUSY_PORT, RADIO_BUSY_PIN ) == 1 );
}

void SX1280HalInit( DioIrqHandler **irqHandlers )
{
    SX1280HalReset( );
    SX1280HalIoIrqInit( irqHandlers );
}

void HAL_GPIO_EXTI_Callback( uint16_t GPIO_Pin )
{
    dioIrqHandlers[0]();
}

void SX1280HalIoIrqInit( DioIrqHandler **irqHandlers )
{
    dioIrqHandlers = irqHandlers;
}

void SX1280HalReset( void )
{
    HAL_Delay( 20 );
    HAL_GPIO_WritePin( RADIO_nRESET_PORT, RADIO_nRESET_PIN, 0 );
    HAL_Delay( 50 );
    HAL_GPIO_WritePin( RADIO_nRESET_PORT, RADIO_nRESET_PIN, 1 );
    HAL_Delay( 20 );
}

void SX1280HalClearInstructionRam( void )
{
    // Clearing the instruction RAM is writing 0x00s on every bytes of the
    // instruction RAM
    uint16_t halSize = 3 + IRAM_SIZE;
    halTxBuffer[0] = RADIO_WRITE_REGISTER;
    halTxBuffer[1] = ( IRAM_START_ADDRESS >> 8 ) & 0x00FF;
    halTxBuffer[2] = IRAM_START_ADDRESS & 0x00FF;
    for( uint16_t index = 0; index < IRAM_SIZE; index++ )
    {
        halTxBuffer[3+index] = 0x00;
    }

    SX1280HalWaitOnBusy( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

#ifdef USE_BK_SPI
    spi_tx_then_rx( &RADIO_SPI_HANDLE, halTxBuffer, halSize, NULL, 0);
#else
    SpiIn( halTxBuffer, halSize );
#endif

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 1 );

    SX1280HalWaitOnBusy( );
}

void SX1280HalWakeup( void )
{
    __disable_irq( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

    uint16_t halSize = 2;
    halTxBuffer[0] = RADIO_GET_STATUS;
    halTxBuffer[1] = 0x00;

#ifdef USE_BK_SPI
    spi_tx_then_rx( &RADIO_SPI_HANDLE, halTxBuffer, halSize, NULL, 0);
#else
    SpiIn( halTxBuffer, halSize );
#endif

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 1 );

    // Wait for chip to be ready.
    SX1280HalWaitOnBusy( );

    __enable_irq( );
}

void SX1280HalWriteCommand( RadioCommands_t command, uint8_t *buffer, uint16_t size )
{
    uint16_t halSize  = size + 1;
    SX1280HalWaitOnBusy( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

    halTxBuffer[0] = command;
    memcpy( halTxBuffer + 1, ( uint8_t * )buffer, size * sizeof( uint8_t ) );

#ifdef USE_BK_SPI
    spi_tx_then_rx( &RADIO_SPI_HANDLE, halTxBuffer, halSize, NULL, 0);
#else
    SpiIn( halTxBuffer, halSize );
#endif

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 1 );

    if( command != RADIO_SET_SLEEP )
    {
        SX1280HalWaitOnBusy( );
    }
}

void SX1280HalReadCommand( RadioCommands_t command, uint8_t *buffer, uint16_t size )
{
    halTxBuffer[0] = command;
    halTxBuffer[1] = 0x00;
#ifndef USE_BK_SPI
    uint16_t halSize = 2 + size;
    for( uint16_t index = 0; index < size; index++ )
    {
        halTxBuffer[2+index] = 0x00;
    }
#endif

    SX1280HalWaitOnBusy( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );


#ifdef USE_BK_SPI
    spi_tx_then_rx( &RADIO_SPI_HANDLE, halTxBuffer, 2, buffer, size);
#else
    SpiInOut( halTxBuffer, halRxBuffer, halSize );
#endif

#ifndef USE_BK_SPI
    memcpy( buffer, halRxBuffer + 2, size );
#endif

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 1 );

    SX1280HalWaitOnBusy( );
}

void SX1280HalWriteRegisters( uint16_t address, uint8_t *buffer, uint16_t size )
{
    uint16_t halSize = size + 3;
    halTxBuffer[0] = RADIO_WRITE_REGISTER;
    halTxBuffer[1] = ( address & 0xFF00 ) >> 8;
    halTxBuffer[2] = address & 0x00FF;
    memcpy( halTxBuffer + 3, buffer, size );

    SX1280HalWaitOnBusy( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

#ifdef USE_BK_SPI
    spi_tx_then_rx( &RADIO_SPI_HANDLE, halTxBuffer, halSize, NULL, 0);
#else
    SpiIn( halTxBuffer, halSize );
#endif

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 1 );

    SX1280HalWaitOnBusy( );
}

void SX1280HalWriteRegister( uint16_t address, uint8_t value )
{
    SX1280HalWriteRegisters( address, &value, 1 );
}

void SX1280HalReadRegisters( uint16_t address, uint8_t *buffer, uint16_t size )
{
    halTxBuffer[0] = RADIO_READ_REGISTER;
    halTxBuffer[1] = ( address & 0xFF00 ) >> 8;
    halTxBuffer[2] = address & 0x00FF;
    halTxBuffer[3] = 0x00;
    for( uint16_t index = 0; index < size; index++ )
    {
        halTxBuffer[4+index] = 0x00;
    }

    SX1280HalWaitOnBusy( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

#ifdef USE_BK_SPI
    spi_tx_then_rx( &RADIO_SPI_HANDLE, halTxBuffer, 4, buffer, size);
#else
    uint16_t halSize = 4 + size;
    SpiInOut( halTxBuffer, halRxBuffer, halSize );
    memcpy( buffer, halRxBuffer + 4, size );
#endif

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 1 );

    SX1280HalWaitOnBusy( );
}

uint8_t SX1280HalReadRegister( uint16_t address )
{
    uint8_t data;

    SX1280HalReadRegisters( address, &data, 1 );

    return data;
}

void SX1280HalWriteBuffer( uint8_t offset, uint8_t *buffer, uint8_t size )
{
    uint16_t halSize = size + 2;
    halTxBuffer[0] = RADIO_WRITE_BUFFER;
    halTxBuffer[1] = offset;
    memcpy( halTxBuffer + 2, buffer, size );

    SX1280HalWaitOnBusy( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

#ifdef USE_BK_SPI
    spi_tx_then_rx( &RADIO_SPI_HANDLE, halTxBuffer, halSize, NULL, 0);
#else
    SpiIn( halTxBuffer, halSize );
#endif

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 1 );

    SX1280HalWaitOnBusy( );
}

void SX1280HalReadBuffer( uint8_t offset, uint8_t *buffer, uint8_t size )
{
    halTxBuffer[0] = RADIO_READ_BUFFER;
    halTxBuffer[1] = offset;
    halTxBuffer[2] = 0x00;
#ifndef USE_BK_SPI
    uint16_t halSize = size + 3;
    for( uint16_t index = 0; index < size; index++ )
    {
        halTxBuffer[3+index] = 0x00;
    }
#endif

    SX1280HalWaitOnBusy( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

#ifdef USE_BK_SPI
    spi_tx_then_rx( &RADIO_SPI_HANDLE, halTxBuffer, 3, buffer, size);
#else
    SpiInOut( halTxBuffer, halRxBuffer, halSize );
    memcpy( buffer, halRxBuffer + 3, size );
#endif

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 1 );

    SX1280HalWaitOnBusy( );
}

uint8_t SX1280HalGetDioStatus( void )
{
	uint8_t Status = HAL_GPIO_ReadPin( RADIO_BUSY_PORT, RADIO_BUSY_PIN );

#if( RADIO_DIO1_ENABLE )
	Status |= (HAL_GPIO_ReadPin( RADIO_DIO1_GPIO_Port, RADIO_DIO1_Pin ) << 1);
#endif
#if( RADIO_DIO2_ENABLE )
	Status |= (HAL_GPIO_ReadPin( RADIO_DIO2_GPIO_Port, RADIO_DIO2_Pin ) << 2);
#endif
#if( RADIO_DIO3_ENABLE )
	Status |= (HAL_GPIO_ReadPin( RADIO_DIO3_GPIO_Port, RADIO_DIO3_Pin ) << 3);
#endif
#if( !RADIO_DIO1_ENABLE && !RADIO_DIO2_ENABLE && !RADIO_DIO3_ENABLE )
#error "Please define a DIO" 
#endif
	
	return Status;
}
