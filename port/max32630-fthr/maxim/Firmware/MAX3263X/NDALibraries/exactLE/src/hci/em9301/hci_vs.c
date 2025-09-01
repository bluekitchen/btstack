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
 * $Date: 2016-03-11 11:46:37 -0600 (Fri, 11 Mar 2016) $ 
 * $Revision: 21839 $
 *
 ******************************************************************************/
 
#include <string.h>
#include "wsf_types.h"
#include "wsf_msg.h"
#include "wsf_trace.h"
#include "bda.h"
#include "bstream.h"
#include "hci_core.h"
#include "hci_api.h"
#include "hci_main.h"
#include "hci_cmd.h"
#include "hci_drv.h"

/**************************************************************************************************
  Macros
**************************************************************************************************/

#define EM_SET_PUBLIC_ADDRESS         0x02
#define EM_SET_POWER_MODE             0x03
#define EM_SVLD                       0x04
#define EM_SET_RF_POWER_LEVEL         0x05
#define EM_POWER_MODE_CONFIGURATION   0x06
#define EM_SET_UART_BAUD_RATE         0x07
#define EM_SET_DCDC_VOLTAGE           0x08 

#define HCI_OPCODE_VS_SET_PUBLIC_ADDRESS        HCI_OPCODE(HCI_OGF_VENDOR_SPEC, EM_SET_PUBLIC_ADDRESS)
#define HCI_OPCODE_VS_SET_RF_POWER_LEVEL        HCI_OPCODE(HCI_OGF_VENDOR_SPEC, EM_SET_RF_POWER_LEVEL)
#define HCI_OPCODE_VS_POWER_MODE_CONFIGURATION  HCI_OPCODE(HCI_OGF_VENDOR_SPEC, EM_POWER_MODE_CONFIGURATION)

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

bool_t resetInProgress;

/*************************************************************************************************/
/*!
 *  \fn     hciCoreResetStart
 *        
 *  \brief  Start the HCI reset sequence.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreResetStart(void)
{
  /* Reset the controller to start the sequence */
  resetInProgress = TRUE;
  hciDrvReset();
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreResetSequence
 *        
 *  \brief  Implement the HCI reset sequence.
 *
 *  \param  pMsg    HCI event message from previous command in the sequence.
 *
 *  \return None.
 */
/*************************************************************************************************/
void hciCoreResetSequence(uint8_t *pMsg)
{
  uint16_t       opcode;
  wsfMsgHdr_t    hdr;
  static uint8_t randCnt;

  /* if event is a command complete event */
  if (*pMsg == HCI_CMD_CMPL_EVT)
  {
    /* parse parameters */
    pMsg += HCI_EVT_HDR_LEN;
    pMsg++;                   /* skip num packets */
    BSTREAM_TO_UINT16(opcode, pMsg);
    pMsg++;                   /* skip status */
    
    /* decode opcode */
    switch (opcode)
    {
      case HCI_OPCODE_RESET:
        if (resetInProgress)
        {      
          /* command complete received */
          resetInProgress = FALSE;    
            
          /* initialize rand command count */
          randCnt = 0;
          
          /* send next command in sequence */
          HciLeSetEventMaskCmd((uint8_t *) hciLeEventMask);
        }
        break;

      case HCI_OPCODE_LE_SET_EVENT_MASK:
        /* send next command in sequence */
        HciReadBdAddrCmd();
        break;

      case HCI_OPCODE_READ_BD_ADDR:
        /* address storing is handled in hciEvtProcessCmdCmpl() */

        /* send next command in sequence */
        HciLeReadBufSizeCmd();
        break;

      case HCI_OPCODE_LE_READ_BUF_SIZE:
        /* parse and store event parameters */
        BSTREAM_TO_UINT16(hciCoreCb.bufSize, pMsg);
        BSTREAM_TO_UINT8(hciCoreCb.numBufs, pMsg);

        /* initialize ACL buffer accounting */
        hciCoreCb.availBufs = hciCoreCb.numBufs;
        
        /* send next command in sequence */
        HciLeReadSupStatesCmd();
        break;

      case HCI_OPCODE_LE_READ_SUP_STATES:
        /* parse and store event parameters */
        memcpy(hciCoreCb.leStates, pMsg, HCI_LE_STATES_LEN);

        /* send next command in sequence */
        HciLeReadWhiteListSizeCmd();
        break;

      case HCI_OPCODE_LE_READ_WHITE_LIST_SIZE:
        /* parse and store event parameters */
        BSTREAM_TO_UINT8(hciCoreCb.whiteListSize, pMsg);

        /* send next command in sequence */
        HciLeRandCmd();
        break;

      case HCI_OPCODE_LE_RAND:
        /* check if need to send second rand command */
        if (randCnt == 0)
        {
          randCnt++;
          HciLeRandCmd();
        }
        else
        {
          /* Transition to BLE Sleep Mode using RC oscillator enabled */
          uint8_t value = 0x01;
          HciVendorSpecificCmd(HCI_OPCODE_VS_POWER_MODE_CONFIGURATION, 1, &value);
        }
        break;

      case HCI_OPCODE_VS_POWER_MODE_CONFIGURATION:
        /* last command in sequence; set resetting state and call callback */
        hciCb.resetting = FALSE;
        hdr.param = 1;
        hdr.event = HCI_RESET_SEQ_CMPL_CBACK_EVT;
        (*hciCb.evtCback)((hciEvt_t *) &hdr);
        break;

      default:
        break;
    }
  }
  /* else if vendor specific event */
  else if (*pMsg == HCI_VENDOR_SPEC_EVT)
  {
    /* the only possible vendor specific event is a power idle event; if received
     * during the reset sequence then it most likely means the reset event was
     * not received, so send reset event again if command complete has not been
     * received
     */
    if (resetInProgress)
    {
      HciResetCmd();
    }
  }
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreVsCmdCmplRcvd
 *        
 *  \brief  Perform internal HCI processing of vendor specific command complete events.
 *
 *  \param  opcode  HCI command opcode.
 *  \param  pMsg    Pointer to input HCI event parameter byte stream.
 *  \param  len     Parameter byte stream length.
 *
 *  \return HCI callback event code or zero.
 */
/*************************************************************************************************/
uint8_t hciCoreVsCmdCmplRcvd(uint16_t opcode, uint8_t *pMsg, uint8_t len)
{
  return HCI_VENDOR_SPEC_CMD_CMPL_CBACK_EVT;
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreVsEvtRcvd
 *        
 *  \brief  Perform internal HCI processing of vendor specific HCI events.
 *
 *  \param  p       Pointer to input HCI event parameter byte stream.
 *  \param  len     Parameter byte stream length.
 *
 *  \return HCI callback event code or zero.
 */
/*************************************************************************************************/
uint8_t hciCoreVsEvtRcvd(uint8_t *p, uint8_t len)
{
  return HCI_VENDOR_SPEC_EVT;
}

/*************************************************************************************************/
/*!
 *  \fn     hciCoreHwErrorRcvd
 *        
 *  \brief  Perform internal HCI processing of hardware error event.
 *
 *  \param  p       Pointer to input HCI event parameter byte stream.
 *
 *  \return HCI callback event code or zero.
 */
/*************************************************************************************************/
uint8_t hciCoreHwErrorRcvd(uint8_t *p)
{
  return 0;
}

/*************************************************************************************************/
/*!
 *  \fn     HciVsInit
 *        
 *  \brief  Vendor-specific controller initialization function.
 *
 *  \param  param    Vendor-specific parameter.
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciVsInit(uint8_t param)
{

}

/*************************************************************************************************/
/*!
 *  \fn     HciVsSetPublicAddr
 *        
 *  \brief  Vendor-specific set public address function.
 *
 *  \param  param    public address
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciVsSetPublicAddr(uint8_t *bdAddr)
{
  BdaCpy(hciCoreCb.bdAddr, bdAddr);
  HciVendorSpecificCmd(HCI_OPCODE_VS_SET_PUBLIC_ADDRESS, sizeof(bdAddr_t), bdAddr);
}

/*************************************************************************************************/
/*!
 *  \fn     HciVsSetTxPower
 *        
 *  \brief  Vendor-specific set RF output power function
 *
 *  \param  param    output power in dB
 *
 *  \return None.
 */
/*************************************************************************************************/
void HciVsSetTxPower(int txPower)
{
  uint8_t pout;

  if (txPower > 1) {
    pout = 7;
  } else if (txPower > -1) {
    pout = 6;
  } else if (txPower > -2) {
    pout = 5;
  } else if (txPower > -5) {
    pout = 4;
  } else if (txPower > -8) {
    pout = 3;
  } else if (txPower > -11) {
    pout = 2;
  } else if (txPower > -14) {
    pout = 1;
  } else {
    pout = 0;
  }

  HciVendorSpecificCmd(HCI_OPCODE_VS_SET_RF_POWER_LEVEL, 1, &pout);
}
