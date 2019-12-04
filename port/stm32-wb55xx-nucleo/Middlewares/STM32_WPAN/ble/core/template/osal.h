/*****************************************************************************
 * @file    osal.h
 * @author  MCD Application Team
 * @brief   This header file defines the OS abstraction layer used by
 *          the BLE stack. OSAL defines the set of functions which needs to be
 *          ported to target operating system and target platform.
 *          Actually, only memset, memcpy and memcmp wrappers are defined.
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

#ifndef OSAL_H__
#define OSAL_H__


/**
 * This function copies size number of bytes from a 
 * memory location pointed by src to a destination 
 * memory location pointed by dest
 * 
 * @param[in] dest Destination address
 * @param[in] src  Source address
 * @param[in] size size in the bytes  
 * 
 * @return  Address of the destination
 */
 
extern void* Osal_MemCpy( void *dest, const void *src, unsigned int size );

/**
 * This function sets first number of bytes, specified
 * by size, to the destination memory pointed by ptr 
 * to the specified value
 * 
 * @param[in] ptr    Destination address
 * @param[in] value  Value to be set
 * @param[in] size   Size in the bytes  
 * 
 * @return  Address of the destination
 */
 
extern void* Osal_MemSet( void *ptr, int value, unsigned int size );

/**
 * This function compares n bytes of two regions of memory
 * 
 * @param[in] s1    First buffer to compare.
 * @param[in] s2    Second buffer to compare.
 * @param[in] size  Number of bytes to compare.   
 * 
 * @return  0 if the two buffers are equal, 1 otherwise
 */
extern int Osal_MemCmp( const void *s1, const void *s2, unsigned int size );


#endif /* OSAL_H__ */
