#include <BTstack.h>
#include <stdio.h>
#include "att_server.h"
#include "gatt_client.h"
#include "ancs_client_lib.h"
#include "sm.h"
#include <SPI.h>

const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, 0x01, 0x02, 
    // Name
    0x05, 0x09, 'A', 'N', 'C', 'S', 
    // Service Solicitation, 128-bit UUIDs - ANCS (little endian)
    0x11,0x15,0xD0,0x00,0x2D,0x12,0x1E,0x4B,0x0F,0xA4,0x99,0x4E,0xCE,0xB5,0x31,0xF4,0x05,0x79
};

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

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    printf("packet_handler type %u, event 0x%02x\n", packet_type, packet[0]);
    ancs_client_hci_event_handler(packet_type, channel, packet, size);
}

void ancs_callback(ancs_event_t * event){
    const char * attribute_name;
    switch (event->type){
        case ANCS_CLIENT_CONNECTED:
            printf("ANCS Client: Connected\n");
            break;
        case ANCS_CLIENT_DISCONNECTED:
            printf("ANCS Client: Disconnected\n");
            break;
        case ANCS_CLIENT_NOTIFICATION:
            attribute_name = ancs_client_attribute_name_for_id(event->attribute_id);
            if (!attribute_name) break;
            printf("Notification: %s - %s\n", attribute_name, event->text);
            break;
        default:
            break;
    }
}

void setup(void){
    setup_printf();
    printf("Main::Setup()\n");
    BT.setup();
    BT.setAdvData(sizeof(adv_data), adv_data);

    // setup packet handler (Client+Server_
    BT.registerPacketHandler(&packet_handler);
  
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING ); 

    // set up GATT Server
    att_set_db(NULL);
    
    // setup GATT client
    gatt_client_init();

    // setup ANCS Client
    ancs_client_init();
    ancs_client_register_callback(&ancs_callback);
}

void loop(void){
    BT.loop();
}
