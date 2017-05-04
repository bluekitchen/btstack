//
// This file is part of the µOS++ III distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include "diag/Trace.h"

// ----------------------------------------------------------------------------

void
__attribute__((noreturn))
__assert_func (const char *file, int line, const char *func,
               const char *failedexpr)
{
  trace_printf ("assertion \"%s\" failed: file \"%s\", line %d%s%s\n",
                failedexpr, file, line, func ? ", function: " : "",
                func ? func : "");
  abort ();
  /* NOTREACHED */
}

// ----------------------------------------------------------------------------

// This is STM32 specific, but can be used on other platforms too.
// If you need it, add the following to your application header:

//#ifdef  USE_FULL_ASSERT
//#define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
//void assert_failed(uint8_t* file, uint32_t line);
//#else
//#define assert_param(expr) ((void)0)
//#endif // USE_FULL_ASSERT

#if defined(USE_FULL_ASSERT)

void
assert_failed (uint8_t* file, uint32_t line);

// Called from the assert_param() macro, usually defined in the stm32f*_conf.h
void
__attribute__((noreturn, weak))
assert_failed (uint8_t* file, uint32_t line)
{
  trace_printf ("assert_param() failed: file \"%s\", line %d\n", file, line);
  abort ();
  /* NOTREACHED */
}

#endif // defined(USE_FULL_ASSERT)

// ----------------------------------------------------------------------------
