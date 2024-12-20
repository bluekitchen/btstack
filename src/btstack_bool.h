/*
 * Copyright (C) 2019 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/*
 * btstack_bool.h
 *
 * Provide bool type */

#ifndef BTSTACK_BOOL_H
#define BTSTACK_BOOL_H

#if !defined(__cplusplus)

//
// Check for C99
// see: https://sourceforge.net/p/predef/wiki/Standards/
//
#if defined(__STDC__)
#   if defined(__STDC_VERSION__)
#     if (__STDC_VERSION__ >= 199901L)
#       define PREDEF_STANDARD_C_1999
#     endif
#  endif
#endif /* __STDC__ */

// Detecting C99 in Visual Studio requires to disable Microsoft Extensions (/Za) which causes other issues
// Workaround: if MSC, assume stdbool.h exists, which is true for Visual Studio 2022
#ifdef _MSC_VER
#define PREDEF_STANDARD_C_1999
#endif

// define boolean type - required for MISRA-C 2012 Essential Type System
#ifdef PREDEF_STANDARD_C_1999

// use <stdbool.h> if C99 or higher
#   include <stdbool.h>

#else /* PREDEF_STANDARD_C_1999 */

// backport for pre-c99 compilers or incorrect detection
#ifndef bool
#define bool unsigned char
#endif
#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif

#endif /* PREDEF_STANDARD_C_1999 */

#endif /* __cplusplus */

#endif /* BTSTACK_BOOL_H */
