/**
 * Arduino Wrapper for BTstack
 */

#include <Arduino.h> 
#include <SPI.h>

#include "BTstack.h"

#include "btstack_memory.h"
#include "btstack/hal_tick.h"
#include "btstack/hal_cpu.h"
#include "btstack/hci_cmds.h"
#include <btstack/utils.h>
#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "bt_control_em9301.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "ad_parser.h"
#include "att.h"
#include "att_server.h"
#include "le_device_db.h"
#include "sm.h"
#include "debug.h"

// Pin 13 has an LED connected on most Arduino boards.
#define PIN_LED 13

// prototypes
extern "C" void embedded_execute_once(void);
extern "C" void hal_uart_dma_process(void);

// btstack state 
static int btstack_state;

// HAL CPU Implementation
extern "C" void hal_cpu_disable_irqs(void){ }
extern "C" void hal_cpu_enable_irqs(void) { }
extern "C" void hal_cpu_enable_irqs_and_sleep(void) { }

static const uint8_t iBeaconAdvertisement01[] = { 0x02, 0x01 };
static const uint8_t iBeaconAdvertisement38[] = { 0x1a, 0xff, 0x4c, 0x00, 0x02, 0x15 };
static const uint8_t adv_data_default[] = { 02, 01, 05,   03, 02, 0xf0, 0xff }; 
static const uint8_t * adv_data = adv_data_default;
static uint16_t adv_data_len = sizeof(adv_data_default);
static uint16_t gatt_client_id;
static int gatt_is_characteristics_query;

typedef enum gattAction {
    gattActionWrite,
    gattActionSubscribe,
    gattActionUnsubscribe,
    gattActionServiceQuery,
    gattActionCharacteristicQuery,
    gattActionRead,
} gattAction_t;

static gattAction_t gattAction;

// static btstack_packet_handler_t client_packet_handler = NULL;
static int client_mode = 0;
static bool have_custom_addr;
static bd_addr_t public_bd_addr;

static timer_source_t connection_timer;

static void (*bleAdvertismentCallback)(BLEAdvertisement * bleAdvertisement) = NULL;
static void (*bleDeviceConnectedCallback)(BLEStatus status, BLEDevice * device)= NULL;
static void (*bleDeviceDisconnectedCallback)(BLEDevice * device) = NULL;
static void (*gattServiceDiscoveredCallback)(BLEStatus status, BLEDevice * device, BLEService * bleService) = NULL;
static void (*gattCharacteristicDiscoveredCallback)(BLEStatus status, BLEDevice * device, BLECharacteristic * characteristic) = NULL;
static void (*gattCharacteristicNotificationCallback)(BLEDevice * device, uint16_t value_handle, uint8_t* value, uint16_t length) = NULL;
static void (*gattCharacteristicIndicationCallback)(BLEDevice * device, uint16_t value_handle, uint8_t* value, uint16_t length) = NULL;
static void (*gattCharacteristicReadCallback)(BLEStatus status, BLEDevice * device, uint8_t * value, uint16_t length) = NULL;
static void (*gattCharacteristicWrittenCallback)(BLEStatus status, BLEDevice * device) = NULL;
static void (*gattCharacteristicSubscribedCallback)(BLEStatus status, BLEDevice * device) = NULL;
static void (*gattCharacteristicUnsubscribedCallback)(BLEStatus status, BLEDevice * device) = NULL;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    bd_addr_t addr;
    uint16_t handle;

    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    btstack_state = packet[2];
                    // bt stack activated, get started 
                    // if (packet[2] == HCI_STATE_WORKING) {
                    //     if (client_mode) {
                    //         emit_stack_ready();
                    //         return;
                    //     }
                    //     // printf("1 - hci_le_set_advertising_parameters\n");
                    // hci_send_cmd(&hci_le_set_advertising_parameters,  0x0400, 0x0800, 0, 0, 0, &addr, 0x07, 0);
                    // }
                    break;
                
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    if (bleDeviceDisconnectedCallback) {
                        handle = READ_BT_16(packet, 3);
                        BLEDevice device(handle);
                        (*bleDeviceDisconnectedCallback)(&device);
                    }
                    break;
                
                case GAP_LE_ADVERTISING_REPORT: {
                    if (bleAdvertismentCallback) {
                        BLEAdvertisement advertisement(packet);
                        (*bleAdvertismentCallback)(&advertisement);
                    }
                    break;
                }

                case HCI_EVENT_COMMAND_COMPLETE:
                    if (COMMAND_COMPLETE_EVENT(packet, hci_read_bd_addr)) {
                        bt_flip_addr(addr, &packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE + 1]);
                        printf("Local Address: %s\n", bd_addr_to_str(addr));
                        break;
                    }
                    // if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_parameters)){
                    //     printf("2 - hci_le_set_advertising_data\n");
                    //     hci_send_cmd(&hci_le_set_advertising_data, adv_data_len, adv_data);
                    //     break;
                    // }
                    // if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_data)){
                    //     printf("3 - hci_le_set_advertise_enable\n");
                    //     hci_send_cmd(&hci_le_set_advertise_enable, 1);
                    //     break;
                    // }
                    // if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertise_enable)){
                    //     emit_stack_ready();
                    //     break;
                    // }
                    break;

                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            handle = READ_BT_16(packet, 4);
                            printf("Connection complete, handle 0x%04x\n", handle);
                            run_loop_remove_timer(&connection_timer);
                            if (!bleDeviceConnectedCallback) break;
                            if (packet[3]){
                                (*bleDeviceConnectedCallback)(BLE_STATUS_CONNECTION_ERROR, NULL);
                            } else {
                                BLEDevice device(handle);
                                (*bleDeviceConnectedCallback)(BLE_STATUS_OK, &device);
                            }
                            break;
                        default:
                            break;
                    }
                    break;                    
            }
    }
    // if (client_packet_handler){
    //     (*client_packet_handler)(packet_type, channel, packet, size);
    // }
}

static void gatt_client_callback(le_event_t * event){
    // le_characteristic_t characteristic;
    // le_characteristic_value_event_t * value_event;
    gatt_complete_event_t * gatt_complete_event;

    BLEDevice device(event->handle);
    switch(event->type){
        case GATT_SERVICE_QUERY_RESULT:
            if (gattServiceDiscoveredCallback) {
                BLEService bleService(((le_service_event_t *) event)->service);
                (*gattServiceDiscoveredCallback)(BLE_STATUS_OK, &device, &bleService);
            }
            break;
        case GATT_CHARACTERISTIC_QUERY_RESULT:
            if (gattCharacteristicDiscoveredCallback){
                BLECharacteristic bleCharacteristic(((le_characteristic_event_t *) event)->characteristic);
               (*gattCharacteristicDiscoveredCallback)(BLE_STATUS_OK, &device, &bleCharacteristic);
            }
            break;
        case GATT_QUERY_COMPLETE:
            gatt_complete_event = (gatt_complete_event_t*) event; 
            switch (gattAction){
                case gattActionWrite:
                    if (gattCharacteristicWrittenCallback) gattCharacteristicWrittenCallback(gatt_complete_event->status ? BLE_STATUS_OTHER_ERROR : BLE_STATUS_OK, &device);
                    break;
                case gattActionSubscribe:
                    if (gattCharacteristicSubscribedCallback) gattCharacteristicSubscribedCallback(gatt_complete_event->status ? BLE_STATUS_OTHER_ERROR : BLE_STATUS_OK, &device);
                    break;
                case gattActionUnsubscribe:
                    if (gattCharacteristicUnsubscribedCallback) gattCharacteristicUnsubscribedCallback(gatt_complete_event->status ? BLE_STATUS_OTHER_ERROR : BLE_STATUS_OK, &device);
                    break;
                case gattActionServiceQuery:
                    if (gattServiceDiscoveredCallback) gattServiceDiscoveredCallback(BLE_STATUS_DONE, &device, NULL);
                    break;
                case gattActionCharacteristicQuery:
                    if (gattCharacteristicDiscoveredCallback) gattCharacteristicDiscoveredCallback(BLE_STATUS_DONE, &device, NULL);
                    break;
                default:
                    break;
            };
            break;
        case GATT_NOTIFICATION:
            if (gattCharacteristicNotificationCallback) {
                le_characteristic_value_event_t * value_event = (le_characteristic_value_event_t *) event;
                (*gattCharacteristicNotificationCallback)(&device, value_event->value_handle, value_event->blob, value_event->blob_length);
            }
            break;
        case GATT_INDICATION:
            if (gattCharacteristicIndicationCallback) {
                le_characteristic_value_event_t * value_event = (le_characteristic_value_event_t *) event;
                (*gattCharacteristicIndicationCallback)(&device, value_event->value_handle, value_event->blob, value_event->blob_length);
            }
            break;
        case GATT_CHARACTERISTIC_VALUE_QUERY_RESULT:
            if (gattCharacteristicReadCallback) {
                le_characteristic_value_event_t * value_event = (le_characteristic_value_event_t *) event;
                (*gattCharacteristicReadCallback)(BLE_STATUS_OK, &device, value_event->blob, value_event->blob_length);
            }
            break;
        default:
            break;
    }
}

static void connection_timeout_handler(timer_source_t * timer){
    // log_info("Cancel outgoing connection");
    le_central_connect_cancel();
    if (!bleDeviceConnectedCallback) return;
    (*bleDeviceConnectedCallback)(BLE_STATUS_CONNECTION_TIMEOUT, NULL);  // page timeout 0x04
}

//

static int nibble_for_char(const char c){
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return 0;
}

/// UUID class
UUID::UUID(){
    memset(uuid, 0, 16);
}

UUID::UUID(const uint8_t uuid[16]){
    memcpy(this->uuid, uuid, 16);
}

UUID::UUID(const char * uuidStr){
    memset(uuid, 0, 16);
    int len = strlen(uuidStr);
    if (len <= 4){
        // Handle 4 Bytes HEX
        uint16_t uuid16;
        int result = sscanf( (char *) uuidStr, "%x", &uuid16);
        if (result == 1){
            sdp_normalize_uuid(uuid, uuid16);
        }
        return;
    }
    
    // quick UUID parser, ignoring dashes
    int i = 0;
    int data = 0;
    int have_nibble = 0;
    while(*uuidStr && i < 16){
        const char c = *uuidStr++;
        if (c == '-') continue;
        data = data << 4 | nibble_for_char(c);
        if (!have_nibble) {
            have_nibble = 1;
            continue;
        }        
        uuid[i++] = data;
        data = 0;
        have_nibble = 0;
    }
}

const uint8_t * UUID::getUuid(void) const {
    return uuid;
}

static char uuid16_buffer[5];
const char * UUID::getUuidString() const {
    // TODO: fix sdp_has_blueooth_base_uuid call to use const 
    if (sdp_has_blueooth_base_uuid((uint8_t*)uuid)){
        sprintf(uuid16_buffer, "%04x", (uint16_t) READ_NET_32(uuid, 0));
        return uuid16_buffer;
    }  else {
        // TODO: fix uuid128_to_str
        return uuid128_to_str((uint8_t*)uuid);
    } 
}

const char * UUID::getUuid128String() const {
    return uuid128_to_str((uint8_t*)uuid);
}

bool UUID::matches(UUID *other)        const {
    return memcmp(this->uuid, other->uuid, 16) == 0;
}


// BD_ADDR class
BD_ADDR::BD_ADDR(){
}

BD_ADDR::BD_ADDR(const char * address_string, BD_ADDR_TYPE address_type ) : address_type(address_type) {
    // TODO: implement
    // log_error("BD_ADDR::BD_ADDR(const char *, BD_ADDR_TYPE) not implemented yet!");
}

BD_ADDR::BD_ADDR(const uint8_t address[6], BD_ADDR_TYPE address_type) : address_type(address_type){
    memcpy(this->address, address, 6);
}

const uint8_t * BD_ADDR::getAddress(){
    return address;
}

const char * BD_ADDR::getAddressString(){
    return bd_addr_to_str(address);
}

BD_ADDR_TYPE BD_ADDR::getAddressType(){
    return address_type;
}


BLEAdvertisement::BLEAdvertisement(uint8_t * event_packet) : 
advertising_event_type(event_packet[2]),
rssi(event_packet[10]),
data_length(event_packet[11])
{
    bd_addr_t addr;
    bt_flip_addr(addr, &event_packet[4]);    
    bd_addr = BD_ADDR(addr, (BD_ADDR_TYPE)event_packet[3]);
    memcpy(data, &event_packet[12], LE_ADVERTISING_DATA_SIZE);
}

const uint8_t * BLEAdvertisement::getAdvData(){
    return data;
}

BD_ADDR * BLEAdvertisement::getBdAddr(void){
    return &bd_addr;
}

int BLEAdvertisement::getRssi(){
    return rssi > 127 ? rssi - 256 : rssi;
}


bool BLEAdvertisement::containsService(UUID * service){
    return ad_data_contains_uuid128(data_length, data, (uint8_t*) service->getUuid());
}

bool BLEAdvertisement::nameHasPrefix(const char * namePrefix){
    return false;
};

bool BLEAdvertisement::isIBeacon(){
    return ((memcmp(iBeaconAdvertisement01,  data,    sizeof(iBeaconAdvertisement01)) == 0) 
      &&    (memcmp(iBeaconAdvertisement38, &data[3], sizeof(iBeaconAdvertisement38)) == 0));
}

const UUID * BLEAdvertisement::getIBeaconUUID(){
    return new UUID(&data[9]);
};
uint16_t     BLEAdvertisement::getIBeaconMajorID(){
    return READ_NET_16(data, 25);
};
uint16_t     BLEAdvertisement::getIBecaonMinorID(){
    return READ_NET_16(data, 27);
};
uint8_t      BLEAdvertisement::getiBeaconMeasuredPower(){
    return data[29];
}


BLECharacteristic::BLECharacteristic() {
}

BLECharacteristic::BLECharacteristic(le_characteristic_t characteristic)
: characteristic(characteristic), uuid(characteristic.uuid128) {        
}

const UUID * BLECharacteristic::getUUID(){
    return &uuid;
}

bool BLECharacteristic::matches(UUID * uuid){
    return this->uuid.matches(uuid);
}

bool BLECharacteristic::isValueHandle(uint16_t value_handle){
    return characteristic.value_handle == value_handle;
}

const le_characteristic_t * BLECharacteristic::getCharacteristic() {
    return &characteristic; 
}


BLEService::BLEService(){
}

BLEService::BLEService(le_service_t service)
: service(service), uuid(service.uuid128){
}

const UUID * BLEService::getUUID(){
    return &uuid;
}

bool BLEService::matches(UUID * uuid){
    return this->uuid.matches(uuid);
}

const le_service_t * BLEService::getService(){
    return &service;
}

// discovery of services and characteristics
BLEDevice::BLEDevice(){
}
BLEDevice::BLEDevice(uint16_t handle)
: handle(handle){
}
uint16_t BLEDevice::getHandle(){
    return handle;
}
int BLEDevice::discoverGATTServices(){
    return BTstack.discoverGATTServices(this);
}
int BLEDevice::discoverCharacteristicsForService(BLEService * service){
    return BTstack.discoverCharacteristicsForService(this, service);
}
int BLEDevice::readCharacteristic(BLECharacteristic * characteristic){
    return BTstack.readCharacteristic(this, characteristic);
}
int BLEDevice::writeCharacteristic(BLECharacteristic * characteristic, uint8_t * data, uint16_t size){
    return BTstack.writeCharacteristic(this, characteristic, data, size);
}
int BLEDevice::writeCharacteristicWithoutResponse(BLECharacteristic * characteristic, uint8_t * data, uint16_t size){
    return BTstack.writeCharacteristicWithoutResponse(this, characteristic, data, size);
}
int BLEDevice::subscribeForNotifications(BLECharacteristic * characteristic){
    return BTstack.subscribeForNotifications(this, characteristic);
}
int BLEDevice::unsubscribeFromNotifications(BLECharacteristic * characteristic){
    return BTstack.unsubscribeFromNotifications(this, characteristic);
}
int BLEDevice::subscribeForIndications(BLECharacteristic * characteristic){
    return BTstack.subscribeForIndications(this, characteristic);
}
int BLEDevice::unsubscribeFromIndications(BLECharacteristic * characteristic){
    return BTstack.unsubscribeFromIndications(this, characteristic);
}


BTstackManager::BTstackManager() {
    // client_packet_handler = NULL;
    have_custom_addr = false;
    // reset handler
    bleAdvertismentCallback = NULL;
    bleDeviceConnectedCallback = NULL;
    bleDeviceDisconnectedCallback = NULL;
    gattServiceDiscoveredCallback = NULL;
    gattCharacteristicDiscoveredCallback = NULL;
    gattCharacteristicNotificationCallback = NULL;
}

void BTstackManager::setBLEAdvertisementCallback(void (*callback)(BLEAdvertisement * bleAdvertisement)){
    bleAdvertismentCallback = callback;
}
void BTstackManager::setBLEDeviceConnectedCallback(void (*callback)(BLEStatus status, BLEDevice * device)){
    bleDeviceConnectedCallback = callback;
}
void BTstackManager::setBLEDeviceDisconnectedCallback(void (*callback)(BLEDevice * device)){
    bleDeviceDisconnectedCallback = callback;
}
void BTstackManager::setGATTServiceDiscoveredCallback(void (*callback)(BLEStatus status, BLEDevice * device, BLEService * bleService)){
    gattServiceDiscoveredCallback = callback;
}
void BTstackManager::setGATTCharacteristicDiscoveredCallback(void (*callback)(BLEStatus status, BLEDevice * device, BLECharacteristic * characteristic)){
    gattCharacteristicDiscoveredCallback = callback;
}
void BTstackManager::setGATTCharacteristicNotificationCallback(void (*callback)(BLEDevice * device, uint16_t value_handle, uint8_t* value, uint16_t length)){
    gattCharacteristicNotificationCallback = callback;
}
void BTstackManager::setGATTCharacteristicIndicationCallback(void (*callback)(BLEDevice * device, uint16_t value_handle, uint8_t* value, uint16_t length)){
    gattCharacteristicIndicationCallback = callback;
}
void BTstackManager::setGATTCharacteristicReadCallback(void (*callback)(BLEStatus status, BLEDevice * device, uint8_t * value, uint16_t length)){
    gattCharacteristicReadCallback = callback;
}
void BTstackManager::setGATTCharacteristicWrittenCallback(void (*callback)(BLEStatus status, BLEDevice * device)){
    gattCharacteristicWrittenCallback = callback;
}
void BTstackManager::setGATTCharacteristicSubscribedCallback(void (*callback)(BLEStatus status, BLEDevice * device)){
    gattCharacteristicSubscribedCallback = callback;
}
void BTstackManager::setGATTCharacteristicUnsubscribedCallback(void (*callback)(BLEStatus status, BLEDevice * device)){
    gattCharacteristicUnsubscribedCallback = callback;
}

int BTstackManager::discoverGATTServices(BLEDevice * device){
    gattAction = gattActionServiceQuery;
    return gatt_client_discover_primary_services(gatt_client_id, device->getHandle());
}
int BTstackManager::discoverCharacteristicsForService(BLEDevice * device, BLEService * service){
    gattAction = gattActionCharacteristicQuery;
    return gatt_client_discover_characteristics_for_service(gatt_client_id, device->getHandle(), (le_service_t*) service->getService());
}
int  BTstackManager::readCharacteristic(BLEDevice * device, BLECharacteristic * characteristic){
    return gatt_client_read_value_of_characteristic(gatt_client_id, device->getHandle(), (le_characteristic_t*) characteristic->getCharacteristic());
}
int  BTstackManager::writeCharacteristic(BLEDevice * device, BLECharacteristic * characteristic, uint8_t * data, uint16_t size){
    gattAction = gattActionWrite;
    return gatt_client_write_value_of_characteristic(gatt_client_id, device->getHandle(), characteristic->getCharacteristic()->value_handle,
        size, data);
}
int  BTstackManager::writeCharacteristicWithoutResponse(BLEDevice * device, BLECharacteristic * characteristic, uint8_t * data, uint16_t size){
    return gatt_client_write_value_of_characteristic_without_response(gatt_client_id, device->getHandle(), characteristic->getCharacteristic()->value_handle,
         size, data);
}
int BTstackManager::subscribeForNotifications(BLEDevice * device, BLECharacteristic * characteristic){
    gattAction = gattActionSubscribe;
    return gatt_client_write_client_characteristic_configuration(gatt_client_id, device->getHandle(), (le_characteristic_t*) characteristic->getCharacteristic(),
     GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
}
int BTstackManager::subscribeForIndications(BLEDevice * device, BLECharacteristic * characteristic){
    gattAction = gattActionSubscribe;
    return gatt_client_write_client_characteristic_configuration(gatt_client_id, device->getHandle(), (le_characteristic_t*) characteristic->getCharacteristic(),
     GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION);
}
int BTstackManager::unsubscribeFromNotifications(BLEDevice * device, BLECharacteristic * characteristic){
    gattAction = gattActionUnsubscribe;
    return gatt_client_write_client_characteristic_configuration(gatt_client_id, device->getHandle(), (le_characteristic_t*) characteristic->getCharacteristic(),
     GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NONE);
}
int BTstackManager::unsubscribeFromIndications(BLEDevice * device, BLECharacteristic * characteristic){
    gattAction = gattActionUnsubscribe;
    return gatt_client_write_client_characteristic_configuration(gatt_client_id, device->getHandle(), (le_characteristic_t*) characteristic->getCharacteristic(),
     GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NONE);
}
void BTstackManager::bleConnect(BLEAdvertisement * advertisement, int timeout_ms){
    bleConnect(advertisement->getBdAddr(), timeout_ms);
}
void BTstackManager::bleConnect(BD_ADDR * address, int timeout_ms){
    bleConnect(address->getAddressType(), address->getAddress(), timeout_ms);
}
void BTstackManager::bleConnect(BD_ADDR_TYPE address_type, const char * address, int timeout_ms){
    // TOOD: implement
    // log_error("BTstackManager::bleConnect(BD_ADDR_TYPE address_type, const char * address, int timeout_ms) not implemented");
}
void BTstackManager::bleConnect(BD_ADDR_TYPE address_type, const uint8_t address[6], int timeout_ms){
    le_central_connect((uint8_t*)address, (bd_addr_type_t) address_type);
    if (!timeout_ms) return;
    run_loop_set_timer(&connection_timer, timeout_ms);
    run_loop_set_timer_handler(&connection_timer, connection_timeout_handler);
    run_loop_add_timer(&connection_timer);
}

void BTstackManager::bleDisconnect(BLEDevice * device){
    run_loop_remove_timer(&connection_timer);
}

void BTstackManager::registerPacketHandler(btstack_packet_handler_t packet_handler){
    // client_packet_handler = packet_handler;
}

void BTstackManager::setClientMode(){
    client_mode = 1;
}

void BTstackManager::setAdvData(uint16_t size, const uint8_t * data){
    adv_data = data;
    adv_data_len = size;
}

void BTstackManager::setPublicBdAddr(bd_addr_t addr){
    have_custom_addr = true;
    memcpy(public_bd_addr, addr ,6);
}

// static hci_uart_config_t config;

void BTstackManager::setup(){

#ifdef PIN_LED
    pinMode(PIN_LED, OUTPUT);
#endif

    printf("BTstackManager::setup()\n");

	btstack_memory_init();
    run_loop_init(RUN_LOOP_EMBEDDED);

    hci_dump_open(NULL, HCI_DUMP_STDOUT);

	hci_transport_t * transport = hci_transport_h4_dma_instance();
    bt_control_t    * control   = bt_control_em9301_instance();
	hci_init(transport, NULL, control, NULL);

    if (have_custom_addr){
        hci_set_bd_addr(public_bd_addr);
    }

    l2cap_init();

    // setup central device db
    le_device_db_init();

    sm_init();
    
    att_server_init(NULL, NULL, NULL);    
    att_server_register_packet_handler(packet_handler);

    gatt_client_init();
    gatt_client_id = gatt_client_register_packet_handler(gatt_client_callback);

    // turn on!
    btstack_state = 0;
    hci_power_control(HCI_POWER_ON);

    // poll until working
    while (btstack_state != HCI_STATE_WORKING){
        loop();
    }
    printf("--> READY <--\n");
}

void BTstackManager::loop(){
    // process data from/to Bluetooth module
    hal_uart_dma_process();
    // BTstack Run Loop
    embedded_execute_once();
}

void BTstackManager::bleStartScanning(){
    printf("Start scanning\n");
    le_central_start_scan();
}
void BTstackManager::bleStopScanning(){
    le_central_stop_scan();
}

BTstackManager BTstack;

