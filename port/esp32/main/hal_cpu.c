#include "hal_cpu.h"

#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"

static portMUX_TYPE global_int_mux = portMUX_INITIALIZER_UNLOCKED;

void hal_cpu_disable_irqs(void){
    //printf("hal_cpu_disable_irqs\n");
    portENTER_CRITICAL(&global_int_mux);
}

void hal_cpu_enable_irqs(void){
    //printf("hal_cpu_enable_irqs\n");
    portEXIT_CRITICAL(&global_int_mux);
}

void hal_cpu_enable_irqs_and_sleep(void){
    //printf("hal_cpu_enable_irqs_and_sleep\n");
    portEXIT_CRITICAL(&global_int_mux);
    // @TODO
}
