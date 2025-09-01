/*************************************************************************************************/
/*!
 *  \file   app_ui.c
 *
 *  \brief  Application framework user interface.
 *
 *          $Date: 2014-08-21 16:34:14 -0500 (Thu, 21 Aug 2014) $
 *          $Revision: 14797 $
 *
 *  Copyright (c) 2011 Wicentric, Inc., all rights reserved.
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
#include "wsf_os.h"
#include "wsf_trace.h"
#include "app_ui.h"

/**************************************************************************************************
  Local Variables
**************************************************************************************************/

/* Button press handler struct */
static struct
{
  appUiBtnCback_t     cback;
} appUiBtn;

/*************************************************************************************************/
/*!
 *  \fn     AppUiAction
 *        
 *  \brief  Perform a user interface action based on the event value passed to the function.
 *
 *  \param  event   User interface event value.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiAction(uint8_t event)
{
  switch (event)
  {
    case APP_UI_NONE:
      /* no action */
      break;
      
    case APP_UI_RESET_CMPL:
      APP_TRACE_INFO0(">>> Reset complete <<<");
      break;

    case APP_UI_ADV_START:
      APP_TRACE_INFO0(">>> Advertising started <<<");
      break;

    case APP_UI_ADV_STOP:
      APP_TRACE_INFO0(">>> Advertising stopped <<<");
      break;

    case APP_UI_SCAN_START:
      APP_TRACE_INFO0(">>> Scanning started <<<");
      break;

    case APP_UI_SCAN_STOP:
      APP_TRACE_INFO0(">>> Scanning stopped <<<");
      break;

    case APP_UI_SCAN_REPORT:
      APP_TRACE_INFO0(">>> Scan data received from peer <<<");
      break;

    case APP_UI_CONN_OPEN:
      APP_TRACE_INFO0(">>> Connection opened <<<");
      break;

    case APP_UI_CONN_CLOSE:
      APP_TRACE_INFO0(">>> Connection closed <<<");
      break;

    case APP_UI_SEC_PAIR_CMPL:
      APP_TRACE_INFO0(">>> Pairing completed successfully <<<");
      break;

    case APP_UI_SEC_PAIR_FAIL:
      APP_TRACE_INFO0(">>> Pairing failed <<<");
      break;

    case APP_UI_SEC_ENCRYPT:
      APP_TRACE_INFO0(">>> Connection encrypted <<<");
      break;

    case APP_UI_SEC_ENCRYPT_FAIL:
      APP_TRACE_INFO0(">>> Encryption failed <<<");
      break;

    case APP_UI_PASSKEY_PROMPT:
      APP_TRACE_INFO0(">>> Prompt user to enter passkey <<<");
      break;

    case APP_UI_ALERT_CANCEL:
      APP_TRACE_INFO0(">>> Cancel a low or high alert <<<");
      break;

    case APP_UI_ALERT_LOW:
      APP_TRACE_INFO0(">>> Low alert <<<");
      break;

    case APP_UI_ALERT_HIGH:
      APP_TRACE_INFO0(">>> High alert <<<");
      break;
  }
}

/*************************************************************************************************/
/*!
 *  \fn     AppUiDisplayPasskey
 *        
 *  \brief  Display a passkey.
 *
 *  \param  passkey   Passkey to display.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiDisplayPasskey(uint32_t passkey)
{
  APP_TRACE_INFO1(">>> Passkey: %d <<<", passkey);
}

/*************************************************************************************************/
/*!
 *  \fn     appUiTimerExpired
 *        
 *  \brief  Handle a UI timer expiration event.
 *
 *  \param  pMsg    Pointer to message.
 *
 *  \return None.
 */
/*************************************************************************************************/
void appUiTimerExpired(wsfMsgHdr_t *pMsg)
{

}

/*************************************************************************************************/
/*!
 *  \fn     AppUiBtnPoll
 *        
 *  \brief  Perform button press polling.  This function is called to handle WSF
 *          message APP_BTN_POLL_IND.
 *
 *  \return None.
 */
/*************************************************************************************************/
void appUiBtnPoll(void)
{

}

/*************************************************************************************************/
/*!
 *  \fn     AppUiBtnRegister
 *        
 *  \brief  Register a callback function to receive button presses.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiBtnRegister(appUiBtnCback_t cback)
{
  appUiBtn.cback = cback;
}

/*************************************************************************************************/
/*!
 *  \fn     AppUiBtnPressed
 *        
 *  \brief  Handle a hardware button press.  This function is called to handle WSF
 *          event APP_BTN_DOWN_EVT.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiBtnPressed(void)
{

}

/*************************************************************************************************/
/*!
 *  \fn     AppUiSoundPlay
 *        
 *  \brief  Play a sound.
 *
 *  \param  pSound   Pointer to sound tone/duration array.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiSoundPlay(const appUiSound_t *pSound)
{

}

/*************************************************************************************************/
/*!
 *  \fn     AppUiSoundStop
 *        
 *  \brief  Stop the sound that is currently playing.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiSoundStop(void)
{

}

/*************************************************************************************************/
/*!
 *  \fn     AppUiBtnTest
 *        
 *  \brief  Button test function-- for test purposes only.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiBtnTest(uint8_t btn)
{
  (*appUiBtn.cback)(btn);
}

