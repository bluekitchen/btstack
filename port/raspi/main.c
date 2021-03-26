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

#define __BTSTACK_FILE__ "main.c"

// *****************************************************************************
//
//   Port for Rasperry Pi with built-in BCM chipset via H4 or H5
//
// *****************************************************************************

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "btstack_config.h"

#include "btstack_debug.h"
#include "btstack_event.h"
#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_posix.h"
#include "bluetooth_company_id.h"
#include "hci.h"
#include "hci_dump.h"
#include "hci_dump_posix_fs.h"
#include "hci_transport.h"
#include "hci_transport_h4.h"
#include "hci_transport_h5.h"
#include "btstack_stdin.h"
#include "btstack_tlv_posix.h"
#include "btstack_uart.h"
#include "btstack_uart_block.h"
#include "btstack_uart_slip_wrapper.h"

#include "btstack_chipset_bcm.h"
#include "btstack_chipset_bcm_download_firmware.h"
#include "btstack_control_raspi.h"


#include "raspi_get_model.h"

int btstack_main(int argc, const char * argv[]);

typedef enum  {
    UART_INVALID,
    UART_SOFTWARE_NO_FLOW,
    UART_HARDWARE_NO_FLOW,
    UART_HARDWARE_FLOW
} uart_type_t;

// default config, updated depending on RasperryPi UART configuration
static hci_transport_config_uart_t transport_config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    0,       // main baudrate
    0,       // flow control
    NULL,
};

static btstack_uart_config_t uart_config;

static int main_argc;
static const char ** main_argv;

static btstack_packet_callback_registration_t hci_event_callback_registration;

#define TLV_DB_PATH_PREFIX "/tmp/btstack_"
#define TLV_DB_PATH_POSTFIX ".tlv"
static char tlv_db_path[100];
static const btstack_tlv_t * tlv_impl;
static btstack_tlv_posix_t   tlv_context;


static int raspi_speed_to_baud(speed_t baud)
{
    switch (baud) {
        case B9600:
            return 9600;
        case B19200:
            return 19200;
        case B38400:
            return 38400;
        case B57600:
            return 57600;
        case B115200:
            return 115200;
        case B230400:
            return 230400;
        case B460800:
            return 460800;
        case B500000:
            return 500000;
        case B576000:
            return 576000;
        case B921600:
            return 921600;
        case B1000000:
            return 1000000;
        case B1152000:
            return 1152000;
        case B1500000:
            return 1500000;
        case B2000000:
            return 2000000;
        case B2500000:
            return 2500000;
        case B3000000:
            return 3000000;
        case B3500000:
            return 3500000;
        case B4000000:
            return 4000000;
        default: 
            return -1;
    }
}

static void raspi_get_terminal_params( hci_transport_config_uart_t *tc )
{
    // open serial terminal and get parameters
    int fd = open( tc->device_name, O_RDONLY );
    if( fd < 0 )
    {
        perror( "can't open serial port" );
        return;
    }
    struct termios tios;
    tcgetattr( fd, &tios );
    close( fd );

    speed_t ospeed = cfgetospeed( &tios );
    int baud = raspi_speed_to_baud( ospeed );
    printf( "current serial terminal parameter baudrate: %d, flow control: %s\n", baud, (tios.c_cflag&CRTSCTS)?"Hardware":"None" );

    // overwrites the initial baudrate only in case it was likely to be altered before
    if( baud > 9600 )
    {
        tc->baudrate_init = baud;
        tc->flowcontrol = (tios.c_cflag & CRTSCTS)?1:0;
    }
}

static void sigint_handler(int param){
    UNUSED(param);

    printf("CTRL-C - SIGINT received, shutting down..\n");   
    log_info("sigint_handler: shutting down");

    // reset anyway
    btstack_stdin_reset();

    // power down
    hci_power_control(HCI_POWER_OFF);
    hci_close();
    log_info("Good bye, see you.\n");    
    exit(0);
}

static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    printf("LED State %u\n", led_state);
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t addr;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
            gap_local_bd_addr(addr);
            printf("BTstack up and running at %s\n",  bd_addr_to_str(addr));
            // setup TLV
            strcpy(tlv_db_path, TLV_DB_PATH_PREFIX);
            strcat(tlv_db_path, bd_addr_to_str(addr));
            strcat(tlv_db_path, TLV_DB_PATH_POSTFIX);
            tlv_impl = btstack_tlv_posix_init_instance(&tlv_context, tlv_db_path);
            btstack_tlv_set_instance(tlv_impl, &tlv_context);
#ifdef ENABLE_CLASSIC
            hci_set_link_key_db(btstack_link_key_db_tlv_get_instance(tlv_impl, &tlv_context));
#endif    
#ifdef ENABLE_BLE
            le_device_db_tlv_configure(tlv_impl, &tlv_context);
#endif
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_local_name)){
                if (hci_event_command_complete_get_return_parameters(packet)[0]) break;
                // terminate, name 248 chars
                packet[6+248] = 0;
                printf("Local name: %s\n", &packet[6]);
                
                btstack_chipset_bcm_set_device_name((const char *)&packet[6]);
            }        
            break;
        default:
            break;
    }
}

// see https://github.com/RPi-Distro/pi-bluetooth/blob/master/usr/bin/btuart
static int raspi_get_bd_addr(bd_addr_t addr){

    FILE *fd = fopen( "/proc/device-tree/serial-number", "r" );
    if( fd == NULL ){
        fprintf(stderr, "can't read serial number, %s\n", strerror( errno ) );
        return -1;
    }
    fscanf( fd, "%*08x" "%*02x" "%02" SCNx8 "%02" SCNx8 "%02" SCNx8, &addr[3], &addr[4], &addr[5] );
    fclose( fd );

    addr[0] =  0xb8; addr[1]  = 0x27; addr[2] =  0xeb;
    addr[3] ^= 0xaa; addr[4] ^= 0xaa; addr[5] ^= 0xaa;

    return 0;
}

// see https://github.com/RPi-Distro/pi-bluetooth/blob/master/usr/bin/btuart
// on UART_INVALID errno is set
static uart_type_t raspi_get_bluetooth_uart_type(void){

    uint8_t deviceUart0[21] = { 0 };
    FILE *fd = fopen( "/proc/device-tree/aliases/uart0", "r" );
    if( fd == NULL ) return UART_INVALID;
    fscanf( fd, "%20s", deviceUart0 );
    fclose( fd );
    
    uint8_t deviceSerial1[21] = { 0 };
    fd = fopen( "/proc/device-tree/aliases/serial1", "r" );
    if( fd == NULL ) return UART_INVALID;
    fscanf( fd, "%20s", deviceSerial1 );
    fclose( fd );
  
    // test if uart0 is an alias for serial1
    if( strncmp( (const char *) deviceUart0, (const char *) deviceSerial1, 21 ) == 0 ){
        // HW uart
        size_t count = 0;
        uint8_t buf[16];
        fd = fopen( "/proc/device-tree/soc/gpio@7e200000/uart0_pins/brcm,pins", "r" );
        if( fd == NULL ) return UART_INVALID;
        count = fread( buf, 1, 16, fd );
        fclose( fd );

        // contains assigned pins
        int pins = count / 4;
        if( pins == 4 ){
            return UART_HARDWARE_FLOW;
        } else {
            return UART_HARDWARE_NO_FLOW;
        }
    } else {
        return UART_SOFTWARE_NO_FLOW;
    }    
}

static void phase2(int status);
int main(int argc, const char * argv[]){

    /// GET STARTED with BTstack ///
    btstack_memory_init();

    // log into file using HCI_DUMP_PACKETLOGGER format
    const char * pklg_path = "/tmp/hci_dump.pklg";
    hci_dump_posix_fs_open(pklg_path, HCI_DUMP_PACKETLOGGER);
    const hci_dump_t * hci_dump_impl = hci_dump_posix_fs_get_instance();
    hci_dump_init(hci_dump_impl);
    printf("Packet Log: %s\n", pklg_path);

    // setup run loop
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
        
    // pick serial port and configure uart block driver
    transport_config.device_name = "/dev/serial1";
    
    // derive bd_addr from serial number
    bd_addr_t addr = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    raspi_get_bd_addr(addr);

    // set UART config based on raspi Bluetooth UART type
    int bt_reg_en_pin = -1;
    bool power_cycle = true;
    switch (raspi_get_bluetooth_uart_type()){
        case UART_INVALID:
            fprintf(stderr, "can't verify HW uart, %s\n", strerror( errno ) );
            return -1;
        case UART_SOFTWARE_NO_FLOW:
            // ??
            bt_reg_en_pin = 128;
            transport_config.baudrate_main = 460800;
            transport_config.flowcontrol   = 0;
            break;
        case UART_HARDWARE_NO_FLOW:
            // Raspberry Pi 3 A
            // Raspberry Pi 3 B
            // power up with H5 and without power cycle untested/unsupported
            bt_reg_en_pin = 128;
            transport_config.baudrate_main = 921600;
            transport_config.flowcontrol   = 0;
            break;
        case UART_HARDWARE_FLOW:
            // Raspberry Pi Zero W gpio 45, 3 mbps does not work (investigation pending)
            // Raspberry Pi 3A+ vgpio 129 but WLAN + BL
            // Raspberry Pi 3B+ vgpio 129 but WLAN + BL
            transport_config.flowcontrol = 1;
            int model = raspi_get_model();
            if (model == MODEL_ZERO_W){
                bt_reg_en_pin =  45;
                transport_config.baudrate_main =  921600;
            } else {
                bt_reg_en_pin = 129;
                transport_config.baudrate_main = 3000000;
            }

#ifdef ENABLE_CONTROLLER_WARM_BOOT
            power_cycle = false;
#else
            // warn about power cycle on devices with shared reg_en pins
            if (model == MODEL_3APLUS || model == MODEL_3BPLUS){
                printf("Wifi and Bluetooth share a single RESET line and BTstack needs to reset Bluetooth -> SSH over Wifi will fail\n");
                printf("Please add ENABLE_CONTROLLER_WARM_BOOT to btstack_config.h to enable startup without RESET\n");
            }
#endif
            break;
    }
    printf("%s, %u, BT_REG_EN at GPIO %u, %s\n", transport_config.flowcontrol ? "H4":"H5", transport_config.baudrate_main, bt_reg_en_pin, power_cycle ? "Reset Controller" : "Warm Boot");

    // get BCM chipset driver
    const btstack_chipset_t * chipset = btstack_chipset_bcm_instance();
    chipset->init(&transport_config);

    // set path to firmware files
    btstack_chipset_bcm_set_hcd_folder_path("/lib/firmware/brcm");

    // setup UART driver
    const btstack_uart_t * uart_driver = btstack_uart_posix_instance();

    // extract UART config from transport config
    uart_config.baudrate    = transport_config.baudrate_init;
    uart_config.flowcontrol = transport_config.flowcontrol;
    uart_config.device_name = transport_config.device_name;
    uart_driver->init(&uart_config);

    // HW with FlowControl -> we can use regular h4 mode
    const hci_transport_t * transport;
    if (transport_config.flowcontrol){
        transport = hci_transport_h4_instance(uart_driver);
    } else {
        transport = hci_transport_h5_instance(uart_driver);
    }

    // setup HCI (to be able to use bcm chipset driver)
    hci_init(transport, (void*) &transport_config);
    hci_set_bd_addr( addr );
    hci_set_chipset(btstack_chipset_bcm_instance());

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // handle CTRL-c
    signal(SIGINT, sigint_handler);

    main_argc = argc;
    main_argv = argv;

    // power cycle Bluetooth controller on older models without flowcontrol
    if (power_cycle){
        btstack_control_raspi_set_bt_reg_en_pin(bt_reg_en_pin);
        btstack_control_t *control = btstack_control_raspi_get_instance();
        control->init(NULL);
        control->off();
        usleep( 100000 );
        control->on();
    }

    if (transport_config.flowcontrol){

        // re-use current terminal speed (if there was no power cycle)
        if (!power_cycle){
            raspi_get_terminal_params( &transport_config );
        }

        // with flowcontrol, we use h4 and are done
        btstack_main(main_argc, main_argv);

    } else {

        // assume BCM4343W used in Pi 3 A/B. Pi 3 A/B+ have a newer controller but support H4 with Flowcontrol
        btstack_chipset_bcm_set_device_name("BCM43430A1");

        // phase #1 download firmware
        printf("Phase 1: Download firmware\n");

        // phase #2 start main app
        btstack_chipset_bcm_download_firmware(uart_driver, transport_config.baudrate_main, &phase2);
    }

    // go
    btstack_run_loop_execute();    
    return 0;
}

static void phase2(int status){

    if (status){
        printf("Download firmware failed\n");
        return;
    }

    printf("Phase 2: Main app\n");

    // setup app
    btstack_main(main_argc, main_argv);
}

