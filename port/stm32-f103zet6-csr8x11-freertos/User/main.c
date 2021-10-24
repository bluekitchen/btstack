#include "includes.h"

#if 1
static void AppTaskCreate (void);

static TaskHandle_t vHandleTaskATCommand = NULL;
//static TaskHandle_t xHandleTaskBT = NULL;
//extern void vBT_Task(void *pvParameters);
#endif

int main(void)
{

	__set_PRIMASK(1);  
	
	bsp_Init(); 
	
	//bt_init();
	
	at_command_init();

	cunit_init();

	AppTaskCreate();
	
    vTaskStartScheduler();

	while(1);
}

#if 1
static void AppTaskCreate (void)
{
    xTaskCreate( vTaskATCommand,
                 "vTaskATCommand",     	
                 512,               	
                 NULL,              	
                 2,                 	
                 &vHandleTaskATCommand );  
	
    /*xTaskCreate( vBT_Task,	      	    
                 "vBT_Task",     	    
                 512,               	
                 NULL,              	
                 4,                 	
                 &xHandleTaskBT );*/
}
#endif
