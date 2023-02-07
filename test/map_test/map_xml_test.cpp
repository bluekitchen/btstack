#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "yxml.h"
#include "map.h"
#include "btstack_defines.h"
#include "btstack_util.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "map_util.h"

const static char * folders = 
"<?xml version='1.0' encoding='utf-8' standalone='yes' ?>"
"<folder-listing version=\"1.0\">"
"    <folder name=\"deleted\" />"
"    <folder name=\"draft\" />"
"    <folder name=\"inbox\" />"
"    <folder name=\"outbox\" />"
"    <folder name=\"sent\" />"
"</folder-listing>";

const char * expected_folders[] = {
    "deleted",
    "draft",
    "inbox",
    "outbox",
    "sent"
};
const int num_expected_folders = 5;

const static char * messages = 
"<?xml version='1.0' encoding='utf-8' standalone='yes' ?>"
"<MAP-msg-listing version=\"1.0\">"
"    <msg handle=\"0400000000000002\" subject=\"Ping\" datetime=\"20190319T223947\" sender_name=\"John Doe\" sender_addressing=\"+41786786211\" recipient_name=\"@@@@@@@@@@@@@@@@\" recipient_addressing=\"+41798155782\" type=\"SMS_GSM\" size=\"4\" text=\"yes\" reception_status=\"complete\" attachment_size=\"0\" priority=\"no\" read=\"no\" sent=\"no\" protected=\"no\" />"
"    <msg handle=\"0400000000000001\" subject=\"Lieber Kunde. Information und Hilfe zur Inbetriebnahme Ihres Mobiltelefons haben wir unter www.swisscom.ch/handy-einrichten für Sie zusammengestellt.\" datetime=\"20190308T224830\" sender_name=\"\" sender_addressing=\"Swisscom\" recipient_name=\"@@@@@@@@@@@@@@@@\" recipient_addressing=\"+41798155782\" type=\"SMS_GSM\" size=\"149\" text=\"yes\" reception_status=\"complete\" attachment_size=\"0\" priority=\"no\" read=\"no\" sent=\"no\" protected=\"no\" />"
"</MAP-msg-listing>";

const map_message_handle_t expected_message_handles[] = {
    {4,0,0,0,0,0,0,2},
    {4,0,0,0,0,0,0,1}
};

const int num_expected_message_handles = 2;

#if 0
const static char * message = 
"BEGIN:BMSG\n"
"VERSION:1.0\n"
"STATUS:UNREAD\n"
"TYPE:SMS_GSM\n"
"FOLDER:telecom/msg/INBOX\n"
"BEGIN:VCARD\n"
"VERSION:3.0\n"
"FN:\n"
"N:\n"
"TEL:Swisscom\n"
"END:VCARD\n"
"BEGIN:BENV\n"
"BEGIN:BBODY\n"
"CHARSET:UTF-8\n"
"LENGTH:172\n"
"BEGIN:MSG\n"
"Lieber Kunde. Information und Hilfe zur Inbetriebnahme Ihres Mobiltelefons haben wir unter www.swisscom.ch/handy-einrichten für Sie zusammengestellt.\n"
"END:MSG\n"
"END:BBODY\n"
"END:BENV\n"
"END:BMSG\n";
#endif

/* xml parser */
static int num_found_items;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
    for (int i=0; i<size; i++){
        // printf("%03u: %02x - %02x\n", i, expected[i], actual[i]);
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if  (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_MAP_META) return;

    int value_len;
    char value[MAP_MAX_VALUE_LEN];
    memset(value, 0, MAP_MAX_VALUE_LEN);
    
    switch (hci_event_goep_meta_get_subevent_code(packet)){
        case MAP_SUBEVENT_FOLDER_LISTING_ITEM:
            value_len = btstack_min(map_subevent_folder_listing_item_get_name_len(packet), MAP_MAX_VALUE_LEN);
            memcpy(value, map_subevent_folder_listing_item_get_name(packet), value_len);
            STRCMP_EQUAL(value, expected_folders[num_found_items]);
            num_found_items++;
            break;
        case MAP_SUBEVENT_MESSAGE_LISTING_ITEM:
            memcpy(value, map_subevent_message_listing_item_get_handle(packet), MAP_MESSAGE_HANDLE_SIZE);
            CHECK_EQUAL_ARRAY((uint8_t *) value, (uint8_t *) expected_message_handles[num_found_items], MAP_MESSAGE_HANDLE_SIZE);
            num_found_items++;
            break;
        default:
            break;
    }
}

TEST_GROUP(MAP_XML){
    btstack_packet_handler_t map_callback;
    uint16_t map_cid;

    void setup(void){
        map_callback = &packet_handler;
        num_found_items = 0;
        map_cid = 1;
    }
};

TEST(MAP_XML, Folders){
    map_client_parse_folder_listing(map_callback, map_cid, (const uint8_t *) folders, strlen(folders));
    CHECK_EQUAL(num_found_items, num_expected_folders);
}

TEST(MAP_XML, Messages){
    map_client_parse_message_listing(map_callback, map_cid, (const uint8_t *) messages, strlen(messages));
    CHECK_EQUAL(num_found_items, num_expected_message_handles);
}

TEST(MAP_XML, Msg2Handle){
    uint8_t expected_handle[] = {4,0,0,0,0,0,0,2};
    map_message_handle_t msg_handle;
    char handle[] = "0400000000000002";
    map_message_str_to_handle(handle, msg_handle);
    CHECK_EQUAL_ARRAY((uint8_t *) msg_handle, (uint8_t *) expected_handle, MAP_MESSAGE_HANDLE_SIZE);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}