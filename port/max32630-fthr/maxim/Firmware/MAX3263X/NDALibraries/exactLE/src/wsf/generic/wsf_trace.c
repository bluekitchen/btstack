/*************************************************************************************************/
/*!
 *  \file   wsf_trace.c
 *        
 *  \brief  Trace message implementation.
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

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "wsf_types.h"

/*************************************************************************************************/
/*!
 *  \fn     WsfTrace
 *        
 *  \brief  Print a trace message.
 *
 *  \param  pStr      Message format string
 *  \param  ...       Additional aguments, printf-style
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTrace(const char *pStr, ...)
{
  va_list           args;

  va_start(args, pStr);
  vprintf(pStr, args);  
  va_end(args);
  printf("\r\n");
}
