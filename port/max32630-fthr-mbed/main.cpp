/*******************************************************************************
 * Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 * Author: Ismail H. Kose <ismail.kose@maximintegrated.com>
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
 ******************************************************************************/

/**
 * @file    main.c
 * @brief   Max32630 CC2564B btstack Example
 */

#include "mbed.h"
#include "max32630fthr.h"
#include "USBSerial.h"
#include "btstack_config.h"
#include "btstack_port.h"
#include "btstack.h"

MAX32630FTHR pegasus(MAX32630FTHR::VIO_3V3);

// Hardware serial port over DAPLink
//Serial daplink(P2_1, P2_0, 115200);

// Virtual serial port over USB
//USBSerial microUSB;

DigitalOut rLED(LED1);
DigitalOut gLED(LED2);
DigitalOut bLED(LED3);

// main() runs in its own thread in the OS
// (note the calls to Thread::wait below for delays)
int main()
{
    int c;

    printf("max32630fthr btstack example\r\n");
//    daplink.printf("max32630fthr btstack - daplink serial port\r\n");
    //microUSB.printf("max32630fthr btstack example on micro USB serial port\r\n");
    rLED = LED_ON;
    gLED = LED_ON;
    bLED = LED_OFF;

    rLED = LED_OFF;
	printf("%s:%d - \r\n", __func__, __LINE__);
	bluetooth_main();

	printf("###### %s:%d - \r\n", __func__, __LINE__);
	while(1);

    while(1) {
       // c = microUSB.getc();
       // microUSB.putc(c);
        //daplink.putc(c);
        //bLED = c & 1;
		hal_btstack_run_loop_execute_once();
		printf("%s:%d - \r\n", __func__, __LINE__);
    }
}

