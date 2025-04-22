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
 * $Id: dbg_log.c 31614 2017-10-26 17:06:25Z zach.metzinger $
 *
 *******************************************************************************
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "dbg_log.h"

char *dbg_evt_type_map[DBG_EVT_MAX_EVT] = { "START",
					    "RESET",
					    "SETUP",
					    "READ",
					    "WRITE",
					    "USB_INT_START",
					    "USB_INT_END",
					    "IN",
					    "IN_SPUR",
					    "OUT",
					    "OUT_SPUR",
					    "OUT_DMA",
					    "OUT_DMA_END",
					    "DMA_INT_START",
					    "DMA_INT_SPUR",
					    "DMA_INT_IN",
					    "DMA_INT_OUT",
					    "DMA_INT_END",
					    "REQ_LODGE",
					    "REQ_LODGE_DMA",
					    "REQ_REMOVE",
					    "REQ_REMOVE_DMA",
					    "CLR_OUTPKTRDY",
					    "OUTCOUNT",
					    "REQLEN",
					    "ACTLEN",
					    "SETUP_IDLE",
					    "SETUP_NODATA",
					    "SETUP_DATA_OUT",
					    "SETUP_DATA_IN",
					    "SETUP_END",
					    "SENT_STALL",
					    "ACKSTAT",
					    "TRIGGER" };



typedef struct {
  uint32_t seqno;
  uint32_t time;
  dbg_evt_type_t evt;
  uint32_t data_ptr;
  char text[32];
} dbg_log_entry_t;


/* Global logfile */
#define LOGSIZE 512
dbg_log_entry_t dbg_log[LOGSIZE];
unsigned int dbg_head = 0;
unsigned int dbg_tail = 0;

int dbg_log_init(void)
{
  dbg_head = dbg_tail = 0;
  bzero(dbg_log, LOGSIZE * sizeof(dbg_log_entry_t));

  return 0;
}

int dbg_log_add(uint32_t t, dbg_evt_type_t e, uint32_t e_p, char *txt)
{

  if (e >= DBG_EVT_MAX_EVT) {
    return -1;
  }

  /* Tail always points to next item to be used */
  dbg_log[dbg_tail].time = t;
  dbg_log[dbg_tail].evt = e;
  dbg_log[dbg_tail].data_ptr = e_p;
  strncpy(dbg_log[dbg_tail].text, txt, 32);

  if (dbg_head == dbg_tail) {
    /* Empty list */
    dbg_log[dbg_tail].seqno = 0;
    dbg_tail++;
  } else {
    if (!dbg_tail) {
      dbg_log[dbg_tail].seqno = dbg_log[LOGSIZE-1].seqno + 1;
    } else {
      dbg_log[dbg_tail].seqno = dbg_log[dbg_tail-1].seqno + 1;
    }
    dbg_tail = (dbg_tail+1) % LOGSIZE;
    if (dbg_tail == dbg_head) {
      /* Move head up one */
      dbg_head = (dbg_tail+1) % LOGSIZE;
    }
  }
    
  return 0;
}

/* num > 0: print that number of recent events (backwards), otherwise print entire log (forwards) */
void dbg_log_print(int num)
{
  unsigned int x;
  uint8_t y[33];

  printf("------------------ Debug log ------------------\n");
  if (num > 0) {
    printf("Not yet implemented\n");
  } else {
    for (x = dbg_head; x != dbg_tail; x = (x+1) % LOGSIZE) {
      memcpy(y, dbg_log[x].text, 32);
      y[32] = 0;
      printf("Seq %04u (@ %04u) Event=%16s Data=0x%08x Text=[ %s ]\n",
	     dbg_log[x].seqno, dbg_log[x].time,
	     dbg_evt_type_map[dbg_log[x].evt],
	     dbg_log[x].data_ptr, y);
    }
  }
  printf("-----------------------------------------------\n");
}
