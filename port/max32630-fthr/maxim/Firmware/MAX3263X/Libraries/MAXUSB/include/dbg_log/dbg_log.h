/*******************************************************************************
 * Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 * $Id: dbg_log.h 31614 2017-10-26 17:06:25Z zach.metzinger $
 *
 *******************************************************************************
 */

#ifndef _LOGGER_H_
#define _LOGGER_H_

typedef enum {
  DBG_EVT_START = 0,
  DBG_EVT_RESET,
  DBG_EVT_SETUP,
  DBG_EVT_READ,
  DBG_EVT_WRITE,
  DBG_EVT_USB_INT_START,
  DBG_EVT_USB_INT_END,
  DBG_EVT_IN,
  DBG_EVT_IN_SPUR,
  DBG_EVT_OUT,
  DBG_EVT_OUT_SPUR,
  DBG_EVT_OUT_DMA,
  DBG_EVT_OUT_DMA_END,
  DBG_EVT_DMA_INT_START,
  DBG_EVT_DMA_INT_SPUR,
  DBG_EVT_DMA_INT_IN,
  DBG_EVT_DMA_INT_OUT,
  DBG_EVT_DMA_INT_END,
  DBG_EVT_REQ_LODGE,
  DBG_EVT_REQ_LODGE_DMA,
  DBG_EVT_REQ_REMOVE,
  DBG_EVT_REQ_REMOVE_DMA,
  DBG_EVT_CLR_OUTPKTRDY,
  DBG_EVT_OUTCOUNT,
  DBG_EVT_REQLEN,
  DBG_EVT_ACTLEN,
  DBG_EVT_SETUP_IDLE,
  DBG_EVT_SETUP_NODATA,
  DBG_EVT_SETUP_DATA_OUT,
  DBG_EVT_SETUP_DATA_IN,
  DBG_EVT_SETUP_END,
  DBG_EVT_SENT_STALL,
  DBG_EVT_ACKSTAT,
  DBG_EVT_TRIGGER,
  DBG_EVT_MAX_EVT
} dbg_evt_type_t;

int dbg_log_init(void);
int dbg_log_add(uint32_t t, dbg_evt_type_t e, uint32_t e_p, char *txt);
void dbg_log_print(int num);

#endif
