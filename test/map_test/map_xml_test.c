#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "yxml.h"
#include "map.h"
#include "btstack_defines.h"
#include "btstack_util.h"
#include "btstack_debug.h"
#include "btstack_event.h"

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
static yxml_t  xml_parser;
static uint8_t xml_buffer[50];
static int num_found_items;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
    for (int i=0; i<size; i++){
        // printf("%03u: %02x - %02x\n", i, expected[i], actual[i]);
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

static void map_client_emit_folder_listing_item_event(btstack_packet_handler_t callback, uint16_t cid, uint8_t * folder_name, uint16_t folder_name_len){
    uint8_t event[7 + MAP_MAX_VALUE_LEN];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_FOLDER_LISTING_ITEM;
    little_endian_store_16(event,pos,cid);
    pos+=2;
    uint16_t value_len = btstack_min(folder_name_len, sizeof(event) - pos);
    little_endian_store_16(event, pos, value_len);
    pos += 2;
    memcpy(event+pos, folder_name, value_len);
    pos += value_len;
    event[1] = pos - 2;
    if (pos > sizeof(event)) log_error("map_client_emit_folder_listing_item_event size %u", pos);
    (*callback)(HCI_EVENT_PACKET, cid, &event[0], pos);
}  

static void map_client_emit_message_listing_item_event(btstack_packet_handler_t callback, uint16_t cid, map_message_handle_t message_handle){
    uint8_t event[7 + MAP_MESSAGE_HANDLE_SIZE];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_MESSAGE_LISTING_ITEM;
    little_endian_store_16(event,pos,cid);
    pos+=2;
    
    memcpy(event+pos, message_handle, MAP_MESSAGE_HANDLE_SIZE);
    pos += MAP_MESSAGE_HANDLE_SIZE;
    
    event[1] = pos - 2;
    if (pos > sizeof(event)) log_error("map_client_emit_message_listing_item_event size %u", pos);
    (*callback)(HCI_EVENT_PACKET, cid, &event[0], pos);
} 

static void map_client_emit_parsing_done_event(btstack_packet_handler_t callback, uint16_t cid){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_PARSING_DONE;
    little_endian_store_16(event,pos,cid);
    pos += 2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_client_emit_parsing_done_event size %u", pos);
    (*callback)(HCI_EVENT_PACKET, cid, &event[0], pos);
}

static void map_client_parse_folder_listing(btstack_packet_handler_t callback, uint16_t cid, const uint8_t * data, uint16_t data_len){
    int folder_found = 0;
    int name_found = 0;
    char name[MAP_MAX_VALUE_LEN];
    
    while (data_len--){
        yxml_ret_t r = yxml_parse(&xml_parser, *data++);
        switch (r){
            case YXML_ELEMSTART:
                folder_found = strcmp("folder", xml_parser.elem) == 0;
                break;
            case YXML_ELEMEND:
                if (folder_found){
                    map_client_emit_folder_listing_item_event(callback, cid, (uint8_t *) name, strlen(name));
                }
                folder_found = 0;
                break;
            case YXML_ATTRSTART:
                if (!folder_found) break;
                if (strcmp("name", xml_parser.attr) == 0){
                    name_found = 1;
                    name[0]    = 0;
                    break;
                }
                break;
            case YXML_ATTRVAL:
                if (name_found) {
                    // "In UTF-8, characters from the U+0000..U+10FFFF range (the UTF-16 accessible range) are encoded using sequences of 1 to 4 octets."
                    if (strlen(name) + 4 + 1 >= sizeof(name)) break;
                    strcat(name, xml_parser.data);
                    break;
                }
                break;
            case YXML_ATTREND:
                name_found = 0;
                break;
            default:
                break;
        }
    }
    map_client_emit_parsing_done_event(callback, cid);
}

static void map_message_str_to_handle(const char * value, map_message_handle_t msg_handle){
    int i;
    for (i = 0; i < MAP_MESSAGE_HANDLE_SIZE; i++) {
        uint8_t upper_nibble = nibble_for_char(*value++);
        uint8_t lower_nibble = nibble_for_char(*value++);
        msg_handle[i] = (upper_nibble << 4) | lower_nibble;
    }
}

static void map_client_parse_message_listing(btstack_packet_handler_t callback, uint16_t cid, const uint8_t * data, uint16_t data_len){
    // now try parsing it
    int message_found = 0;
    int handle_found = 0;
    char handle[MAP_MESSAGE_HANDLE_SIZE * 2];
    map_message_handle_t msg_handle;

    while (data_len--){
        yxml_ret_t r = yxml_parse(&xml_parser, *data++);
        switch (r){
            case YXML_ELEMSTART:
                message_found = strcmp("msg", xml_parser.elem) == 0;
                break;
            case YXML_ELEMEND:
                if (!message_found) break;
                message_found = 0;
                if (strlen(handle) != MAP_MESSAGE_HANDLE_SIZE * 2){
                    log_info("message handle string length != 16");
                    break;
                }
                map_message_str_to_handle(handle, msg_handle);
                map_client_emit_message_listing_item_event(callback, cid, msg_handle);
                break;
            case YXML_ATTRSTART:
                if (!message_found) break;
                if (strcmp("handle", xml_parser.attr) == 0){
                    handle_found = 1;
                    handle[0]    = 0;
                    break;
                }
                break;
            case YXML_ATTRVAL:
                if (handle_found) {

                    strcat(handle, xml_parser.data);
                    break;
                }
                break;
            case YXML_ATTREND:
                handle_found = 0;
                break;
            default:
                break;
        }
    }
    map_client_emit_parsing_done_event(callback, cid);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if  (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_MAP_META) return;

    char value[MAP_MAX_VALUE_LEN];
    int value_len;
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
        yxml_init(&xml_parser, xml_buffer, sizeof(xml_buffer));
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