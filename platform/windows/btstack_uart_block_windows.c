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

#define BTSTACK_FILE__ "btstack_uart_block_windows.c"

/*
 *  btstack_uart_block_windows.c
 *
 *  Common code to access serial port via asynchronous block read/write commands
 *
 */

#include "btstack_uart_block.h"
#include "btstack_run_loop.h"
#include "btstack_debug.h"

#include <fcntl.h>    /* File control definitions */
#include <string.h>
#include <errno.h>

#include <Windows.h>

// uart config
static const btstack_uart_config_t * uart_config;

// data source for integration with BTstack Runloop
static btstack_data_source_t transport_data_source_read;
static btstack_data_source_t transport_data_source_write;

// block write
static int             write_bytes_len;
static const uint8_t * write_bytes_data;

// block read
static uint16_t  read_bytes_len;
static uint8_t * read_bytes_data;

// callbacks
static void (*block_sent)(void);
static void (*block_received)(void);

// port and async control structure
static HANDLE serial_port_handle;
static OVERLAPPED overlapped_read;
static OVERLAPPED overlapped_write;

// fix for chipset drivers that need to download firmware before stack starts up
bool serial_port_open = false;

// -- engine that retries send/receive if not all bytes have been transferred

static void btstack_uart_windows_send_engine(void){
    // start write
    DWORD bytes_written;
    BOOL ok = WriteFile(serial_port_handle,  // handle
                        write_bytes_data,    // (LPCSTR) 8-bit data
                        write_bytes_len,     // length
                        &bytes_written,      // amount written
                        &overlapped_write);  // overlapped structure

    if (ok){
        // assert all bytes written
        if (bytes_written != write_bytes_len){
            log_error("btstack_uart_windows_send_block: requested write %u but %u were written", (int) write_bytes_len, (int) bytes_written);
            return;
        }

        //
        // TODO: to defer sending done event by enabling POLL Callback for Write
        //

        // notify done
        if (block_sent){
            block_sent();
        }
        return;
    }

    DWORD err = GetLastError();
    if (err != ERROR_IO_PENDING){
        log_error("btstack_uart_windows_send_block: error writing");
        return;
    }

    // IO_PENDING -> wait for completed
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source_write, DATA_SOURCE_CALLBACK_WRITE);
}

static void btstack_uart_windows_receive_engine(void){
    DWORD bytes_read;
    BOOL ok = ReadFile(serial_port_handle,  // handle
                        read_bytes_data,    // (LPCSTR) 8-bit data
                        read_bytes_len,     // length
                        &bytes_read,        // amount read
                        &overlapped_read);  // overlapped structure

    if (ok){
        // assert all bytes read
        if (bytes_read != read_bytes_len){
            log_error("btstack_uart_windows_receive_block: requested read %u but %u were read", (int) read_bytes_len, (int) bytes_read);
            return;
        }

        //
        // TODO: to defer sending done event by enabling POLL Callback
        //

        // notify done
        if (block_received){
            block_received();
        }
        return;
    }

    DWORD err = GetLastError();
    if (err != ERROR_IO_PENDING){
        log_error("error reading");
        return;
    }

    // IO_PENDING -> wait for completed
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source_read, DATA_SOURCE_CALLBACK_READ);
}


// -- overlapped IO handlers for read & write

static void btstack_uart_windows_process_write(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);

    DWORD bytes_written;
    BOOL ok = GetOverlappedResult(serial_port_handle, &overlapped_write, &bytes_written, FALSE);
    if(!ok){
        DWORD err = GetLastError();
        if (err == ERROR_IO_INCOMPLETE){
            // IO_INCOMPLETE -> wait for completed
            btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
        } else {
            log_error("write: error writing");
        }
        return;
    }

    // assert all bytes written
    if (bytes_written != write_bytes_len){
        log_debug("write: requested to write %u but %u were written, try again", (int) write_bytes_len, (int) bytes_written);
        btstack_uart_windows_send_engine();
        write_bytes_data += bytes_written;
        write_bytes_len  -= bytes_written;
        return;
    }

    // notify done
    if (block_sent){
        block_sent();
    }
}


static void btstack_uart_windows_process_read(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);

    DWORD bytes_read;
    BOOL ok = GetOverlappedResult(serial_port_handle, &overlapped_read, &bytes_read, FALSE);
    if(!ok){
        DWORD err = GetLastError();
        if (err == ERROR_IO_INCOMPLETE){
            // IO_INCOMPLETE -> wait for completed
            btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);
        } else {
            log_error("error reading");
        }
        return;
    }


    // assert all bytes read
    if (bytes_read != read_bytes_len){
        log_debug("read: requested read %u but %u were read, try again", (int)  read_bytes_len, (int)  bytes_read);
        read_bytes_data += bytes_read;
        read_bytes_len  -= (uint16_t) bytes_read;
        btstack_uart_windows_receive_engine();
        return;
    }

    // notify done
    if (block_received){
        block_received();
    }
}

// -- API implementation

static int btstack_uart_windows_init(const btstack_uart_config_t * config){
    uart_config = config;
    return 0;
}

static void btstack_uart_windows_send_block(const uint8_t *data, uint16_t size){
    // setup async write
    write_bytes_data = data;
    write_bytes_len  = size;

    // go
    btstack_uart_windows_send_engine();
}

static void btstack_uart_windows_receive_block(uint8_t *buffer, uint16_t len){
    // setup async read
    read_bytes_data = buffer;
    read_bytes_len = len;

    // go
    btstack_uart_windows_receive_engine();
}

static void btstack_uart_windows_set_baudrate_option(DCB * serial_params, uint32_t baudrate){
    serial_params->BaudRate = baudrate;
}

static void btstack_uart_windows_set_parity_option(DCB * serial_params, uint32_t parity){
    serial_params->Parity = parity;
}

static void btstack_uart_windows_set_flowcontrol_option(DCB * serial_params, uint32_t flowcontrol){
    // Flowcontrol
    serial_params->fOutxCtsFlow = flowcontrol;
    serial_params->fRtsControl  = flowcontrol ? RTS_CONTROL_HANDSHAKE : 0;
}

static int btstack_uart_windows_set_baudrate(uint32_t baudrate){
    DCB serial_params;
    memset(&serial_params, 0, sizeof(DCB));
    serial_params.DCBlength = sizeof(DCB);
        
    int ok = GetCommState(serial_port_handle, &serial_params);
    if (!ok){
        log_error("windows_set_baudrate: Couldn't get serial parameters");
        return -1;
    }
    btstack_uart_windows_set_baudrate_option(&serial_params, baudrate);
    ok = SetCommState(serial_port_handle, &serial_params);
    if (!ok){
        log_error("windows_set_baudrate: Couldn't serial parameters");
        return -1;
    }

    return 0;
}

static int btstack_uart_windows_set_parity(int parity){
    DCB serial_params;
    memset(&serial_params, 0, sizeof(DCB));
    serial_params.DCBlength = sizeof(DCB);
        
    int ok = GetCommState(serial_port_handle, &serial_params);
    if (!ok){
        log_error("windows_set_parity: Couldn't get serial parameters");
        return -1;
    }
    btstack_uart_windows_set_parity_option(&serial_params, parity);
    ok = SetCommState(serial_port_handle, &serial_params);
    if (!ok){
        log_error("windows_set_parity: Couldn't serial parameters");
        return -1;
    }
    return 0;
}

static int btstack_uart_windows_set_flowcontrol(int flowcontrol){
    DCB serial_params;
    memset(&serial_params, 0, sizeof(DCB));
    serial_params.DCBlength = sizeof(DCB);
        
    int ok = GetCommState(serial_port_handle, &serial_params);
    if (!ok){
        log_error("windows_set_parity: Couldn't get serial parameters");
        return -1;
    }
    btstack_uart_windows_set_flowcontrol_option(&serial_params, flowcontrol);
    ok = SetCommState(serial_port_handle, &serial_params);
    if (!ok){
        log_error("windows_set_flowcontrol: Couldn't serial parameters");
        return -1;
    }
    return 0;
}

static int btstack_uart_windows_open(void){

    // allow to call open again
    if (serial_port_open){
        log_info("Serial port already open");
        return 0;
    }

    const char * device_name = uart_config->device_name;
    const uint32_t baudrate  = uart_config->baudrate;
    const int flowcontrol    = uart_config->flowcontrol;

    serial_port_handle = CreateFile( device_name,  
                        GENERIC_READ | GENERIC_WRITE, 
                        0, 
                        0, 
                        OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED,
                        0);

    if (serial_port_handle == INVALID_HANDLE_VALUE){
        log_error("windows_open: Unable to open port %s", device_name);
        return -1;
    }

    DCB serial_params;
    memset(&serial_params, 0, sizeof(DCB));
    serial_params.DCBlength = sizeof(DCB);
        
    int ok; 

#if 0
    // test - try to set internal buffer
    ok = SetupComm(serial_port_handle, 64, 64);
    printf("SetupCommL ok %u\n", ok);
#endif

#if 0
    // test - read internal buffer sizes
    COMMPROP comm_prop;
    GetCommProperties(serial_port_handle, &comm_prop);
    printf("dwMaxTxQueue %ld\n", comm_prop.dwMaxTxQueue);
    printf("dwMaxRxQueue %ld\n", comm_prop.dwMaxRxQueue);
    printf("dwCurrentTxQueue %ld\n", comm_prop.dwCurrentTxQueue);
    printf("dwCurrentRxQueue %ld\n", comm_prop.dwCurrentRxQueue);
#endif

    // Caveat: with the default FTDI driver and a FT232R on Windows 10, the default USB RX/TX buffer sizes are 4096
    // this causes a problem when data is received back to back, like with SCO audio data

    // Workaround: manually set these values in the Device Manager to 64 bytes

    ok = GetCommState(serial_port_handle, &serial_params);

    if (!ok){
        log_error("windows_open: Couldn't get serial parameters");
        return -1;
    }

    // 8-N-1
    serial_params.ByteSize     = 8; 
    serial_params.StopBits     = ONESTOPBIT;
    serial_params.Parity       = NOPARITY;

    // baudrate
    btstack_uart_windows_set_baudrate_option(&serial_params, baudrate);

    // flow control
    btstack_uart_windows_set_flowcontrol_option(&serial_params, flowcontrol);

    // parity none
    btstack_uart_windows_set_parity_option(&serial_params, 0);

    // commit changes
    ok = SetCommState(serial_port_handle, &serial_params);
    if (!ok){
        log_error("windows_open: Couldn't serial parameters");
        return -1;
    }

    // setup overlapped structures for async io
    memset(&overlapped_read,  0, sizeof(overlapped_read));
    memset(&overlapped_write, 0, sizeof(overlapped_write));
    overlapped_read.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    overlapped_write.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // setup read + write data sources
    transport_data_source_read.source.handle = overlapped_read.hEvent;
    transport_data_source_write.source.handle = overlapped_write.hEvent;
    btstack_run_loop_set_data_source_handler(&transport_data_source_read,  &btstack_uart_windows_process_read);
    btstack_run_loop_set_data_source_handler(&transport_data_source_write, &btstack_uart_windows_process_write);
    btstack_run_loop_add_data_source(&transport_data_source_read);
    btstack_run_loop_add_data_source(&transport_data_source_write);

    serial_port_open = true;

    return 0;
} 

static int btstack_uart_windows_close_new(void){

    serial_port_open = false;

    // first remove run loop handler
    btstack_run_loop_remove_data_source(&transport_data_source_read);
    btstack_run_loop_remove_data_source(&transport_data_source_write);
    
    // note: an event cannot be freed while a kernel function is waiting.
    //       in our single-threaded environment, this cannot happen.

    // free events
    CloseHandle(overlapped_read.hEvent);
    CloseHandle(overlapped_write.hEvent); 
    CloseHandle(serial_port_handle);

    // set pointers to zero
    overlapped_read.hEvent = NULL;
    overlapped_write.hEvent = NULL;
    serial_port_handle = NULL;
    return 0;
}

static void btstack_uart_windows_set_block_received( void (*block_handler)(void)){
    block_received = block_handler;
}

static void btstack_uart_windows_set_block_sent( void (*block_handler)(void)){
    block_sent = block_handler;
}

// static void btstack_uart_windows_set_sleep(uint8_t sleep){
// }
// static void btstack_uart_windows_set_csr_irq_handler( void (*csr_irq_handler)(void)){
// }

static const btstack_uart_block_t btstack_uart_windows = {
    /* int  (*init)(hci_transport_config_uart_t * config); */         &btstack_uart_windows_init,
    /* int  (*open)(void); */                                         &btstack_uart_windows_open,
    /* int  (*close)(void); */                                        &btstack_uart_windows_close_new,
    /* void (*set_block_received)(void (*handler)(void)); */          &btstack_uart_windows_set_block_received,
    /* void (*set_block_sent)(void (*handler)(void)); */              &btstack_uart_windows_set_block_sent,
    /* int  (*set_baudrate)(uint32_t baudrate); */                    &btstack_uart_windows_set_baudrate,
    /* int  (*set_parity)(int parity); */                             &btstack_uart_windows_set_parity,
    /* int  (*set_flowcontrol)(int flowcontrol); */                   &btstack_uart_windows_set_flowcontrol,
    /* void (*receive_block)(uint8_t *buffer, uint16_t len); */       &btstack_uart_windows_receive_block,
    /* void (*send_block)(const uint8_t *buffer, uint16_t length); */ &btstack_uart_windows_send_block,
    /* int (*get_supported_sleep_modes); */                           NULL,
    /* void (*set_sleep)(btstack_uart_sleep_mode_t sleep_mode); */    NULL,
    /* void (*set_wakeup_handler)(void (*handler)(void)); */          NULL,
    NULL, NULL, NULL, NULL,
};

const btstack_uart_block_t * btstack_uart_block_windows_instance(void){
	return &btstack_uart_windows;
}
