;/*
;    FreeRTOS V8.2.3 - Copyright (C) 2015 Real Time Engineers Ltd.
;    All rights reserved
;
;
;    ***************************************************************************
;     *                                                                       *
;     *    FreeRTOS tutorial books are available in pdf and paperback.        *
;     *    Complete, revised, and edited pdf reference manuals are also       *
;     *    available.                                                         *
;     *                                                                       *
;     *    Purchasing FreeRTOS documentation will not only help you, by       *
;     *    ensuring you get running as quickly as possible and with an        *
;     *    in-depth knowledge of how to use FreeRTOS, it will also help       *
;     *    the FreeRTOS project to continue with its mission of providing     *
;     *    professional grade, cross platform, de facto standard solutions    *
;     *    for microcontrollers - completely free of charge!                  *
;     *                                                                       *
;     *    >>> See http://www.FreeRTOS.org/Documentation for details. <<<     *
;     *                                                                       *
;     *    Thank you for using FreeRTOS, and thank you for your support!      *
;     *                                                                       *
;    ***************************************************************************
;
;
;    This file is part of the FreeRTOS distribution.
;
;    FreeRTOS is free software; you can redistribute it and/or modify it under
;    the terms of the GNU General Public License (version 2) as published by the
;    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
;    >>>NOTE<<< The modification to the GPL is included to allow you to
;    distribute a combined work that includes FreeRTOS without being obliged to
;    provide the source code for proprietary components outside of the FreeRTOS
;    kernel.  FreeRTOS is distributed in the hope that it will be useful, but
;    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
;    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
;    more details. You should have received a copy of the GNU General Public
;    License and the FreeRTOS license exception along with FreeRTOS; if not it
;    can be viewed here: http://www.freertos.org/a00114.html and also obtained
;    by writing to Richard Barry, contact details for whom are available on the
;    FreeRTOS WEB site.
;
;    1 tab == 4 spaces!
;
;    http://www.FreeRTOS.org - Documentation, latest information, license and
;    contact details.
;
;    http://www.SafeRTOS.com - A version that is certified for use in safety
;    critical systems.
;
;    http://www.OpenRTOS.com - Commercial support, development, porting,
;    licensing and training services.
;*/


	.extern pxCurrentTCB
	.extern vTaskSwitchContext
	.extern ulMaxSyscallInterruptPriorityConst

	.global _vector_14
	.global _lc_ref__vector_pp_14
	.global SVC_Handler
	.global vPortStartFirstTask
	.global vPortEnableVFP
	.global ulPortSetInterruptMask
	.global vPortClearInterruptMask

;-----------------------------------------------------------

	.section .text
	.thumb
	.align 4
_vector_14: .type func

	mrs r0, psp
	isb

	;Get the location of the current TCB.
	ldr.w	r3, =pxCurrentTCB
	ldr	r2, [r3]

	;Is the task using the FPU context?  If so, push high vfp registers.
	tst r14, #0x10
	it eq
	vstmdbeq r0!, {s16-s31}

	;Save the core registers.
	stmdb r0!, {r4-r11, r14}

	;Save the new top of stack into the first member of the TCB.
	str r0, [r2]

	stmdb sp!, {r3}
	ldr.w r0, =ulMaxSyscallInterruptPriorityConst
	ldr r0, [r0]
	msr basepri, r0
	bl vTaskSwitchContext
	mov r0, #0
	msr basepri, r0
	ldmia sp!, {r3}

	;The first item in pxCurrentTCB is the task top of stack.
	ldr r1, [r3]
	ldr r0, [r1]

	;Pop the core registers.
	ldmia r0!, {r4-r11, r14}

	;Is the task using the FPU context?  If so, pop the high vfp registers too.
	tst r14, #0x10
	it eq
	vldmiaeq r0!, {s16-s31}

	msr psp, r0
	isb
	bx r14

	.size	_vector_14, $-_vector_14
	.endsec

;-----------------------------------------------------------

; This function is an XMC4000 silicon errata workaround.  It will get used when
; the SILICON_BUG_PMC_CM_001 linker macro is defined.
	.section .text
	.thumb
	.align 4
_lc_ref__vector_pp_14: .type func

	mrs r0, psp
	isb

	;Get the location of the current TCB.
	ldr.w	r3, =pxCurrentTCB
	ldr	r2, [r3]

	;Is the task using the FPU context?  If so, push high vfp registers.
	tst r14, #0x10
	it eq
	vstmdbeq r0!, {s16-s31}

	;Save the core registers.
	stmdb r0!, {r4-r11, r14}

	;Save the new top of stack into the first member of the TCB.
	str r0, [r2]

	stmdb sp!, {r3}
	ldr.w r0, =ulMaxSyscallInterruptPriorityConst
	ldr r0, [r0]
	msr basepri, r0
	bl vTaskSwitchContext
	mov r0, #0
	msr basepri, r0
	ldmia sp!, {r3}

	;The first item in pxCurrentTCB is the task top of stack.
	ldr r1, [r3]
	ldr r0, [r1]

	;Pop the core registers.
	ldmia r0!, {r4-r11, r14}

	;Is the task using the FPU context?  If so, pop the high vfp registers too.
	tst r14, #0x10
	it eq
	vldmiaeq r0!, {s16-s31}

	msr psp, r0
	isb
	push { lr }
	pop { pc } ; XMC4000 specific errata workaround.  Do not used "bx lr" here.

	.size	_lc_ref__vector_pp_14, $-_lc_ref__vector_pp_14
	.endsec

;-----------------------------------------------------------

	.section .text
	.thumb
	.align 4
SVC_Handler: .type func
	;Get the location of the current TCB.
	ldr.w	r3, =pxCurrentTCB
	ldr r1, [r3]
	ldr r0, [r1]
	;Pop the core registers.
	ldmia r0!, {r4-r11, r14}
	msr psp, r0
	isb
	mov r0, #0
	msr	basepri, r0
	bx r14
	.size	SVC_Handler, $-SVC_Handler
	.endsec

;-----------------------------------------------------------

	.section .text
	.thumb
	.align 4
vPortStartFirstTask .type func
	;Use the NVIC offset register to locate the stack.
	ldr.w r0, =0xE000ED08
	ldr r0, [r0]
	ldr r0, [r0]
	;Set the msp back to the start of the stack.
	msr msp, r0
	;Call SVC to start the first task.
	cpsie i
	cpsie f
	dsb
	isb
	svc 0
	.size	vPortStartFirstTask, $-vPortStartFirstTask
	.endsec

;-----------------------------------------------------------

	.section .text
	.thumb
	.align 4
vPortEnableVFP .type func
	;The FPU enable bits are in the CPACR.
	ldr.w r0, =0xE000ED88
	ldr	r1, [r0]

	;Enable CP10 and CP11 coprocessors, then save back.
	orr	r1, r1, #( 0xf << 20 )
	str r1, [r0]
	bx	r14
	.size	vPortEnableVFP, $-vPortEnableVFP
	.endsec

;-----------------------------------------------------------

	.section .text
	.thumb
	.align 4
ulPortSetInterruptMask:
	mrs r0, basepri
	ldr.w r1, =ulMaxSyscallInterruptPriorityConst
	ldr r1, [r1]
	msr basepri, r1
	bx r14
	.size	ulPortSetInterruptMask, $-ulPortSetInterruptMask
	.endsec

;-----------------------------------------------------------

	.section .text
	.thumb
	.align 4
vPortClearInterruptMask:
	msr basepri, r0
	bx r14
	.size	vPortClearInterruptMask, $-vPortClearInterruptMask
	.endsec

;-----------------------------------------------------------

	.end

