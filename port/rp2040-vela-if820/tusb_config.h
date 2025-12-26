#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// Board Configuration
//--------------------------------------------------------------------+

#define CFG_TUSB_MCU              OPT_MCU_RP2040
// #define CFG_TUSB_OS               OPT_OS_NONE
#define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define BOARD_DEVICE_RHPORT_NUM   0

// Use pico SDK's main loop instead of TinyUSB RTOS integration
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN        __attribute__ ((aligned(4)))

//--------------------------------------------------------------------+
// Common Configuration
//--------------------------------------------------------------------+

#define CFG_TUD_ENDPOINT0_SIZE    64

//--------------------------------------------------------------------+
// Device Configuration
//--------------------------------------------------------------------+

// Enable the classes in CMakeFile
#define CFG_TUD_CDC               1
#define CFG_TUD_AUDIO             1

// CDC FIFO sizes
#define CFG_TUD_CDC_RX_BUFSIZE    256
#define CFG_TUD_CDC_TX_BUFSIZE    256

//--------------------------------------------------------------------
// AUDIO CLASS DRIVER CONFIGURATION
//--------------------------------------------------------------------

// Allow volume controlled by on-baord button
#define CFG_TUD_AUDIO_ENABLE_INTERRUPT_EP                            1

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                                TUD_AUDIO_HEADSET_STEREO_DESC_LEN

// How many formats are used, need to adjust USB descriptor if changed
#define CFG_TUD_AUDIO_FUNC_1_N_FORMATS                               1

// Audio format type I specifications
/* 24bit/48kHz is the best quality for headset or 24bit/96kHz for 2ch speaker,
   high-speed is needed beyond this */
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE                         48000
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX                           2
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX                           2

// 16bit in 16bit slots
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX          2
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX                  16
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX          2
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX                  16


// EP and buffer size - for isochronous EP´s, the buffer and EP size are equal (different sizes would not make sense)
#define CFG_TUD_AUDIO_ENABLE_EP_IN                1

#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN    TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)

#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ      CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN*2
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX         CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN // Maximum EP IN size for all AS alternate settings used

// EP and buffer size - for isochronous EP´s, the buffer and EP size are equal (different sizes would not make sense)
#define CFG_TUD_AUDIO_ENABLE_EP_OUT               1

#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT   TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)

#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ     CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT*2
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX        CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT // Maximum EP IN size for all AS alternate settings used

// Number of Standard AS Interface Descriptors (4.9.1) defined per audio function - this is required to be able to remember the current alternate settings of these interfaces - We restrict us here to have a constant number for all audio functions (which means this has to be the maximum number of AS interfaces an audio function has and a second audio function with less AS interfaces just wastes a few bytes)
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT 	          2

// Size of control request buffer
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ	64

// USB Interface enumeration
enum {
    // CDC Requires two interfaces
    ITF_NUM_CDC_CONTROL = 0,
    ITF_NUM_CDC_DATA,
#if PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE
    ITF_NUM_RPI_RESET,
#endif
#if CFG_TUD_AUDIO
    // Audio requires three interfaces
    ITF_NUM_AUDIO_CONTROL,
    ITF_NUM_AUDIO_STREAMING_SPK,
    ITF_NUM_AUDIO_STREAMING_MIC,
#endif
    ITF_NUM_TOTAL
};

// 3 for bi-directional
#define ITF_NUM_AUDIO_INTERFACES   3

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
