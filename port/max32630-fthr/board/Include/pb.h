/**
 * @file
 * @brief   Pushbutton driver header file.
 */
/* ****************************************************************************
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
 * $Date: 2017-02-28 17:26:15 -0600 (Tue, 28 Feb 2017) $
 * $Revision: 26771 $
 *
 *************************************************************************** */

#ifndef _PB_H_
#define _PB_H_

#include "gpio.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @ingroup bsp
 * @defgroup pushbutton_evkit Push button driver board support
 * @{
 */
/* **** Global Variables **** */
extern const gpio_cfg_t pb_pin[];
extern const unsigned int num_pbs;

/* **** Function Prototypes **** */

/**
 * @brief      Initialize all push buttons.
 * @retval     #E_NO_ERROR  Push buttons intialized successfully.
 * @retval     "Error Code" @ref MXC_Error_Codes "Error Code" if unsuccessful.
 *
 */
int PB_Init(void);

/**
 * Type alias @c pb_callback for the push button callback.
 * @details The function is of type:
 * @code
 *  void pb_callback(void * pb)
 * @endcode
 * To recieve notification of a push button event, define a callback
 * function and pass it as a pointer to the PB_RegisterCallback(unsigned int pb, pb_callback callback) function.
 * @param      pb    Pointer to the push button index that triggered the
 *                   callback.
 */
typedef void (*pb_callback)(void *pb);

/**
 * @brief      Register or Unregister a callback handler for events on the @p pb push button.
 * @details
 * - Calling this function with a pointer to a function @p callback, configures the pushbutton @p pb and enables the
 * interrupt to handle the push button events.
 * - Calling this function with a <tt>NULL</tt> pointer will disable the interrupt and unregister the
 * callback function.
 * @p pb must be a value between 0 and #num_pbs.
 *
 * @param      pb        push button index to receive event callbacks.
 * @param      callback  Callback function pointer of type @c pb_callback
 * @retval     #E_NO_ERROR if configured and callback registered successfully.
 * @retval     "Error Code" @ref MXC_Error_Codes "Error Code" if unsuccessful.
 */
int PB_RegisterCallback(unsigned int pb, pb_callback callback);

/**
 * @brief   Enable a callback interrupt.
 * @note    PB_RegisterCallback must be called prior to enabling the callback interrupt.
 * @param   pb          push button index value between 0 and #num_pbs.
 */
__STATIC_INLINE void PB_IntEnable(unsigned int pb)
{
    MXC_ASSERT(pb < num_pbs);
    GPIO_IntEnable(&pb_pin[pb]);
}

/**
 * @brief   Disable a callback interrupt.
 * @param   pb          push button index
 */
__STATIC_INLINE void PB_IntDisable(unsigned int pb)
{
    MXC_ASSERT(pb < num_pbs);
    GPIO_IntDisable(&pb_pin[pb]);
}

/**
 * @brief   Clear a callback interrupt.
 * @param   pb          push button index value between 0 and #num_pbs.
 */
__STATIC_INLINE void PB_IntClear(unsigned int pb)
{
    MXC_ASSERT(pb < num_pbs);
    GPIO_IntClr(&pb_pin[pb]);
}

/**
 * @brief      Get the current state of the push button.
 * @param      pb     push button index value between 0 and #num_pbs.
 * @retval     TRUE   The button is pressed.
 * @retval     FALSE  The button is not pressed.
 */
__STATIC_INLINE int PB_Get(unsigned int pb)
{
    MXC_ASSERT(pb < num_pbs);
    return !GPIO_InGet(&pb_pin[pb]);
}
/**@}*/
#ifdef __cplusplus
}
#endif

#endif /* _PB_H_ */
