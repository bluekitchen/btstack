#ifndef TEST_QUEUE_H
#define TEST_QUEUE_H
#include "FreeRTOS.h"
typedef void * QueueHandle_t;
QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t size);
BaseType_t xQueueSendToBack(QueueHandle_t queue, const void * item, uint32_t timeout);
BaseType_t xQueueReceive(QueueHandle_t queue, void * item, uint32_t timeout);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);
void vQueueDelete(QueueHandle_t queue);
#endif
