/*****************************************************************************
 * @file    osal.c
 * @author  MCD Application Team
 * @brief   Implements the interface defined in "osal.h" needed by the stack.
 *          Actually, only memset, memcpy and memcmp wrappers are implemented.
 *****************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */

#include <string.h>
#include "osal.h"

 
/**
 * Osal_MemCpy
 * 
 */
 
void* Osal_MemCpy( void *dest, const void *src, unsigned int size )
{
  return memcpy( dest, src, size ); 
}

/**
 * Osal_MemSet
 * 
 */
 
void* Osal_MemSet( void *ptr, int value, unsigned int size )
{
  return memset( ptr, value, size );
}

/**
 * Osal_MemCmp
 * 
 */
int Osal_MemCmp( const void *s1, const void *s2, unsigned int size )
{
  return memcmp( s1, s2, size );
}
