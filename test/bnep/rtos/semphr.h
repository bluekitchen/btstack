#ifndef TEST_SEMPHR_H
#define TEST_SEMPHR_H
#include "FreeRTOS.h"
typedef void * SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, uint32_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
#endif
