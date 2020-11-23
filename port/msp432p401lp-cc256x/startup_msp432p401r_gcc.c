/*
 * -------------------------------------------
 *    MSP432 DriverLib - v3_10_00_09 
 * -------------------------------------------
 *
 * --COPYRIGHT--,BSD,BSD
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//*****************************************************************************
//
// Copyright (C) 2012 - 2015 Texas Instruments Incorporated - http://www.ti.com/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//
//  Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the
//  distribution.
//
//  Neither the name of Texas Instruments Incorporated nor the names of
//  its contributors may be used to endorse or promote products derived
//  from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// MSP432 Family Interrupt Vector Table for GCC
//
//****************************************************************************

#include <stdint.h>

/* Forward declaration of the default fault handlers. */
void resetISR(void);
static void nmiISR(void);
static void faultISR(void);
static void defaultISR(void);

#ifndef HWREG
#define HWREG(x) (*((volatile uint32_t *)(x)))
#endif

/* system initialization funtion */
extern void SystemInit(void);

/* Entry point for the application. */
extern int main(void);

/* External declarations for the interrupt handlers used by the application. */
/* To be added by the user */
extern void EUSCIA2_IRQHandler(void);
extern void DMA_INT1_IRQHandler(void);
extern void DMA_INT2_IRQHandler(void);

/* To be added by the user */
extern void SysTick_Handler(void);

/* Interrupt vector table.  Note that the proper constructs must be placed on this to */
/* ensure that it ends up at physical address 0x0000.0000 or at the start of          */
/* the program if located at a start address other than 0.                            */
void (* const interruptVectors[])(void) __attribute__ ((section (".intvecs"))) =
{
    (void (*)(void))((uint32_t)0x20010000),
                                            /* The initial stack pointer */
    resetISR,                               /* The reset handler         */
    nmiISR,                                 /* The NMI handler           */
    faultISR,                               /* The hard fault handler    */
    defaultISR,                             /* The MPU fault handler     */
    defaultISR,                             /* The bus fault handler     */
    defaultISR,                             /* The usage fault handler   */
    0,                                      /* Reserved                  */
    0,                                      /* Reserved                  */
    0,                                      /* Reserved                  */
    0,                                      /* Reserved                  */
    defaultISR,                             /* SVCall handler            */
    defaultISR,                             /* Debug monitor handler     */
    0,                                      /* Reserved                  */
    defaultISR,                             /* The PendSV handler        */
    SysTick_Handler,                        /* The SysTick handler       */
    defaultISR,                             /* PSS ISR                   */
    defaultISR,                             /* CS ISR                    */
    defaultISR,                             /* PCM ISR                   */
    defaultISR,                             /* WDT ISR                   */
    defaultISR,                             /* FPU ISR                   */
    defaultISR,                             /* FLCTL ISR                 */
    defaultISR,                             /* COMP0 ISR                 */
    defaultISR,                             /* COMP1 ISR                 */
    defaultISR,                             /* TA0_0 ISR                 */
    defaultISR,                             /* TA0_N ISR                 */
    defaultISR,                             /* TA1_0 ISR                 */
    defaultISR,                             /* TA1_N ISR                 */
    defaultISR,                             /* TA2_0 ISR                 */
    defaultISR,                             /* TA2_N ISR                 */
    defaultISR,                             /* TA3_0 ISR                 */
    defaultISR,                             /* TA3_N ISR                 */
    defaultISR,                             /* EUSCIA0 ISR               */
    defaultISR,                             /* EUSCIA1 ISR               */
    defaultISR,                             /* EUSCIA2 ISR               */
    defaultISR,                             /* EUSCIA3 ISR               */
    defaultISR,                             /* EUSCIB0 ISR               */
    defaultISR,                             /* EUSCIB1 ISR               */
    defaultISR,                             /* EUSCIB2 ISR               */
    defaultISR,                             /* EUSCIB3 ISR               */
    defaultISR,                             /* ADC14 ISR                 */
    defaultISR,                             /* T32_INT1 ISR              */
    defaultISR,                             /* T32_INT2 ISR              */
    defaultISR,                             /* T32_INTC ISR              */
    defaultISR,                             /* AES ISR                   */
    defaultISR,                             /* RTC ISR                   */
    defaultISR,                             /* DMA_ERR ISR               */
    defaultISR,                             /* DMA_INT3 ISR              */
    DMA_INT2_IRQHandler,                    /* DMA_INT2 ISR              */
    DMA_INT1_IRQHandler,                    /* DMA_INT1 ISR              */
    defaultISR,                             /* DMA_INT0 ISR              */
    defaultISR,                             /* PORT1 ISR                 */
    defaultISR,                             /* PORT2 ISR                 */
    defaultISR,                             /* PORT3 ISR                 */
    defaultISR,                             /* PORT4 ISR                 */
    defaultISR,                             /* PORT5 ISR                 */
    defaultISR,                             /* PORT6 ISR                 */
    defaultISR,                             /* Reserved 41               */
    defaultISR,                             /* Reserved 42               */
    defaultISR,                             /* Reserved 43               */
    defaultISR,                             /* Reserved 44               */
    defaultISR,                             /* Reserved 45               */
    defaultISR,                             /* Reserved 46               */
    defaultISR,                             /* Reserved 47               */
    defaultISR,                             /* Reserved 48               */
    defaultISR,                             /* Reserved 49               */
    defaultISR,                             /* Reserved 50               */
    defaultISR,                             /* Reserved 51               */
    defaultISR,                             /* Reserved 52               */
    defaultISR,                             /* Reserved 53               */
    defaultISR,                             /* Reserved 54               */
    defaultISR,                             /* Reserved 55               */
    defaultISR,                             /* Reserved 56               */
    defaultISR,                             /* Reserved 57               */
    defaultISR,                             /* Reserved 58               */
    defaultISR,                             /* Reserved 59               */
    defaultISR,                             /* Reserved 60               */
    defaultISR,                             /* Reserved 61               */
    defaultISR,                             /* Reserved 62               */
    defaultISR                              /* Reserved 63               */
};

/* This is the code that gets called when the processor first starts execution */
/* following a reset event.  Only the absolutely necessary set is performed,   */
/* after which the application supplied entry() routine is called.  Any fancy  */
/* actions (such as making decisions based on the reset cause register, and    */
/* resetting the bits in that register) are left solely in the hands of the    */
/* application.                                                                */
extern uint32_t __data_load__;
extern uint32_t __data_start__;
extern uint32_t __data_end__;

void resetISR(void)
{
    /* Copy the data segment initializers from flash to SRAM. */
    uint32_t *pui32Src, *pui32Dest;

    pui32Src = &__data_load__;
    for(pui32Dest = &__data_start__; pui32Dest < &__data_end__; )
    {
        *pui32Dest++ = *pui32Src++;
    }

    /* Zero fill the bss segment. */
    __asm("    ldr     r0, =__bss_start__\n"
          "    ldr     r1, =__bss_end__\n"
          "    mov     r2, #0\n"
          "    .thumb_func\n"
          "zero_loop:\n"
          "        cmp     r0, r1\n"
          "        it      lt\n"
          "        strlt   r2, [r0], #4\n"
          "        blt     zero_loop");

    /* Call system initialization routine */
  SystemInit();

    /* Call the application's entry point. */
    main();
}


/* This is the code that gets called when the processor receives a NMI.  This  */
/* simply enters an infinite loop, preserving the system state for examination */
/* by a debugger.                                                              */
static void nmiISR(void)
{
    /* Enter an infinite loop. */
    while(1)
    {
    }
}


/* This is the code that gets called when the processor receives a fault        */
/* interrupt.  This simply enters an infinite loop, preserving the system state */
/* for examination by a debugger.                                               */
static void faultISR(void)
{
    /* Enter an infinite loop. */
    while(1)
    {
    }
}


/* This is the code that gets called when the processor receives an unexpected  */
/* interrupt.  This simply enters an infinite loop, preserving the system state */
/* for examination by a debugger.                                               */
static void defaultISR(void)
{
    /* Enter an infinite loop. */
    while(1)
    {
    }
}
