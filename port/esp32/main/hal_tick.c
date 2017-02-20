#include "hal_tick.h"

#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"

static void dummy_handler(void){};

static void (*tick_handler)(void) = &dummy_handler;

void hal_tick_init(void){
    printf("hal_tick_init\n");
}

void hal_tick_set_handler(void (*handler)(void)){
    printf("hal_tick_set_handler\n");
    if (handler == NULL){
        tick_handler = &dummy_handler;
        return;
    }
    tick_handler = handler;
}

int  hal_tick_get_tick_period_in_ms(void){
    printf("hal_tick_get_tick_period_in_ms\n");
    return portTICK_PERIOD_MS;
}
