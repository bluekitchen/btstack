/**
  ******************************************************************************
  * @file    stm32f429i_discovery_sdram.c
  * @author  MCD Application Team
  * @brief   This file provides a set of functions needed to drive the
  *          IS42S16400J SDRAM memory mounted on STM32F429I-Discovery Kit.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f429i_discovery_sdram.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F429I_DISCOVERY
  * @{
  */

/** @defgroup STM32F429I_DISCOVERY_SDRAM STM32F429I DISCOVERY SDRAM
  * @{
  */

/** @defgroup STM32F429I_DISCOVERY_SDRAM_Private_Types_Definitions STM32F429I DISCOVERY SDRAM Private Types Definitions
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_SDRAM_Private_Defines STM32F429I DISCOVERY SDRAM Private Defines
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_SDRAM_Private_Macros STM32F429I DISCOVERY SDRAM Private Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_SDRAM_Private_Variables STM32F429I DISCOVERY SDRAM Private Variables
  * @{
  */
static SDRAM_HandleTypeDef SdramHandle;
static FMC_SDRAM_TimingTypeDef Timing;
static FMC_SDRAM_CommandTypeDef Command;
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_SDRAM_Private_Function_Prototypes STM32F429I DISCOVERY SDRAM Private Function Prototypes
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_SDRAM_Private_Functions STM32F429I DISCOVERY SDRAM Private Functions
  * @{
  */

/**
  * @brief  Initializes the SDRAM device.
  */
uint8_t BSP_SDRAM_Init(void)
{
  static uint8_t sdramstatus = SDRAM_ERROR;

  /* SDRAM device configuration */
  SdramHandle.Instance = FMC_SDRAM_DEVICE;

  /* FMC Configuration -------------------------------------------------------*/
  /* FMC SDRAM Bank configuration */
  /* Timing configuration for 90 Mhz of SD clock frequency (180Mhz/2) */
  /* TMRD: 2 Clock cycles */
  Timing.LoadToActiveDelay    = 2;
  /* TXSR: min=70ns (7x11.11ns) */
  Timing.ExitSelfRefreshDelay = 7;
  /* TRAS: min=42ns (4x11.11ns) max=120k (ns) */
  Timing.SelfRefreshTime      = 4;
  /* TRC:  min=70 (7x11.11ns) */
  Timing.RowCycleDelay        = 7;
  /* TWR:  min=1+ 7ns (1+1x11.11ns) */
  Timing.WriteRecoveryTime    = 2;
  /* TRP:  20ns => 2x11.11ns*/
  Timing.RPDelay              = 2;
  /* TRCD: 20ns => 2x11.11ns */
  Timing.RCDDelay             = 2;

  /* FMC SDRAM control configuration */
  SdramHandle.Init.SDBank             = FMC_SDRAM_BANK2;
  /* Row addressing: [7:0] */
  SdramHandle.Init.ColumnBitsNumber   = FMC_SDRAM_COLUMN_BITS_NUM_8;
  /* Column addressing: [11:0] */
  SdramHandle.Init.RowBitsNumber      = FMC_SDRAM_ROW_BITS_NUM_12;
  SdramHandle.Init.MemoryDataWidth    = SDRAM_MEMORY_WIDTH;
  SdramHandle.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  SdramHandle.Init.CASLatency         = SDRAM_CAS_LATENCY;
  SdramHandle.Init.WriteProtection    = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  SdramHandle.Init.SDClockPeriod      = SDCLOCK_PERIOD;
  SdramHandle.Init.ReadBurst          = SDRAM_READBURST;
  SdramHandle.Init.ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_1;

  /* SDRAM controller initialization */
  /* __weak function can be surcharged by the application code */
  BSP_SDRAM_MspInit(&SdramHandle, (void *)NULL);
  if (HAL_SDRAM_Init(&SdramHandle, &Timing) != HAL_OK)
  {
    sdramstatus = SDRAM_ERROR;
  }
  else
  {
    sdramstatus = SDRAM_OK;
  }

  /* SDRAM initialization sequence */
  BSP_SDRAM_Initialization_sequence(REFRESH_COUNT);

  return sdramstatus;
}

/**
  * @brief  Programs the SDRAM device.
  * @param  RefreshCount: SDRAM refresh counter value
  */
void BSP_SDRAM_Initialization_sequence(uint32_t RefreshCount)
{
  __IO uint32_t tmpmrd = 0;

  /* Step 1:  Configure a clock configuration enable command */
  Command.CommandMode             = FMC_SDRAM_CMD_CLK_ENABLE;
  Command.CommandTarget           = FMC_SDRAM_CMD_TARGET_BANK2;
  Command.AutoRefreshNumber       = 1;
  Command.ModeRegisterDefinition  = 0;

  /* Send the command */
  HAL_SDRAM_SendCommand(&SdramHandle, &Command, SDRAM_TIMEOUT);

  /* Step 2: Insert 100 us minimum delay */
  /* Inserted delay is equal to 1 ms due to systick time base unit (ms) */
  HAL_Delay(1);

  /* Step 3: Configure a PALL (precharge all) command */
  Command.CommandMode             = FMC_SDRAM_CMD_PALL;
  Command.CommandTarget           = FMC_SDRAM_CMD_TARGET_BANK2;
  Command.AutoRefreshNumber       = 1;
  Command.ModeRegisterDefinition  = 0;

  /* Send the command */
  HAL_SDRAM_SendCommand(&SdramHandle, &Command, SDRAM_TIMEOUT);

  /* Step 4: Configure an Auto Refresh command */
  Command.CommandMode             = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  Command.CommandTarget           = FMC_SDRAM_CMD_TARGET_BANK2;
  Command.AutoRefreshNumber       = 4;
  Command.ModeRegisterDefinition  = 0;

  /* Send the command */
  HAL_SDRAM_SendCommand(&SdramHandle, &Command, SDRAM_TIMEOUT);

  /* Step 5: Program the external memory mode register */
  tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1          |
           SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |
           SDRAM_MODEREG_CAS_LATENCY_3           |
           SDRAM_MODEREG_OPERATING_MODE_STANDARD |
           SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

  Command.CommandMode             = FMC_SDRAM_CMD_LOAD_MODE;
  Command.CommandTarget           = FMC_SDRAM_CMD_TARGET_BANK2;
  Command.AutoRefreshNumber       = 1;
  Command.ModeRegisterDefinition  = tmpmrd;

  /* Send the command */
  HAL_SDRAM_SendCommand(&SdramHandle, &Command, SDRAM_TIMEOUT);

  /* Step 6: Set the refresh rate counter */
  /* Set the device refresh rate */
  HAL_SDRAM_ProgramRefreshRate(&SdramHandle, RefreshCount);
}

/**
  * @brief  Reads an mount of data from the SDRAM memory in polling mode.
  * @param  uwStartAddress : Read start address
  * @param  pData : Pointer to data to be read
  * @param  uwDataSize: Size of read data from the memory
  */
uint8_t BSP_SDRAM_ReadData(uint32_t uwStartAddress, uint32_t *pData, uint32_t uwDataSize)
{
  if (HAL_SDRAM_Read_32b(&SdramHandle, (uint32_t *)uwStartAddress, pData, uwDataSize) != HAL_OK)
  {
    return SDRAM_ERROR;
  }
  else
  {
    return SDRAM_OK;
  }
}

/**
  * @brief  Reads an mount of data from the SDRAM memory in DMA mode.
  * @param  uwStartAddress : Read start address
  * @param  pData : Pointer to data to be read
  * @param  uwDataSize: Size of read data from the memory
  */
uint8_t BSP_SDRAM_ReadData_DMA(uint32_t uwStartAddress, uint32_t *pData, uint32_t uwDataSize)
{
  if (HAL_SDRAM_Read_DMA(&SdramHandle, (uint32_t *)uwStartAddress, pData, uwDataSize) != HAL_OK)
  {
    return SDRAM_ERROR;
  }
  else
  {
    return SDRAM_OK;
  }
}

/**
  * @brief  Writes an mount of data to the SDRAM memory in polling mode.
  * @param  uwStartAddress : Write start address
  * @param  pData : Pointer to data to be written
  * @param  uwDataSize: Size of written data from the memory
  */
uint8_t BSP_SDRAM_WriteData(uint32_t uwStartAddress, uint32_t *pData, uint32_t uwDataSize)
{
  /* Disable write protection */
  HAL_SDRAM_WriteProtection_Disable(&SdramHandle);

  /*Write 32-bit data buffer to SDRAM memory*/
  if (HAL_SDRAM_Write_32b(&SdramHandle, (uint32_t *)uwStartAddress, pData, uwDataSize) != HAL_OK)
  {
    return SDRAM_ERROR;
  }
  else
  {
    return SDRAM_OK;
  }
}

/**
  * @brief  Writes an mount of data to the SDRAM memory in DMA mode.
  * @param  uwStartAddress : Write start address
  * @param  pData : Pointer to data to be written
  * @param  uwDataSize: Size of written data from the memory
  */
uint8_t BSP_SDRAM_WriteData_DMA(uint32_t uwStartAddress, uint32_t *pData, uint32_t uwDataSize)
{
  if (HAL_SDRAM_Write_DMA(&SdramHandle, (uint32_t *)uwStartAddress, pData, uwDataSize) != HAL_OK)
  {
    return SDRAM_ERROR;
  }
  else
  {
    return SDRAM_OK;
  }
}

/**
  * @brief  Sends command to the SDRAM bank.
  * @param  SdramCmd: Pointer to SDRAM command structure
  * @retval HAL status
  */
uint8_t BSP_SDRAM_Sendcmd(FMC_SDRAM_CommandTypeDef *SdramCmd)
{
  if (HAL_SDRAM_SendCommand(&SdramHandle, SdramCmd, SDRAM_TIMEOUT) != HAL_OK)
  {
    return SDRAM_ERROR;
  }
  else
  {
    return SDRAM_OK;
  }
}

/**
  * @brief  Handles SDRAM DMA transfer interrupt request.
  */
void BSP_SDRAM_DMA_IRQHandler(void)
{
  HAL_DMA_IRQHandler(SdramHandle.hdma);
}

/**
  * @brief  Initializes SDRAM MSP.
  * @note   This function can be surcharged by application code.
  * @param  hsdram: pointer on SDRAM handle
  * @param  Params: pointer on additional configuration parameters, can be NULL.
  */
__weak void BSP_SDRAM_MspInit(SDRAM_HandleTypeDef  *hsdram, void *Params)
{
  static DMA_HandleTypeDef dmaHandle;
  GPIO_InitTypeDef GPIO_InitStructure;

  if (hsdram != (SDRAM_HandleTypeDef *)NULL)
  {
    /* Enable FMC clock */
    __HAL_RCC_FMC_CLK_ENABLE();

    /* Enable chosen DMAx clock */
    __DMAx_CLK_ENABLE();

    /* Enable GPIOs clock */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /*-- GPIOs Configuration -----------------------------------------------------*/
    /*
     +-------------------+--------------------+--------------------+--------------------+
     +                       SDRAM pins assignment                                      +
     +-------------------+--------------------+--------------------+--------------------+
     | PD0  <-> FMC_D2   | PE0  <-> FMC_NBL0  | PF0  <-> FMC_A0    | PG0  <-> FMC_A10   |
     | PD1  <-> FMC_D3   | PE1  <-> FMC_NBL1  | PF1  <-> FMC_A1    | PG1  <-> FMC_A11   |
     | PD8  <-> FMC_D13  | PE7  <-> FMC_D4    | PF2  <-> FMC_A2    | PG8  <-> FMC_SDCLK |
     | PD9  <-> FMC_D14  | PE8  <-> FMC_D5    | PF3  <-> FMC_A3    | PG15 <-> FMC_NCAS  |
     | PD10 <-> FMC_D15  | PE9  <-> FMC_D6    | PF4  <-> FMC_A4    |--------------------+
     | PD14 <-> FMC_D0   | PE10 <-> FMC_D7    | PF5  <-> FMC_A5    |
     | PD15 <-> FMC_D1   | PE11 <-> FMC_D8    | PF11 <-> FMC_NRAS  |
     +-------------------| PE12 <-> FMC_D9    | PF12 <-> FMC_A6    |
                         | PE13 <-> FMC_D10   | PF13 <-> FMC_A7    |
                         | PE14 <-> FMC_D11   | PF14 <-> FMC_A8    |
                         | PE15 <-> FMC_D12   | PF15 <-> FMC_A9    |
     +-------------------+--------------------+--------------------+
     | PB5 <-> FMC_SDCKE1|
     | PB6 <-> FMC_SDNE1 |
     | PC0 <-> FMC_SDNWE |
     +-------------------+

    */

    /* Common GPIO configuration */
    GPIO_InitStructure.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStructure.Speed = GPIO_SPEED_FAST;
    GPIO_InitStructure.Pull  = GPIO_NOPULL;
    GPIO_InitStructure.Alternate = GPIO_AF12_FMC;

    /* GPIOB configuration */
    GPIO_InitStructure.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* GPIOC configuration */
    GPIO_InitStructure.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* GPIOD configuration */
    GPIO_InitStructure.Pin = GPIO_PIN_0 | GPIO_PIN_1  | GPIO_PIN_8 |
                             GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 |
                             GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStructure);

    /* GPIOE configuration */
    GPIO_InitStructure.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_7 |
                             GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 |
                             GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
                             GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStructure);

    /* GPIOF configuration */
    GPIO_InitStructure.Pin = GPIO_PIN_0  | GPIO_PIN_1 | GPIO_PIN_2 |
                             GPIO_PIN_3  | GPIO_PIN_4 | GPIO_PIN_5 |
                             GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
                             GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStructure);

    /* GPIOG configuration */
    GPIO_InitStructure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 |
                             GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStructure);

    /* Configure common DMA parameters */
    dmaHandle.Init.Channel             = SDRAM_DMAx_CHANNEL;
    dmaHandle.Init.Direction           = DMA_MEMORY_TO_MEMORY;
    dmaHandle.Init.PeriphInc           = DMA_PINC_ENABLE;
    dmaHandle.Init.MemInc              = DMA_MINC_ENABLE;
    dmaHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    dmaHandle.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
    dmaHandle.Init.Mode                = DMA_NORMAL;
    dmaHandle.Init.Priority            = DMA_PRIORITY_HIGH;
    dmaHandle.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    dmaHandle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    dmaHandle.Init.MemBurst            = DMA_MBURST_SINGLE;
    dmaHandle.Init.PeriphBurst         = DMA_PBURST_SINGLE;

    dmaHandle.Instance = SDRAM_DMAx_STREAM;

    /* Associate the DMA handle */
    __HAL_LINKDMA(hsdram, hdma, dmaHandle);

    /* Deinitialize the stream for new transfer */
    HAL_DMA_DeInit(&dmaHandle);

    /* Configure the DMA stream */
    HAL_DMA_Init(&dmaHandle);

    /* NVIC configuration for DMA transfer complete interrupt */
    HAL_NVIC_SetPriority(SDRAM_DMAx_IRQn, 0x0F, 0);
    HAL_NVIC_EnableIRQ(SDRAM_DMAx_IRQn);
  } /* of if(hsdram != (SDRAM_HandleTypeDef  *)NULL) */
}

/**
  * @brief  DeInitializes SDRAM MSP.
  * @note   This function can be surcharged by application code.
  * @param  hsdram: pointer on SDRAM handle
  * @param  Params: pointer on additional configuration parameters, can be NULL.
  */
__weak void BSP_SDRAM_MspDeInit(SDRAM_HandleTypeDef  *hsdram, void *Params)
{
  static DMA_HandleTypeDef dma_handle;

  if (hsdram != (SDRAM_HandleTypeDef *)NULL)
  {
    /* Disable NVIC configuration for DMA interrupt */
    HAL_NVIC_DisableIRQ(SDRAM_DMAx_IRQn);

    /* Deinitialize the stream for new transfer */
    dma_handle.Instance = SDRAM_DMAx_STREAM;
    HAL_DMA_DeInit(&dma_handle);

    /* DeInit GPIO pins can be done in the application
     (by surcharging this __weak function) */

    /* GPIO pins clock, FMC clock and DMA clock can be shut down in the application
     by surcharging this __weak function */

  } /* of if(hsdram != (SDRAM_HandleTypeDef  *)NULL) */
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
