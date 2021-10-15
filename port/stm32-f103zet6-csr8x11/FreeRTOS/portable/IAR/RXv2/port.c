/*
    FreeRTOS V8.2.3 - Copyright (C) 2015 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for the SH2A port.
 *----------------------------------------------------------*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Library includes. */
#include "string.h"

/* Hardware specifics. */
#include <machine.h>

/*-----------------------------------------------------------*/

/* Tasks should start with interrupts enabled and in Supervisor mode, therefore
PSW is set with U and I set, and PM and IPL clear. */
#define portINITIAL_PSW	 ( ( StackType_t ) 0x00030000 )
#define portINITIAL_FPSW	( ( StackType_t ) 0x00000100 )

/*-----------------------------------------------------------*/

/*
 * Function to start the first task executing - written in asm code as direct
 * access to registers is required.
 */
extern void prvStartFirstTask( void );

/*
 * The tick ISR handler.  The peripheral used is configured by the application
 * via a hook/callback function.
 */
__interrupt void vTickISR( void );

/*-----------------------------------------------------------*/

extern void *pxCurrentTCB;

/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	/* R0 is not included as it is the stack pointer. */

	*pxTopOfStack = 0x00;
	pxTopOfStack--;
 	*pxTopOfStack = portINITIAL_PSW;
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) pxCode;

	/* When debugging it can be useful if every register is set to a known
	value.  Otherwise code space can be saved by just setting the registers
	that need to be set. */
	#ifdef USE_FULL_REGISTER_INITIALISATION
	{
		pxTopOfStack--;
		*pxTopOfStack = 0xffffffff;	/* r15. */
		pxTopOfStack--;
		*pxTopOfStack = 0xeeeeeeee;
		pxTopOfStack--;
		*pxTopOfStack = 0xdddddddd;
		pxTopOfStack--;
		*pxTopOfStack = 0xcccccccc;
		pxTopOfStack--;
		*pxTopOfStack = 0xbbbbbbbb;
		pxTopOfStack--;
		*pxTopOfStack = 0xaaaaaaaa;
		pxTopOfStack--;
		*pxTopOfStack = 0x99999999;
		pxTopOfStack--;
		*pxTopOfStack = 0x88888888;
		pxTopOfStack--;
		*pxTopOfStack = 0x77777777;
		pxTopOfStack--;
		*pxTopOfStack = 0x66666666;
		pxTopOfStack--;
		*pxTopOfStack = 0x55555555;
		pxTopOfStack--;
		*pxTopOfStack = 0x44444444;
		pxTopOfStack--;
		*pxTopOfStack = 0x33333333;
		pxTopOfStack--;
		*pxTopOfStack = 0x22222222;
		pxTopOfStack--;
	}
	#else
	{
		pxTopOfStack -= 15;
	}
	#endif

	*pxTopOfStack = ( StackType_t ) pvParameters; /* R1 */
	pxTopOfStack--;
	*pxTopOfStack = portINITIAL_FPSW;
	pxTopOfStack--;
	*pxTopOfStack = 0x11111111; /* Accumulator 0. */
	pxTopOfStack--;
	*pxTopOfStack = 0x22222222; /* Accumulator 0. */
	pxTopOfStack--;
	*pxTopOfStack = 0x33333333; /* Accumulator 0. */
	pxTopOfStack--;
	*pxTopOfStack = 0x44444444; /* Accumulator 1. */
	pxTopOfStack--;
	*pxTopOfStack = 0x55555555; /* Accumulator 1. */
	pxTopOfStack--;
	*pxTopOfStack = 0x66666666; /* Accumulator 1. */

	return pxTopOfStack;
}
/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler( void )
{
extern void vApplicationSetupTimerInterrupt( void );

	/* Use pxCurrentTCB just so it does not get optimised away. */
	if( pxCurrentTCB != NULL )
	{
		/* Call an application function to set up the timer that will generate the
		tick interrupt.  This way the application can decide which peripheral to
		use.  A demo application is provided to show a suitable example. */
		vApplicationSetupTimerInterrupt();

		/* Enable the software interrupt. */
		_IEN( _ICU_SWINT ) = 1;

		/* Ensure the software interrupt is clear. */
		_IR( _ICU_SWINT ) = 0;

		/* Ensure the software interrupt is set to the kernel priority. */
		_IPR( _ICU_SWINT ) = configKERNEL_INTERRUPT_PRIORITY;

		/* Start the first task. */
		prvStartFirstTask();
	}

	/* Should not get here. */
	return pdFAIL;
}
/*-----------------------------------------------------------*/

#pragma vector = configTICK_VECTOR
__interrupt void vTickISR( void )
{
	/* Re-enable interrupts. */
	__enable_interrupt();

	/* Increment the tick, and perform any processing the new tick value
	necessitates. */
	__set_interrupt_level( configMAX_SYSCALL_INTERRUPT_PRIORITY );
	{
		if( xTaskIncrementTick() != pdFALSE )
		{
			taskYIELD();
		}
	}
	__set_interrupt_level( configKERNEL_INTERRUPT_PRIORITY );
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
	/* Not implemented in ports where there is nothing to return to.
	Artificially force an assert. */
	configASSERT( pxCurrentTCB == NULL );
}
/*-----------------------------------------------------------*/



