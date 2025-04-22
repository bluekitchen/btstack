/*************************************************************************************************/
/*!
 *  \file   app_ui.h
 *
 *  \brief  Application framework user interface.
 *
 *          $Date: 2015-08-25 13:59:48 -0500 (Tue, 25 Aug 2015) $
 *          $Revision: 18761 $
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
#ifndef APP_UI_H
#define APP_UI_H

#ifdef __cplusplus
extern "C" {
#endif


/**************************************************************************************************
  Macros
**************************************************************************************************/

/*! UI event enumeration  */
enum
{
  APP_UI_NONE,                            /*! No event */
  APP_UI_RESET_CMPL,                      /*! Reset complete */
  APP_UI_DISCOVERABLE,                    /*! Enter discoverable mode */
  APP_UI_ADV_START,                       /*! Advertising started */
  APP_UI_ADV_STOP,                        /*! Advertising stopped */
  APP_UI_SCAN_START,                      /*! Scanning started */
  APP_UI_SCAN_STOP,                       /*! Scanning stopped */
  APP_UI_SCAN_REPORT,                     /*! Scan data received from peer device */
  APP_UI_CONN_OPEN,                       /*! Connection opened */
  APP_UI_CONN_CLOSE,                      /*! Connection closed */
  APP_UI_SEC_PAIR_CMPL,                   /*! Pairing completed successfully */
  APP_UI_SEC_PAIR_FAIL,                   /*! Pairing failed or other security failure */
  APP_UI_SEC_ENCRYPT,                     /*! Connection encrypted */
  APP_UI_SEC_ENCRYPT_FAIL,                /*! Encryption failed */
  APP_UI_PASSKEY_PROMPT,                  /*! Prompt user to enter passkey */
  APP_UI_ALERT_CANCEL,                    /*! Cancel a low or high alert */
  APP_UI_ALERT_LOW,                       /*! Low alert */
  APP_UI_ALERT_HIGH                       /*! High alert */
};

/*! Button press enumeration */
enum
{
  APP_UI_BTN_NONE,                        /*! No button press */
  APP_UI_BTN_1_SHORT,                     /*! Button 1 short press */
  APP_UI_BTN_1_MED,                       /*! Button 1 medium press */
  APP_UI_BTN_1_LONG,                      /*! Button 1 long press */
  APP_UI_BTN_1_EX_LONG,                   /*! Button 1 extra long press */
  APP_UI_BTN_2_SHORT,                     /*! Button 2 short press */
  APP_UI_BTN_2_MED,                       /*! Button 2 medium press */
  APP_UI_BTN_2_LONG,                      /*! Button 2 long press */
  APP_UI_BTN_2_EX_LONG                    /*! Button 2 extra long press */
};

/*! LED values */
#define APP_UI_LED_NONE     0x00          /*! No LED */
#define APP_UI_LED_1        0x01          /*! LED 1 */
#define APP_UI_LED_2        0x02          /*! LED 2 */
#define APP_UI_LED_3        0x04          /*! LED 3 */
#define APP_UI_LED_4        0x08          /*! LED 4 */
#define APP_UI_LED_WRAP     0xFF          /*! Wrap to beginning of sequence */

/*! Sound tone value for wrap/repeat */
#define APP_UI_SOUND_WRAP   0xFFFF

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Button press callback */
typedef void (*appUiBtnCback_t)(uint8_t btn);

/*! Sound data structure */
typedef struct
{
  uint16_t      tone;                     /*! Sound tone in Hz.  Use 0 for silence. */
  uint16_t      duration;                 /*! Sound duration in milliseconds */
} appUiSound_t;

/*! LED data structure */
typedef struct
{
  uint8_t       led;                      /*! LED to control */
  uint8_t       state;                    /*! On or off */
  uint16_t      duration;                 /*! duration in milliseconds */
} appUiLed_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

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
void AppUiAction(uint8_t event);

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
void AppUiDisplayPasskey(uint32_t passkey);

/*************************************************************************************************/
/*!
 *  \fn     AppUiBtnRegister
 *        
 *  \brief  Register a callback function to receive button presses.
 *
 *  \param  cback   Application button callback function.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiBtnRegister(appUiBtnCback_t cback);

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
void AppUiBtnPressed(void);

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
void AppUiSoundPlay(const appUiSound_t *pSound);

/*************************************************************************************************/
/*!
 *  \fn     AppUiSoundStop
 *        
 *  \brief  Stop the sound that is currently playing.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiSoundStop(void);

/*************************************************************************************************/
/*
 *  \fn     AppUiLedStart
 *        
 *  \brief  Start LED blinking.
 *
 *  \param  pLed   Pointer to LED data structure.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiLedStart(const appUiLed_t *pLed);

/*************************************************************************************************/
/*
 *  \fn     AppUiLedStop
 *        
 *  \brief  Stop LED blinking.
 *
 *  \return None. 
 */
/*************************************************************************************************/
void AppUiLedStop(void);

/*************************************************************************************************/
/*!
 *  \fn     AppUiBtnTest
 *        
 *  \brief  Button test function-- for test purposes only.
 *
 *  \return None.
 */
/*************************************************************************************************/
void AppUiBtnTest(uint8_t btn);

#ifdef __cplusplus
};
#endif

#endif /* APP_UI_H */
