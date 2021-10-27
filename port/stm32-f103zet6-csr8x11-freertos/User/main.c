#include "includes.h"

#if 1
static void AppTaskCreate (void);
static TaskHandle_t vHandleTaskATCommand = NULL;
static TaskHandle_t xHandleTaskBT = NULL;
//extern void vBT_Task(void *pvParameters);
#endif

int main(void)
{

	__set_PRIMASK(1);  
	
	bsp_Init(); 
	
	at_command_init();

	cunit_init();

	AppTaskCreate();
	
    vTaskStartScheduler();
}

#if 1

void vBT_Task(void *pvParameters)
{
    uint32_t i = 0;
    printf("this is bt task! \r\n");
	while(1)
    {
        printf("this is bt task! i = %d! \r\n", i);
        i++;
    }
}

static void AppTaskCreate (void)
{
    xTaskCreate( vTaskATCommand,
                 "vTaskATCommand",     	
                 512,               	
                 NULL,              	
                 2,                 	
                 &vHandleTaskATCommand );  
	
    xTaskCreate( vBT_Task,	      	    
                 "vBT_Task",     	    
                 512,               	
                 NULL,              	
                 4,                 	
                 &xHandleTaskBT );
}
#endif
