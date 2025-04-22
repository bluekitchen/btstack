/*************************************************************************************************/
/*!
 *  \file   wsf_trace.h
 *
 *  \brief  Trace message interface.
 *
 *          $Date: 2015-08-25 13:40:30 -0500 (Tue, 25 Aug 2015) $
 *          $Revision: 18759 $
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
#ifndef WSF_TRACE_H
#define WSF_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Function Prototypes
**************************************************************************************************/

void WsfTrace(const char *pStr, ...);

/**************************************************************************************************
  Macros
**************************************************************************************************/

#ifndef __CROSSWORKS

#if WSF_TRACE_ENABLED == TRUE
#define WSF_TRACE(msg, ...)                         WsfTrace(msg, __VA_ARGS__)
#else
#define WSF_TRACE(msg, ...)
#endif

#define WSF_TRACE_INFO0(msg)
#define WSF_TRACE_INFO1(msg, var1)
#define WSF_TRACE_INFO2(msg, var1, var2)
#define WSF_TRACE_INFO3(msg, var1, var2, var3)
#define WSF_TRACE_WARN0(msg)                        WSF_TRACE(msg, 0)
#define WSF_TRACE_WARN1(msg, var1)                  WSF_TRACE(msg, var1)
#define WSF_TRACE_WARN2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define WSF_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define WSF_TRACE_ERR0(msg)                         WSF_TRACE(msg, 0)
#define WSF_TRACE_ERR1(msg, var1)                   WSF_TRACE(msg, var1)
#define WSF_TRACE_ERR2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define WSF_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)
#define WSF_TRACE_ALLOC0(msg)
#define WSF_TRACE_ALLOC1(msg, var1)
#define WSF_TRACE_ALLOC2(msg, var1, var2)
#define WSF_TRACE_ALLOC3(msg, var1, var2, var3)
#define WSF_TRACE_FREE0(msg)
#define WSF_TRACE_FREE1(msg, var1)
#define WSF_TRACE_FREE2(msg, var1, var2)
#define WSF_TRACE_FREE3(msg, var1, var2, var3)
#define WSF_TRACE_MSG0(msg)
#define WSF_TRACE_MSG1(msg, var1)
#define WSF_TRACE_MSG2(msg, var1, var2)
#define WSF_TRACE_MSG3(msg, var1, var2, var3)

#define HCI_TRACE_INFO0(msg)                        WSF_TRACE(msg, 0)
#define HCI_TRACE_INFO1(msg, var1)                  WSF_TRACE(msg, var1)
#define HCI_TRACE_INFO2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define HCI_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define HCI_TRACE_WARN0(msg)                        WSF_TRACE(msg, 0)
#define HCI_TRACE_WARN1(msg, var1)                  WSF_TRACE(msg, var1)
#define HCI_TRACE_WARN2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define HCI_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define HCI_TRACE_ERR0(msg)                         WSF_TRACE(msg, 0)
#define HCI_TRACE_ERR1(msg, var1)                   WSF_TRACE(msg, var1)
#define HCI_TRACE_ERR2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define HCI_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)

#define HCI_PDUMP_CMD(len, pBuf)
#define HCI_PDUMP_EVT(len, pBuf)
#define HCI_PDUMP_TX_ACL(len, pBuf)
#define HCI_PDUMP_RX_ACL(len, pBuf)

#define DM_TRACE_INFO0(msg)                         WSF_TRACE(msg, 0)
#define DM_TRACE_INFO1(msg, var1)                   WSF_TRACE(msg, var1)
#define DM_TRACE_INFO2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define DM_TRACE_INFO3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)
#define DM_TRACE_WARN0(msg)                         WSF_TRACE(msg, 0)
#define DM_TRACE_WARN1(msg, var1)                   WSF_TRACE(msg, var1)
#define DM_TRACE_WARN2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define DM_TRACE_WARN3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)
#define DM_TRACE_ERR0(msg)                          WSF_TRACE(msg, 0)
#define DM_TRACE_ERR1(msg, var1)                    WSF_TRACE(msg, var1)
#define DM_TRACE_ERR2(msg, var1, var2)              WSF_TRACE(msg, var1, var2)
#define DM_TRACE_ERR3(msg, var1, var2, var3)        WSF_TRACE(msg, var1, var2, var3)
#define DM_TRACE_ALLOC0(msg)                        WSF_TRACE(msg, 0)
#define DM_TRACE_ALLOC1(msg, var1)                  WSF_TRACE(msg, var1)
#define DM_TRACE_ALLOC2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define DM_TRACE_ALLOC3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define DM_TRACE_FREE0(msg)                         WSF_TRACE(msg, 0)
#define DM_TRACE_FREE1(msg, var1)                   WSF_TRACE(msg, var1)
#define DM_TRACE_FREE2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define DM_TRACE_FREE3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)

#define L2C_TRACE_INFO0(msg)                        WSF_TRACE(msg, 0)
#define L2C_TRACE_INFO1(msg, var1)                  WSF_TRACE(msg, var1)
#define L2C_TRACE_INFO2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define L2C_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define L2C_TRACE_WARN0(msg)                        WSF_TRACE(msg, 0)
#define L2C_TRACE_WARN1(msg, var1)                  WSF_TRACE(msg, var1)
#define L2C_TRACE_WARN2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define L2C_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define L2C_TRACE_ERR0(msg)                         WSF_TRACE(msg, 0)
#define L2C_TRACE_ERR1(msg, var1)                   WSF_TRACE(msg, var1)
#define L2C_TRACE_ERR2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define L2C_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)

#define ATT_TRACE_INFO0(msg)                        WSF_TRACE(msg, 0)
#define ATT_TRACE_INFO1(msg, var1)                  WSF_TRACE(msg, var1)
#define ATT_TRACE_INFO2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define ATT_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define ATT_TRACE_WARN0(msg)                        WSF_TRACE(msg, 0)
#define ATT_TRACE_WARN1(msg, var1)                  WSF_TRACE(msg, var1)
#define ATT_TRACE_WARN2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define ATT_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define ATT_TRACE_ERR0(msg)                         WSF_TRACE(msg, 0)
#define ATT_TRACE_ERR1(msg, var1)                   WSF_TRACE(msg, var1)
#define ATT_TRACE_ERR2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define ATT_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)

#define SMP_TRACE_INFO0(msg)                        WSF_TRACE(msg, 0)
#define SMP_TRACE_INFO1(msg, var1)                  WSF_TRACE(msg, var1)
#define SMP_TRACE_INFO2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define SMP_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define SMP_TRACE_WARN0(msg)                        WSF_TRACE(msg, 0)
#define SMP_TRACE_WARN1(msg, var1)                  WSF_TRACE(msg, var1)
#define SMP_TRACE_WARN2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define SMP_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define SMP_TRACE_ERR0(msg)                         WSF_TRACE(msg, 0)
#define SMP_TRACE_ERR1(msg, var1)                   WSF_TRACE(msg, var1)
#define SMP_TRACE_ERR2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define SMP_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)

#define APP_TRACE_INFO0(msg)                        WSF_TRACE(msg, 0)
#define APP_TRACE_INFO1(msg, var1)                  WSF_TRACE(msg, var1)
#define APP_TRACE_INFO2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define APP_TRACE_INFO3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define APP_TRACE_WARN0(msg)                        WSF_TRACE(msg, 0)
#define APP_TRACE_WARN1(msg, var1)                  WSF_TRACE(msg, var1)
#define APP_TRACE_WARN2(msg, var1, var2)            WSF_TRACE(msg, var1, var2)
#define APP_TRACE_WARN3(msg, var1, var2, var3)      WSF_TRACE(msg, var1, var2, var3)
#define APP_TRACE_ERR0(msg)                         WSF_TRACE(msg, 0)
#define APP_TRACE_ERR1(msg, var1)                   WSF_TRACE(msg, var1)
#define APP_TRACE_ERR2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define APP_TRACE_ERR3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)

#define LL_TRACE_INFO0(msg)                         WSF_TRACE(msg, 0)
#define LL_TRACE_INFO1(msg, var1)                   WSF_TRACE(msg, var1)
#define LL_TRACE_INFO2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define LL_TRACE_INFO3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)
#define LL_TRACE_WARN0(msg)                         WSF_TRACE(msg, 0)
#define LL_TRACE_WARN1(msg, var1)                   WSF_TRACE(msg, var1)
#define LL_TRACE_WARN2(msg, var1, var2)             WSF_TRACE(msg, var1, var2)
#define LL_TRACE_WARN3(msg, var1, var2, var3)       WSF_TRACE(msg, var1, var2, var3)
#define LL_TRACE_ERR0(msg)                          WSF_TRACE(msg, 0)
#define LL_TRACE_ERR1(msg, var1)                    WSF_TRACE(msg, var1)
#define LL_TRACE_ERR2(msg, var1, var2)              WSF_TRACE(msg, var1, var2)
#define LL_TRACE_ERR3(msg, var1, var2, var3)        WSF_TRACE(msg, var1, var2, var3)

#else

#define WSF_TRACE_INFO0(msg)
#define WSF_TRACE_INFO1(msg, var1)
#define WSF_TRACE_INFO2(msg, var1, var2)
#define WSF_TRACE_INFO3(msg, var1, var2, var3)
#define WSF_TRACE_WARN0(msg)                        
#define WSF_TRACE_WARN1(msg, var1)                  
#define WSF_TRACE_WARN2(msg, var1, var2)            
#define WSF_TRACE_WARN3(msg, var1, var2, var3)      
#define WSF_TRACE_ERR0(msg)                         
#define WSF_TRACE_ERR1(msg, var1)                   
#define WSF_TRACE_ERR2(msg, var1, var2)             
#define WSF_TRACE_ERR3(msg, var1, var2, var3)       
#define WSF_TRACE_ALLOC0(msg)
#define WSF_TRACE_ALLOC1(msg, var1)
#define WSF_TRACE_ALLOC2(msg, var1, var2)
#define WSF_TRACE_ALLOC3(msg, var1, var2, var3)
#define WSF_TRACE_FREE0(msg)
#define WSF_TRACE_FREE1(msg, var1)
#define WSF_TRACE_FREE2(msg, var1, var2)
#define WSF_TRACE_FREE3(msg, var1, var2, var3)
#define WSF_TRACE_MSG0(msg)
#define WSF_TRACE_MSG1(msg, var1)
#define WSF_TRACE_MSG2(msg, var1, var2)
#define WSF_TRACE_MSG3(msg, var1, var2, var3)

#define HCI_TRACE_INFO0(msg)                        
#define HCI_TRACE_INFO1(msg, var1)                  
#define HCI_TRACE_INFO2(msg, var1, var2)            
#define HCI_TRACE_INFO3(msg, var1, var2, var3)      
#define HCI_TRACE_WARN0(msg)                        
#define HCI_TRACE_WARN1(msg, var1)                  
#define HCI_TRACE_WARN2(msg, var1, var2)            
#define HCI_TRACE_WARN3(msg, var1, var2, var3)      
#define HCI_TRACE_ERR0(msg)                         
#define HCI_TRACE_ERR1(msg, var1)                   
#define HCI_TRACE_ERR2(msg, var1, var2)             
#define HCI_TRACE_ERR3(msg, var1, var2, var3)       

#define HCI_PDUMP_CMD(len, pBuf)
#define HCI_PDUMP_EVT(len, pBuf)
#define HCI_PDUMP_TX_ACL(len, pBuf)
#define HCI_PDUMP_RX_ACL(len, pBuf)

#define DM_TRACE_INFO0(msg)                         
#define DM_TRACE_INFO1(msg, var1)                   
#define DM_TRACE_INFO2(msg, var1, var2)             
#define DM_TRACE_INFO3(msg, var1, var2, var3)       
#define DM_TRACE_WARN0(msg)                         
#define DM_TRACE_WARN1(msg, var1)                   
#define DM_TRACE_WARN2(msg, var1, var2)             
#define DM_TRACE_WARN3(msg, var1, var2, var3)       
#define DM_TRACE_ERR0(msg)                          
#define DM_TRACE_ERR1(msg, var1)                    
#define DM_TRACE_ERR2(msg, var1, var2)              
#define DM_TRACE_ERR3(msg, var1, var2, var3)        
#define DM_TRACE_ALLOC0(msg)                        
#define DM_TRACE_ALLOC1(msg, var1)                  
#define DM_TRACE_ALLOC2(msg, var1, var2)            
#define DM_TRACE_ALLOC3(msg, var1, var2, var3)      
#define DM_TRACE_FREE0(msg)                         
#define DM_TRACE_FREE1(msg, var1)                   
#define DM_TRACE_FREE2(msg, var1, var2)             
#define DM_TRACE_FREE3(msg, var1, var2, var3)       

#define L2C_TRACE_INFO0(msg)                        
#define L2C_TRACE_INFO1(msg, var1)                  
#define L2C_TRACE_INFO2(msg, var1, var2)            
#define L2C_TRACE_INFO3(msg, var1, var2, var3)      
#define L2C_TRACE_WARN0(msg)                        
#define L2C_TRACE_WARN1(msg, var1)                  
#define L2C_TRACE_WARN2(msg, var1, var2)            
#define L2C_TRACE_WARN3(msg, var1, var2, var3)      
#define L2C_TRACE_ERR0(msg)                         
#define L2C_TRACE_ERR1(msg, var1)                   
#define L2C_TRACE_ERR2(msg, var1, var2)             
#define L2C_TRACE_ERR3(msg, var1, var2, var3)       

#define ATT_TRACE_INFO0(msg)                        
#define ATT_TRACE_INFO1(msg, var1)                  
#define ATT_TRACE_INFO2(msg, var1, var2)            
#define ATT_TRACE_INFO3(msg, var1, var2, var3)      
#define ATT_TRACE_WARN0(msg)                        
#define ATT_TRACE_WARN1(msg, var1)                  
#define ATT_TRACE_WARN2(msg, var1, var2)            
#define ATT_TRACE_WARN3(msg, var1, var2, var3)      
#define ATT_TRACE_ERR0(msg)                         
#define ATT_TRACE_ERR1(msg, var1)                   
#define ATT_TRACE_ERR2(msg, var1, var2)             
#define ATT_TRACE_ERR3(msg, var1, var2, var3)       

#define SMP_TRACE_INFO0(msg)                        
#define SMP_TRACE_INFO1(msg, var1)                  
#define SMP_TRACE_INFO2(msg, var1, var2)            
#define SMP_TRACE_INFO3(msg, var1, var2, var3)      
#define SMP_TRACE_WARN0(msg)                        
#define SMP_TRACE_WARN1(msg, var1)                  
#define SMP_TRACE_WARN2(msg, var1, var2)            
#define SMP_TRACE_WARN3(msg, var1, var2, var3)      
#define SMP_TRACE_ERR0(msg)                         
#define SMP_TRACE_ERR1(msg, var1)                   
#define SMP_TRACE_ERR2(msg, var1, var2)             
#define SMP_TRACE_ERR3(msg, var1, var2, var3)       

#define APP_TRACE_INFO0(msg)                        
#define APP_TRACE_INFO1(msg, var1)                  
#define APP_TRACE_INFO2(msg, var1, var2)            
#define APP_TRACE_INFO3(msg, var1, var2, var3)      
#define APP_TRACE_WARN0(msg)                        
#define APP_TRACE_WARN1(msg, var1)                  
#define APP_TRACE_WARN2(msg, var1, var2)            
#define APP_TRACE_WARN3(msg, var1, var2, var3)      
#define APP_TRACE_ERR0(msg)                         
#define APP_TRACE_ERR1(msg, var1)                   
#define APP_TRACE_ERR2(msg, var1, var2)             
#define APP_TRACE_ERR3(msg, var1, var2, var3)       

#define LL_TRACE_INFO0(msg)                         
#define LL_TRACE_INFO1(msg, var1)                   
#define LL_TRACE_INFO2(msg, var1, var2)             
#define LL_TRACE_INFO3(msg, var1, var2, var3)       
#define LL_TRACE_WARN0(msg)                         
#define LL_TRACE_WARN1(msg, var1)                   
#define LL_TRACE_WARN2(msg, var1, var2)             
#define LL_TRACE_WARN3(msg, var1, var2, var3)       
#define LL_TRACE_ERR0(msg)                          
#define LL_TRACE_ERR1(msg, var1)                    
#define LL_TRACE_ERR2(msg, var1, var2)              
#define LL_TRACE_ERR3(msg, var1, var2, var3)        

#endif

#ifdef __cplusplus
};
#endif

#endif /* WSF_TRACE_H */
