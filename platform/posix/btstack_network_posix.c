/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "btstack_network_posix.c"

/*
 * btstack_network.c
 * Implementation of the btstack_network.h interface for POSIX systems
 */


#include "btstack_network.h"

#include "btstack_config.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if_arp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <net/if.h>
#include <net/if_types.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#endif

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __linux
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

#include "btstack.h"

static int  tap_fd = -1;
static uint8_t network_buffer[BNEP_MTU_MIN];
static size_t  network_buffer_len = 0;
static char tap_dev_name[16];

#if defined(__APPLE__) || defined(__FreeBSD__)
// tuntaposx provides fixed set of tapX devices
static const char * tap_dev = "/dev/tap0";
static const char * tap_dev_name_template = "tap0";
#endif

#ifdef __linux
// Linux uses single control device to bring up tunX or tapX interface
static const char * tap_dev = "/dev/net/tun";
static const char * tap_dev_name_template =  "bnep%d";
#endif

static btstack_data_source_t tap_dev_ds;

static void (*btstack_network_send_packet_callback)(const uint8_t * packet, uint16_t size);

/*
 * @text Listing processTapData shows how a packet is received from the TAP network interface
 * and forwarded over the BNEP connection.
 * 
 * After successfully reading a network packet, the call to
 * the *bnep_can_send_packet_now* function checks, if BTstack can forward
 * a network packet now. If that's not possible, the received data stays
 * in the network buffer and the data source elements is removed from the
 * run loop. The *process_tap_dev_data* function will not be called until
 * the data source is registered again. This provides a basic flow control.
 */

/* LISTING_START(processTapData): Process incoming network packets */
static void process_tap_dev_data(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) 
{
    UNUSED(ds);
    UNUSED(callback_type);

    ssize_t len;
    len = read(ds->source.fd, network_buffer, sizeof(network_buffer));
    if (len <= 0){
        fprintf(stderr, "TAP: Error while reading: %s\n", strerror(errno));
        return;
    }

    network_buffer_len = len;

    // disable reading from netif
    btstack_run_loop_disable_data_source_callbacks(&tap_dev_ds, DATA_SOURCE_CALLBACK_READ);

    // let client now
    (*btstack_network_send_packet_callback)(network_buffer, network_buffer_len);
}

/**
 * @brief Initialize network interface
 * @param send_packet_callback
 */
void btstack_network_init(void (*send_packet_callback)(const uint8_t * packet, uint16_t size)){
    btstack_network_send_packet_callback = send_packet_callback;
}

/**
 * @text This code requries a TUN/TAP interface to connect the Bluetooth network interface
 * with the native system. It has been tested on Linux and OS X, but should work on any
 * system that provides TUN/TAP with minor modifications.
 * 
 * On Linux, TUN/TAP is available by default. On OS X, tuntaposx from
 * http://tuntaposx.sourceforge.net needs to be installed.
 *
 * The *tap_alloc* function sets up a virtual network interface with the given Bluetooth Address.
 * It is rather low-level as it sets up and configures a network interface.
 *
 * @brief Bring up network interfacd
 * @param network_address
 * @return 0 if ok
 */
int btstack_network_up(bd_addr_t network_address){

    struct ifreq ifr;
    int fd_dev;
    int fd_socket;

    if( (fd_dev = open(tap_dev, O_RDWR)) < 0 ) {
        fprintf(stderr, "TAP: Error opening %s: %s\n", tap_dev, strerror(errno));
        return -1;
    }

#ifdef __linux
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI; 
    strncpy(ifr.ifr_name, tap_dev_name_template, IFNAMSIZ);  // device name pattern

    int err;
    if( (err = ioctl(fd_dev, TUNSETIFF, (void *) &ifr)) < 0 ) {
        fprintf(stderr, "TAP: Error setting device name: %s\n", strerror(errno));
        close(fd_dev);
        return -1;
    }  
    strcpy(tap_dev_name, ifr.ifr_name);
#endif
#ifdef __APPLE__
    strcpy(tap_dev_name, tap_dev_name_template);
#endif    

    fd_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd_socket < 0) {
        close(fd_dev);
        fprintf(stderr, "TAP: Error opening netlink socket: %s\n", strerror(errno));
        return -1;
    }

    // Configure the MAC address of the newly created bnep(x) 
    // device to the local bd_address
    memset (&ifr, 0, sizeof(struct ifreq));
    strcpy(ifr.ifr_name, tap_dev_name);
#ifdef __linux
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    memcpy(ifr.ifr_hwaddr.sa_data, network_address, sizeof(bd_addr_t));
    if (ioctl(fd_socket, SIOCSIFHWADDR, &ifr) == -1) {
        close(fd_dev);
        close(fd_socket);
        fprintf(stderr, "TAP: Error setting hw addr: %s\n", strerror(errno));
        exit(1);
        return -1;
    }
#endif
#ifdef __APPLE__
    ifr.ifr_addr.sa_len = ETHER_ADDR_LEN;
    ifr.ifr_addr.sa_family = AF_LINK;
    (void)memcpy(ifr.ifr_addr.sa_data, network_address, ETHER_ADDR_LEN);
    if (ioctl(fd_socket, SIOCSIFLLADDR, &ifr) == -1) {
        close(fd_dev);
        close(fd_socket);
        fprintf(stderr, "TAP: Error setting hw addr: %s\n", strerror(errno));
        exit(1);
        return -1;
}
#endif    

    // Bring the interface up
    if (ioctl(fd_socket, SIOCGIFFLAGS, &ifr) == -1) {
        close(fd_dev);
        close(fd_socket);
        fprintf(stderr, "TAP: Error reading interface flags: %s\n", strerror(errno));
        return -1;
    }

    if ((ifr.ifr_flags & IFF_UP) == 0) {
        ifr.ifr_flags |= IFF_UP;

        if (ioctl(fd_socket, SIOCSIFFLAGS, &ifr) == -1) {
            close(fd_dev);
            close(fd_socket);
            fprintf(stderr, "TAP: Error set IFF_UP: %s\n", strerror(errno));
            return -1;
        }
    }

    close(fd_socket);

    tap_fd = fd_dev;
    log_info("BNEP device \"%s\" allocated", tap_dev_name);

    /* Create and register a new runloop data source */
    btstack_run_loop_set_data_source_fd(&tap_dev_ds, tap_fd);
    btstack_run_loop_set_data_source_handler(&tap_dev_ds, &process_tap_dev_data);
    btstack_run_loop_add_data_source(&tap_dev_ds);
    btstack_run_loop_enable_data_source_callbacks(&tap_dev_ds, DATA_SOURCE_CALLBACK_READ);

    return 0;
}

/**
 * @brief Get network name after network was activated
 * @note e.g. tapX on Linux, might not be useful on all platforms
 * @returns network name
 */
const char * btstack_network_get_name(void){
    return tap_dev_name;
}

/**
 * @brief Bring up network interface
 * @param network_address
 * @return 0 if ok
 */
int btstack_network_down(void){
    log_info("BNEP channel closed");
    btstack_run_loop_remove_data_source(&tap_dev_ds);
    if (tap_fd >= 0){
        close(tap_fd);
    }
    tap_fd = -1;
    return 0;
}

/** 
 * @brief Receive packet on network interface, e.g., forward packet to TCP/IP stack 
 * @param packet
 * @param size
 */
void btstack_network_process_packet(const uint8_t * packet, uint16_t size){

    if (tap_fd < 0) return;
    // Write out the ethernet frame to the tap device 

    int rc = write(tap_fd, packet, size);
    if (rc < 0) {
        log_error("TAP: Could not write to TAP device: %s", strerror(errno));
    } else 
    if (rc != size) {
        log_error("TAP: Package written only partially %d of %d bytes", rc, size);
    }
}

/** 
 * @brief Notify network interface that packet from send_packet_callback was sent and the next packet can be delivered.
 */
void btstack_network_packet_sent(void){

    network_buffer_len = 0;

    // Re-enable the tap device data source
    btstack_run_loop_enable_data_source_callbacks(&tap_dev_ds, DATA_SOURCE_CALLBACK_READ);
}
