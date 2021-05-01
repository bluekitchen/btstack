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

// static uint8_t halRxBuffer[MAX_HAL_BUFFER_SIZE];
static uint8_t halTxBuffer[MAX_HAL_BUFFER_SIZE];
const static uint8_t halZeroBuffer[MAX_HAL_BUFFER_SIZE];
static DioIrqHandler **dioIrqHandlers;

extern SPI_HandleTypeDef RADIO_SPI_HANDLE;
extern DMA_HandleTypeDef RADIO_SPI_DMA_RX;
extern DMA_HandleTypeDef RADIO_SPI_DMA_TX;


#ifdef USE_BK_SPI

static void spi_enable(SPI_HandleTypeDef *hspi){
	/* Set fiforxthreshold according the reception data length: 8bit */
	SET_BIT(hspi->Instance->CR2, SPI_RXFIFO_THRESHOLD);

	/* Check if the SPI is already enabled */
	if ((hspi->Instance->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE)
	{
		/* Enable SPI peripheral */
		__HAL_SPI_ENABLE(hspi);
	}
}

static void spi_tx(SPI_HandleTypeDef *hspi, const uint8_t * tx_data, uint16_t tx_len){
    // send tx / ignore rx
    uint8_t tx_byte = *tx_data++;
    while (tx_len > 0){
        tx_len--;
        // while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE) == 0);
        *(__IO uint8_t *)&hspi->Instance->DR = tx_byte;
        tx_byte = *tx_data++;
        while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_RXNE) == 0);
        uint8_t rx_byte = *(__IO uint8_t *)&hspi->Instance->DR;
        (void) rx_byte;
    }
}

static void spi_rx(SPI_HandleTypeDef *hspi,  uint8_t * rx_buffer, uint16_t rx_len){
    // send NOP / store rx
    while (rx_len > 0){
        rx_len--;
        // while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE) == 0);
        *(__IO uint8_t *)&hspi->Instance->DR = 0;
        while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_RXNE) == 0);
        *rx_buffer++ = *(__IO uint8_t *)&hspi->Instance->DR;
    }
}

static void spi_tx_only_dma(const uint8_t * tx_data, uint16_t tx_len) {

	HAL_DMA_Start(&RADIO_SPI_DMA_TX, (uintptr_t) tx_data, (uintptr_t) &RADIO_SPI_HANDLE.Instance->DR, tx_len);

	/* Enable Tx DMA Request */
	SET_BIT(RADIO_SPI_HANDLE.Instance->CR2, SPI_CR2_TXDMAEN);

	HAL_DMA_PollForTransfer(&RADIO_SPI_DMA_TX, HAL_DMA_FULL_TRANSFER, HAL_MAX_DELAY);

	/* Discard received byte */
	(void) RADIO_SPI_HANDLE.Instance->DR;
}

static void spi_tx_rx_dma(const uint8_t * tx_data, uint8_t * rx_buffer, uint16_t size) {

	/* Enable Rx DMA Request */
	SET_BIT(RADIO_SPI_HANDLE.Instance->CR2, SPI_CR2_RXDMAEN);

	HAL_DMA_Start(&RADIO_SPI_DMA_RX, (uintptr_t) &RADIO_SPI_HANDLE.Instance->DR, (uintptr_t) rx_buffer, size);
	HAL_DMA_Start(&RADIO_SPI_DMA_TX, (uintptr_t) tx_data, (uintptr_t) &RADIO_SPI_HANDLE.Instance->DR, size);

	/* Enable Tx DMA Request */
	SET_BIT(RADIO_SPI_HANDLE.Instance->CR2, SPI_CR2_TXDMAEN);

	HAL_DMA_PollForTransfer(&RADIO_SPI_DMA_TX, HAL_DMA_FULL_TRANSFER, HAL_MAX_DELAY);
	HAL_DMA_PollForTransfer(&RADIO_SPI_DMA_RX, HAL_DMA_FULL_TRANSFER, HAL_MAX_DELAY);
}

#endif

// assert: tx_data == tx_buffer (local call), tx_len > 0
// DMA is disabled as one extra byte is read
void SX1280HalSpiTxThenRx(uint16_t tx_len, uint8_t * rx_buffer, uint16_t rx_len){

	spi_enable(&RADIO_SPI_HANDLE);

	// min size for dma to be faster
	const uint16_t dma_min_size_tx = 100;

	if (tx_len < dma_min_size_tx) {
		// Custom Polling
		spi_tx(&RADIO_SPI_HANDLE, halTxBuffer, tx_len);
	} else {
		// Custom DMA
		spi_tx_only_dma( halTxBuffer, tx_len );
	}

	// 'Flush' Fifo by reading until marked empty
	HAL_SPIEx_FlushRxFifo(&RADIO_SPI_HANDLE);

	if (rx_len == 0) return;

	// min size for dma to be faster
	const uint16_t dma_min_size_rx = 100;

	if (rx_len < dma_min_size_rx){
		// Custom Polling
		spi_rx(&RADIO_SPI_HANDLE, rx_buffer, rx_len);
	} else {
		// Custom DMA
		spi_tx_rx_dma( halZeroBuffer, rx_buffer, rx_len);
	}
}

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
    SX1280HalWakeup();
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

#if 0
// commented out as (3+IRAM_SIZE) > sizeof(halTxBuffer)
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

	SX1280HalSpiTxThenRx( halTxBuffer, halSize, NULL, 0);

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 1 );

    SX1280HalWaitOnBusy( );
}
#endif

void SX1280HalWakeup( void )
{
    __disable_irq( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

    uint16_t halSize = 2;
    halTxBuffer[0] = RADIO_GET_STATUS;
    halTxBuffer[1] = 0x00;

	SX1280HalSpiTxThenRx( halSize, NULL, 0);

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

	SX1280HalSpiTxThenRx( halSize, NULL, 0);

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

    SX1280HalWaitOnBusy( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

	SX1280HalSpiTxThenRx( 2, buffer, size);

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

	SX1280HalSpiTxThenRx( halSize, NULL, 0);

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

    SX1280HalWaitOnBusy( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

	SX1280HalSpiTxThenRx( 4, buffer, size);

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

	SX1280HalSpiTxThenRx( halSize, NULL, 0);

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 1 );

    SX1280HalWaitOnBusy( );
}

void SX1280HalReadBuffer( uint8_t offset, uint8_t *buffer, uint8_t size )
{
    halTxBuffer[0] = RADIO_READ_BUFFER;
    halTxBuffer[1] = offset;
    halTxBuffer[2] = 0x00;

    SX1280HalWaitOnBusy( );

    HAL_GPIO_WritePin( RADIO_NSS_PORT, RADIO_NSS_PIN, 0 );

	SX1280HalSpiTxThenRx( 3, buffer, size);

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
