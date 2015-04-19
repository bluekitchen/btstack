#include <BTstack.h>
#include <stdio.h>
#include "att_server.h"
#include "gatt_client.h"
#include "hci.h"

#include <SPI.h>

static bd_addr_t em9301_addr = { 0x0C, 0xF3, 0xEE, 0x00, 0x00, 0x00 };
static gatt_client_t gatt_context;
static uint16_t con_handle;
static uint8_t sensor_value;

// retarget printf
#ifdef ENERGIA
extern "C" {
    int putchar(int c) {
        Serial.write((uint8_t)c);
        return c;
    }
}
static void setup_printf(void) {
  Serial.begin(9600);
}
#else
static FILE uartout = {0} ;
static int uart_putchar (char c, FILE *stream) {
    Serial.write(c);
    return 0;
}
static void setup_printf(void) {
  Serial.begin(115200);
  fdev_setup_stream (&uartout, uart_putchar, NULL, _FDEV_SETUP_WRITE);
  stdout = &uartout;
}  
#endif

// test profile
#include "profile.h"

// write requests
static int att_write_callback(uint16_t con_handle, uint16_t handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    printf("WRITE Callback, handle 0x%04x\n", handle);

    switch(handle){
        case ATT_CHARACTERISTIC_FFF1_01_VALUE_HANDLE:
            buffer[buffer_size]=0;
            printf("New text: %s\n", buffer);
            break;
        case ATT_CHARACTERISTIC_FFF2_01_VALUE_HANDLE:
            printf("New value: %u\n", buffer[0]);
#ifdef PIN_LED
            if (buffer[0]){
                digitalWrite(PIN_LED, HIGH); 
            } else {
                digitalWrite(PIN_LED, LOW); 
            }
#endif
            break;
    }
    return 0;
}


static void try_send(){
    if (!con_handle) return;
    
    // try write
    gatt_client_write_value_of_characteristic_without_response(&gatt_context, ATT_CHARACTERISTIC_FFF2_01_VALUE_HANDLE, 1, &sensor_value);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    printf("packet type %x\n", packet[0]);
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                case BTSTACK_EVENT_STATE:
                    if (packet[2] == HCI_STATE_WORKING) {
                        printf("Connecting to server\n");
                        le_central_connect(&em9301_addr, BD_ADDR_TYPE_LE_PUBLIC);
                    }
                    break;
 
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    con_handle = 0;
                    break;

                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
	                    // store connection info
                            con_handle = READ_BT_16(packet, 4);
                            
                            // start GATT Client
                            gatt_client_start(&gatt_context, con_handle);
                            break;

                        default:
                            break;
                    }
                    break;
              }
    }
    try_send();
}

void setup(){

  setup_printf();

  printf("Main::Setup()\n");
  BT.setup();
  
  // set up GATT Server
  att_set_db(profile_data);
  att_set_write_callback(att_write_callback);

  // set up GATT Client
  gatt_client_init();
  BT.setClientMode();
  
  // setup packet handler (Client+Server_
  BT.registerPacketHandler(&packet_handler);
}

void loop(){
  BT.loop();
  sensor_value = analogRead(A0) >> 2;
}
