/*
 * TinyUSB host configuration, based on the HCI USB-to-UART bridge port.
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// Pico SDK 2.3's TinyUSB integration supplies the MCU and OS through compiler
// definitions. pico.h provides the RP2040 placement attributes used by
// TinyUSB 0.21's host implementation.
#include "pico.h"

//--------------------------------------------------------------------
// Common Configuration
//--------------------------------------------------------------------

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

#ifndef CFG_TUH_MEM_SECTION
#define CFG_TUH_MEM_SECTION
#endif

#ifndef CFG_TUH_MEM_ALIGN
#define CFG_TUH_MEM_ALIGN __attribute__((aligned(4)))
#endif

//--------------------------------------------------------------------
// Host Configuration
//--------------------------------------------------------------------

#define CFG_TUH_ENABLED 1

#if CFG_TUSB_MCU == OPT_MCU_RP2040
// #define CFG_TUH_RPI_PIO_USB 1 // Use PIO USB as host controller.
// #define CFG_TUH_MAX3421 1     // Use MAX3421 as host controller.

#if (defined(CFG_TUH_RPI_PIO_USB) && CFG_TUH_RPI_PIO_USB) || \
    (defined(CFG_TUH_MAX3421) && CFG_TUH_MAX3421)
#define BOARD_TUH_RHPORT 1
#endif
#endif

#define CFG_TUH_MAX_SPEED BOARD_TUH_MAX_SPEED

#ifndef BOARD_TUH_RHPORT
#define BOARD_TUH_RHPORT 0
#endif

#ifndef BOARD_TUH_MAX_SPEED
#define BOARD_TUH_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// Driver Configuration
//--------------------------------------------------------------------

#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HUB 1
#define CFG_TUH_DEVICE_MAX (3 * CFG_TUH_HUB + 1)

// The Bluetooth HCI transport will use TinyUSB's raw endpoint-transfer API.
#define CFG_TUH_API_EDPT_XFER 1
#define CFG_TUH_ENDPOINT_MAX 8

#ifdef __cplusplus
}
#endif

#endif
