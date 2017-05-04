//
// This file is part of the µOS++ III distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------

#if defined(TRACE)

#include <stdio.h>
#include <stdarg.h>
#include "diag/Trace.h"
#include "string.h"

#ifndef OS_INTEGER_TRACE_PRINTF_TMP_ARRAY_SIZE
#define OS_INTEGER_TRACE_PRINTF_TMP_ARRAY_SIZE (128)
#endif

// ----------------------------------------------------------------------------

int
trace_printf(const char* format, ...)
{
  int ret;
  va_list ap;

  va_start (ap, format);

  // TODO: rewrite it to no longer use newlib, it is way too heavy

  static char buf[OS_INTEGER_TRACE_PRINTF_TMP_ARRAY_SIZE];

  // Print to the local buffer
  ret = vsnprintf (buf, sizeof(buf), format, ap);
  if (ret > 0)
    {
      // Transfer the buffer to the device
      ret = trace_write (buf, (size_t)ret);
    }

  va_end (ap);
  return ret;
}

int
trace_puts(const char *s)
{
  trace_write(s, strlen(s));
  return trace_write("\n", 1);
}

int
trace_putchar(int c)
{
  trace_write((const char*)&c, 1);
  return c;
}

void
trace_dump_args(int argc, char* argv[])
{
  trace_printf("main(argc=%d, argv=[", argc);
  for (int i = 0; i < argc; ++i)
    {
      if (i != 0)
        {
          trace_printf(", ");
        }
      trace_printf("\"%s\"", argv[i]);
    }
  trace_printf("]);\n");
}

// ----------------------------------------------------------------------------

#endif // TRACE
