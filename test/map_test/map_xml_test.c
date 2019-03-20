#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "yxml.h"
#include "map.h"

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

const char * expected_message_handles[] = {
    "0400000000000002",
    "0400000000000001"
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

TEST_GROUP(MAP_XML){
    void setup(void){
        printf("setup\n");
        yxml_init(&xml_parser, xml_buffer, sizeof(xml_buffer));
    }
};

TEST(MAP_XML, Folders){
    printf("Parse Folders\n");
    const uint8_t  * data = (const uint8_t *) folders;
    uint16_t data_len = strlen(folders);
    
    int folder_found = 0;
    int name_found = 0;
    char name[MAP_MAX_VALUE_LEN];
    int num_found_folders = 0;

    while (data_len--){
        yxml_ret_t r = yxml_parse(&xml_parser, *data++);
        switch (r){
            case YXML_ELEMSTART:
                folder_found = strcmp("folder", xml_parser.elem) == 0;
                break;
            case YXML_ELEMEND:
                if (folder_found){
                    if (num_found_folders < num_expected_folders){
                        printf("Found folder \'%s\'\n", name);
                        STRCMP_EQUAL(name, expected_folders[num_found_folders]);
                    }
                    num_found_folders++;
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
    CHECK_EQUAL(num_found_folders, num_expected_folders);
}

TEST(MAP_XML, Messages){
    printf("Parse Messages\n");

    const uint8_t  * data = (const uint8_t *) messages;
    uint16_t data_len = strlen(messages);
    
    // now try parsing it
    int message_found = 0;
    int handle_found = 0;
    char handle[MAP_MAX_HANDLE_LEN];
    int num_found_messages = 0;

    while (data_len--){
        yxml_ret_t r = yxml_parse(&xml_parser, *data++);
        switch (r){
            case YXML_ELEMSTART:
                message_found = strcmp("msg", xml_parser.elem) == 0;
                break;
            case YXML_ELEMEND:
                if (message_found){
                    printf("Found handle \'%s\'\n\n", handle);
                    STRCMP_EQUAL(handle, expected_message_handles[num_found_messages]);
                    num_found_messages++;
                }
                message_found = 0;
                break;
            case YXML_ATTRSTART:
                if (!message_found) break;
                printf("%s\n", xml_parser.attr);
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
    CHECK_EQUAL(num_found_messages, num_expected_message_handles);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}