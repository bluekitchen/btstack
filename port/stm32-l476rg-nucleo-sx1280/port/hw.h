
// TODO: get rid of this header
// NOTE: this header merges defines in sx1280.h and sx1280-hal.h, which hides a cyclic dependency between these
// NOTE: includes from this header should be moved to actual c files

#ifndef __HW_H__
#define __HW_H__

#include "stdio.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Define the core */
#include "stm32l4xx_hal.h"
#include "stm32l4xx_it.h"

#include "sx1280.h"
#include "sx1280-hal.h"

#include "main.h"

#define RADIO_SPI_HANDLE hspi1
#define RADIO_SPI_DMA_RX hdma_spi1_rx
#define RADIO_SPI_DMA_TX hdma_spi1_tx

#define USE_BK_SPI

#endif // __HW_H__