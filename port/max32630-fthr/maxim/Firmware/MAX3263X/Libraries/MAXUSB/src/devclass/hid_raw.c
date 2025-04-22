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
#include "hid_raw.h"
#include "fifo.h"

/***** Definitions *****/

#define FIFO_SIZE         (HID_MAX_PACKET + 1)

/***** File Scope Data *****/

// Endpoint configuration
static hid_cfg_t hid_cfg;

// Write (IN) data
static usb_req_t wreq;
#ifdef __IAR_SYSTEMS_ICC__
#pragma data_alignment = 4
#elif __GNUC__
__attribute__((aligned(4)))
#endif
static uint8_t wepbuf[HID_MAX_PACKET];
static uint8_t wbuf[FIFO_SIZE];
static fifo_t wfifo;

// Read (OUT) data
static volatile int rreq_complete;
static usb_req_t rreq;
#ifdef __IAR_SYSTEMS_ICC__
#pragma data_alignment = 4
#elif __GNUC__
__attribute__((aligned(4)))
#endif
static uint8_t repbuf[HID_MAX_PACKET];
static uint8_t rbuf[FIFO_SIZE];
static fifo_t rfifo;

static const hid_descriptor_t *hid_desc;
static const uint8_t *report_desc;

static int (*callback)(void);

/***** Function Prototypes *****/
static void getdescriptor(usb_setup_pkt *sud, const uint8_t **desc, uint16_t *desclen);
static int class_req(usb_setup_pkt *sud, void *cbdata);
static void svc_out_from_host(void);
static void out_callback(void *cbdata);
static void svc_in_to_host(void *cbdata);

/******************************************************************************/
int hidraw_init(const hid_descriptor_t *hid_descriptor, const uint8_t *report_descriptor)
{
  memset(&hid_cfg, 0, sizeof(hid_cfg_t));
  callback = NULL;

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
int hidraw_configure(const hid_cfg_t *cfg)
{
  int err;

  if ( (cfg->out_maxpacket > HID_MAX_PACKET) ||
       (cfg->in_maxpacket > HID_MAX_PACKET) ) {
    return -1;
  }

  /* Configure the endpoints */
  if ((err = usb_config_ep(cfg->out_ep, MAXUSB_EP_TYPE_OUT, cfg->out_maxpacket)) != 0) {
    hidraw_deconfigure();
    return err;
  }
  if ((err = usb_config_ep(cfg->in_ep, MAXUSB_EP_TYPE_IN, cfg->in_maxpacket)) != 0) {
    hidraw_deconfigure();
    return err;
  }

  /* Prepare IN */
  fifo_init(&wfifo, wbuf, FIFO_SIZE);
  wreq.ep = cfg->in_ep;
  wreq.data = wepbuf;
  wreq.reqlen = 0;
  wreq.callback = svc_in_to_host;
  wreq.cbdata = &wreq;
  wreq.type = MAXUSB_TYPE_TRANS;

  /* Prepare OUT schedule initial request */
  fifo_init(&rfifo, rbuf, FIFO_SIZE);
  rreq_complete = 0;
  rreq.ep = cfg->out_ep;
  rreq.data = repbuf;
  rreq.reqlen = cfg->out_maxpacket;
  rreq.callback = out_callback;
  rreq.cbdata = &rreq;
  rreq.type = MAXUSB_TYPE_PKT;
  usb_read_endpoint(&rreq);

  /* Save the configuration */
  memcpy(&hid_cfg, cfg, sizeof(hid_cfg_t));

  return 0;
}

/******************************************************************************/
int hidraw_deconfigure(void)
{
  /* Deconfigure EPs */
  if (hid_cfg.out_ep != 0) {
    usb_reset_ep(hid_cfg.out_ep);
  }
  if (hid_cfg.in_ep != 0) {
    usb_reset_ep(hid_cfg.in_ep);
  }

  /* clear driver state */
  fifo_clear(&wfifo);
  fifo_clear(&rfifo);
  memset(&hid_cfg, 0, sizeof(hid_cfg_t));

  return 0;
}

/******************************************************************************/
void hidraw_register_callback(int (*func)(void))
{
  callback = func;
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
        ep0req.ep = 0;
        ep0req.data = NULL;
        ep0req.reqlen = 0;
        ep0req.callback = NULL;
        ep0req.cbdata = NULL;
        result = usb_write_endpoint(&ep0req);
	if (!result) {
	  /* Has data stage */
	  result = 1;
	}
        break;
      case HID_GET_IDLE:
        /* Stall */
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
int hidraw_read(uint8_t *data, unsigned int len)
{
  unsigned int i;
  uint8_t byte;

  for (i = 0; i < len; i++) {
    while (fifo_get8(&rfifo, &byte) != 0) {
      /* Write available data into the FIFO */
      svc_out_from_host();
    }

    data[i] = byte;
  }

  return i;
}

/******************************************************************************/
int hidraw_canread(void)
{
  /* Write available data into the FIFO first */
  svc_out_from_host();

  return fifo_level(&rfifo);
}

/******************************************************************************/
int hidraw_write(const uint8_t *data, unsigned int len)
{
  unsigned int i = 0;

  if (enum_getconfig() == 0) {
    return -1;
  }

  // Write data into the FIFO
  for (i = 0; i < len; i++) {
    if (fifo_put8(&wfifo, data[i]) != 0) {
      break;
    }
  }

  // If there is not active request, start one
  if (wreq.reqlen == 0) {
    svc_in_to_host(&wreq);
  }

  // Write remaining data to FIFO
  while (i < len) {
    if (fifo_put8(&wfifo, data[i]) == 0) {
      i++;
    }
  }

  return i;
}

/******************************************************************************/
static void svc_out_from_host(void)
{
  int newdata = 0;

  if (rreq_complete) {
    // Copy as much data into the local buffer as possible
    for (; rreq.actlen > 0; rreq.actlen--) {
      if (fifo_put8(&rfifo, *rreq.data) != 0) {
        break;
      }
      newdata = 1;
      rreq.data++;
    }

    /* After all of the data has been consumed, register the next request if
     * still configured. An error will occur when the host has been
     * disconnected.
     */
    if ((rreq.actlen == 0) && (rreq.error_code == 0) && (hid_cfg.out_ep > 0)) {
      rreq_complete = 0;
      rreq.data = repbuf;
      rreq.error_code = 0;
      usb_read_endpoint(&rreq);
    }
  }

  // Call the callback if there is new data
  if (newdata && (callback != NULL)) {
    callback();
  }
}

/******************************************************************************/
static void out_callback(void *cbdata)
{
  rreq_complete = 1;
  svc_out_from_host();
}

/******************************************************************************/
static void svc_in_to_host(void *cbdata)
{
  int i;
  uint8_t byte;

  // An error will occur when the host has been disconnected.
  // Register the next request if still configured and there is data to send
  if ((wreq.error_code == 0) && (hid_cfg.in_ep > 0) && !fifo_empty(&wfifo)) {
    for (i = 0; i < sizeof(wepbuf); i++) {
      if (fifo_get8(&wfifo, &byte) != 0) {
        break;
      }
      wepbuf[i] = byte;
    }

    wreq.data = wepbuf;
    wreq.reqlen = i;
    wreq.actlen = 0;

    // Register the next request
    usb_write_endpoint(&wreq);
  } else {
    // Clear the request length to indicate that there is not an active request
    wreq.reqlen = 0;
    wreq.error_code = 0;
  }
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
