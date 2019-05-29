/*****************************************************************************
 * @file    compiler.h
 * @author  MCD Application Team
 * @brief   This file contains the definitions which are compiler dependent.
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

#ifndef COMPILER_H__
#define COMPILER_H__


/**
  * @brief  This is the section dedicated to IAR toolchain
  */
#if defined(__ICCARM__) || defined(__IAR_SYSTEMS_ASM__)

#define QUOTE_(a) #a

/**
  * @brief  PACKED
  *         Use the PACKED macro for variables that needs to be packed.
  *         Usage:  PACKED(struct) myStruct_s
  *                 PACKED(union) myStruct_s
  */
#define PACKED(decl)                   __packed decl

/**
  * @brief  SECTION
  *         Use the SECTION macro to assign data or code in a specific section.
  *         Usage:  SECTION(".my_section")
  */
#define SECTION(name)                  _Pragma(QUOTE_(location=name))

/**
  * @brief  ALIGN_DEF
  *         Use the ALIGN_DEF macro to specify the alignment of a variable.
  *         Usage:  ALIGN_DEF(4)
  */
#define ALIGN_DEF(v)                   _Pragma(QUOTE_(data_alignment=v))

/**
  * @brief  WEAK_FUNCTION
  *         Use the WEAK_FUNCTION macro to declare a weak function.
  *         Usage:  WEAK_FUNCTION(int my_weak_function(void))
  */
#define WEAK_FUNCTION(function)        __weak function

/**
  * @brief  NO_INIT
  *         Use the NO_INIT macro to declare a not initialized variable.
  *         Usage:  NO_INIT(int my_no_init_var)
  *         Usage:  NO_INIT(uint16_t my_no_init_array[10])
  */
#define NO_INIT(var)                   __no_init var

/**
  * @brief  This is the section dedicated to Atollic toolchain
  */
#else
#ifdef __GNUC__

/**
  * @brief  PACKED
  *         Use the PACKED macro for variables that needs to be packed.
  *         Usage:  PACKED(struct) myStruct_s
  *                 PACKED(union) myStruct_s
  */
#define PACKED(decl)                    decl __attribute__((packed))

/**
  * @brief  SECTION
  *         Use the SECTION macro to assign data or code in a specific section.
  *         Usage:  SECTION(".my_section")
  */
#define SECTION(name)                   __attribute__((section(name)))

/**
  * @brief  ALIGN_DEF
  *         Use the ALIGN_DEF macro to specify the alignment of a variable.
  *         Usage:  ALIGN_DEF(4)
  */
#define ALIGN_DEF(N)                    __attribute__((aligned(N)))

/**
  * @brief  WEAK_FUNCTION
  *         Use the WEAK_FUNCTION macro to declare a weak function.
  *         Usage:  WEAK_FUNCTION(int my_weak_function(void))
  */
#define WEAK_FUNCTION(function)         __attribute__((weak)) function 

/**
  * @brief  NO_INIT
  *         Use the NO_INIT macro to declare a not initialized variable.
  *         Usage:  NO_INIT(int my_no_init_var)
  *         Usage:  NO_INIT(uint16_t my_no_init_array[10])
  */
#define NO_INIT(var)                    var  __attribute__((section(".noinit")))

/**
  * @brief  This is the section dedicated to Keil toolchain
  */
#else
#ifdef __CC_ARM	

/**
  * @brief  PACKED
  *         Use the PACKED macro for variables that needs to be packed.
  *         Usage:  PACKED(struct) myStruct_s
  *                 PACKED(union) myStruct_s
  */
#define PACKED(decl)                        decl __attribute__((packed))

/**
  * @brief  SECTION
  *         Use the SECTION macro to assign data or code in a specific section.
  *         Usage:  SECTION(".my_section")
  */
#define SECTION(name)                       __attribute__((section(name)))

/**
  * @brief  ALIGN_DEF
  *         Use the ALIGN_DEF macro to specify the alignment of a variable.
  *         Usage:  ALIGN_DEF(4)
  */
#define ALIGN_DEF(N)                        __attribute__((aligned(N)))

/**
  * @brief  WEAK_FUNCTION
  *         Use the WEAK_FUNCTION macro to declare a weak function.
  *         Usage:  WEAK_FUNCTION(int my_weak_function(void))
  */
#define WEAK_FUNCTION(function)             __weak function

/**
  * @brief  NO_INIT
  *         Use the NO_INIT macro to declare a not initialized variable.
  *         Usage:  NO_INIT(int my_no_init_var)
  *         Usage:  NO_INIT(uint16_t my_no_init_array[10])
  */
#define NO_INIT(var)                        var __attribute__((section("NoInit")))

#else

#error Neither ICCARM, CC ARM nor GNUC C detected. Define your macros.

#endif
#endif
#endif


#endif /* ! COMPILER_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE***/
