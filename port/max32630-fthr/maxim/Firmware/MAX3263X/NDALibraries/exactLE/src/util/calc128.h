/*************************************************************************************************/
/*!
 *  \file   calc128.h
 *
 *  \brief  128-bit integer utilities.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
 *
 *  Copyright (c) 2010 Wicentric, Inc., all rights reserved.
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
#ifndef CALC128_H
#define CALC128_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! 128-bit integer length in bytes */
#define CALC128_LEN                 16

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/*! 128-bit zero value */
extern const uint8_t calc128Zeros[CALC128_LEN];

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     Calc128Cpy
 *        
 *  \brief  Copy a 128-bit integer from source to destination.
 *
 *  \param  pDst    Pointer to destination.
 *  \param  pSrc    Pointer to source.
 *
 *  \return None.
 */
/*************************************************************************************************/
void Calc128Cpy(uint8_t *pDst, uint8_t *pSrc);

/*************************************************************************************************/
/*!
 *  \fn     Calc128Cpy64
 *        
 *  \brief  Copy a 64-bit integer from source to destination.
 *
 *  \param  pDst    Pointer to destination.
 *  \param  pSrc    Pointer to source.
 *
 *  \return None.
 */
/*************************************************************************************************/
void Calc128Cpy64(uint8_t *pDst, uint8_t *pSrc);

/*************************************************************************************************/
/*!
 *  \fn     Calc128Xor
 *        
 *  \brief  Exclusive-or two 128-bit integers and return the result in pDst.
 *
 *  \param  pDst    Pointer to destination.
 *  \param  pSrc    Pointer to source.
 *
 *  \return None.
 */
/*************************************************************************************************/
void Calc128Xor(uint8_t *pDst, uint8_t *pSrc);

#ifdef __cplusplus
};
#endif

#endif /* CALC128_H */
