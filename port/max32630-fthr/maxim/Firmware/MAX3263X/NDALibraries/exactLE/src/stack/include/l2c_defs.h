/*************************************************************************************************/
/*!
 *  \file   l2c_defs.h
 *        
 *  \brief  L2CAP constants and definitions from the Bluetooth specification.
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
#ifndef L2C_DEFS_H
#define L2C_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! Packet definitions */
#define L2C_HDR_LEN                   4         /*! L2CAP packet header length */
#define L2C_MIN_MTU                   23        /*! Minimum packet payload MTU for LE */
#define L2C_SIG_HDR_LEN               4         /*! L2CAP signaling command header length */

/*! Start of L2CAP payload in an HCI ACL packet buffer */
#define L2C_PAYLOAD_START             (HCI_ACL_HDR_LEN + L2C_HDR_LEN)

/*! L2CAP signaling packet base length, including HCI header */
#define L2C_SIG_PKT_BASE_LEN          (HCI_ACL_HDR_LEN + L2C_HDR_LEN + L2C_SIG_HDR_LEN)

/*! Signaling packet parameter lengths */
#define L2C_SIG_CONN_UPDATE_REQ_LEN   8
#define L2C_SIG_CONN_UPDATE_RSP_LEN   2
#define L2C_SIG_CMD_REJ_LEN           2

/*! Connection identifiers */
#define L2C_CID_ATT                   0x0004    /*! CID for attribute protocol */
#define L2C_CID_LE_SIGNALING          0x0005    /*! CID for LE signaling */
#define L2C_CID_SMP                   0x0006    /*! CID for security manager protocol */

/*! Signaling codes */
#define L2C_SIG_CMD_REJ               0x01      /*! Comand reject */
#define L2C_SIG_CONN_UPDATE_REQ       0x12      /*! Connection parameter update request */
#define L2C_SIG_CONN_UPDATE_RSP       0x13      /*! Connection parameter update response */

/*! Signaling response code flag */
#define L2C_SIG_RSP_FLAG              0x01

/*! Command reject reason codes */
#define L2C_REJ_NOT_UNDERSTOOD        0x0000    /*! Command not understood */
#define L2C_REJ_MTU_EXCEEDED          0x0001    /*! Signaling MTU exceeded */
#define L2C_REJ_INVALID_CID           0x0002    /*! Invalid CID in request */

/*! Connection parameter update result */
#define L2C_CONN_PARAM_ACCEPTED       0x0000    /*! Connection parameters accepted */
#define L2C_CONN_PARAM_REJECTED       0x0001    /*! Connection parameters rejected */


#ifdef __cplusplus
};
#endif

#endif /* L2C_DEFS_H */
