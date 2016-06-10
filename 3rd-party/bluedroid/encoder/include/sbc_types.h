/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  Data type declarations.
 *
 ******************************************************************************/

#ifndef SBC_TYPES_H
#define SBC_TYPES_H

#ifdef BUILDCFG
#include "bt_target.h"
#endif


/* BK4BTSTACK_CHANGE START */
#if 0
// #include "data_types.h"

typedef short SINT16;
typedef long SINT32;

#if (SBC_IPAQ_OPT == TRUE)

#if (SBC_FOR_EMBEDDED_LINUX == TRUE)
typedef long long SINT64;
#else
typedef __int64 SINT64;
#endif

#elif (SBC_IS_64_MULT_IN_WINDOW_ACCU == TRUE) || (SBC_IS_64_MULT_IN_IDCT == TRUE)

#if (SBC_FOR_EMBEDDED_LINUX == TRUE)
typedef long long SINT64;
#else
typedef __int64 SINT64;
#endif

#endif
#else

#include <stdint.h>
typedef int8_t SINT8;
typedef int16_t SINT16;
typedef int32_t SINT32;
typedef int64_t SINT64;
typedef uint8_t  UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;

#endif
/* BK4BTSTACK_CHANGE STOP */

#define SBC_API
#define abs32(x) ( (x >= 0) ? x : (-x) )

#endif
