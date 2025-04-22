/*************************************************************************************************/
/*!
 *  \file   wsf_buf.c
 *        
 *  \brief  Buffer pool service.
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

#include "wsf_types.h"
#include "wsf_buf.h"
#include "wsf_assert.h"
#include "wsf_trace.h"
#include "wsf_cs.h"

/**************************************************************************************************
  Configuration
**************************************************************************************************/

/* check if trying to free a buffer that is already free */
#ifndef WSF_BUF_FREE_CHECK
#define WSF_BUF_FREE_CHECK  TRUE
#endif

/* assert on buffer allocation failure */
#ifndef WSF_BUF_ALLOC_FAIL_ASSERT
#define WSF_BUF_ALLOC_FAIL_ASSERT TRUE
#endif

/* buffer allocation stats */
#ifndef WSF_BUF_STATS
#define WSF_BUF_STATS FALSE
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* magic number used to check for free buffer */
#define WSF_BUF_FREE_NUM            0xFAABD00D

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Unit of memory storage-- a structure containing a pointer */
typedef struct wsfBufMem_tag
{
  struct wsfBufMem_tag  *pNext;
#if WSF_BUF_FREE_CHECK == TRUE
  uint32_t              free;
#endif
} wsfBufMem_t;

/* Internal buffer pool */
typedef struct
{
  wsfBufPoolDesc_t  desc;           /* number of buffers and length */
  wsfBufMem_t       *pStart;        /* start of pool */
  wsfBufMem_t       *pFree;         /* first free buffer in pool */
#if WSF_BUF_STATS == TRUE
  uint8_t           numAlloc;       /* number of buffers currently allocated from pool */
  uint8_t           maxAlloc;       /* maximum buffers ever allocated from pool */
#endif
} wsfBufPool_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* Number of pools */
uint8_t wsfBufNumPools;

/* Memory used for pools */
wsfBufMem_t *wsfBufMem = NULL;

/* Currently use for debugging only */
uint16_t wsfBufMemLen;

#if WSF_BUF_STATS == TRUE
/* Buffer allocation counter */
uint8_t wsfBufAllocCount[WSF_BUF_STATS_MAX_LEN];
#endif

/*************************************************************************************************/
/*!
 *  \fn     WsfBufInit
 *        
 *  \brief  Initialize the buffer pool service.  This function should only be called once
 *          upon system initialization.
 *
 *  \param  bufMemLen Length of free memory
 *  \param  pBufMem   Free memory buffer for building buffer pools
 *  \param  numPools  Number of buffer pools.
 *  \param  pDesc     Array of buffer pool descriptors, one for each pool.
 *
 *  \return TRUE if initialization was successful, FALSE otherwise.
 */
/*************************************************************************************************/
bool_t WsfBufInit(uint16_t bufMemLen, uint8_t *pBufMem, uint8_t numPools, wsfBufPoolDesc_t *pDesc)
{
  wsfBufPool_t  *pPool;
  wsfBufMem_t   *pStart;
  uint16_t      len;
  uint8_t       i;
    
  wsfBufMem = (wsfBufMem_t *) pBufMem;
  pPool = (wsfBufPool_t *) wsfBufMem;
  
  /* buffer storage starts after the pool structs */
  pStart = (wsfBufMem_t *) (pPool + numPools);
  
  wsfBufNumPools = numPools;
  
  /* create each pool; see loop exit condition below */
  while (TRUE)
  {
    /* verify we didn't overrun memory; if we did, abort */
    if (pStart > &wsfBufMem[bufMemLen / sizeof(wsfBufMem_t)])
    {
      WSF_ASSERT(FALSE);
      return FALSE;
    }

    /* exit loop after verification check */
    if (numPools-- == 0)
    {
      break;
    }
        
    /* adjust pool lengths for minimum size and alignment */
    if (pDesc->len < sizeof(wsfBufMem_t))
    {
      pPool->desc.len = sizeof(wsfBufMem_t);
    }
    else if ((pDesc->len % sizeof(wsfBufMem_t)) != 0)
    {
      pPool->desc.len = pDesc->len + sizeof(wsfBufMem_t) - (pDesc->len % sizeof(wsfBufMem_t));
    }
    else
    {
      pPool->desc.len = pDesc->len;
    }
    
    pPool->desc.num = pDesc->num;
    pDesc++;

    pPool->pStart = pStart;
    pPool->pFree = pStart;
#if WSF_BUF_STATS == TRUE
    pPool->numAlloc = 0;
    pPool->maxAlloc = 0;
#endif
    
    WSF_TRACE_INFO3("Creating pool len=%u num=%u pStart=0x%x",
                    pPool->desc.len, pPool->desc.num, pPool->pStart);
           
    /* initialize free list */
    len = pPool->desc.len / sizeof(wsfBufMem_t);
    for (i = pPool->desc.num; i > 1; i--)
    {
      /* pointer to the next free buffer is stored in the buffer itself */
      pStart->pNext = pStart + len;
      pStart += len;
    }
    
    /* last one in list points to NULL */
    pStart->pNext = NULL;
    pStart += len;

    /* next pool */
    pPool++;
  }

  wsfBufMemLen = (uint8_t *) pStart - (uint8_t *) wsfBufMem;
  WSF_TRACE_INFO1("Created buffer pools; using %u bytes", wsfBufMemLen);
  
  return TRUE;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfBufAlloc
 *        
 *  \brief  Allocate a buffer.
 *
 *  \param  len     Length of buffer to allocate.
 *
 *  \return Pointer to allocated buffer or NULL if allocation fails.
 */
/*************************************************************************************************/
void *WsfBufAlloc(uint16_t len)
{
  wsfBufPool_t  *pPool;
  wsfBufMem_t   *pBuf;  
  uint8_t       i;

  WSF_CS_INIT(cs);

  WSF_ASSERT(len > 0);

  pPool = (wsfBufPool_t *) wsfBufMem;
    
  for (i = wsfBufNumPools; i > 0; i--, pPool++)
  {
    /* if buffer is big enough */
    if (len <= pPool->desc.len)
    {
      /* enter critical section */
      WSF_CS_ENTER(cs);
  
      /* if buffers available */
      if (pPool->pFree != NULL)
      {
        /* allocation succeeded */
        pBuf = pPool->pFree;
        
        /* next free buffer is stored inside current free buffer */
        pPool->pFree = pBuf->pNext;

#if WSF_BUF_FREE_CHECK == TRUE
        pBuf->free = 0;
#endif
#if WSF_BUF_STATS == TRUE
      /* increment count for buffers of this length */
      if (len < WSF_BUF_STATS_MAX_LEN)
      {
        wsfBufAllocCount[len]++;
      }
      else
      {
        wsfBufAllocCount[0]++;
      }
      if (++pPool->numAlloc > pPool->maxAlloc)
      {
        pPool->maxAlloc = pPool->numAlloc;
      }
#endif              
        /* exit critical section */
        WSF_CS_EXIT(cs);
        
        WSF_TRACE_ALLOC2("WsfBufAlloc len:%u pBuf:%08x", pPool->desc.len, pBuf);
        
        return pBuf;
      }
      
      /* exit critical section */ 
      WSF_CS_EXIT(cs);
    }
  }
  
  /* allocation failed */
  WSF_TRACE_WARN1("WsfBufAlloc failed len:%u", len);
#if WSF_BUF_ALLOC_FAIL_ASSERT == TRUE
  WSF_ASSERT(FALSE);
#endif
  
  return NULL;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfBufFree
 *        
 *  \brief  Free a buffer.
 *
 *  \param  pBuf    Buffer to free.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfBufFree(void *pBuf)
{
  wsfBufPool_t  *pPool;
  wsfBufMem_t   *p = pBuf;

  WSF_CS_INIT(cs);
  
  /* verify pointer is within range */
  WSF_ASSERT(p >= ((wsfBufPool_t *) wsfBufMem)->pStart);
  WSF_ASSERT(p < (wsfBufMem_t *)(((uint8_t *) wsfBufMem) + wsfBufMemLen));
  
  /* iterate over pools starting from last pool */
  pPool = (wsfBufPool_t *) wsfBufMem + (wsfBufNumPools - 1);
  while (pPool >= (wsfBufPool_t *) wsfBufMem)
  {
    /* if the buffer memory is located inside this pool */
    if (p >= pPool->pStart)
    {
      /* enter critical section */
      WSF_CS_ENTER(cs);

#if WSF_BUF_FREE_CHECK == TRUE
      WSF_ASSERT(p->free != WSF_BUF_FREE_NUM);
      p->free = WSF_BUF_FREE_NUM;
#endif
#if WSF_BUF_STATS == TRUE
      pPool->numAlloc--;
#endif
      
      /* pool found; put buffer back in free list */
      p->pNext = pPool->pFree;
      pPool->pFree = p;
      
      /* exit critical section */
      WSF_CS_EXIT(cs);
      
      WSF_TRACE_FREE2("WsfBufFree len:%u pBuf:%08x", pPool->desc.len, pBuf);

      return;
    }

    /* next pool */
    pPool--;
  }

  /* should never get here */
  WSF_ASSERT(FALSE);
  
  return;  
}

/*************************************************************************************************/
/*!
 *  \fn     WsfBufGetMaxAlloc
 *        
 *  \brief  Diagnostic function to get maximum allocated buffers from a pool.
 *
 *  \param  pool    Buffer pool number.
 *
 *  \return Number of allocated buffers.
 */
/*************************************************************************************************/
uint8_t WsfBufGetMaxAlloc(uint8_t pool)
{
#if WSF_BUF_STATS == TRUE
  if (pool < wsfBufNumPools)
  {
    return ((wsfBufPool_t *) wsfBufMem)[pool].maxAlloc;
  }
#endif
  
  return 0;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfBufGetNumAlloc
 *        
 *  \brief  Diagnostic function to get the number of currently allocated buffers in a pool.
 *
 *  \param  pool    Buffer pool number.
 *
 *  \return Number of allocated buffers.
 */
/*************************************************************************************************/
uint8_t WsfBufGetNumAlloc(uint8_t pool)
{
#if WSF_BUF_STATS == TRUE
  if (pool < wsfBufNumPools)
  {
    return ((wsfBufPool_t *) wsfBufMem)[pool].numAlloc;
  }
#endif
  
  return 0;
}

/*************************************************************************************************/
/*!
 *  \fn     WsfBufGetAllocStats
 *        
 *  \brief  Diagnostic function to get the buffer allocation statistics.
 *
 *  \return Buffer allocation statistics array.
 */
/*************************************************************************************************/
uint8_t *WsfBufGetAllocStats(void)
{
#if WSF_BUF_STATS == TRUE
  return wsfBufAllocCount;
#else
  return NULL;
#endif
}
