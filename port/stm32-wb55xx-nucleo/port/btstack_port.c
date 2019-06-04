/*
 *        File: btstack_port.c
 *  Created on: 2019
 *      Author: ftrefou
 * 
 * Header is TODO
 */

#define BTSTACK_FILE__ "port.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"


/********************************************
 *      system calls implementation
 *******************************************/
extern UART_HandleTypeDef hTuart;

int _write(int file, char* ptr, int len)
{
    HAL_UART_Transmit(&hTuart, (uint8_t*)ptr, len, 1000);

    return len;
}

int _read(int file, char* ptr, int len)
{
    HAL_UART_Receive(&hTuart, (uint8_t*)ptr, len, 1000);

    return len;
}

/********************************************
 *      hal_time_ms.h implementation
 *******************************************/
#include "hal_time_ms.h"
uint32_t hal_time_ms(void)
{
	return HAL_GetTick();
}


/*******************************************
 *       hal_cpu.h implementation
 ******************************************/
#include "hal_cpu.h"

void hal_cpu_disable_irqs(void){
    __disable_irq();
}

void hal_cpu_enable_irqs(void){
    __enable_irq();
}

void hal_cpu_enable_irqs_and_sleep(void){
    __enable_irq();
    __asm__("wfe"); // go to sleep if event flag isn't set. if set, just clear it. IRQs set event flag
}



/*******************************************
 *       transport implementation
 ******************************************/

#include "btstack.h"
#include "btstack_config.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_freertos.h"
#include "hci.h"
#include "hci_dump.h"
#include "btstack_debug.h"
#include "ble/gatt-service/battery_service_server.h"

#include "app_conf.h"
#include "stm32_wpan_common.h"
#include "tl.h"
#include "shci_tl.h"
#include "shci.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


#define POOL_SIZE (CFG_TLBLE_EVT_QUEUE_LENGTH * 4 * \
		DIVC((sizeof(TL_PacketHeader_t) + TL_BLE_EVENT_FRAME_SIZE), 4))

/* Private variables ---------------------------------------------------------*/
PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t BleCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t EvtPool[POOL_SIZE];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static TL_CmdPacket_t SystemCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t SystemSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t	BleSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t	HciAclDataBuffer[sizeof(TL_PacketHeader_t) + 5 + 251];
    
static void (*transport_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static SemaphoreHandle_t C2StartedSem;
static SemaphoreHandle_t shciSem;
static QueueHandle_t hciEvtQueue;
static int hci_acl_can_send_now;

// data source for integration with BTstack Runloop
static btstack_data_source_t transport_data_source;




void shci_cmd_resp_release(uint32_t flag)
{
    UNUSED(flag);
    
    static BaseType_t yield = pdFALSE;

    xSemaphoreGiveFromISR(shciSem, &yield);

    portYIELD_FROM_ISR(yield);
}

void shci_cmd_resp_wait(uint32_t timeout)
{
    UNUSED(timeout);
    xSemaphoreTake(shciSem, portMAX_DELAY);
}

void shci_notify_asynch_evt(void* pdata)
{
    UNUSED(pdata);
	shci_user_evt_proc(); //TODO should not be called from isr context
}

void ipcc_reset(void)
{
	/* Reset IPCC */
	LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_IPCC);

	LL_C1_IPCC_ClearFlag_CHx(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	LL_C2_IPCC_ClearFlag_CHx(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	LL_C1_IPCC_DisableTransmitChannel(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	LL_C2_IPCC_DisableTransmitChannel(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	LL_C1_IPCC_DisableReceiveChannel(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	LL_C2_IPCC_DisableReceiveChannel(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);
}

// VHCI callbacks, run from VHCI Task "BT Controller"
static void ble_Init_and_start(void)
{
    SHCI_CmdStatus_t st;
	SHCI_C2_Ble_Init_Cmd_Packet_t ble_init_cmd_packet = {
	  { { 0, 0, 0 } },                     /**< Header unused */
	  { 0,                                 /** pBleBufferAddress not used */
	    0,                                 /** BleBufferSize not used */
	    0,
	    0,
	    0,
	    CFG_BLE_NUM_LINK,
	    CFG_BLE_DATA_LENGTH_EXTENSION,
	    CFG_BLE_PREPARE_WRITE_LIST_SIZE,
	    CFG_BLE_MBLOCK_COUNT,
	    CFG_BLE_MAX_ATT_MTU,
	    CFG_BLE_SLAVE_SCA,
	    CFG_BLE_MASTER_SCA,
	    CFG_BLE_LSE_SOURCE,
	    CFG_BLE_MAX_CONN_EVENT_LENGTH,
	    CFG_BLE_HSE_STARTUP_TIME,
	    CFG_BLE_VITERBI_MODE,
	    CFG_BLE_LL_ONLY,
	    0 }
	};

	/**
	 * Starts the BLE Stack on CPU2
	 */
	if ((st = SHCI_C2_BLE_Init(&ble_init_cmd_packet)) != SHCI_Success)
        log_error("BLE stack not running on CPU2 : 0x%02X", st);
}

static void sys_evt_received(void *pdata)
{
    static BaseType_t yield = pdFALSE;

    TL_EvtSerial_t* shciEvt = &((tSHCI_UserEvtRxParam*)pdata)->pckt->evtserial;

    if (shciEvt->evt.evtcode == SHCI_EVTCODE)
    {
        if (little_endian_read_16(shciEvt->evt.payload, 0) == SHCI_SUB_EVT_CODE_READY)
            xSemaphoreGiveFromISR(C2StartedSem, &yield);
    }

    portYIELD_FROM_ISR(yield);
}

static void ble_evt_received(TL_EvtPacket_t *hcievt)
{
    static BaseType_t yield = pdFALSE;

    xQueueSendFromISR(hciEvtQueue, (void*)&hcievt, &yield);

    portYIELD_FROM_ISR(yield);
}

static void ble_acl_acknowledged(void)
{
    hci_acl_can_send_now = 1;
}


// run from main thread

static void transport_notify_packet_send(void){
    // notify upper stack that it might be possible to send again
    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    transport_packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
}

static void transport_send_hardware_error(uint8_t error_code){
    uint8_t event[] = { HCI_EVENT_HARDWARE_ERROR, 1, error_code};
    transport_packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
}

static void transport_deliver_hci_packets(void){
    TL_EvtPacket_t *hcievt;
	TL_AclDataSerial_t *acl;

    while (xQueueReceive(hciEvtQueue, &hcievt, portMAX_DELAY) == pdTRUE)
    {
        switch (hcievt->evtserial.type)
        {
            case HCI_EVENT_PACKET:
                /* Send buffer to upper stack */
                transport_packet_handler(
                    hcievt->evtserial.type,
                    (uint8_t*)&hcievt->evtserial.evt,
                    hcievt->evtserial.evt.plen+2);
                break;

            case HCI_ACL_DATA_PACKET:
		        acl = &(((TL_AclDataPacket_t *)hcievt)->AclDataSerial);
                /* Send buffer to upper stack */
                transport_packet_handler(
                    acl->type,
                    &((uint8_t*)acl)[1],
                    acl->length+4);
                break;
            
            default:
                transport_send_hardware_error(0x01);  // invalid HCI packet
                break;
        }
        
        /* Release buffer for memory manager */
        TL_MM_EvtDone(hcievt);
    }
}

static void transport_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_POLL:
            transport_deliver_hci_packets();
            break;
        default:
            break;
    }
}

/**
 * init transport
 * @param transport_config
 */
static void transport_init(const void *transport_config){
	TL_MM_Config_t tl_mm_config;
	TL_BLE_InitConf_t tl_ble_config;
	SHCI_TL_HciInitConf_t shci_init_config;

    log_info("transport_init");

	/* Take BLE out of reset */
	ipcc_reset();

	log_debug("shared SRAM2 buffers");
	log_debug(" *BleCmdBuffer          : 0x%08X", (void *)&BleCmdBuffer);
	log_debug(" *HciAclDataBuffer      : 0x%08X", (void *)&HciAclDataBuffer);
	log_debug(" *SystemCmdBuffer       : 0x%08X", (void *)&SystemCmdBuffer);
	log_debug(" *EvtPool               : 0x%08X", (void *)&EvtPool);
	log_debug(" *SystemSpareEvtBuffer  : 0x%08X", (void *)&SystemSpareEvtBuffer);
	log_debug(" *BleSpareEvtBuffer     : 0x%08X", (void *)&BleSpareEvtBuffer);

    /**< FreeRTOS implementation variables initialization */
    C2StartedSem = xSemaphoreCreateBinary();
    shciSem = xSemaphoreCreateBinary();
    hciEvtQueue = xQueueCreate(CFG_TLBLE_EVT_QUEUE_LENGTH, sizeof(TL_EvtPacket_t*));
    hci_acl_can_send_now = 1;

	/**< Reference table initialization */
	TL_Init();

	/**< System channel initialization */
	shci_init_config.p_cmdbuffer = (uint8_t *)&SystemCmdBuffer;
	shci_init_config.StatusNotCallBack = NULL;
	shci_init(sys_evt_received, (void *) &shci_init_config);

	/**< Memory Manager channel initialization */
	tl_mm_config.p_BleSpareEvtBuffer = BleSpareEvtBuffer;
	tl_mm_config.p_SystemSpareEvtBuffer = SystemSpareEvtBuffer;
	tl_mm_config.p_AsynchEvtPool = EvtPool;
	tl_mm_config.AsynchEvtPoolSize = POOL_SIZE;
	TL_MM_Init(&tl_mm_config);

	TL_Enable();
    
	/**< BLE channel initialization */
	tl_ble_config.p_cmdbuffer = (uint8_t *)&BleCmdBuffer;
	tl_ble_config.p_AclDataBuffer = HciAclDataBuffer;
	tl_ble_config.IoBusEvtCallBack = ble_evt_received;
	tl_ble_config.IoBusAclDataTxAck = ble_acl_acknowledged;
	TL_BLE_Init((void *)&tl_ble_config);

    // set up polling data_source
    btstack_run_loop_set_data_source_handler(&transport_data_source, &transport_process);
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&transport_data_source);
}

/**
 * open transport connection
 */
static int transport_open(void){
    log_info("transport_open");

	/* Device will let us know when it's ready */
    xSemaphoreTake(C2StartedSem, portMAX_DELAY);
	log_info("BLE stack on CPU 2 running");

	ble_Init_and_start();
    
    return 0;
}

/**
 * close transport connection
 */
static int transport_close(void){
    log_info("transport_close");

    return 0;
}

/**
 * register packet handler for HCI packets: ACL and Events
 */
static void transport_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    log_info("transport_register_packet_handler");
    transport_packet_handler = handler;
}

/**
 * support async transport layers, e.g. IRQ driven without buffers
 */
static int transport_can_send_packet_now(uint8_t packet_type) {
    switch (packet_type)
    {
        case HCI_COMMAND_DATA_PACKET:
            return 1;

        case HCI_ACL_DATA_PACKET:
            return hci_acl_can_send_now;
    }
    return 1;
}

/**
 * send packet
 */
static int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){
	TL_CmdPacket_t *ble_cmd_buff = &BleCmdBuffer;

    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            ble_cmd_buff->cmdserial.type = packet_type;
            ble_cmd_buff->cmdserial.cmd.plen = size;
            memcpy((void *)&ble_cmd_buff->cmdserial.cmd, packet, size);
            TL_BLE_SendCmd(NULL, 0);
            transport_notify_packet_send();
            break;

        case HCI_ACL_DATA_PACKET:
            memcpy((void *)&((TL_AclDataPacket_t *)HciAclDataBuffer)->AclDataSerial,packet, size);
            TL_BLE_SendAclData(NULL, 0);
            transport_notify_packet_send();
            break;

        default:
            transport_send_hardware_error(0x01);  // invalid HCI packet
            break;
    }
    return 0;  
}

static const hci_transport_t transport = {
    "stm32wb-vhci",
    &transport_init,
    &transport_open,
    &transport_close,
    &transport_register_packet_handler,
    &transport_can_send_packet_now,
    &transport_send_packet,
    NULL, // set baud rate
    NULL, // reset link
    NULL, // set SCO config
};


static const hci_transport_t * transport_get_instance(void){
    return &transport;
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            printf("BTstack: up and running.\n");
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_local_version_information)){
                // @TODO
            }
            break;
        default:
            break;
    }
}

extern int btstack_main(int argc, const char * argv[]);
void port_thread(void* args){

    // enable packet logger
    //hci_dump_open(NULL, HCI_DUMP_STDOUT);

    /// GET STARTED with BTstack ///
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_freertos_get_instance());

    // init HCI
    hci_init(transport_get_instance(), NULL);

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    btstack_main(0, NULL);

    log_info("btstack executing run loop...");
    btstack_run_loop_execute();
}