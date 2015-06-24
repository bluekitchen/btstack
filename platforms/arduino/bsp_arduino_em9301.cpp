/**
 * Arduino + Energia Wrapper for BTstack
 */

#if !defined(ARDUINO)
#error "Not compiling for Arduino/Energia"
#endif

#include <Arduino.h>
#ifdef ENERGIA
#include <Energia.h>
#endif
#include <SPI.h>

#include "btstack/hal_uart_dma.h"

#define HAVE_SHUTDOWN

#ifdef ENERGIA

// CMM 9301 Configuration for TI Launchpad
#define PIN_SPI_SCK      7
#define PIN_CS       8
#define PIN_SHUTDOWN 11
#define PIN_IRQ_DATA 13
#define PIN_SPI_MISO     14
#define PIN_SPI_MOSI     15
#else // ARDUINO

// CMM 9301 Configuration for Arduino
#define PIN_IRQ_DATA 2
#define PIN_CS 4
#define PIN_SHUTDOWN 5

// -- SPI defines for Arduino Mega
#ifndef PIN_SPI_MISO
#define PIN_SPI_MISO 50
#endif
#ifndef PIN_SPI_MOSI
#define PIN_SPI_MOSI 51
#endif
#ifndef PIN_SPI_SCK
#define PIN_SPI_SCK  52
#endif

#endif

// rx state
static uint16_t  bytes_to_read = 0;
static uint8_t * rx_buffer_ptr = 0;

// tx state
static uint16_t  bytes_to_write = 0;
static uint8_t * tx_buffer_ptr = 0;

// handlers
static void dummy_handler(void){};
static void (*rx_done_handler)(void) = dummy_handler;
static void (*tx_done_handler)(void) = dummy_handler;

static void bt_setup(void){
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_SPI_MOSI, OUTPUT);
    pinMode(PIN_SPI_SCK, OUTPUT);
    pinMode(PIN_SHUTDOWN, OUTPUT);
    pinMode(PIN_IRQ_DATA, INPUT);

    digitalWrite(PIN_CS, HIGH);
    digitalWrite(PIN_SPI_MOSI, LOW);
    digitalWrite(PIN_SHUTDOWN, HIGH);

    // SPI settings are reset in SPI.begin() - calls hang on Arduino Zero, too.
    // SPI.setBitOrder(MSBFIRST);
    // SPI.setDataMode(SPI_MODE0);
    // SPI.end();
}

#ifdef HAVE_SHUTDOWN
static void bt_power_cycle(void){

    // power cycle. set CPU outputs to input to not power EM9301 via IOs
    // pinMode(PIN_SPI_MOSI, INPUT);
    // pinMode(PIN_CS, INPUT);
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_SPI_MOSI, OUTPUT);
    pinMode(PIN_SPI_SCK, OUTPUT);
    pinMode(PIN_SHUTDOWN, OUTPUT);
    digitalWrite(PIN_CS, LOW);
    digitalWrite(PIN_SPI_MOSI, LOW);
    digitalWrite(PIN_SPI_SCK, LOW);
    digitalWrite(PIN_SHUTDOWN, HIGH);
    delay(500);

    pinMode(PIN_SPI_MOSI, OUTPUT);
    pinMode(PIN_CS, OUTPUT);
    digitalWrite(PIN_SPI_MOSI, LOW);
    digitalWrite(PIN_CS, HIGH);
    digitalWrite(PIN_SHUTDOWN, LOW);
    delay(1000);
}
#endif

#ifndef HAVE_SHUTDOWN
static void bt_send_illegal(void){
    digitalWrite(PIN_SPI_MOSI, HIGH);
    digitalWrite(PIN_CS, LOW);
    printf("Illegal start\n");
    SPI.begin(); 
    int i;
    for (i=0;i<255;i++){
        SPI.transfer(0xff);
        printf(".");
    }    
    SPI.end(); 
    printf("\nIllegal stop\n");
    digitalWrite(PIN_CS, HIGH);
}

static void bt_flush_input(void){
    digitalWrite(PIN_SPI_MOSI, LOW);
    digitalWrite(PIN_CS, LOW);
    SPI.begin(); 
    while (digitalRead(PIN_IRQ_DATA) == HIGH){
        SPI.transfer(0x00);
    }
    SPI.end(); 
    digitalWrite(PIN_CS, HIGH);
}

static void bt_send_reset(void){
      digitalWrite(PIN_SPI_MOSI, HIGH);
      digitalWrite(PIN_CS, LOW);
      SPI.begin(); 
      SPI.transfer(0x01);
      SPI.transfer(0x03);
      SPI.transfer(0x0c);
      SPI.transfer(0x00);
      SPI.end(); 
      digitalWrite(PIN_CS, HIGH);
}
#endif


static void bt_try_send(void){
    
    if (!bytes_to_write) return;

    // activate module
    pinMode(PIN_SPI_MOSI, OUTPUT);
    digitalWrite(PIN_SPI_MOSI, HIGH);
    digitalWrite(PIN_CS, LOW);

    // module ready
    int tx_done = 0;
    if (digitalRead(PIN_SPI_MISO) == HIGH){
        // printf("Sending: ");

        SPI.begin(); 
        while (bytes_to_write){
            // printf("%02x ", *tx_buffer_ptr);
            SPI.transfer(*tx_buffer_ptr);
            tx_buffer_ptr++;
            bytes_to_write--;
        }
        SPI.end();
        // printf(".\n");
        tx_done = 1;
    } 

    // deactivate module
    digitalWrite(PIN_CS, HIGH);
    digitalWrite(PIN_SPI_MOSI, LOW);
    pinMode(PIN_SPI_MOSI, OUTPUT);

    // notify upper layer
    if (tx_done) {
        (*tx_done_handler)();
    }
}

static int bt_try_read(void){

    // check if data available and buffer is ready
    if (digitalRead(PIN_IRQ_DATA) == LOW) return 0;
    if (bytes_to_read == 0) return 0;

    int num_bytes_read = 0;

    // printf("Reading (%u): ", bytes_to_read);

    // activate module
    digitalWrite(PIN_SPI_MOSI, LOW);
    digitalWrite(PIN_CS, LOW);
    SPI.begin(); 
    do {
        uint8_t byte_read = SPI.transfer(0x00);
        // printf("%02x ", byte_read); 
        *rx_buffer_ptr = byte_read;
        rx_buffer_ptr++;
        bytes_to_read--;
        num_bytes_read++;
    } while (bytes_to_read > 0);
    SPI.end(); 
    digitalWrite(PIN_CS, HIGH);

    // printf("\n"); 

    // notify upper layer
    (*rx_done_handler)();

    return num_bytes_read;
}

extern "C" void hal_uart_dma_init(void){
    bt_setup();

#ifdef HAVE_SHUTDOWN
    bt_power_cycle(); 
#else
    // bring EM9301 into defined state
    bt_send_illegal();
    bt_send_reset();
    bt_flush_input();
#endif
}

extern "C" void hal_uart_dma_set_block_received( void (*block_handler)(void)){
    rx_done_handler = block_handler;
}
extern "C" void hal_uart_dma_set_block_sent( void (*block_handler)(void)){
    tx_done_handler = block_handler;
}

extern "C" void hal_uart_dma_set_csr_irq_handler( void (*csr_irq_handler)(void)){
    // only used for eHCILL
}

extern "C" int  hal_uart_dma_set_baud(uint32_t baud){
	return 0;
}

extern "C" void hal_uart_dma_send_block(const uint8_t *buffer, uint16_t length){
    // printf("send_block, bytes %u\n", length);
    tx_buffer_ptr = (uint8_t *) buffer;
    bytes_to_write = length;
}

extern "C" void hal_uart_dma_receive_block(uint8_t *buffer, uint16_t length){
    rx_buffer_ptr = buffer;
    bytes_to_read = length;
}

extern "C" void hal_uart_dma_set_sleep(uint8_t sleep){
    // not needed for SPI (doesn't need internal clock to work)
}

extern "C" void hal_uart_dma_process(void){
    int num_bytes_read = bt_try_read();
    if (num_bytes_read == 0){
        bt_try_send();
    }
}

extern "C" uint32_t hal_time_ms(void){
    return millis();
}

