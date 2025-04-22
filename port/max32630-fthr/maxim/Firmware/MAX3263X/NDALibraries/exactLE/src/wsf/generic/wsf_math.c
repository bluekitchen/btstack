/*************************************************************************************************/
/*!
 *  \file   wsf_math.c
 *
 *  \brief  Common math utilities generic implementation file.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
 *
 *  Copyright (c) 2013 Wicentric, Inc., all rights reserved.
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

#include "wsf_math.h"

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

static uint32_t wsfRngW = 88675123;
static uint32_t wsfRngX = 123456789;
static uint32_t wsfRngY = 362436069;
static uint32_t wsfRngZ = 521288629;

/*************************************************************************************************/
/*!
 *  \fn     WsfMathInit
 *
 *  \brief  Initialize math routines.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfMathInit(void)
{
  /* Seed PRNG. */
  wsfRngW = 88675123;
  wsfRngX = 123456789;
  wsfRngY = 362436069;
  wsfRngZ = 521288629;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfRandNum
 *
 *  \brief  Generate random number.
 *
 *  \return 32-bit random number.
 *
 *  This implementation uses a xorshift random number generator. Follow this link for details:
 *      http://en.wikipedia.org/wiki/Xorshift
 */
/*************************************************************************************************/
uint32_t WsfRandNum(void)
{
  uint32_t t;

  t = wsfRngX ^ (wsfRngX << 11);
  wsfRngX = wsfRngY;
  wsfRngY = wsfRngZ;
  wsfRngZ = wsfRngW;
  wsfRngW = wsfRngW ^ (wsfRngW >> 19) ^ (t ^ (t >> 8));
  return wsfRngW;
}
