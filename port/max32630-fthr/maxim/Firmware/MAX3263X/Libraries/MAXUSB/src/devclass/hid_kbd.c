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
 * $Date: 2017-10-16 17:48:14 -0500 (Mon, 16 Oct 2017) $ 
 * $Revision: 31410 $
 *
 ******************************************************************************/

#include <string.h>
#include "usb.h"
#include "enumerate.h"
#include "usb_event.h"
#include "hid_kbd.h"

/***** File Scope Data *****/

static uint8_t notify_ep;

/* Idle is set to '0' for infinity. Changing Idle is not currently supported. */
#ifdef __IAR_SYSTEMS_ICC__
#pragma data_alignment = 4
#elif __GNUC__
__attribute__((aligned(4)))
#endif
static uint8_t hid_idle = 0;

static uint8_t send_key;

#ifdef __IAR_SYSTEMS_ICC__
#pragma data_alignment = 4
#elif __GNUC__
__attribute__((aligned(4)))
#endif
static uint8_t hid_report[3];

static usb_req_t notifyreq;

static const hid_descriptor_t *hid_desc;
static const uint8_t *report_desc;


/***** Function Prototypes *****/
static void getdescriptor(usb_setup_pkt *sud, const uint8_t **desc, uint16_t *desclen);
static int class_req(usb_setup_pkt *sud, void *cbdata);
static void notification_cb(void *cbdata);


/******************************************************************************/
int hidkbd_init(const hid_descriptor_t *hid_descriptor, const uint8_t *report_descriptor)
{
  hid_desc = hid_descriptor;
  /* NOTE: report_descriptor must be 32-bit aligned on ARM platforms. */
  report_desc = report_descriptor;

  /* This callback handles class-specific GET_DESCRIPTOR requests */
  enum_register_getdescriptor(getdescriptor);

  /* Handle class-specific SETUP requests */
  enum_register_callback(ENUM_CLASS_REQ, class_req, NULL);

  return 0;
}

/******************************************************************************/
int hidkbd_configure(unsigned int ep)
{
  int err;

  // Configure the endpoint
  notify_ep = ep;
  if ((err = usb_config_ep(ep, MAXUSB_EP_TYPE_IN, sizeof(hid_report))) != 0) {
    return err;
  }

  // No keys pressed
  hid_report[0] = 0x00;
  hid_report[1] = 0x00;
  hid_report[2] = 0x00;
  send_key = 0;

  // Prepare and send an initial report with no keys pressed */
  memset(&notifyreq, 0, sizeof(usb_req_t));
  notifyreq.ep = ep;
  notifyreq.data = hid_report;
  notifyreq.reqlen = 3;
  notifyreq.callback = notification_cb;
  notifyreq.cbdata = &notifyreq;
  if ((err = usb_write_endpoint(&notifyreq)) != 0) {
    return err;
  }

  return 0;
}

/******************************************************************************/
int hidkbd_deconfigure(void)
{
  /* deconfigure EP */
  if (notify_ep != 0) {
    usb_reset_ep(notify_ep);
    notify_ep = 0;
  }

  return 0;
}

/******************************************************************************/
int hidkbd_keypress(uint8_t key)
{
  int result;

  /* only send updates if configured */
  if (enum_getconfig() == 0) {
    return -1;
  }

  if (notifyreq.reqlen == 0) {
    // There is no notify pending. Send the notification immediately.
    if ((result = hidkbd_encode_report(hid_report, &key, 1)) < 0) {
      return -1;
    }
    notifyreq.data = hid_report;
    notifyreq.reqlen = result;
    return usb_write_endpoint(&notifyreq);
  } else {
    // There is a notify pending. Save this key for the next notification.
    send_key = key;
  }

  return 0;
}

/******************************************************************************/
static void notification_cb(void *cbdata)
{
  int result;
  usb_req_t *req = (usb_req_t*)cbdata;

  if (enum_getconfig() == 0) {
    // Send nothing while not configured
    req->reqlen = 0;
  } else if (send_key && ((result = hidkbd_encode_report(hid_report, &send_key, 1)) > 0)) {
    // There is a key pending. Send the report.
    req->data = hid_report;
    req->reqlen = result;
    usb_write_endpoint(req);
  } else if ((req->reqlen > 0) && (hid_report[2] != 0)) {
    // There is no key pending. Send 'No keys pressed.'
    hid_report[0] = 0x00;
    hid_report[1] = 0x00;
    hid_report[2] = 0x00;
    req->data = hid_report;
    req->reqlen = 3;
    usb_write_endpoint(req);
  } else {
    // Nothing else to send now
    req->reqlen = 0;
  }

  send_key = 0;
}

/******************************************************************************/
/*
 * ASCII-to-HID mapping (partial)
 *
 * HID Keyboard/Keypad Page codes do not map directly to ASCII, unfortunately.
 * This partial mapping of the most commonly used codes is provided for 
 *  demonstration. Please refer to the HID Usage Tables 1.12 section 10 
 *  "Keyboard/Keypad Page (0x07)" section for a complete listing.
 *
 * A table look-up could be implemented at the cost of initialized data space.
 * 
 * rpt must point to 3 * num bytes of storage, as each report is 3 bytes
 *
 */
int hidkbd_encode_report(uint8_t *rpt, uint8_t *ascii, int num)
{
  int rpt_count = 0;
  uint8_t c, shift, code;

  while (num--) {
    c = *ascii++;
    code = shift = 0;

    /* Is it a letter? */
    if (((c & ~0x20) > 0x40) && ((c & ~0x20) < 0x5b)) {
      /* Shift for upper case is 0x02 */
      /* This is inverted from ASCII */
      if ((c & 0x20) == 0) {
        shift = 0x02;
      }
      /* Re-align ASCII to HID codes */
      code = (c & ~0x20) - 0x3d;
    } else {
      /* Is it a number 0-9? */
      if ((c >= 0x30) && (c < 0x3a)) {
        /* Re-align ASCII to HID codes */
        if (c == 0x30) {
          code = 0x27;
        } else {
          code = c - 0x13;
        }
      } else {
        switch (c) {
          case 0x0a:  /* New line */
          case 0x0d:  /* Carriage return */
            code = 0x28;  /* Enter */
            break;
          case 0x08:  /* Backspace */
            code = 0x2a;
            break;
          case 0x1b:  /* Escape */
            code = 0x29;
            break;
          case 0x20:  /* Space */
            code = 0x2c;
            break;
          case 0x2c:  /* Comma */
            code = 0x36;
            break;
          case 0x2e:  /* Period */
            code = 0x37;
            break;
          case 0x21:  /* Exclamation point */
            shift = 0x02;
            code = 0x1e;
            break;
          default:
            /* Others mapped to question mark */
            shift = 0x02;
            code = 0x38;
            break;
        }
      }
    }

    /* Construct report */
    *rpt++ = shift;
    *rpt++ = 0x00;
    *rpt++ = code;
    rpt_count += 3;
  }

  return rpt_count;
}

/******************************************************************************/
static int class_req(usb_setup_pkt *sud, void *cbdata)
{
  int result = -1;
  static usb_req_t ep0req;

  if ((((sud->bmRequestType & RT_RECIP_MASK) & RT_RECIP_IFACE) == 1) && (sud->wIndex == 0)) {

    switch (sud->bRequest) {
      case HID_GET_REPORT:
        /* The host should not use this as a substitute for the Interrupt EP */
        /* so we simply send a "No keys" report when queried this way. This */
        /* could be changed to send the next key in the queue, if desired. */
        hid_report[0] = 0;
        hid_report[1] = 0;
        hid_report[2] = 0;

        ep0req.ep = 0;
        ep0req.data = hid_report;
        ep0req.reqlen = 3;
        ep0req.callback = NULL;
        ep0req.cbdata = NULL;
        result = usb_write_endpoint(&ep0req);
	if (!result) {
	    result = 1;
	}
        break;
      case HID_GET_IDLE:
        /* We don't support Report ID != 0 */
        if ((sud->wValue & 0xff) == 0) {
          ep0req.ep = 0;
          ep0req.data = &hid_idle;
          ep0req.reqlen = 1;
          ep0req.callback = NULL;
          ep0req.cbdata = NULL;
          result = usb_write_endpoint(&ep0req);
	  if (!result) {
	      result = 1;
	  }
        }       
        break;
      case HID_GET_PROTOCOL:
        /* Stall */
        break;
      case HID_SET_REPORT:
        if (sud->wLength <= 64) {
          /* Accept and ignore */
          result = 0;
        }
        break;
      case HID_SET_IDLE:
        /* We don't support Report ID != 0 */
        if ((sud->wValue & 0xff) == 0)  {
          /* Idle is set to '0' for infinity. Changing Idle is not currently supported. */
          if ((sud->wValue >> 8) == 0) {
            result = 0;
          }
        }
        break;
      case HID_SET_PROTOCOL:
        /* Stall */
        break;
      default:
        /* Stall */
        break;
    }
  }  

  return result;
}

/******************************************************************************/
/* Reply with HID and Report descriptors */
static void getdescriptor(usb_setup_pkt *sud, const uint8_t **desc, uint16_t *desclen)
{
  // The HID descriptor in the configuration descriptor may not be 32-bit aligned
#ifdef __IAR_SYSTEMS_ICC__
#pragma data_alignment = 4
  static hid_descriptor_t hid_descriptor;
#elif __GNUC__
  static __attribute__((aligned(4))) hid_descriptor_t hid_descriptor;
#else
  static hid_descriptor_t hid_descriptor;
#endif

  *desc = NULL;
  *desclen = 0;

  switch (sud->wValue >> 8) {
    case DESC_HID:
      if (sud->wIndex == 0) {
        /* Copy descriptor into aligned structure from hid_desc source pointer. */
        memcpy(&hid_descriptor, hid_desc, sizeof(hid_descriptor_t));
        *desc = (uint8_t*)&hid_descriptor;
        *desclen = hid_descriptor.bFunctionalLength;
      }
      break;
    case DESC_REPORT:
      if (sud->wIndex ==0) {
        *desc = report_desc;
        *desclen = hid_desc->wDescriptorLength;
      }
      break;
    default:
      /* Stall */
      break;
  }
}
