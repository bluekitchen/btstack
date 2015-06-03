#include <BTstack.h>
#include <stdio.h>
#include "att_server.h"
#include "gatt_client.h"
#include "ancs_client_lib.h"
#include "sm.h"
#include <SPI.h>

/* 
 * EXAMPLE_START(ANCS): ANCS Client
 */

/*
 * @section Advertisement
 * @text An ANCS Client needs to include the ANCS UUID in its advertisement to
 * get recognized by iOS
 */

/* LISTING_START(ANCSAdvertisement): ANCS Advertisement */
const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, 0x01, 0x02, 
    // Name
    0x05, 0x09, 'A', 'N', 'C', 'S', 
    // Service Solicitation, 128-bit UUIDs - ANCS (little endian)
    0x11,0x15,0xD0,0x00,0x2D,0x12,0x1E,0x4B,0x0F,0xA4,0x99,0x4E,0xCE,0xB5,0x31,0xF4,0x05,0x79
};
/* LISTING_END(ANCSAdvertisement): ANCS Advertisement */

/* 
 * @section Setup
 *
 * @text In the setup, the LE Security Manager is configured to accept pairing requests.
 * Then, the ANCS Client library is initialized and and ancs_callback registered.
 * Finally, the Advertisement data is set and Advertisements are started.
 */

/* LISTING_START(ANCSSetup): ANCS Setup */
void setup(void){

    Serial.begin(9600);
    Serial.println("BTstack ANCS Client starting up...");

    // startup BTstack and configure log_info/log_error
    BTstack.setup();

    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING ); 

    // setup ANCS Client
    ancs_client_init();
    ancs_client_register_callback(&ancs_callback);

    // enable advertisements
    BTstack.setAdvData(sizeof(adv_data), adv_data);
    BTstack.startAdvertising();
}
/* LISTING_END(ANCSSetup): ANCS Setup */

void loop(void){
    BTstack.loop();
}

/* 
 * @section ANCS Callback
 * @text In the ANCS Callback, connect and disconnect events are received.
 * For actual notifications, ancs_client_attribute_name_for_id allows to 
 * look up the name. To get the notification body, e.g., the actual message,
 * the GATT Client needs to be used direclty.
 */

/* LISTING_START(ANCSCallback): ANCS Callback */
void ancs_callback(ancs_event_t * event){
    const char * attribute_name;
    switch (event->type){
        case ANCS_CLIENT_CONNECTED:
            Serial.println("ANCS Client: Connected");
            break;
        case ANCS_CLIENT_DISCONNECTED:
            Serial.println("ANCS Client: Disconnected");
            break;
        case ANCS_CLIENT_NOTIFICATION:
            attribute_name = ancs_client_attribute_name_for_id(event->attribute_id);
            if (!attribute_name) break;
            Serial.print("Notification: ");
            Serial.print(attribute_name);
            Serial.print(" - ");
            Serial.println(event->text);
            break;
        default:
            break;
    }
}
/* LISTING_END(ANCSCallback): ANCS Callback */

