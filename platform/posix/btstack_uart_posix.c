/*
 * Copyright (C) 2016 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/*
 *  btstack_uart_posix.c
 *
 *  Common code to access serial port via POSIX interface
 *  Used by hci_transport_h4_posix.c and hci_transport_h5.posix
 *
 */

#include "btstack_uart_posix.h"
#include "btstack_debug.h"

#include <termios.h>  /* POSIX terminal control definitions */
#include <fcntl.h>    /* File control definitions */
// #include <unistd.h>   /* UNIX standard function definitions */
// #include <stdio.h>
// #include <string.h>

int btstack_uart_posix_set_baudrate(int fd, uint32_t baudrate){
    log_info("h4_set_baudrate %u", baudrate);

    struct termios toptions;

    if (tcgetattr(fd, &toptions) < 0) {
        perror("init_serialport: Couldn't get term attributes");
        return -1;
    }
    
    speed_t brate = baudrate; // let you override switch below if needed
    switch(baudrate) {
        case 57600:  brate=B57600;  break;
        case 115200: brate=B115200; break;
#ifdef B230400
        case 230400: brate=B230400; break;
#endif
#ifdef B460800
        case 460800: brate=B460800; break;
#endif
#ifdef B921600
        case 921600: brate=B921600; break;
#endif

// Hacks to switch to 2/3 mbps on FTDI FT232 chipsets
// requires special config in Info.plist or Registry
        case 2000000: 
#if defined(HAVE_POSIX_B300_MAPPED_TO_2000000)
            log_info("hci_transport_posix: using B300 for 2 mbps");
            brate=B300; 
#elif defined(HAVE_POSIX_B1200_MAPPED_TO_2000000)
           log_info("hci_transport_posix: using B1200 for 2 mbps");
            brate=B1200;
#endif
            break;
        case 3000000:
#if defined(HAVE_POSIX_B600_MAPPED_TO_3000000)
            log_info("hci_transport_posix: using B600 for 3 mbps");
            brate=B600;
#elif defined(HAVE_POSIX_B2400_MAPPED_TO_3000000)
            log_info("hci_transport_posix: using B2400 for 3 mbps");
            brate=B2400;
#endif
            break;
        default:
            break;
    }
    cfsetospeed(&toptions, brate);
    cfsetispeed(&toptions, brate);

    if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
        perror("init_serialport: Couldn't set term attributes");
        return -1;
    }

    return 0;
}
