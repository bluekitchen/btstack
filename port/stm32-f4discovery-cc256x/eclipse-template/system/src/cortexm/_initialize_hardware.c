//
// This file is part of the µOS++ III distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------

#include "cmsis_device.h"

// ----------------------------------------------------------------------------

extern unsigned int __vectors_start;

// Forward declarations.

void
__initialize_hardware_early(void);

void
__initialize_hardware(void);

// ----------------------------------------------------------------------------

// This is the early hardware initialisation routine, it can be
// redefined in the application for more complex cases that
// require early inits (before BSS init).
//
// Called early from _start(), right before data & bss init.
//
// After Reset the Cortex-M processor is in Thread mode,
// priority is Privileged, and the Stack is set to Main.

void
__attribute__((weak))
__initialize_hardware_early(void)
{
  // Call the CSMSIS system initialisation routine.
  SystemInit();

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
  // Set VTOR to the actual address, provided by the linker script.
  // Override the manual, possibly wrong, SystemInit() setting.
  SCB->VTOR = (uint32_t)(&__vectors_start);
#endif

  // The current version of SystemInit() leaves the value of the clock
  // in a RAM variable (SystemCoreClock), which will be cleared shortly,
  // so it needs to be recomputed after the RAM initialisations
  // are completed.

#if defined(OS_INCLUDE_STARTUP_INIT_FP) || (defined (__VFP_FP__) && !defined (__SOFTFP__))

  // Normally FP init is done by SystemInit(). In case this is not done
  // there, it is possible to force its inclusion by defining
  // OS_INCLUDE_STARTUP_INIT_FP.

  // Enable the Cortex-M4 FPU only when -mfloat-abi=hard.
  // Code taken from Section 7.1, Cortex-M4 TRM (DDI0439C)

  // Set bits 20-23 to enable CP10 and CP11 coprocessor
  SCB->CPACR |= (0xF << 20);

#endif // (__VFP_FP__) && !(__SOFTFP__)

#if defined(OS_DEBUG_SEMIHOSTING_FAULTS)
  SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk;
#endif
}

// This is the second hardware initialisation routine, it can be
// redefined in the application for more complex cases that
// require custom inits (before constructors), otherwise these can
// be done in main().
//
// Called from _start(), right after data & bss init, before
// constructors.

void
__attribute__((weak))
__initialize_hardware(void)
{
  // Call the CSMSIS system clock routine to store the clock frequency
  // in the SystemCoreClock global RAM location.
  SystemCoreClockUpdate();
}

// ----------------------------------------------------------------------------
