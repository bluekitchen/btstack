@==============================================================================
@ (C) Copyright Albis Technologies Ltd 2011
@==============================================================================
@                STM32 Example Code
@==============================================================================
@ File name:     vector_table.s
@
@ Notes:         STM32 vector table.
@                Must be located at beginning of flash image.
@==============================================================================

    .extern   _cm3_main_stack
    .extern   startup
    .extern   App_NMI_ISR
    .extern   App_Fault_ISR
    .extern   App_MemFault_ISR
    .extern   App_BusFault_ISR
    .extern   App_UsageFault_ISR
    .extern   App_Spurious_ISR
    .extern   OS_CPU_PendSVHandler
    .extern   OS_CPU_SysTickHandler

    .extern   BSP_IntHandlerWWDG
    .extern   BSP_IntHandlerPVD
    .extern   BSP_IntHandlerTAMPER
    .extern   BSP_IntHandlerRTC
    .extern   BSP_IntHandlerFLASH
    .extern   BSP_IntHandlerRCC
    .extern   BSP_IntHandlerEXTI0
    .extern   BSP_IntHandlerEXTI1
    .extern   BSP_IntHandlerEXTI2
    .extern   BSP_IntHandlerEXTI3
    .extern   BSP_IntHandlerEXTI4
    .extern   BSP_IntHandlerDMA1_CH1
    .extern   BSP_IntHandlerDMA1_CH2
    .extern   BSP_IntHandlerDMA1_CH3
    .extern   BSP_IntHandlerDMA1_CH4
    .extern   BSP_IntHandlerDMA1_CH5

    .extern   BSP_IntHandlerDMA1_CH6
    .extern   BSP_IntHandlerDMA1_CH7
    .extern   BSP_IntHandlerADC1_2
    .extern   BSP_IntHandlerUSB_HP_CAN_TX
    .extern   BSP_IntHandlerUSB_LP_CAN_RX0
    .extern   BSP_IntHandlerCAN_RX1
    .extern   BSP_IntHandlerCAN_SCE
    .extern   BSP_IntHandlerEXTI9_5
    .extern   BSP_IntHandlerTIM1_BRK
    .extern   BSP_IntHandlerTIM1_UP
    .extern   BSP_IntHandlerTIM1_TRG_COM
    .extern   BSP_IntHandlerTIM1_CC
    .extern   BSP_IntHandlerTIM2
    .extern   BSP_IntHandlerTIM3
    .extern   BSP_IntHandlerTIM4
    .extern   BSP_IntHandlerI2C1_EV

    .extern   BSP_IntHandlerI2C1_ER
    .extern   BSP_IntHandlerI2C2_EV
    .extern   BSP_IntHandlerI2C2_ER
    .extern   BSP_IntHandlerSPI1
    .extern   BSP_IntHandlerSPI2
    .extern   BSP_IntHandlerUSART1
    .extern   BSP_IntHandlerUSART2
    .extern   BSP_IntHandlerUSART3
    .extern   BSP_IntHandlerEXTI15_10
    .extern   BSP_IntHandlerRTCAlarm
    .extern   BSP_IntHandlerUSBWakeUp
    .extern   BSP_IntHandlerTIM8_BRK
    .extern   BSP_IntHandlerTIM8_UP
    .extern   BSP_IntHandlerTIM8_TRG_COM
    .extern   BSP_IntHandlerTIM8_CC
    .extern   BSP_IntHandlerADC3

    .extern   BSP_IntHandlerFSMC
    .extern   BSP_IntHandlerSDIO
    .extern   BSP_IntHandlerTIM5
    .extern   BSP_IntHandlerSPI3
    .extern   BSP_IntHandlerUART4
    .extern   BSP_IntHandlerUART5
    .extern   BSP_IntHandlerTIM6
    .extern   BSP_IntHandlerTIM7
    .extern   BSP_IntHandlerDMA2_CH1
    .extern   BSP_IntHandlerDMA2_CH2
    .extern   BSP_IntHandlerDMA2_CH3
    .extern   BSP_IntHandlerDMA2_CH4_5

    .global   vectorTableBegin

    .section .text

vectorTableBegin:

    .word _cm3_main_stack                      @     Main stack
    .word startup                              @     Reset handler.
    .word App_NMI_ISR                          @  2, NMI.
    .word App_Fault_ISR                        @  3, Hard Fault.
    .word App_MemFault_ISR                     @  4, Memory Management.
    .word App_BusFault_ISR                     @  5, Bus Fault.
    .word App_UsageFault_ISR                   @  6, Usage Fault.
    .word App_Spurious_ISR                     @  7, Reserved.
    .word App_Spurious_ISR                     @  8, Reserved.
    .word App_Spurious_ISR                     @  9, Reserved.
    .word App_Spurious_ISR                     @ 10, Reserved.
    .word App_Spurious_ISR                     @ 11, SVCall.
    .word App_Spurious_ISR                     @ 12, Debug Monitor.
    .word App_Spurious_ISR                     @ 13, Reserved.
    .word OS_CPU_PendSVHandler + 1             @ 14, PendSV Handler.
    .word OS_CPU_SysTickHandler + 1            @ 15, uC/OS-II Tick ISR Handler.

    .word BSP_IntHandlerWWDG                   @ 16, INTISR[  0]  Window Watchdog.
    .word BSP_IntHandlerPVD                    @ 17, INTISR[  1]  PVD through EXTI Line Detection.
    .word BSP_IntHandlerTAMPER                 @ 18, INTISR[  2]  Tamper Interrupt.
    .word BSP_IntHandlerRTC                    @ 19, INTISR[  3]  RTC Global Interrupt.
    .word BSP_IntHandlerFLASH                  @ 20, INTISR[  4]  FLASH Global Interrupt.
    .word BSP_IntHandlerRCC                    @ 21, INTISR[  5]  RCC Global Interrupt.
    .word BSP_IntHandlerEXTI0                  @ 22, INTISR[  6]  EXTI Line0 Interrupt.
    .word BSP_IntHandlerEXTI1                  @ 23, INTISR[  7]  EXTI Line1 Interrupt.
    .word BSP_IntHandlerEXTI2                  @ 24, INTISR[  8]  EXTI Line2 Interrupt.
    .word BSP_IntHandlerEXTI3                  @ 25, INTISR[  9]  EXTI Line3 Interrupt.
    .word BSP_IntHandlerEXTI4                  @ 26, INTISR[ 10]  EXTI Line4 Interrupt.
    .word BSP_IntHandlerDMA1_CH1               @ 27, INTISR[ 11]  DMA Channel1 Global Interrupt.
    .word BSP_IntHandlerDMA1_CH2               @ 28, INTISR[ 12]  DMA Channel2 Global Interrupt.
    .word BSP_IntHandlerDMA1_CH3               @ 29, INTISR[ 13]  DMA Channel3 Global Interrupt.
    .word BSP_IntHandlerDMA1_CH4               @ 30, INTISR[ 14]  DMA Channel4 Global Interrupt.
    .word BSP_IntHandlerDMA1_CH5               @ 31, INTISR[ 15]  DMA Channel5 Global Interrupt.

    .word BSP_IntHandlerDMA1_CH6               @ 32, INTISR[ 16]  DMA Channel6 Global Interrupt.
    .word BSP_IntHandlerDMA1_CH7               @ 33, INTISR[ 17]  DMA Channel7 Global Interrupt.
    .word BSP_IntHandlerADC1_2                 @ 34, INTISR[ 18]  ADC1 & ADC2 Global Interrupt.
    .word BSP_IntHandlerUSB_HP_CAN_TX          @ 35, INTISR[ 19]  USB High Prio / CAN TX  Interrupts.
    .word BSP_IntHandlerUSB_LP_CAN_RX0         @ 36, INTISR[ 20]  USB Low  Prio / CAN RX0 Interrupts.
    .word BSP_IntHandlerCAN_RX1                @ 37, INTISR[ 21]  CAN RX1 Interrupt.
    .word BSP_IntHandlerCAN_SCE                @ 38, INTISR[ 22]  CAN SCE Interrupt.
    .word BSP_IntHandlerEXTI9_5                @ 39, INTISR[ 23]  EXTI Line[9:5] Interrupt.
    .word BSP_IntHandlerTIM1_BRK               @ 40, INTISR[ 24]  TIM1 Break  Interrupt.
    .word BSP_IntHandlerTIM1_UP                @ 41, INTISR[ 25]  TIM1 Update Interrupt.
    .word BSP_IntHandlerTIM1_TRG_COM           @ 42, INTISR[ 26]  TIM1 Trig & Commutation Interrupts.
    .word BSP_IntHandlerTIM1_CC                @ 43, INTISR[ 27]  TIM1 Capture Compare Interrupt.
    .word BSP_IntHandlerTIM2                   @ 44, INTISR[ 28]  TIM2 Global Interrupt.
    .word BSP_IntHandlerTIM3                   @ 45, INTISR[ 29]  TIM3 Global Interrupt.
    .word BSP_IntHandlerTIM4                   @ 46, INTISR[ 30]  TIM4 Global Interrupt.
    .word BSP_IntHandlerI2C1_EV                @ 47, INTISR[ 31]  I2C1 Event  Interrupt.

    .word BSP_IntHandlerI2C1_ER                @ 48, INTISR[ 32]  I2C1 Error  Interrupt.
    .word BSP_IntHandlerI2C2_EV                @ 49, INTISR[ 33]  I2C2 Event  Interrupt.
    .word BSP_IntHandlerI2C2_ER                @ 50, INTISR[ 34]  I2C2 Error  Interrupt.
    .word BSP_IntHandlerSPI1                   @ 51, INTISR[ 35]  SPI1 Global Interrupt.
    .word BSP_IntHandlerSPI2                   @ 52, INTISR[ 36]  SPI2 Global Interrupt.
    .word BSP_IntHandlerUSART1                 @ 53, INTISR[ 37]  USART1 Global Interrupt.
    .word BSP_IntHandlerUSART2                 @ 54, INTISR[ 38]  USART2 Global Interrupt.
    .word BSP_IntHandlerUSART3                 @ 55, INTISR[ 39]  USART3 Global Interrupt.
    .word BSP_IntHandlerEXTI15_10              @ 56, INTISR[ 40]  EXTI Line [15:10] Interrupts.
    .word BSP_IntHandlerRTCAlarm               @ 57, INTISR[ 41]  RTC Alarm EXT Line Interrupt.
    .word BSP_IntHandlerUSBWakeUp              @ 58, INTISR[ 42]  USB Wakeup from Suspend EXTI Int.
    .word BSP_IntHandlerTIM8_BRK               @ 59, INTISR[ 43]  TIM8 Break Interrupt.
    .word BSP_IntHandlerTIM8_UP                @ 60, INTISR[ 44]  TIM8 Update Interrupt.
    .word BSP_IntHandlerTIM8_TRG_COM           @ 61, INTISR[ 45]  TIM8 Trigg/Commutation Interrupts.
    .word BSP_IntHandlerTIM8_CC                @ 62, INTISR[ 46]  TIM8 Capture Compare Interrupt.
    .word BSP_IntHandlerADC3                   @ 63, INTISR[ 47]  ADC3 Global Interrupt.

    .word BSP_IntHandlerFSMC                   @ 64, INTISR[ 48]  FSMC Global Interrupt.
    .word BSP_IntHandlerSDIO                   @ 65, INTISR[ 49]  SDIO Global Interrupt.
    .word BSP_IntHandlerTIM5                   @ 66, INTISR[ 50]  TIM5 Global Interrupt.
    .word BSP_IntHandlerSPI3                   @ 67, INTISR[ 51]  SPI3 Global Interrupt.
    .word BSP_IntHandlerUART4                  @ 68, INTISR[ 52]  UART4 Global Interrupt.
    .word BSP_IntHandlerUART5                  @ 69, INTISR[ 53]  UART5 Global Interrupt.
    .word BSP_IntHandlerTIM6                   @ 70, INTISR[ 54]  TIM6 Global Interrupt.
    .word BSP_IntHandlerTIM7                   @ 71, INTISR[ 55]  TIM7 Global Interrupt.
    .word BSP_IntHandlerDMA2_CH1               @ 72, INTISR[ 56]  DMA2 Channel1 Global Interrupt.
    .word BSP_IntHandlerDMA2_CH2               @ 73, INTISR[ 57]  DMA2 Channel2 Global Interrupt.
    .word BSP_IntHandlerDMA2_CH3               @ 74, INTISR[ 58]  DMA2 Channel3 Global Interrupt.
    .word BSP_IntHandlerDMA2_CH4_5             @ 75, INTISR[ 59]  DMA2 Channel4/5 Global Interrups.
