/*=============================================================================
* (C) Copyright Albis Technologies Ltd 2011
*==============================================================================
*                STM32 Example Code
*==============================================================================
* File name:     bsp_debug.h
*
* Notes:         STM32 evaluation board UM0780 debug utilities BSP.
*============================================================================*/

#ifndef BSP_DEBUG_H
#define BSP_DEBUG_H

void fatalErrorHandler(const int reset,
                       const char *fileName,
                       unsigned short lineNumber);

// Whether or not source code file names are revealed.
#define REVEAL_SOURCE_FILE_NAMES 1

#if (REVEAL_SOURCE_FILE_NAMES == 1)
  #define DEFINE_THIS_FILE static char const _this_file_name_[] = __FILE__;
#else
  #define DEFINE_THIS_FILE static char const _this_file_name_[] = "***";
#endif // REVEAL_SOURCE_FILE_NAMES

#define SYS_ERROR(reset) fatalErrorHandler(reset, \
                                           _this_file_name_, \
                                           __LINE__)

#define SYS_ASSERT(cond) if(!(cond)) { SYS_ERROR(1); }

void initSerialDebug(void);

void printos(const char *sz, ...);

void restartSystem(void);

#endif // BSP_DEBUG_H
