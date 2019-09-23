#include <stdint.h>

typedef int StaticQueue_t;
typedef int QueueHandle_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef int TickType_t;

#define pdMS_TO_TICKS(ms) ms

BaseType_t xQueueSendToBack(
                                   QueueHandle_t xQueue,
                                   const void * pvItemToQueue,
                                   TickType_t xTicksToWait
                               );

 BaseType_t xQueueSendToBackFromISR
                    (
                        QueueHandle_t xQueue,
                        const void *pvItemToQueue,
                        BaseType_t *pxHigherPriorityTaskWoken
                    );
                    
BaseType_t xQueueReceive(
                       QueueHandle_t xQueue,
                       void *pvBuffer,
                       TickType_t xTicksToWait
                    );

QueueHandle_t xQueueCreateStatic(
                             UBaseType_t uxQueueLength,
                             UBaseType_t uxItemSize,
                             uint8_t *pucQueueStorageBuffer,
                             StaticQueue_t *pxQueueBuffer );

