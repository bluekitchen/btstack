/*************************************************************************************************/
/*!
 *  \file   bstream.h
 *
 *  \brief  Byte stream to integer conversion macros.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
 *
 *  Copyright (c) 2009 Wicentric, Inc., all rights reserved.
 *  Wicentric confidential and proprietary.
 *
 *  IMPORTANT.  Your use of this file is governed by a Software License Agreement
 *  ("Agreement") that must be accepted in order to download or otherwise receive a
 *  copy of this file.  You may not use or copy this file for any purpose other than
 *  as described in the Agreement.  If you do not agree to all of the terms of the
 *  Agreement do not use this file and delete all copies in your possession or control;
 *  if you do not have a copy of the Agreement, you must contact Wicentric, Inc. prior
 *  to any use, copying or further distribution of this software.
 */
/*************************************************************************************************/
#ifndef BSTREAM_H
#define BSTREAM_H

#include "bda.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*!
 * Macros for converting a little endian byte buffer to integers.
 */
#define BYTES_TO_UINT16(n, p)     {n = ((uint16_t)(p)[0] + ((uint16_t)(p)[1] << 8));}

#define BYTES_TO_UINT24(n, p)     {n = ((uint16_t)(p)[0] + ((uint16_t)(p)[1] << 8) + \
                                        ((uint16_t)(p)[2] << 16));}

#define BYTES_TO_UINT32(n, p)     {n = ((uint32_t)(p)[0] + ((uint32_t)(p)[1] << 8) + \
                                        ((uint32_t)(p)[2] << 16) + ((uint32_t)(p)[3] << 24));}

#define BYTES_TO_UINT40(n, p)     {n = ((uint64_t)(p)[0] + ((uint64_t)(p)[1] << 8) + \
                                        ((uint64_t)(p)[2] << 16) + ((uint64_t)(p)[3] << 24) + \
                                        ((uint64_t)(p)[4] << 32));}

#define BYTES_TO_UINT64(n, p)     {n = ((uint64_t)(p)[0] + ((uint64_t)(p)[1] << 8) + \
                                        ((uint64_t)(p)[2] << 16) + ((uint64_t)(p)[3] << 24) + \
                                        ((uint64_t)(p)[2] << 32) + ((uint64_t)(p)[3] << 40) + \
                                        ((uint64_t)(p)[2] << 48) + ((uint64_t)(p)[3] << 56));}

/*!
 * Macros for converting little endian integers to array of bytes
 */
#define UINT16_TO_BYTES(n)        ((uint8_t) (n)), ((uint8_t)((n) >> 8))

/*!
 * Macros for converting little endian integers to single bytes
 */
#define UINT16_TO_BYTE0(n)        ((uint8_t) (n))
#define UINT16_TO_BYTE1(n)        ((uint8_t) ((n) >> 8))

#define UINT32_TO_BYTE0(n)        ((uint8_t) (n))
#define UINT32_TO_BYTE1(n)        ((uint8_t) ((n) >> 8))
#define UINT32_TO_BYTE2(n)        ((uint8_t) ((n) >> 16))
#define UINT32_TO_BYTE3(n)        ((uint8_t) ((n) >> 24))

/*!
 * Macros for converting a little endian byte stream to integers, with increment.
 */
#define BSTREAM_TO_INT8(n, p)     {n = (int8_t)(*(p)++);}
#define BSTREAM_TO_UINT8(n, p)    {n = (uint8_t)(*(p)++);}
#define BSTREAM_TO_UINT16(n, p)   {BYTES_TO_UINT16(n, p); p += 2;}
#define BSTREAM_TO_UINT24(n, p)   {BYTES_TO_UINT24(n, p); p += 3;}
#define BSTREAM_TO_UINT32(n, p)   {BYTES_TO_UINT32(n, p); p += 4;}
#define BSTREAM_TO_UINT40(n, p)   {BYTES_TO_UINT40(n, p); p += 5;}
#define BSTREAM_TO_UINT64(n, p)   {BYTES_TO_UINT64(n, p); p += 8;}
#define BSTREAM_TO_BDA(bda, p)    {BdaCpy(bda, p); p += BDA_ADDR_LEN;}
#define BSTREAM_TO_BDA64(bda, p)  {bda = (uint64_t)p[0] <<  0 | (uint64_t)p[1] <<  8 | (uint64_t)p[2] << 16 |\
                                         (uint64_t)p[3] << 24 | (uint64_t)p[4] << 32 | (uint64_t)p[5] << 40; \
                                   p += BDA_ADDR_LEN;}

/*!
 * Macros for converting integers to a little endian byte stream, with increment.
 */
#define UINT8_TO_BSTREAM(p, n)    {*(p)++ = (uint8_t)(n);}
#define UINT16_TO_BSTREAM(p, n)   {*(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8);}
#define UINT24_TO_BSTREAM(p, n)   {*(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8); \
                                  *(p)++ = (uint8_t)((n) >> 16);}
#define UINT32_TO_BSTREAM(p, n)   {*(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8); \
                                  *(p)++ = (uint8_t)((n) >> 16); *(p)++ = (uint8_t)((n) >> 24);}
#define UINT40_TO_BSTREAM(p, n)   {*(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8); \
                                  *(p)++ = (uint8_t)((n) >> 16); *(p)++ = (uint8_t)((n) >> 24); \
                                  *(p)++ = (uint8_t)((n) >> 32);}
#define UINT64_TO_BSTREAM(p, n)   {*(p)++ = (uint8_t)(n); *(p)++ = (uint8_t)((n) >> 8); \
                                  *(p)++ = (uint8_t)((n) >> 16); *(p)++ = (uint8_t)((n) >> 24); \
                                  *(p)++ = (uint8_t)((n) >> 32); *(p)++ = (uint8_t)((n) >> 40); \
                                  *(p)++ = (uint8_t)((n) >> 48); *(p)++ = (uint8_t)((n) >> 56);}
#define BDA_TO_BSTREAM(p, bda)    {BdaCpy(p, bda); p += BDA_ADDR_LEN;}
#define BDA64_TO_BSTREAM(p, bda)  {p[0] = bda >>  0; p[1] = bda >>  8; p[2] = bda >> 16; \
                                   p[3] = bda >> 24; p[4] = bda >> 32; p[5] = bda >> 40; \
                                   p += BDA_ADDR_LEN;}

/*!
 * Macros for comparing a little endian byte buffer to integers.
 */
#define BYTES_UINT16_CMP(p, n)    ((p)[1] == UINT16_TO_BYTE1(n) && (p)[0] == UINT16_TO_BYTE0(n))

/*!
 * Macros for IEEE FLOAT type:  exponent = byte 3, mantissa = bytes 2-0
 */
#define FLT_TO_UINT32(m, e)       ((m) | ((int32_t)(e) << 24))
#define UINT32_TO_FLT(m, e, n)    {m = UINT32_TO_FLT_M(n); e = UINT32_TO_FLT_E(n);}
#define UINT32_TO_FLT_M(n)        ((((n) & 0x00FFFFFF) >= 0x00800000) ? \
                                   ((int32_t)(((n) | 0xFF000000))) : ((int32_t)((n) & 0x00FFFFFF)))
#define UINT32_TO_FLT_E(n)        ((int8_t)(n >> 24))
/*!
 * Macros for IEEE SFLOAT type:  exponent = bits 15-12, mantissa = bits 11-0
 */
#define SFLT_TO_UINT16(m, e)      ((m) | ((int16_t)(e) << 12))
#define UINT16_TO_SFLT(m, e, n)   {m = UINT16_TO_SFLT_M(n); e = UINT16_TO_SFLT_E(n);}
#define UINT16_TO_SFLT_M(n)       ((((n) & 0x0FFF) >= 0x0800) ? \
                                   ((int16_t)(((n) | 0xF000))) : ((int16_t)((n) & 0x0FFF)))
#define UINT16_TO_SFLT_E(n)       (((n >> 12) >= 0x0008) ? \
                                   ((int8_t)(((n >> 12) | 0xF0))) : ((int8_t)(n >> 12)))


#ifdef __cplusplus
};
#endif

#endif /* BSTREAM_H */
